#include "ConnectStateHandler.h"
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

ConnectStateHandler::ConnectStateHandler(
    std::shared_ptr<SessionContext> sessionContext,
    std::shared_ptr<SpClient> spClient)
    : sessionContext(std::move(sessionContext)), spClient(std::move(spClient)) {
  connectDeviceState = std::make_shared<ConnectDeviceState>(
      this->sessionContext, this->spClient);
  trackProvider =
      std::make_shared<TrackProvider>(this->sessionContext, this->spClient);
}

bell::Result<> ConnectStateHandler::handlePlayerCommand(
    tao::json::value& messageJson) {
  auto& payload = messageJson.at("payload");
  auto& command = payload.at("command");
  std::string endpoint = command.at("endpoint").get_string();

  // Assign the last message ID and device ID
  connectDeviceState->assignLastMessageId(
      payload.at("message_id").get_unsigned(),
      payload.at("sent_by_device_id").get_string());

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

bell::Result<> ConnectStateHandler::putState(PutStateReason reason) {
  return this->connectDeviceState->putState(reason);
}

bell::Result<> ConnectStateHandler::handleTransferCommand(
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

  auto stateLock = connectDeviceState->lock();

  auto& playerState = connectDeviceState->getPlayerState();

  auto transferRes = trackProvider->resolveContext(
      transferState.current_session.context.url,
      transferState.current_session.currentUid, playerState);

  if (!transferRes) {
    BELL_LOG(error, LOG_TAG, "Failed to transfer context: {}",
             transferRes.errorMessage());
    return transferRes;
  }
  for (const auto& track : playerState.prevTracks) {
    BELL_LOG(info, LOG_TAG, "Previous track: {}", track.uid);
  }
  for (const auto& track : playerState.nextTracks) {
    BELL_LOG(info, LOG_TAG, "Next track: {}", track.uid);
  }

  return {};
}
