#include "ContextTrackResolver.h"
#include <algorithm>
#include <span>

using namespace cspot;

ContextTrackResolver::ContextTrackResolver(std::shared_ptr<SpClient> client,
                                           uint32_t maxWindow,
                                           uint32_t updateThreshold)
    : spClient(std::move(client)),
      maxWindowSize(maxWindow),
      trackUpdateThreshold(updateThreshold) {}

void ContextTrackResolver::updateContext(const std::string& url,
                                         std::optional<std::string>& uid,
                                         std::optional<std::string>& uri) {
  rootContextUrl = url;
  currentTrackId = TrackId(uid.value_or(""), uri.value_or(""));
  trackCache.clear();
  currentTrackInCacheIndex.reset();
}

bell::Result<cspot_proto::ContextTrack> ContextTrackResolver::getCurrentTrack() {
  if (currentTrackInCacheIndex && *currentTrackInCacheIndex < trackCache.size()) {
    return trackCache[*currentTrackInCacheIndex];
  }
  return std::errc::state_not_recoverable;
}

std::span<cspot_proto::ContextTrack> ContextTrackResolver::previousTracks() {
  size_t end = currentTrackInCacheIndex.value_or(0);
  return {trackCache.data(), end};
}

std::span<cspot_proto::ContextTrack> ContextTrackResolver::nextTracks() {
  if (!currentTrackInCacheIndex) return {};
  size_t idx = *currentTrackInCacheIndex + 1;
  return {trackCache.data() + idx, trackCache.size() - idx};
}

bell::Result<cspot_proto::ContextTrack> ContextTrackResolver::skipForward(
    const cspot_proto::ContextTrack&) {
  return std::errc::not_supported;
}

bell::Result<cspot_proto::ContextTrack> ContextTrackResolver::skipBackward(
    const cspot_proto::ContextTrack&) {
  return std::errc::not_supported;
}

bell::Result<cspot_proto::ContextTrack> ContextTrackResolver::next() {
  return std::errc::not_supported;
}

bell::Result<cspot_proto::ContextTrack> ContextTrackResolver::previous() {
  return std::errc::not_supported;
}

bool ContextTrackResolver::prepareParseState() { return false; }
void ContextTrackResolver::updateTracksFromParseState() {}
bell::Result<> ContextTrackResolver::ensureContextTracks() {
  return std::errc::not_supported;
}
bell::Result<> ContextTrackResolver::resolveRootContext() {
  return std::errc::not_supported;
}
bell::Result<> ContextTrackResolver::resolveContextPage(ResolvedContextPage&) {
  return std::errc::not_supported;
}
