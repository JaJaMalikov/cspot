#pragma once

// Protobufs
#include "ConnectDeviceState.h"
#include "bell/Result.h"
#include "connect.pb.h"

#include "SessionContext.h"
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
  const char* LOG_TAG = "ConnectStateHandler";

  std::shared_ptr<SessionContext> sessionContext;
  std::shared_ptr<SpClient> spClient;
  std::shared_ptr<ConnectDeviceState> connectDeviceState;

  cspot_proto::PutStateRequest putStateRequestProto;

  void initialize();

  bell::Result<> handleTransferCommand(std::string_view payloadDataStr);
};
}  // namespace cspot
