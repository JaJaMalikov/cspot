#include "TrackProvider.h"

#include <fmt/format.h>

#include "proto/ConnectPb.h"
#include "proto/NanoPBHelper.h"

#include <bell/Logger.h>
#include <algorithm>
// #include "proto/SpotifyId.h"

using namespace cspot;

namespace {
const uint32_t maxEncodedTracksWindow = 5;
}

TrackProvider::TrackProvider(std::shared_ptr<SessionContext> sessionContext,
                             std::shared_ptr<SpClient> spClient)
    : sessionContext(std::move(sessionContext)), spClient(std::move(spClient)) {
  this->contextTrackResolver =
      std::make_unique<ContextTrackResolver>(this->spClient);
}

void TrackProvider::setQueue(const cspot_proto::Queue& queue) {
  trackQueue = queue.tracks;
  isPlayingQueue = queue.isPlayingQueue;

  if (isPlayingQueue) {}
}

std::optional<cspot_proto::ProvidedTrack> TrackProvider::currentTrack() {
  cspot_proto::ProvidedTrack track;

  if (isPlayingQueue) {
    track.uri = trackQueue[trackQueueIndex].uri;
    track.uid = fmt::format("q{}", trackQueueIndex);
    track.provider = "queue";
    return track;
  }

  auto contextTrackRes = contextTrackResolver->getCurrentTrack();
  if (!contextTrackRes) {
    BELL_LOG(error, LOG_TAG, "Failed to resolve current track");
    return std::nullopt;
  }
  track.uri = contextTrackRes.getValue().uri;
  track.uid = contextTrackRes.getValue().uid;
  track.provider = "context";
  return track;
}

bell::Result<> TrackProvider::skipToNextTrack(
    cspot_proto::ContextTrack* track) {
  auto currentTrackRes = contextTrackResolver->getCurrentTrack();
  if (!currentTrackRes) {
    BELL_LOG(error, LOG_TAG, "Failed to resolve current track");
    return currentTrackRes.getError();
  }

  auto res = contextTrackResolver->next();
  if (!res) {
    BELL_LOG(error, LOG_TAG, "Failed to resolve next track");
    return res.getError();
  }

  previousTracks.push_back({.uri = currentTrackRes.getValue().uri,
                            .uid = currentTrackRes.getValue().uid,
                            .provider = "context"});

  if (previousTracks.size() > maxEncodedTracksWindow) {
    previousTracks.erase(previousTracks.begin());
  }

  nextTracks.erase(nextTracks.begin());

  if (contextTrackResolver->nextTracks().size() >= maxEncodedTracksWindow) {
    auto& ctxNext =
        contextTrackResolver->nextTracks()[maxEncodedTracksWindow - 1];
    nextTracks.push_back(
        {.uri = ctxNext.uri, .uid = ctxNext.uid, .provider = "context"});
  }

  return {};
}

bell::Result<> TrackProvider::skipToPreviousTrack(
    cspot_proto::ContextTrack* track) {
  return {};
}

std::optional<cspot_proto::ContextIndex> TrackProvider::currentContextIndex() {
  if (isPlayingQueue) {
    // No context index for queue
    return std::nullopt;
  }

  auto contextTrackRes = contextTrackResolver->getCurrentTrack();
  if (!contextTrackRes) {
    BELL_LOG(error, LOG_TAG, "Failed to resolve current context index");
    return std::nullopt;
  }

  return contextTrackRes.getValue().index;
};

bool TrackProvider::encodePbTracks(pb_ostream_t* stream,
                                   const pb_field_t* field,
                                   bool isPreviousTracks) {
  for (auto& track : isPreviousTracks ? previousTracks : nextTracks) {
    void* trackPtr = &track;
    if (!nanopb_helper::StructCodec<
            cspot_proto::ProvidedTrack>::encodeSubmessage(stream, field,
                                                          &trackPtr)) {
      return false;
    }
  }
  return true;
}

bell::Result<> TrackProvider::loadTrackAndContext(
    std::optional<std::string> trackUid, std::optional<std::string> trackUri,
    const cspot_proto::Context& context) {
  contextTrackResolver->updateContext(context.url, trackUid, trackUri);

  auto currentTrackRes = contextTrackResolver->getCurrentTrack();
  if (!currentTrackRes) {
    BELL_LOG(error, LOG_TAG, "Failed to resolve current track");
    return currentTrackRes.getError();
  }

  auto prevCtx = contextTrackResolver->previousTracks();
  for (auto it = prevCtx.rbegin(); it != prevCtx.rend(); ++it) {
    previousTracks.push_back(
        {.uri = it->uri, .uid = it->uid, .provider = "context"});

    if (previousTracks.size() >= maxEncodedTracksWindow) {
      break;
    }
  }

  // Reverse the previous tracks
  std::reverse(previousTracks.begin(), previousTracks.end());

  auto nextCtx = contextTrackResolver->nextTracks();
  for (auto* it = nextCtx.begin(); it != nextCtx.end(); ++it) {
    nextTracks.push_back(
        {.uri = it->uri, .uid = it->uid, .provider = "context"});

    if (nextTracks.size() >= maxEncodedTracksWindow) {
      break;
    }
  }

  return {};
}
