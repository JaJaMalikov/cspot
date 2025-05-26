#pragma once

// Standard includes
#include <string>
#include <vector>

// Library includes
#include <bell/Result.h>

// Protobufs
#include "connect.pb.h"

#include "SessionContext.h"
#include "proto/ConnectPb.h"

namespace cspot {
class SpClient {
 public:
  SpClient(std::shared_ptr<SessionContext> sessionContext);

  bell::Result<> putConnectStateInactive(int retryCount = 3);
  bell::Result<> putConnectState(cspot_proto::PutStateRequest& stateRequest,
                                 int retryCount = 3);
  bell::Result<tao::json::value> contextResolve(const std::string& contextUri);

  bell::Result<tao::json::value> hmRequest(const std::string& request);

  bell::Result<tao::json::value> radioApollo(const std::string& scope,
                                             const std::string& contextUri,
                                             bool autoplay, int pageSize);

 private:
  const char* LOG_TAG = "SpClient";

  std::shared_ptr<SessionContext> sessionContext;
  std::vector<std::uint8_t> requestBuffer;
};
}  // namespace cspot
