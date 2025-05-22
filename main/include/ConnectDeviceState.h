#pragma once

#include <string>

// Protobufs
#include "bell/Result.h"
#include "connect.pb.h"

#include "SessionContext.h"
#include "TrackQueue.h"
#include "api/SpClient.h"

namespace cspot {

class ConnectDeviceState {
 public:
  ConnectDeviceState(std::shared_ptr<SessionContext> sessionContext,
                     std::shared_ptr<SpClient> spClient);

  void resetState();

  void setActive(bool active);

  bell::Result<> handlePlayerCommand(tao::json::value& messageJson);

  bell::Result<> putState(
      PutStateReason reason = PutStateReason_PLAYER_STATE_CHANGED);

 private:
  const char* LOG_TAG = "ConnectDeviceState";

  std::shared_ptr<SessionContext> sessionContext;
  std::shared_ptr<SpClient> spClient;
  std::shared_ptr<TrackQueue> trackQueue;

  PutStateRequest stateRequestProto{};

  std::string deviceName;
  std::string deviceId;

  std::string playerContextUri;
  std::string playerContextUrl;

  bool isActive = false;

  void initialize();

  bell::Result<> handleTransferCommand(std::string_view payloadDataStr);

  std::chrono::system_clock::time_point activeSince;
};
}  // namespace cspot
