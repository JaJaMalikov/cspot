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

      if (idx == 0) {
        contextPage->firstId = trackId;
      }

      contextPage->lastId = trackId;

      // Check if running from the root context
      contextPage->isInRoot = isRoot;

      // Store the current track in the context state
      currentTrack.index.track = static_cast<int32_t>(idx);
      currentTrack.index.page = contextPage->pageIndex;

      bool addTrackToCache = true;

      if (trackId == parseState->targetTrackId) {
        // If this is the current track, update the index in the cache
        parseState->foundTrackIndex.emplace(
            static_cast<uint32_t>(parseState->tracks.size()));
      } else if ((parseState->foundTrackIndex &&
                  parseState->fetchWindow ==
                      ContextTrackResolver::ContextFetchWindow::BeforeID) ||
                 (!parseState->foundTrackIndex &&
                  parseState->fetchWindow ==
                      ContextTrackResolver::ContextFetchWindow::AfterID)) {
        // In before ID mode, we only add tracks until we find the target
        // In after ID mode, we only add tracks after the target
        addTrackToCache = false;
      } else if (parseState->foundTrackIndex &&
                 parseState->fetchWindow ==
                     ContextTrackResolver::ContextFetchWindow::AroundID) {
        if ((parseState->tracks.size() - parseState->foundTrackIndex.value()) >
            parseState->maxNextTracksCount) {
          // Already have enough tracks after the found index
          addTrackToCache = false;
        }
      }

      if (addTrackToCache) {
        parseState->tracks.push_back(currentTrack);

        if (parseState->tracks.size() >
            (parseState->maxNextTracksCount +
             parseState->maxPreviousTracksCount + 1)) {
          // If we exceed the total track count, remove the oldest
          parseState->tracks.erase(parseState->tracks.begin());
          if (parseState->foundTrackIndex.has_value()) {
            // If we have a found track index, adjust it
            parseState->foundTrackIndex =
                parseState->foundTrackIndex.value() - 1;
          }
        }
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

                                           uint32_t maxPreviousTracksCount,
                                           uint32_t maxNextTracksCount,
                                           uint32_t trackUpdateThreshold)
    : spClient(std::move(spClient)),
      maxPreviousTracksCount(maxPreviousTracksCount),
      maxNextTracksCount(maxNextTracksCount),
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

    // Update the current tracks
    updateTracksFromParseState();
  }

  if (((trackCache.size() - currentTrackInCacheIndex.value()) <
       trackUpdateThreshold)) {
    BELL_LOG(debug, LOG_TAG,
             "Not enough tracks after current track, resolving more tracks");
    auto& lastTrackIndex = trackCache.back().index;
    uint32_t pageIndex = lastTrackIndex.page;
    if (resolvedContextPages[pageIndex].lastId == trackCache.back()) {
      // Skip page, as we are on the last track
      pageIndex++;
    }
    if (pageIndex >= resolvedContextPages.size()) {
      BELL_LOG(error, LOG_TAG,
               "No more pages to resolve, cannot find current track index");
      return {};
    }

    resetContextParseState();
    auto res = resolvedContextPages[pageIndex].isInRoot
                   ? resolveRootContext()
                   : resolveContextPage(resolvedContextPages[pageIndex]);
    if (!res) {
      BELL_LOG(error, LOG_TAG, "Failed to resolve context page: {}",
               res.errorMessage());
      return res.getError();
    }

    updateTracksFromParseState();
  } else if (currentTrackInCacheIndex.value() < trackUpdateThreshold &&
             !(trackCache.front().index.page == 0 &&
               trackCache.front().index.track == 0)) {
    BELL_LOG(debug, LOG_TAG,
             "Not enough tracks before current track, resolving more tracks");

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

    resetContextParseState();
    auto res = resolvedContextPages[pageIndex].isInRoot
                   ? resolveRootContext()
                   : resolveContextPage(resolvedContextPages[pageIndex]);
    if (!res) {
      BELL_LOG(error, LOG_TAG, "Failed to resolve context page: {}",
               res.errorMessage());
      return res.getError();
    }

    updateTracksFromParseState();
  }

  return {};
}

void ContextTrackResolver::resetContextParseState() {
  if (!currentTrackInCacheIndex.has_value()) {
    contextParseState = {
        .fetchWindow = ContextFetchWindow::AroundID,
        .foundTrackIndex = std::nullopt,
        .maxNextTracksCount = maxNextTracksCount,
        .maxPreviousTracksCount = maxPreviousTracksCount,
        .targetTrackId = currentTrackId,
        .tracks = {},
    };
  } else if ((trackCache.size() - currentTrackInCacheIndex.value()) <
             trackUpdateThreshold) {
    // If we are close to the end of the cache, we need to fetch more tracks
    contextParseState = {
        .fetchWindow = ContextFetchWindow::AfterID,
        .foundTrackIndex = currentTrackInCacheIndex,
        .maxNextTracksCount = maxNextTracksCount,
        .maxPreviousTracksCount = 0,
        .targetTrackId = {trackCache.back().uid, trackCache.back().uri},
        .tracks = {},
    };

  } else if (currentTrackInCacheIndex.value() < trackUpdateThreshold) {
    // Not enough previous tracks, fetch more
    contextParseState = {
        .fetchWindow = ContextFetchWindow::BeforeID,
        .foundTrackIndex = currentTrackInCacheIndex,
        .maxNextTracksCount = 0,
        .maxPreviousTracksCount = maxPreviousTracksCount,
        .targetTrackId = {trackCache.front().uid, trackCache.front().uri},
        .tracks = {},
    };
  } else {
    BELL_LOG(error, LOG_TAG, "Reset context called without tracks to fill");
  }
}

void ContextTrackResolver::updateTracksFromParseState() {
  BELL_LOG(debug, LOG_TAG, "Updating tracks from parse state, found index: {}",
           contextParseState.foundTrackIndex.has_value()
               ? std::to_string(contextParseState.foundTrackIndex.value())
               : "N/A");
  if (contextParseState.fetchWindow == ContextFetchWindow::AfterID) {
    // If we are in after ID mode, we only add tracks after the found index
    trackCache.insert(trackCache.end(), contextParseState.tracks.begin(),
                      contextParseState.tracks.end());
    BELL_LOG(debug, LOG_TAG, "Added tracks after found index, size: {}",
             trackCache.size());
  } else if (contextParseState.fetchWindow == ContextFetchWindow::BeforeID) {
    // If we are in before ID mode, we only add tracks before the found index
    trackCache.insert(trackCache.begin(), contextParseState.tracks.begin(),
                      contextParseState.tracks.end());
  } else if (contextParseState.fetchWindow == ContextFetchWindow::AroundID) {
    // In around ID mode, we add all tracks
    trackCache = std::move(contextParseState.tracks);
    currentTrackInCacheIndex =
        contextParseState.foundTrackIndex;  // Update the current track index
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

  resetContextParseState();
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

  BELL_LOG(info, LOG_TAG, "Context page resolved successfully");
  return {};
}
