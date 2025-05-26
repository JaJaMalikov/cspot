#include "ConnectDeviceState.h"

#include <tao/json.hpp>
#include "NanoPBExtensions.h"
#include "SessionContext.h"
#include "api/SpClient.h"
#include "bell/Logger.h"
#include "bell/Result.h"
#include "connect.pb.h"
#include "mbedtls/base64.h"

using namespace cspot;

namespace {
std::string spircVersion = "3.2.6";
std::string deviceSoftwareVersion = "1.0.0";
std::string connectCapabilities;
std::vector<std::string> supportedTypes = {"audio/track", "audio/episode"};
};  // namespace

ConnectDeviceState::ConnectDeviceState(
    std::shared_ptr<SessionContext> sessionContext,
    std::shared_ptr<SpClient> spClient)
    : sessionContext(std::move(sessionContext)), spClient(std::move(spClient)) {
  trackQueue = std::make_shared<TrackQueue>(sessionContext, spClient);

  this->initialize();
}

void ConnectDeviceState::initialize() {
  auto& deviceProto = stateRequestProto.device;

  auto& deviceInfo = deviceProto.deviceInfo;
  deviceInfo.canPlay = true;
  deviceInfo.volume = 100;
  deviceInfo.name = sessionContext->loginBlob->getDeviceName();

  deviceInfo.deviceType = DeviceType_SPEAKER;
  deviceInfo.deviceSoftwareVersion = deviceSoftwareVersion;
  deviceInfo.deviceId = sessionContext->loginBlob->getDeviceId();
  deviceInfo.clientId = deviceId;
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
  capabilities.supports_gzip_pushes = false;  // TODO: Should we support this?
  capabilities.has_supports_hifi = false;
  deviceInfo.capabilities.supportedTypes = supportedTypes;

  resetState();
}

void ConnectDeviceState::resetState() {
  this->isActive = false;
  this->activeSince = std::chrono::system_clock::now();

  auto& playerState = stateRequestProto.device.playerState;
  playerState.isSystemInitiated = true;
  playerState.playbackSpeed = 1.0;
}

void ConnectDeviceState::setActive(bool active) {
  this->isActive = active;
  this->activeSince = std::chrono::system_clock::now();

  putState(PutStateReason_PLAYER_STATE_CHANGED);
}

bell::Result<> ConnectDeviceState::putState(PutStateReason reason) {
  // get milliseconds since epoch;
  stateRequestProto.clientSideTimestamp =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count();
  stateRequestProto.isActive = this->isActive;
  stateRequestProto.memberType = MemberType_CONNECT_STATE;
  stateRequestProto.putStateReason = reason;

  return this->spClient->putConnectState(stateRequestProto);
}

bell::Result<> ConnectDeviceState::handlePlayerCommand(
    tao::json::value& messageJson) {
  auto& payload = messageJson.at("payload");
  auto& command = payload.at("command");
  std::string endpoint = command.at("endpoint").get_string();
  stateRequestProto.lastCommandMessageId =
      payload.at("message_id").get_unsigned();
  stateRequestProto.lastCommandSentByDeviceId =
      payload.at("sent_by_device_id").get_string();

  if (endpoint == "transfer") {
    BELL_LOG(info, LOG_TAG, "Received transfer command");
    std::string_view payloadDataStr = command.as<std::string_view>("data");
    return handleTransferCommand(payloadDataStr);
  } else {
    BELL_LOG(info, LOG_TAG, "Received unknown command: {}", endpoint);
    return std::errc::operation_not_supported;
  }

  return {};
}

bell::Result<> ConnectDeviceState::handleTransferCommand(
    std::string_view payloadDataStr) {
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

  std::cout << "Transfer state decoded successfully" << std::endl;
  std::cout << "Context URI: " << transferState.current_session.context.uri
            << std::endl;

  auto ctx = spClient->contextResolve(transferState.current_session.context.uri)
                 .unwrap();
  auto& currentPage = ctx["pages"][0];

  auto& playerState = stateRequestProto.device.playerState;

  for (tao::json::value& track : currentPage["tracks"].get_array()) {
    cspot_proto::ProvidedTrack providedTrack;
    providedTrack.provider = "context";
    providedTrack.uri = track["uri"].as<std::string>();
    providedTrack.uid = track["uid"].as<std::string>();

    playerState.nextTracks.push_back(providedTrack);
  }

  playerState.track = playerState.nextTracks.front();
  playerState.nextTracks.erase(playerState.nextTracks.begin());




  // auto& playerState = stateRequestProto.device.player_state;
  // playerState.next_tracks.funcs.encode = &cspot::pbEncodeProvidedTrackList;
  // playerState.next_tracks.arg = &nextTracks;
  // playerState.prev_tracks.funcs.encode = &cspot::pbEncodeProvidedTrackList;
  // playerState.prev_tracks.arg = &prevTracks;

  // playerState.track.uid.arg = &currentTrack.uid;
  // playerState.track.uid.funcs.encode = &pbEncodeString;

  // playerState.track.provider.arg = &currentTrack.provider;
  // playerState.track.provider.funcs.encode = &pbEncodeString;

  // playerState.track.uri.arg = &currentTrack.uri;
  // playerState.track.uri.funcs.encode = &pbEncodeString;

  // playerState.context_uri.arg = &contextUri;
  // playerState.context_uri.funcs.encode = &pbEncodeString;

  // playerState.context_url.arg = &contextUrl;
  // playerState.context_url.funcs.encode = &pbEncodeString;

  // playerState.has_index = true;
  // playerState.index.page = 0;
  // playerState.index.track = 0;
  // playerState.has_track = true;
  // playerState.is_playing = true;
  // playerState.is_buffering = false;
  // playerState.is_paused = false;
  // playerState.playback_speed = 1.0;
  // playerState.duration = 3600;
  // playerState.position = 50;

  // playerState.session_id.arg = &sessionId;
  // playerState.session_id.funcs.encode = &pbEncodeString;

  // playerState.timestamp = transferState.playback.timestamp;
  // playerState.position_as_of_timestamp =
  //     transferState.playback.position_as_of_timestamp;
  // playerState.position = transferState.playback.position_as_of_timestamp;

  // std::cout << "Device has been updated" << std::endl;
  // std::cout << nextTracks.size() << " next tracks" << std::endl;
  // std::cout << prevTracks.size() << " prev tracks" << std::endl;

  // stateRequestProto.started_playing_at =
  //     std::chrono::duration_cast<std::chrono::milliseconds>(
  //         std::chrono::system_clock::now().time_since_epoch())
  //         .count();
  // stateRequestProto.last_command_message_id = lastCommandMessageId;
  // stateRequestProto.last_command_sent_by_device_id.arg =
  //     &lastCommandFromDeviceId;
  // stateRequestProto.last_command_sent_by_device_id.funcs.encode =
  //     &pbEncodeString;

  // setActive(true);

  return {};
}
