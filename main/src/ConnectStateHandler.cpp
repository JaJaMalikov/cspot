#include "ConnectStateHandler.h"

#include <cJSON.h>
#include "ContextTrackResolver.h"
#include "SessionContext.h"
#include "api/SpClient.h"
#include "bell/Logger.h"
#include "bell/Result.h"
#include "connect.pb.h"
#include "events/EventLoop.h"
#include "mbedtls/base64.h"
#include "metadata.pb.h"
#include "proto/SpotifyId.h"

using namespace cspot;

namespace {
std::string spircVersion = "3.2.6";
std::string deviceSoftwareVersion = "1.0.0";
std::string clientId = "65b708073fc0480ea92a077233ca87bd";  // Spotify client ID
std::string connectCapabilities;
std::vector<std::string> supportedTypes = {"audio/track", "audio/episode"};
std::string sessionIdChars =
    "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

// Generates a random session ID of 16 characters
std::string generateSessionId() {
  static std::independent_bits_engine<std::default_random_engine, CHAR_BIT,
                                      unsigned char>
      randomEngine;
  std::string sessionId;
  sessionId.reserve(16);  // Reserve space for 16 characters

  std::generate_n(std::back_inserter(sessionId), 16, []() {
    return sessionIdChars[randomEngine() % sessionIdChars.size()];
  });
  return sessionId;
}
};  // namespace

ConnectStateHandler::ConnectStateHandler(
    std::shared_ptr<SessionContext> sessionContext,
    std::shared_ptr<SpClient> spClient)
    : sessionContext(std::move(sessionContext)), spClient(std::move(spClient)) {
  trackProvider =
      std::make_shared<TrackProvider>(this->sessionContext, this->spClient);

  // this->sessionContext->eventLoop->registerHandler(
  //     EventLoop::EventType::CURRENT_TRACK_METADATA,
  //     [this](cspot::EventLoop::Event&& event) {
  //       auto& currentTrackMetadata =
  //           std::get<cspot::CurrentTrackMetadata>(event.payload);

  //       auto& playerState = putStateRequestProto.device.playerState;
  //       playerState.duration = currentTrackMetadata.durationMs;
  //     });

  // this->sessionContext->eventLoop->registerHandler(
  //     EventLoop::EventType::TRACKPROVIDER_UPDATED,
  //     [this](cspot::EventLoop::Event&& event) {
  //       auto& playerState = putStateRequestProto.device.playerState;
  //       // playerState.track = trackProvider->getCurrentTrack();
  //       playerState.nextTracks = trackProvider->getNextTracks();
  //       playerState.prevTracks = trackProvider->getPreviousTracks();

  //       auto currentContextIndex = trackProvider->getCurrentContextIndex();
  //       if (currentContextIndex.has_value()) {
  //         playerState.index.value = currentContextIndex.value();
  //         playerState.index.hasValue = true;
  //       } else {
  //         playerState.index.hasValue = false;
  //       }

  //       BELL_LOG(info, LOG_TAG, "Updated player state with current track: {}",
  //                playerState.track.uri);
  //       // Update state
  //       this->putState();
  //     });

  initialize();
}

