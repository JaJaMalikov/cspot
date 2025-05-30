#pragma once

// Standard includes
#include <string>
#include <vector>

// Library includes
#include <bell/Result.h>

// Protobufs
#include "bell/http/Reader.h"
#include "connect.pb.h"

#include "SessionContext.h"
#include "proto/ConnectPb.h"
#include "proto/MetadataPb.h"
#include "proto/SpotifyId.h"

namespace cspot {
class SpClient {
 public:
  SpClient(std::shared_ptr<SessionContext> sessionContext);

  bell::Result<> putConnectStateInactive(int retryCount = 3);
  bell::Result<> putConnectState(cspot_proto::PutStateRequest& stateRequest,
                                 int retryCount = 3);
  bell::Result<tao::json::value> contextResolve(const std::string& contextUri);

  bell::Result<bell::HTTPReader> doRequest(bell::HTTPMethod method,
                                           const std::string& requestUrl);

  bell::Result<cspot_proto::Track> trackMetadata(const SpotifyId& trackId);

  bell::Result<cspot_proto::Episode> episodeMetadata(
      const SpotifyId& episodeId);

 private:
  const char* LOG_TAG = "SpClient";

  std::shared_ptr<SessionContext> sessionContext;
  std::vector<std::uint8_t> requestBuffer;
};
}  // namespace cspot
