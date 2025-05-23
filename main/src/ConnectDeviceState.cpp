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
  stateRequestProto = PutStateRequest_init_zero;
  this->deviceId = this->sessionContext->loginBlob->getDeviceId();
  this->deviceName = this->sessionContext->loginBlob->getDeviceName();

  stateRequestProto.has_device = true;

  auto& deviceProto = stateRequestProto.device;

  deviceProto.has_device_info = true;
  deviceProto.has_player_state = true;

  auto& deviceInfo = deviceProto.device_info;
  deviceInfo.can_play = true;
  deviceInfo.volume = 100;

  deviceInfo.name.funcs.encode = &cspot::pbEncodeString;
  deviceInfo.name.arg = &this->deviceName;

  deviceInfo.device_type = DeviceType_SPEAKER;
  deviceInfo.device_software_version.funcs.encode = &cspot::pbEncodeString;
  deviceInfo.device_software_version.arg = &deviceSoftwareVersion;
  deviceInfo.device_id.funcs.encode = &cspot::pbEncodeString;
  deviceInfo.device_id.arg = &deviceName;

  deviceInfo.client_id.funcs.encode = &cspot::pbEncodeString;
  deviceInfo.client_id.arg = &deviceId;

  deviceInfo.spirc_version.funcs.encode = &cspot::pbEncodeString;
  deviceInfo.spirc_version.arg = &spircVersion;

  deviceInfo.capabilities = Capabilities_init_zero;
  deviceInfo.has_capabilities = true;

  auto& capabilities = deviceInfo.capabilities;

  // Init capatilities
  capabilities.can_be_player = true;
  capabilities.restrict_to_local = false;
  capabilities.gaia_eq_connect_id = true;
  capabilities.supports_logout = true;
  capabilities.is_observable = true;

  capabilities.supported_types.funcs.encode = &cspot::pbEncodeStringVector;
  capabilities.supported_types.arg = &supportedTypes;

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

  capabilities.connect_capabilities.funcs.encode = &cspot::pbEncodeString;
  capabilities.connect_capabilities.arg =
      &connectCapabilities;  // TODO: Set to actual capabilities

  // auto& playerState = stateRequestProto.device.player_state;

  // playerState.context_uri.funcs.encode = &cspot::pbEncodeString;
  // playerState.context_uri.arg = &this->playerContextUri;
  // playerState.context_url.funcs.encode = &cspot::pbEncodeString;
  // playerState.context_url.arg = &this->playerContextUrl;
  resetState();
}

void ConnectDeviceState::resetState() {
  this->isActive = false;
  this->activeSince = std::chrono::system_clock::now();

  auto& playerState = stateRequestProto.device.player_state;
  playerState.is_system_initiated = true;
  playerState.playback_speed = 1.0;
}

void ConnectDeviceState::setActive(bool active) {
  this->isActive = active;
  this->activeSince = std::chrono::system_clock::now();

  putState(PutStateReason_PLAYER_STATE_CHANGED);
}

bell::Result<> ConnectDeviceState::putState(PutStateReason reason) {
  // get milliseconds since epoch;
  stateRequestProto.client_side_timestamp =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count();
  stateRequestProto.is_active = this->isActive;
  stateRequestProto.member_type = MemberType_CONNECT_STATE;
  stateRequestProto.put_state_reason = reason;
  stateRequestProto.has_device = true;

  return this->spClient->putConnectState(stateRequestProto);
}

