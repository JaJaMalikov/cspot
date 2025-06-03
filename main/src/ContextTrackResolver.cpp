#include "ContextTrackResolver.h"

#include <span>
#include <system_error>

#include "bell/Logger.h"
#include "bell/http/Common.h"
#include "picojson.h"
#include "proto/ConnectPb.h"

using namespace cspot;

namespace {
// PicoJSON parse for context track data
class ContextTrackParseContext : public picojson::null_parse_context {
 public:
  ContextTrackParseContext(cspot_proto::ContextTrack* contextTrack)
      : contextTrack(contextTrack) {}

  template <typename Iter>
  bool parse_string(picojson::input<Iter>& in) {
    if (currentObjectKey == "uid") {
      contextTrack->uid.clear();  // Clear previous value
      return picojson::_parse_string(contextTrack->uid, in);
    }

    if (currentObjectKey == "uri") {
      contextTrack->uri.clear();  // Clear previous value
      return picojson::_parse_string(contextTrack->uri, in);
    }

    return picojson::null_parse_context::parse_string(in);
  }

  template <typename Iter>
  bool parse_object_item(picojson::input<Iter>& in, const std::string& key) {
    // Store the key for later use
    currentObjectKey = key;
    return _parse(*this, in);
  }

 private:
  std::string currentObjectKey;
  cspot_proto::ContextTrack* contextTrack;
};

// PicoJSON parser for context page data
class ContextPageParseContext : public picojson::null_parse_context {
 public:
  ContextPageParseContext(
      ContextTrackResolver::ContextTrackParseState* parseState,
      ContextTrackResolver::ResolvedContextPage* contextPage,
      bool isRoot = false)
      : parseState(parseState), contextPage(contextPage), isRoot(isRoot) {}

  template <typename Iter>
  bool parse_array_item(picojson::input<Iter>& in, size_t idx) {
    if (currentObjectKey == "tracks") {
      _parse(contextTrackParser, in);

      ContextTrackResolver::TrackId trackId(currentTrack.uid, currentTrack.uri);

      if (contextPage->trackIndexes.size() < idx + 1) {
        // Keep track of the index of each track in the context page
        contextPage->trackIndexes.push_back(idx);

        if ((contextPage->fetchWindowEnd - contextPage->fetchWindowStart) <
            (parseState->maxWindowSize)) {
          contextPage->fetchWindowEnd++;  // Expand the fetch window
        }
      }

      if (idx == 0) {
        contextPage->firstId = trackId;
      }

      contextPage->lastId = trackId;

      // Check if running from the root context
      contextPage->isInRoot = isRoot;

      // Store the current track in the context state
      currentTrack.index.track = static_cast<int32_t>(idx);
      currentTrack.index.page = contextPage->pageIndex;

      if (!parseState->foundTrackIndex &&
          (contextPage->fetchWindowEnd - contextPage->fetchWindowStart) >
              parseState->maxWindowSize) {
        // Move the window forward
        contextPage->fetchWindowStart++;
        contextPage->fetchWindowEnd++;

        // Remove the oldest track from the cache
        parseState->tracks.erase(parseState->tracks.begin());
      }

      if ((trackId == parseState->targetTrackId) &&
          !parseState->foundTrackIndex) {
        uint32_t previousTracksInWindow = (idx - contextPage->fetchWindowStart);
        uint32_t maxPreviousTracks = (parseState->maxWindowSize - 1) / 2;
        if (previousTracksInWindow > maxPreviousTracks) {
          uint32_t tracksToRemove = previousTracksInWindow - maxPreviousTracks;
          contextPage->fetchWindowStart += tracksToRemove;
          parseState->tracks.erase(parseState->tracks.begin(),
                                   parseState->tracks.begin() + tracksToRemove);
        }

        // If this is the current track, update the index in the cache
        parseState->foundTrackIndex.emplace(
            static_cast<uint32_t>(parseState->tracks.size()));
      }

      // Construct the fetch window
      tcb::span<uint32_t> fetchWindowIds = {
          contextPage->trackIndexes.data() + contextPage->fetchWindowStart,
          contextPage->trackIndexes.data() + contextPage->fetchWindowEnd};

      auto* idxInWindow =
          std::find(fetchWindowIds.begin(), fetchWindowIds.end(), idx);
      if (idxInWindow != fetchWindowIds.end()) {
        uint32_t indexToInsert =
            std::distance(fetchWindowIds.begin(), idxInWindow);
        if (parseState->tracks.size() < indexToInsert + 1) {
          parseState->tracks.resize(indexToInsert + 1);
        }

        // Insert the current track at the correct index
        parseState->tracks[indexToInsert] = currentTrack;
      }

      return true;
    }

    return _parse(*this, in);
  }

