#include "ContextTrackResolver.h"
#include "bell/Logger.h"

using namespace cspot;

ContextTrackResolver::ContextTrackResolver(
    std::shared_ptr<SessionContext> sessionContext,
    std::shared_ptr<SpClient> spClient)
    : sessionContext(std::move(sessionContext)),
      spClient(std::move(spClient)) {}

bell::Result<> ContextTrackResolver::resolveContext(
    const std::string& contextUrl, const std::string& currentTrackUid) {
  if (contextUrl.empty() || !contextUrl.starts_with("context://")) {
    BELL_LOG(error, LOG_TAG, "Invalid context URL: {}", contextUrl);
    return std::errc::invalid_argument;
  }

  BELL_LOG(info, LOG_TAG, "Resolving context: {}", contextUrl);
  trackIndex.reset();
  nextTracks.clear();
  previousTracks.clear();

  this->currentTrackUid = currentTrackUid;

  this->rootContextUrl = contextUrl.substr(10);  // Remove "context://"
  auto res = resolveRootContext(this->rootContextUrl);
  if (!res) {
    BELL_LOG(error, LOG_TAG, "Failed to resolve context: {}",
             res.errorMessage());
    return res.getError();
  }

  if (!trackIndex.has_value()) {
    BELL_LOG(error, LOG_TAG, "Failed to find current track index in context");
    return std::errc::invalid_argument;
  }

  auto updateTracksRes = updateTracks();
  if (!updateTracksRes) {
    BELL_LOG(error, LOG_TAG, "Failed to update tracks: {}", res.errorMessage());
    return res.getError();
  }

  return {};
}

bell::Result<bool> ContextTrackResolver::updateTracks() {
  if (!trackIndex.has_value()) {
    // If we don't have a current track index, we can't update tracks
    return std::errc::invalid_argument;
  }

  size_t tailPageIndex = trackIndex.value().tailTrackPageIndex;
  size_t tailTrackIndex = trackIndex.value().tailTrackIndex;

  if (tailPageIndex > resolvedContextPages.size()) {
    BELL_LOG(error, LOG_TAG, "Current track index is out of bounds");
    return std::errc::invalid_argument;
  }

  if (nextTracks.size() > nextTracksThreshold) {
    return false;  // No need to update tracks if we have enough next tracks
  }

  BELL_LOG(info, LOG_TAG, "Updating tracks for current track index [{}, {}]",
           tailPageIndex, tailTrackIndex);

  auto& currentPage =
      resolvedContextPages[trackIndex.value().tailTrackPageIndex];
  if (currentPage.lastUid.has_value() &&
      currentPage.lastUid == nextTracks.back().uid) {
    BELL_LOG(
        info, LOG_TAG,
        "Current page last UID matches the last next track UID, skipping page");
    currentPage = resolvedContextPages[tailPageIndex + 1];
  }

  if (currentPage.isInRoot) {
    auto res = resolveRootContext(rootContextUrl);

    if (!res) {
      return res.getError();
    }
  } else {
    auto res = resolveContextPage(currentPage);
    if (!res) {
      return res.getError();
    }
  }

  return true;
}

bell::Result<> ContextTrackResolver::resolveRootContext(
    const std::string& contextUrl) {
  BELL_LOG(info, LOG_TAG, "Resolving root context: {}", contextUrl);

  auto res = spClient->contextResolve(contextUrl);
  if (!res) {
    BELL_LOG(error, LOG_TAG, "Failed to resolve root context: {}",
             res.errorMessage());
    return res.getError();
  }

  auto contextJson = res.takeValue();

  if (!contextJson.at("pages").is_array()) {
    BELL_LOG(error, LOG_TAG, "Context pages are not an array");
    return std::errc::invalid_argument;
  }

  auto& pages = contextJson.at("pages").get_array();

  // Ensure we have enough space in the resolved context pages
  if (resolvedContextPages.size() < pages.size()) {
    resolvedContextPages.resize(pages.size());
  }

  size_t pageIdx = 0;

  if (trackIndex.has_value()) {
    // If we have a last track index, start from the next page
    pageIdx = trackIndex->tailTrackPageIndex + 1;
  }

  while (pageIdx < pages.size()) {
    auto& page = pages[pageIdx];
    if (!page.is_object()) {
      BELL_LOG(error, LOG_TAG, "Context page is not an object");
      return std::errc::invalid_argument;
    }

    auto& resolvedPage = resolvedContextPages[pageIdx];

    if (page["page_url"].is_string_type()) {
      resolvedPage.pageUrl = page["page_url"].get_string();
    }

    if ((page["next_page_url"].is_string_type()) &&
        (pageIdx == resolvedContextPages.size() - 1)) {
      // Add a reference to the next page URL
      resolvedContextPages.push_back({
          .pageUrl = page["next_page_url"].get_string(),
      });
    }

    resolvedPage.isInRoot = page["tracks"].is_array();

    if (!resolvedPage.isInRoot) {
      pageIdx++;
      continue;
    }

    const auto& tracks = page.at("tracks").get_array();

    iterateContextPage(tracks, resolvedPage);

    pageIdx++;
  }

  auto pageItr =
      std::find_if(resolvedContextPages.begin(), resolvedContextPages.end(),
                   [](const ResolvedContextPage& p) { return !p.isInRoot; });

  while (!trackIndex.has_value()) {
    auto res = resolveContextPage(*pageItr);
    if (!res) {
      BELL_LOG(error, LOG_TAG, "Failed to resolve context page: {}",
               res.errorMessage());
      return res.getError();
    }

    if (!trackIndex.has_value()) {
      // Move to the next page
      ++pageItr;
      if (pageItr == resolvedContextPages.end()) {
        BELL_LOG(error, LOG_TAG, "No more pages to resolve");
        return std::errc::invalid_argument;
      }
    } else {
      BELL_LOG(info, LOG_TAG, "Found current track idx [{}, {}]",
               trackIndex.value().currentTrackPageIndex,
               trackIndex.value().currentTrackIndex);
    }
  }

  return {};
}

