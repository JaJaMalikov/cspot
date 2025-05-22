#include "TrackQueue.h"
#include "NanoPBExtensions.h"
#include "connect.pb.h"
#include "pb_decode.h"

using namespace cspot;

namespace {
// Decode callback for std::vector<ContextTrack>
bool pbDecodeContextTrackList(pb_istream_t* stream, const pb_field_t* /*field*/,
                              void** arg) {
  auto& trackQueue = *static_cast<std::vector<cspot::ContextTrack>*>(*arg);

  _ContextTrack contextTrackProto = ContextTrack_init_zero;
  cspot::ContextTrack decodedTrack;

  contextTrackProto.uid.funcs.decode = &cspot::pbDecodeString;
  contextTrackProto.uid.arg = &decodedTrack.uid;

  contextTrackProto.uri.funcs.decode = &cspot::pbDecodeString;
  contextTrackProto.uri.arg = &decodedTrack.uri;

  contextTrackProto.gid.funcs.decode = &cspot::pbDecodeUint8Vector;
  contextTrackProto.gid.arg = &decodedTrack.gid;

  if (!pb_decode(stream, ContextTrack_fields, &contextTrackProto)) {
    return false;
  }

  trackQueue.push_back(std::move(decodedTrack));
  return true;
}
}  // namespace

TrackQueue::TrackQueue(std::shared_ptr<SessionContext> sessionContext,
                       std::shared_ptr<SpClient> spClient)
    : sessionContext(std::move(sessionContext)),
      spClient(std::move(spClient)) {}

void TrackQueue::pbAssignDecodeCallbacksForQueue(Queue* queueProto) {
  trackQueue.clear();

  queueProto->tracks.funcs.decode = &pbDecodeContextTrackList;
  queueProto->tracks.arg = &trackQueue;
}