void ConnectStateHandler::initialize() {
  auto& deviceProto = putStateRequestProto.device;

  auto& deviceInfo = deviceProto.deviceInfo;
  deviceInfo.canPlay = true;
  deviceInfo.volume = 100;
  deviceInfo.name = sessionContext->loginBlob->getDeviceName();

  deviceInfo.deviceType = DeviceType_SPEAKER;
  deviceInfo.deviceSoftwareVersion = deviceSoftwareVersion;
  deviceInfo.deviceId = sessionContext->loginBlob->getDeviceId();
  deviceInfo.clientId = clientId;
  deviceInfo.spircVersion = spircVersion;

  auto& capabilities = deviceInfo.capabilities.rawProto;

  // Init capatilities
  capabilities.can_be_player = true;
  capabilities.restrict_to_local = false;
  capabilities.gaia_eq_connect_id = true;
  capabilities.supports_logout = true;
  capabilities.is_observable = true;
  capabilities.volume_steps = 100;
  capabilities.command_acks = true;
  capabilities.supports_rename = false;
  capabilities.hidden = false;
  capabilities.disable_volume = false;
  capabilities.connect_disabled = false;
  capabilities.supports_playlist_v2 = true;
  capabilities.is_controllable = true;
  capabilities.supports_external_episodes = false;
  capabilities.supports_set_backend_metadata = true;
  capabilities.supports_transfer_command = true;
  capabilities.supports_command_request = true;
  capabilities.is_voice_enabled = false;
  capabilities.needs_full_player_state = false;
  capabilities.supports_set_options_command = true;
  capabilities.supports_gzip_pushes = false;  // TODO: Should we support this?
  capabilities.has_supports_hifi = false;

  deviceInfo.capabilities.supportedTypes = supportedTypes;

  auto& playerState = deviceProto.playerState;
  playerState.isSystemInitiated = true;

  // Assign next and previous tracks encode callbacks
  playerState.nextTracks.funcs.encode = TrackProvider::pbEncodeNextTracks;
  playerState.prevTracks.funcs.encode = TrackProvider::pbEncodePreviousTracks;
  playerState.nextTracks.arg = trackProvider.get();
  playerState.prevTracks.arg = trackProvider.get();
}

bell::Result<> ConnectStateHandler::handlePlayerCommand(cJSON* messageJson) {
  cJSON* payload = cJSON_GetObjectItem(messageJson, "payload");
  cJSON* command = cJSON_GetObjectItem(payload, "command");
  cJSON* endpointItem = cJSON_GetObjectItem(command, "endpoint");
  if (!cJSON_IsString(endpointItem)) {
    return std::errc::bad_message;
  }
  std::string endpoint = endpointItem->valuestring;

  // Assign the last message ID and device ID
  cJSON* msgId = cJSON_GetObjectItem(payload, "message_id");
  cJSON* sentBy = cJSON_GetObjectItem(payload, "sent_by_device_id");
  if (cJSON_IsNumber(msgId))
    putStateRequestProto.lastCommandMessageId = msgId->valueint;
  if (cJSON_IsString(sentBy))
    putStateRequestProto.lastCommandSentByDeviceId = sentBy->valuestring;

  if (endpoint == "transfer") {
    BELL_LOG(info, LOG_TAG, "Received transfer command");
    cJSON* dataItem = cJSON_GetObjectItem(command, "data");
    cJSON* optionsItem = cJSON_GetObjectItem(command, "options");
    std::string payloadDataStr = cJSON_IsString(dataItem) ? dataItem->valuestring : "";
    return handleTransferCommand(payloadDataStr, optionsItem);
  } else if (endpoint == "skip_next") {
    BELL_LOG(info, LOG_TAG, "Received skip_next command");
    return handleSkipNextCommand();
  } else {
    BELL_LOG(info, LOG_TAG, "Received unknown command: {}", endpoint);
    return std::errc::operation_not_supported;
  }

  return {};
}

bell::Result<> ConnectStateHandler::putState(PutStateReason reason) {
  // get milliseconds since epoch;
  putStateRequestProto.clientSideTimestamp =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count();
  putStateRequestProto.memberType = MemberType_CONNECT_STATE;
  putStateRequestProto.putStateReason = reason;

  return this->spClient->putConnectState(putStateRequestProto);
  return {};
}

