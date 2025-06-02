#include "ContextTrackResolver.h"

#include <system_error>

#include "bell/Logger.h"
#include "bell/http/Common.h"
#include "picojson.h"

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

      if (idx == 0) {
        contextPage->firstUid =
            currentTrack.uid;  // Store the first track UID in the page
      }

      contextPage->lastUid = currentTrack.uid;  // Update last UID

      // Check if running from the root context
      contextPage->isInRoot = isRoot;

      // Store the current track in the context state
      currentTrack.index.track = static_cast<int32_t>(idx);
      currentTrack.index.page = contextPage->pageIndex;

      bool addTrackToCache = true;

      if (currentTrack.uid == parseState->targetTrackUid) {
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

      std::cout << "Parsed context page " << idx << " with URL: "
                << contextPages->at(idx).pageUrl.value_or("N/A") << std::endl;
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
                                           std::string rootContextUrl,
                                           std::string currentTrackUid,
                                           uint32_t maxPreviousTracksCount,
                                           uint32_t maxNextTracksCount,
                                           uint32_t trackUpdateThreshold)
    : spClient(std::move(spClient)),
      rootContextUrl(std::move(rootContextUrl)),
      currentTrackUid(std::move(currentTrackUid)),
      maxPreviousTracksCount(maxPreviousTracksCount),
      maxNextTracksCount(maxNextTracksCount),
      trackUpdateThreshold(trackUpdateThreshold) {

  this->rootContextUrl =
      this->rootContextUrl.substr(10);  // Remove "context://"
}

bell::Result<cspot_proto::ContextTrack>
ContextTrackResolver::getCurrentTrack() {
  auto res = ensureContextTracks();
  if (!res) {
    BELL_LOG(error, LOG_TAG, "Failed to ensure context tracks: {}",
             res.errorMessage());
    return res.getError();
  }
  return std::errc::executable_format_error;
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

  if ((trackCache.size() - currentTrackInCacheIndex.value()) <
      trackUpdateThreshold) {
          BELL_LOG(debug, LOG_TAG,
                 "Not enough tracks after current track, resolving more tracks");
    auto& lastTrackIndex = trackCache.back().index;
    uint32_t pageIndex = lastTrackIndex.page;
    if (resolvedContextPages[pageIndex].lastUid == trackCache.back().uid) {
      // Skip page, as we are on the last track
      pageIndex++;
      if (pageIndex >= resolvedContextPages.size()) {
        BELL_LOG(error, LOG_TAG,
                 "No more pages to resolve, cannot find current track index");
        return {};
        ;
      }

      resetContextParseState();
      auto res = resolveContextPage(resolvedContextPages[pageIndex]);
      if (!res) {
        BELL_LOG(error, LOG_TAG, "Failed to resolve context page: {}",
                 res.errorMessage());
        return res.getError();
      }

      updateTracksFromParseState();
    } else if (currentTrackInCacheIndex.value() < trackUpdateThreshold) {
        BELL_LOG(debug, LOG_TAG,
               "Not enough tracks before current track, resolving more tracks");

      auto& firstTrackIndex = trackCache.front().index;
      if (resolvedContextPages[firstTrackIndex.page].firstUid ==
          trackCache.front().uid) {
        // Skip page, as we are on the first track
        if (firstTrackIndex.page == 0) {
          return {};  // No previous page to resolve
        }
        pageIndex = firstTrackIndex.page - 1;
      }

      resetContextParseState();
      auto res = resolveContextPage(resolvedContextPages[pageIndex]);
      if (!res) {
        BELL_LOG(error, LOG_TAG, "Failed to resolve context page: {}",
                 res.errorMessage());
        return res.getError();
      }

      updateTracksFromParseState();
    }
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
        .targetTrackUid = currentTrackUid,
        .tracks = {},
    };
  } else if ((trackCache.size() - currentTrackInCacheIndex.value()) <
             trackUpdateThreshold) {
    // If we are close to the end of the cache, we need to fetch more tracks
    contextParseState = {
        .fetchWindow = ContextFetchWindow::AfterID,
        .foundTrackIndex = std::nullopt,
        .maxNextTracksCount = maxNextTracksCount,
        .maxPreviousTracksCount = 0,
        .targetTrackUid = trackCache.back().uid,
        .tracks = {},
    };
  } else if (currentTrackInCacheIndex.value() < trackUpdateThreshold) {
    // Not enough previous tracks, fetch more
    contextParseState = {
        .fetchWindow = ContextFetchWindow::BeforeID,
        .foundTrackIndex = currentTrackInCacheIndex,
        .maxNextTracksCount = 0,
        .maxPreviousTracksCount = maxPreviousTracksCount,
        .targetTrackUid = trackCache.front().uid,
        .tracks = {},
    };
  } else {
    BELL_LOG(error, LOG_TAG, "Reset context called without tracks to fill");
  }
}

void ContextTrackResolver::updateTracksFromParseState() {
    BELL_LOG(debug, LOG_TAG,
           "Updating tracks from parse state, found index: {}",
           contextParseState.foundTrackIndex.has_value()
               ? std::to_string(contextParseState.foundTrackIndex.value())
               : "N/A");
  if (contextParseState.fetchWindow == ContextFetchWindow::AfterID) {
    // If we are in after ID mode, we only add tracks after the found index
    trackCache.insert(trackCache.end(), contextParseState.tracks.begin(),
                      contextParseState.tracks.end());
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

  // Prepare the parse state
  ContextTrackParseState parserState = {
      .targetTrackUid = currentTrackUid,
  };

  resetContextParseState();
  auto parseCtx = ContextRootParseContext(&parserState, &resolvedContextPages);
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
  while (!parserState.foundTrackIndex.has_value()) {
    BELL_LOG(info, LOG_TAG,
             "Current track index not found, resolving next page");
    uint32_t nextPageIndex = parserState.tracks.back().index.page + 1;
    std::cout << "Next page index: " << nextPageIndex << std::endl;
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

  BELL_LOG(info, LOG_TAG,
           "Current track index found: {}",
           parserState.foundTrackIndex.has_value()
               ? std::to_string(parserState.foundTrackIndex.value())
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

  // Prepare the parse state
  ContextTrackParseState parserState = {
      .targetTrackUid = currentTrackUid,
  };

  auto parseCtx = ContextPageParseContext(&parserState, &page);
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
