#pragma once

// Standard includes
#include <string>
#include <vector>

// Library includes
#include <bell/Result.h>

// Protobufs
#include "connect.pb.h"

#include "SessionContext.h"

namespace cspot {
class SpClient {
 public:
  SpClient(std::shared_ptr<SessionContext> sessionContext);

  bell::Result<> putConnectStateInactive(int retryCount = 3);
  bell::Result<> putConnectState(const PutStateRequest& stateRequest,
                                 int retryCount = 3);

 private:
  const char* LOG_TAG = "SpClient";

  std::shared_ptr<SessionContext> sessionContext;
  std::vector<std::byte> requestBuffer;
};
}  // namespace cspot