bell::Result<> ConnectDeviceState::handlePlayerCommand(
    tao::json::value& messageJson) {
  auto& payload = messageJson.at("payload");
  auto& command = payload.at("command");
  std::string endpoint = command.at("endpoint").get_string();
  lastCommandMessageId = payload.at("message_id").get_unsigned();
  lastCommandFromDeviceId = payload.at("sent_by_device_id").get_string();

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

  TransferState transferState = TransferState_init_zero;

  std::string contextUri;
  std::string contextUrl;
  std::string sessionId;
  std::vector<ContextPage> contextPages;

  // trackQueue->pbAssignDecodeCallbacksForQueue(&transferState.queue);

  // Assign the decode functions for the protobuf fields
  transferState.current_session.context.uri.funcs.decode =
      &cspot::pbDecodeString;
  transferState.current_session.context.uri.arg = &contextUri;
  transferState.current_session.context.url.funcs.decode =
      &cspot::pbDecodeString;
  transferState.current_session.context.url.arg = &contextUrl;

  transferState.current_session.context.pages.funcs.decode =
      &pbDecodeContextPageList;
  transferState.current_session.context.pages.arg = &contextPages;

  transferState.current_session.original_session_id.funcs.decode =
      &pbDecodeString;
  transferState.current_session.original_session_id.arg = &sessionId;

  auto decodeRes = pbDecodeMessage(decodedData.data(), decodedData.size(),
                                   TransferState_fields, &transferState);
  if (!decodeRes) {
    BELL_LOG(error, LOG_TAG, "Failed to decode transfer state");
    return decodeRes.getError();
  }

  std::cout << "Transfer state decoded successfully" << std::endl;
  std::cout << "Context URI: " << contextUri << std::endl;
  std::cout << "Context URL: " << contextUrl << std::endl;

  auto ctx = spClient->contextResolve(contextUri).unwrap();
  auto& currentPage = ctx["pages"][0];

  for (tao::json::value& track : currentPage["tracks"].get_array()) {
    ProvidedTrack pTrack;
    pTrack.provider = "context";
    pTrack.uri = track["uri"].as<std::string>();
    pTrack.uid = track["uid"].as<std::string>();

    nextTracks.push_back(pTrack);
    if (nextTracks.size() > 6) {
      break;
    }
  }

  currentTrack = nextTracks[0];
  nextTracks.erase(nextTracks.begin());

  auto& playerState = stateRequestProto.device.player_state;
  playerState.next_tracks.funcs.encode = &cspot::pbEncodeProvidedTrackList;
  playerState.next_tracks.arg = &nextTracks;
  playerState.prev_tracks.funcs.encode = &cspot::pbEncodeProvidedTrackList;
  playerState.prev_tracks.arg = &prevTracks;

  playerState.track.uid.arg = &currentTrack.uid;
  playerState.track.uid.funcs.encode = &pbEncodeString;

  playerState.track.provider.arg = &currentTrack.provider;
  playerState.track.provider.funcs.encode = &pbEncodeString;

  playerState.track.uri.arg = &currentTrack.uri;
  playerState.track.uri.funcs.encode = &pbEncodeString;

  playerState.context_uri.arg = &contextUri;
  playerState.context_uri.funcs.encode = &pbEncodeString;

  playerState.context_url.arg = &contextUrl;
  playerState.context_url.funcs.encode = &pbEncodeString;

  playerState.has_index = true;
  playerState.index.page = 0;
  playerState.index.track = 0;
  playerState.has_track = true;
  playerState.is_playing = true;
  playerState.is_buffering = false;
  playerState.is_paused = false;
  playerState.playback_speed = 1.0;
  playerState.duration = 3600;
  playerState.position = 50;

  playerState.session_id.arg = &sessionId;
  playerState.session_id.funcs.encode = &pbEncodeString;

  playerState.timestamp = transferState.playback.timestamp;
  playerState.position_as_of_timestamp =
      transferState.playback.position_as_of_timestamp;
  playerState.position = transferState.playback.position_as_of_timestamp;

  std::cout << "Device has been updated" << std::endl;
  std::cout << nextTracks.size() << " next tracks" << std::endl;
  std::cout << prevTracks.size() << " prev tracks" << std::endl;

  stateRequestProto.started_playing_at =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count();
  stateRequestProto.last_command_message_id = lastCommandMessageId;
  stateRequestProto.last_command_sent_by_device_id.arg =
      &lastCommandFromDeviceId;
  stateRequestProto.last_command_sent_by_device_id.funcs.encode =
      &pbEncodeString;

  setActive(true);

  return {};
}