  template <typename Iter>
  bool parse_string(picojson::input<Iter>& in) {
    if (currentObjectKey == "page_url") {
      contextPage->pageUrl.emplace();
      return picojson::_parse_string(contextPage->pageUrl.value(), in);
    }

    if (currentObjectKey == "next_page_url") {
      contextPage->nextPageUrl.emplace();
      return picojson::_parse_string(contextPage->nextPageUrl.value(), in);
    }

    return picojson::null_parse_context::parse_string(in);
  }

  template <typename Iter>
  bool parse_object_item(picojson::input<Iter>& in, const std::string& key) {
    // Store the key for later use
    currentObjectKey = key;
    return _parse(*this, in);
  }

 private:
  std::string currentObjectKey;

  ContextTrackResolver::ContextTrackParseState* parseState;

  ContextTrackResolver::ResolvedContextPage* contextPage;
  bool isRoot = false;

  cspot_proto::ContextTrack currentTrack;
  ContextTrackParseContext contextTrackParser{&currentTrack};
};

// PicoJSON parser for context root data
class ContextRootParseContext : public picojson::null_parse_context {
 public:
  ContextRootParseContext(
      ContextTrackResolver::ContextTrackParseState* parseState,
      std::vector<ContextTrackResolver::ResolvedContextPage>* contextPages)
      : parseState(parseState), contextPages(contextPages) {}

  template <typename Iter>
  bool parse_array_item(picojson::input<Iter>& in, size_t idx) {
    if (currentObjectKey == "pages") {
      if (contextPages->size() <= idx) {
        // Ensure we have enough space in the context pages vector
        contextPages->resize(idx + 1);
      }

      // Assign page index to the context page
      std::cout << "Assigning page index " << idx << " to context page " << std::endl;
      contextPages->at(idx).pageIndex = static_cast<int>(idx);

      auto pageCtx =
          ContextPageParseContext(parseState, &(*contextPages)[idx], true);

      // Parse the context page
      _parse(pageCtx, in);

      return true;
    }

    return _parse(*this, in);
  }

  template <typename Iter>
  bool parse_object_item(picojson::input<Iter>& in, const std::string& key) {
    // Store the key for later use
    currentObjectKey = key;
    return _parse(*this, in);
  }

  bool parse_object_stop() { return true; }

 private:
  std::string currentObjectKey;

  ContextTrackResolver::ContextTrackParseState* parseState;
  std::vector<ContextTrackResolver::ResolvedContextPage>* contextPages;
};
}  // namespace

ContextTrackResolver::ContextTrackResolver(std::shared_ptr<SpClient> spClient,
                                           uint32_t maxWindowSize,
                                           uint32_t trackUpdateThreshold)
    : spClient(std::move(spClient)),
      maxWindowSize(maxWindowSize),
      trackUpdateThreshold(trackUpdateThreshold) {}

void ContextTrackResolver::updateContext(
    const std::string& rootContextUrl,
    std::optional<std::string>& currentTrackUid,
    std::optional<std::string>& currentTrackUri) {
  this->rootContextUrl = rootContextUrl.substr(10);  // remove context://
  this->currentTrackId.uid = currentTrackUid;
  this->currentTrackId.uri = currentTrackUri;
}

bell::Result<cspot_proto::ContextTrack>
ContextTrackResolver::getCurrentTrack() {
  auto res = ensureContextTracks();
  if (!res) {
    BELL_LOG(error, LOG_TAG, "Failed to ensure context tracks: {}",
             res.errorMessage());
    return res.getError();
  }

  return trackCache[currentTrackInCacheIndex.value()];
}

tcb::span<cspot_proto::ContextTrack> ContextTrackResolver::previousTracks() {
  if (!currentTrackInCacheIndex.has_value()) {
    return {};
  }
  return {trackCache.data(), currentTrackInCacheIndex.value()};
}

