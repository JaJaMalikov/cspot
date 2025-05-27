#pragma once

// Protobufs
#include "ConnectDeviceState.h"
#include "bell/Result.h"
#include "connect.pb.h"

#include "SessionContext.h"
#include "TrackProvider.h"
#include "api/SpClient.h"

namespace cspot {

class ConnectStateHandler {
 public:
  ConnectStateHandler(std::shared_ptr<SessionContext> sessionContext,
                      std::shared_ptr<SpClient> spClient);

  bell::Result<> handlePlayerCommand(tao::json::value& messageJson);

  bell::Result<> putState(
      PutStateReason reason = PutStateReason_PLAYER_STATE_CHANGED);

 private:
  const char* LOG_TAG = "ConnectDeviceState";

  std::shared_ptr<SessionContext> sessionContext;
  std::shared_ptr<SpClient> spClient;
  std::shared_ptr<TrackProvider> trackProvider;
  std::shared_ptr<ConnectDeviceState> connectDeviceState;

  bell::Result<> handleTransferCommand(std::string_view payloadDataStr);
};
}  // namespace cspot