bell::Result<> ConnectStateHandler::handleTransferCommand(
    std::string_view payloadDataStr, cJSON* options) {
  size_t olen = 0;

  // Get the size of the base64 decoded data
  auto base64DecodeRes = mbedtls_base64_decode(
      nullptr, 0, &olen,
      reinterpret_cast<const uint8_t*>(payloadDataStr.data()),
      payloadDataStr.size());
  if (base64DecodeRes == 0) {
    BELL_LOG(error, LOG_TAG, "Failed to base64 decode payload data");
    return std::errc::bad_message;
  }

  std::vector<uint8_t> decodedData(olen);
  // Decode the base64 data
  base64DecodeRes = mbedtls_base64_decode(
      decodedData.data(), decodedData.size(), &olen,
      reinterpret_cast<const uint8_t*>(payloadDataStr.data()),
      payloadDataStr.size());
  if (base64DecodeRes != 0) {
    BELL_LOG(error, LOG_TAG, "Failed to base64 decode payload data");
    return std::errc::bad_message;
  }

  cspot_proto::TransferState transferState;

  bool res = nanopb_helper::decodeFromVector(transferState, decodedData);
  if (!res) {
    BELL_LOG(error, LOG_TAG, "Failed to decode transfer state");
    return std::errc::bad_message;
  }

  BELL_LOG(info, LOG_TAG, "Transfer state decoded successfully");

  // Set active state
  putStateRequestProto.isActive = true;

  auto& playerState = putStateRequestProto.device.playerState;

  if (transferState.current_session.originalSessionId.hasValue) {
    playerState.sessionId =
        transferState.current_session.originalSessionId.value;
  } else {
    // Generate random 16-byte session ID
    playerState.sessionId = generateSessionId();
  }

  // No playback yet
  playerState.isPlaying = true;
  playerState.isBuffering = false;
  playerState.timestamp = transferState.playback.timestamp;

  bool shouldPause = transferState.playback.isPaused;
  if (options) {
    cJSON* restore = cJSON_GetObjectItem(options, "restore_paused");
    if (cJSON_IsString(restore) && std::string(restore->valuestring) == "restore") {
      shouldPause = true;
    }
  }

  playerState.isPaused = shouldPause;
  playerState.contextUri = transferState.current_session.context.uri;
  playerState.contextUrl = transferState.current_session.context.url;
  playerState.options = transferState.options;
  playerState.track.uid = transferState.current_session.currentUid;
  playerState.position = 0;
  playerState.positionAsOfTimestamp =
      transferState.playback.positionAsOfTimestamp;
  putStateRequestProto.startedPlayingAt = transferState.playback.timestamp;
  putStateRequestProto.hasBeenPlayingForMs = 0;

  trackProvider->setQueue(transferState.queue);

  SpotifyIdType trackType =
      SpotifyId::getTypeFromContext(transferState.current_session.context.uri);
  SpotifyId trackId =
      SpotifyId(trackType, transferState.playback.currentTrack.gid);

  auto provideRes = trackProvider->loadTrackAndContext(
      transferState.current_session.currentUid, trackId.uri,
      transferState.current_session.context);
  if (!provideRes) {
    BELL_LOG(error, LOG_TAG, "Failed to provide current track: {}",
             provideRes.errorMessage());
    return provideRes.getError();
  }

  auto track = trackProvider->currentTrack();
  if (track) {
    playerState.track = *track;
    playerState.index.hasValue = true;
    playerState.index.value = trackProvider->currentContextIndex().value();
  } else {
    playerState.index.hasValue = false;
  }

  putState();

  return {};
}

bell::Result<> ConnectStateHandler::handleSkipNextCommand() {
  auto res = trackProvider->skipToNextTrack();
  if (!res) {
    BELL_LOG(error, LOG_TAG, "Failed to skip next track");
    return res.getError();
  }

  auto& playerState = putStateRequestProto.device.playerState;
  auto track = trackProvider->currentTrack();
  if (track) {
    playerState.track = *track;
    playerState.index.hasValue = true;
    playerState.index.value = trackProvider->currentContextIndex().value();
  } else {
    playerState.index.hasValue = false;
  }

  playerState.positionAsOfTimestamp = 0;
  playerState.timestamp =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count();

  putState();

  return {};
}