bell::Result<> ContextTrackResolver::resolveContextPage(
    ResolvedContextPage& page) {
  if (!page.pageUrl.has_value()) {
    BELL_LOG(error, LOG_TAG, "Page URL is not set for context page");
    return std::errc::invalid_argument;
  }

  BELL_LOG(info, LOG_TAG, "Resolving context page: {}",
           page.pageUrl.value_or(""));

  auto res = spClient->hmRequest(page.pageUrl.value());
  if (!res) {
    BELL_LOG(error, LOG_TAG, "Failed to resolve context page: {}",
             res.errorMessage());
    return res.getError();
  }

  auto contextJson = res.takeValue();

  if ((page.pageUrl == resolvedContextPages.back().pageUrl) &&
      (contextJson["next_page_url"].is_string_type())) {
    // Append a new page
    resolvedContextPages.push_back({
        .pageUrl = contextJson["next_page_url"].get_string(),
    });
  }

  if (!contextJson["tracks"].is_array()) {
    BELL_LOG(error, LOG_TAG, "Context tracks are not an array");
    return std::errc::invalid_argument;
  }

  iterateContextPage(contextJson["tracks"].get_array(), page);

  return {};
}

void ContextTrackResolver::iterateContextPage(
    const tao::json::value::array_t& tracks, ResolvedContextPage& page) {
  size_t pageIdx = std::find(resolvedContextPages.begin(),
                             resolvedContextPages.end(), page) -
                   resolvedContextPages.begin();

  size_t trackIdx = 0;
  bool pushNextTracks = false;
  bool pushPreviousTracks = !trackIndex.has_value();

  // Check if we haven't found the current track index yet
  if (trackIndex.has_value()) {
    // Last track index is at end of the previous page
    pushNextTracks =
        nextTracks.back().uid == resolvedContextPages[pageIdx - 1].lastUid;

    // Last track index is in the middle of current page
    if (trackIndex->tailTrackPageIndex == pageIdx &&
        trackIndex->tailTrackIndex < tracks.size()) {
      pushNextTracks = true;
      trackIdx = trackIndex->tailTrackIndex + 1;  // Start from the next track
    }
  }

  while (trackIdx < tracks.size()) {
    const auto& track = tracks[trackIdx];

    if (trackIdx == 0) {
      page.firstUid = track.at("uid").get_string();
    }

    page.lastUid = track.at("uid").get_string();

    if (track.at("uid") == currentTrackUid) {
      // Get index of the current page in resolvedContextPages
      BELL_LOG(info, LOG_TAG, "Found current track at index {}", trackIdx);
      trackIndex = ResolverTrackIndex{
          .currentTrackIndex = trackIdx,
          .currentTrackPageIndex = pageIdx,
          .tailTrackIndex = trackIdx,  // This will be updated later
          .tailTrackPageIndex = pageIdx,
      };

      if (pushPreviousTracks) {

        pushPreviousTracks =
            false;  // We found the current track, no need to push previous tracks
        pushNextTracks = true;  // We can push next tracks now
      }

      trackIdx++;  // Move to the next track to continue processing
      continue;
    }

    if (pushPreviousTracks) {
      // Add to previous tracks if we haven't found the current track yet
      previousTracks.push_back(contextTrackJsonToProvidedTrack(track));

      if (previousTracks.size() > maxPreviousTracksCount) {
        previousTracks.erase(previousTracks.begin());
      }
    } else if (pushNextTracks && nextTracks.size() < maxNextTracksCount) {
      // Add to next tracks if we have found the current track
      nextTracks.push_back(contextTrackJsonToProvidedTrack(track));
      trackIndex->tailTrackIndex = trackIdx;
      trackIndex->tailTrackPageIndex = pageIdx;
    }

    trackIdx++;
  }
}

cspot_proto::ProvidedTrack
ContextTrackResolver::contextTrackJsonToProvidedTrack(
    const tao::json::value& contextTrackJson) {
  cspot_proto::ProvidedTrack providedTrack;

  providedTrack.uri = contextTrackJson.at("uri").get_string();
  providedTrack.uid = contextTrackJson.at("uid").get_string();
  providedTrack.provider = "context";

  return providedTrack;
}