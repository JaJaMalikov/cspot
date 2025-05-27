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

  this->initialize();
}

void ConnectDeviceState::initialize() {
  auto& deviceProto = putStateRequestProto.device;

  auto& deviceInfo = deviceProto.deviceInfo;
  deviceInfo.canPlay = true;
  deviceInfo.volume = 100;
  deviceInfo.name = sessionContext->loginBlob->getDeviceName();

  deviceInfo.deviceType = DeviceType_SPEAKER;
  deviceInfo.deviceSoftwareVersion = deviceSoftwareVersion;
  deviceInfo.deviceId = sessionContext->loginBlob->getDeviceId();
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
  putStateRequestProto.isActive = false;

  auto& playerState = putStateRequestProto.device.playerState;
  playerState.isSystemInitiated = true;
  playerState.playbackSpeed = 1.0;
}

void ConnectDeviceState::setActive(bool active) {
  std::scoped_lock lock(protoMutex);

  putStateRequestProto.isActive = active;

  putState(PutStateReason_PLAYER_STATE_CHANGED);
}

bell::Result<> ConnectDeviceState::putState(PutStateReason reason) {
  // get milliseconds since epoch;
  putStateRequestProto.clientSideTimestamp =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count();
  putStateRequestProto.memberType = MemberType_CONNECT_STATE;
  putStateRequestProto.putStateReason = reason;

  return this->spClient->putConnectState(putStateRequestProto);
}

void ConnectDeviceState::assignLastMessageId(
    uint32_t messageId, const std::string& sentByDeviceId) {
  std::scoped_lock lock(protoMutex);
  putStateRequestProto.mesasgeId = messageId;
  putStateRequestProto.lastCommandSentByDeviceId = sentByDeviceId;
}

cspot_proto::PlayerState& ConnectDeviceState::getPlayerState() {
  return putStateRequestProto.device.playerState;
}

std::scoped_lock<std::mutex> ConnectDeviceState::lock() {
  return std::scoped_lock(protoMutex);
}