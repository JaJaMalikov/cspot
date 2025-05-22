#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "SessionContext.h"
#include "api/SpClient.h"

#include "connect.pb.h"

namespace cspot {
struct ContextTrack {
  std::string uri;
  std::string uid;
  std::vector<uint8_t> gid;
  std::unordered_map<std::string, std::string> metadata;
};

class TrackQueue {
 public:
  TrackQueue(std::shared_ptr<SessionContext> sessionContext,
             std::shared_ptr<SpClient> spClient);

  // Assigns a nanopb decode callback to decode ContextTracks from queue
  void pbAssignDecodeCallbacksForQueue(Queue* queueProto);

 private:
  const char* LOG_TAG = "TrackQueue";
  std::shared_ptr<SessionContext> sessionContext;
  std::shared_ptr<SpClient> spClient;

  std::vector<ContextTrack> trackQueue;
};
}  // namespace cspot