tcb::span<cspot_proto::ContextTrack> ContextTrackResolver::nextTracks() {
  if (!currentTrackInCacheIndex.has_value()) {
    return {};
  }
  return {trackCache.data() + currentTrackInCacheIndex.value() + 1,
          trackCache.size() - currentTrackInCacheIndex.value() - 1};
}

bell::Result<cspot_proto::ContextTrack> ContextTrackResolver::next() {
  auto res = ensureContextTracks();
  if (!res) {
    BELL_LOG(error, LOG_TAG, "Failed to ensure context tracks: {}",
             res.errorMessage());
    return res.getError();
  }

  if (currentTrackInCacheIndex.value() + 1 >= trackCache.size()) {
    BELL_LOG(error, LOG_TAG, "No next track available");
    return std::errc::no_message_available;
  }

  if (currentTrackInCacheIndex.value() > trackUpdateThreshold) {
    // Erase oldest track
    trackCache.erase(trackCache.begin());
  } else {
    currentTrackInCacheIndex.value()++;
  }

  return trackCache[currentTrackInCacheIndex.value()];
}

bell::Result<cspot_proto::ContextTrack> ContextTrackResolver::previous() {
  auto res = ensureContextTracks();
  if (!res) {
    BELL_LOG(error, LOG_TAG, "Failed to ensure context tracks: {}",
             res.errorMessage());
    return res.getError();
  }

  if (currentTrackInCacheIndex.value() + 1 >= trackCache.size()) {
    BELL_LOG(error, LOG_TAG, "No next track available");
    return std::errc::no_message_available;
  }

  trackCache.erase(trackCache.end());

  // Go to previous track
  currentTrackInCacheIndex.value() -= 1;

  return trackCache[currentTrackInCacheIndex.value()];
}

bell::Result<> ContextTrackResolver::ensureContextTracks() {
  if (!currentTrackInCacheIndex.has_value()) {
    // No track index yet, resolve root context
    auto res = resolveRootContext();
    if (!res) {
      BELL_LOG(error, LOG_TAG, "Failed to resolve root context: {}",
               res.errorMessage());
      return res.getError();
    }
  }

  if (((trackCache.size() - currentTrackInCacheIndex.value()) <
       trackUpdateThreshold)) {
    auto& lastTrackIndex = trackCache.back().index;
    uint32_t pageIndex = lastTrackIndex.page;
    if (resolvedContextPages[pageIndex].fetchWindowEnd ==
        resolvedContextPages[pageIndex].trackIndexes.size()) {
      // Skip page, as we are on the last track
      pageIndex++;
    }
    if (pageIndex >= resolvedContextPages.size()) {
      return {};  // No more pages to resolve
    }

    BELL_LOG(debug, LOG_TAG,
             "Not enough tracks after current track, resolving more tracks");

    auto res = resolvedContextPages[pageIndex].isInRoot
                   ? resolveRootContext()
                   : resolveContextPage(resolvedContextPages[pageIndex]);
    if (!res) {
      BELL_LOG(error, LOG_TAG, "Failed to resolve context page: {}",
               res.errorMessage());
      return res.getError();
    }

  } else if (currentTrackInCacheIndex.value() < trackUpdateThreshold &&
             !(trackCache.front().index.page == 0 &&
               resolvedContextPages[0].fetchWindowStart == 0)) {
    BELL_LOG(
        debug, LOG_TAG,
        "Not enough tracks before current track, resolving more tracks, {} - {}",
        resolvedContextPages[0].fetchWindowStart,
        trackCache.front().index.page);

    auto& firstTrackIndex = trackCache.front().index;

    uint32_t pageIndex = firstTrackIndex.page;

    if (resolvedContextPages[firstTrackIndex.page].firstId ==
        trackCache.front()) {
      // Skip page, as we are on the first track
      if (firstTrackIndex.page == 0) {
        return {};  // No previous page to resolve
      }
      pageIndex = firstTrackIndex.page - 1;
    }

    auto res = resolvedContextPages[pageIndex].isInRoot
                   ? resolveRootContext()
                   : resolveContextPage(resolvedContextPages[pageIndex]);
    if (!res) {
      BELL_LOG(error, LOG_TAG, "Failed to resolve context page: {}",
               res.errorMessage());
      return res.getError();
    }
  }

  return {};
}

