#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "SessionContext.h"
#include "api/SpClient.h"

#include "connect.pb.h"

namespace cspot {

class TrackQueue {
 public:
  TrackQueue(std::shared_ptr<SessionContext> sessionContext,
             std::shared_ptr<SpClient> spClient);

  // Assigns a nanopb decode callback to decode ContextTracks from queue
  void pbAssignDecodeCallbacksForTransfer(TransferState* transferProto);

 private:
  const char* LOG_TAG = "TrackQueue";
  std::shared_ptr<SessionContext> sessionContext;
  std::shared_ptr<SpClient> spClient;

  std::vector<ContextTrack> trackQueue;

  std::vector<ContextPage> contextPages;
  std::string contextUri;
  std::string contextUrl;
};
}  // namespace cspot
