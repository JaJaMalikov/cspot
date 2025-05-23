#include "TrackQueue.h"
#include "connect.pb.h"
#include "pb_decode.h"

using namespace cspot;

namespace {}  // namespace

TrackQueue::TrackQueue(std::shared_ptr<SessionContext> sessionContext,
                       std::shared_ptr<SpClient> spClient)
    : sessionContext(std::move(sessionContext)),
      spClient(std::move(spClient)) {}