bool ContextTrackResolver::prepareParseState() {
  if (!currentTrackInCacheIndex.has_value()) {
    contextParseState = {.foundTrackIndex = std::nullopt,
                         .maxWindowSize = maxWindowSize,
                         .targetTrackId = currentTrackId,
                         .tracks = {},
                         .fetchMode = FetchMode::AddNext};
  } else if ((trackCache.size() - currentTrackInCacheIndex.value()) <
             trackUpdateThreshold) {
    uint32_t pageIdx = trackCache.front().index.page;
    auto& page = resolvedContextPages[trackCache.front().index.page];

    if (page.fetchWindowEnd == page.trackIndexes.size()) {
      // Already at end of the window, go up one page
      if (resolvedContextPages.size() < pageIdx + 2) {
        BELL_LOG(error, LOG_TAG, "Already at end of context");
        return false;
      }

      page = resolvedContextPages[pageIdx + 1];
      page.fetchWindowStart = page.fetchWindowEnd - maxWindowSize;
    } else {
      uint32_t windowSize = std::min(
          maxWindowSize, static_cast<uint32_t>(page.trackIndexes.size()) -
                             page.fetchWindowEnd);
      page.fetchWindowStart += windowSize;
      page.fetchWindowEnd += windowSize;
    }

    // If we are close to the end of the cache, we need to fetch more tracks
    contextParseState = {
        .foundTrackIndex = currentTrackInCacheIndex,
        .maxWindowSize = page.fetchWindowEnd - page.fetchWindowStart,
        .tracks = {},
        .fetchMode = FetchMode::AddNext};
  } else if (currentTrackInCacheIndex.value() < trackUpdateThreshold) {
    uint32_t pageIdx = trackCache.back().index.page;
    auto& page = resolvedContextPages[trackCache.back().index.page];

    if (page.fetchWindowStart == 0) {
      // Already at beginning of the window, go back one page
      if (trackCache.back().index.page == 0) {
        BELL_LOG(error, LOG_TAG, "Already at beginning of cache");
        return false;
      }

      page = resolvedContextPages[pageIdx - 1];
      page.fetchWindowStart = page.fetchWindowEnd - maxWindowSize;
    } else {
      uint32_t windowSize = std::min(maxWindowSize, page.fetchWindowStart);
      page.fetchWindowStart -= windowSize;
      page.fetchWindowEnd -= windowSize;
    }

    // If we are close to the end of the cache, we need to fetch more tracks
    contextParseState = {
        .foundTrackIndex = currentTrackInCacheIndex,
        .maxWindowSize = page.fetchWindowEnd - page.fetchWindowStart,
        .tracks = {},
        .fetchMode = FetchMode::AddPrevious};
  } else {
    BELL_LOG(error, LOG_TAG, "Reset context called without tracks to fill");
  }

  return true;
}

void ContextTrackResolver::updateTracksFromParseState() {
  BELL_LOG(debug, LOG_TAG, "Updating tracks from parse state, found index: {}",
           contextParseState.foundTrackIndex.has_value()
               ? std::to_string(contextParseState.foundTrackIndex.value())
               : "N/A");
  if (contextParseState.fetchMode == FetchMode::AddNext) {
    if (!currentTrackInCacheIndex.has_value() && (trackCache.size() + contextParseState.tracks.size() > maxWindowSize)) {
      uint32_t tracksToRemove =
          trackCache.size() + contextParseState.tracks.size() - maxWindowSize;
      trackCache.erase(trackCache.begin(), trackCache.begin() + tracksToRemove);
      if (currentTrackInCacheIndex.has_value()) {
        currentTrackInCacheIndex.value() -= tracksToRemove;
      }
    }

    // If we are in add next, we only add tracks after the found index
    trackCache.insert(trackCache.end(), contextParseState.tracks.begin(),
                      contextParseState.tracks.end());

    currentTrackInCacheIndex = contextParseState.foundTrackIndex;
  } else if (contextParseState.fetchMode == FetchMode::AddPrevious) {
    // If we are in add prev mode, we only add tracks before the found index
    trackCache.insert(trackCache.begin(), contextParseState.tracks.begin(),
                      contextParseState.tracks.end());
    currentTrackInCacheIndex.value() += contextParseState.tracks.size();
  }
}

bell::Result<> ContextTrackResolver::resolveRootContext() {
  BELL_LOG(info, LOG_TAG, "Resolving root context: {}", rootContextUrl);

  auto reader = spClient->contextResolve(rootContextUrl);
  if (!reader) {
    BELL_LOG(error, LOG_TAG, "Failed to resolve root context: {}",
             reader.errorMessage());
    return reader.getError();
  }

  auto* rawDataStream = reader.getValue().getStream();

  if (!prepareParseState()) {
    BELL_LOG(error, LOG_TAG, "Failed to prepare parse state");
    return {};
  }

  auto parseCtx =
      ContextRootParseContext(&contextParseState, &resolvedContextPages);
  std::string parseError;
  picojson::_parse(parseCtx,
                   std::istreambuf_iterator<char>(rawDataStream->rdbuf()),
                   std::istreambuf_iterator<char>(), &parseError);

  if (!parseError.empty()) {
    BELL_LOG(error, LOG_TAG, "Failed to parse context data: {}", parseError);
    return std::errc::invalid_argument;
  }

  if (resolvedContextPages.end()->nextPageUrl.has_value()) {
    // If the last page has a next page URL, we need to add it to the resolved pages
    resolvedContextPages.push_back({
        .pageUrl = resolvedContextPages.end()->nextPageUrl.value(),
    });
  }

  updateTracksFromParseState();

  BELL_LOG(info, LOG_TAG, "Root context resolved successfully");
  uint32_t nextPageIndex = contextParseState.tracks.back().index.page;

  while (!contextParseState.foundTrackIndex.has_value()) {
    nextPageIndex++;
    BELL_LOG(info, LOG_TAG,
             "Current track index not found, resolving next page");
    BELL_LOG(info, LOG_TAG, "Next page index: {}", nextPageIndex);
    if (nextPageIndex >= resolvedContextPages.size()) {
      BELL_LOG(error, LOG_TAG,
               "No more pages to resolve, cannot find current track index");
      return std::errc::invalid_argument;
    }

    auto res = resolveContextPage(resolvedContextPages[nextPageIndex]);
    if (!res) {
      BELL_LOG(error, LOG_TAG, "Failed to resolve context page: {}",
               res.errorMessage());
      return res.getError();
    }

    if (resolvedContextPages.end()->nextPageUrl.has_value()) {
      // If the last page has a next page URL, we need to add it to the resolved pages
      resolvedContextPages.push_back({
          .pageUrl = resolvedContextPages.end()->nextPageUrl.value(),
      });
    }
  }

  BELL_LOG(info, LOG_TAG, "Current track index found: {}",
           contextParseState.foundTrackIndex.has_value()
               ? std::to_string(contextParseState.foundTrackIndex.value())
               : "N/A");

  return {};
}

bell::Result<> ContextTrackResolver::resolveContextPage(
    ResolvedContextPage& page) {
  if (!page.pageUrl.has_value()) {
    BELL_LOG(error, LOG_TAG, "Context page URL is not set");
  }
  BELL_LOG(info, LOG_TAG, "Resolving context page: {}", page.pageUrl.value());

  if (!prepareParseState()) {
    BELL_LOG(error, LOG_TAG, "Failed to prepare parse state");
    return std::errc::invalid_argument;
  }

  auto reader = spClient->doRequest(bell::http::Method::GET,
                                    page.pageUrl.value().substr(5));
  if (!reader) {
    BELL_LOG(error, LOG_TAG, "Failed to resolve context page: {}",
             reader.errorMessage());
    return reader.getError();
  }

  auto* rawDataStream = reader.getValue().getStream();

  auto parseCtx = ContextPageParseContext(&contextParseState, &page);
  std::string parseError;
  picojson::_parse(parseCtx,
                   std::istreambuf_iterator<char>(rawDataStream->rdbuf()),
                   std::istreambuf_iterator<char>(), &parseError);

  if (!parseError.empty()) {
    BELL_LOG(error, LOG_TAG, "Failed to parse context page data: {}",
             parseError);
    return std::errc::invalid_argument;
  }

  updateTracksFromParseState();
  BELL_LOG(info, LOG_TAG, "Context page resolved successfully");
  return {};
}
