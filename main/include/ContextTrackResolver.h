#pragma once

#include <string>
#include "SessionContext.h"
#include "api/SpClient.h"

namespace cspot {

class ContextTrackResolver {
 public:
  ContextTrackResolver(std::shared_ptr<SessionContext> sessionContext,
                       std::shared_ptr<SpClient> spClients);

  bell::Result<> resolveContext(const std::string& contextUrl,
                                const std::string& currentTrackUid);

  bell::Result<bool> updateTracks();

  static cspot_proto::ProvidedTrack contextTrackJsonToProvidedTrack(
      const tao::json::value& contextTrackJson);

 private:
  const char* LOG_TAG = "ContextTrackResolver";

  static const int maxPreviousTracksCount = 5;
  static const int maxNextTracksCount = 16;
  static const int nextTracksThreshold = 8;

  std::shared_ptr<SessionContext> sessionContext;
  std::shared_ptr<SpClient> spClient;

  // Represents a resolved context page, can either link to a page URL or be a root context
  struct ResolvedContextPage {
    std::optional<std::string> pageUrl = std::nullopt;
    std::optional<std::string> lastUid = std::nullopt;
    std::optional<std::string> firstUid = std::nullopt;

    bool isInRoot = false;

    // Implement comparison operator
    bool operator==(const ResolvedContextPage& other) const {
      return pageUrl == other.pageUrl && lastUid == other.lastUid &&
             firstUid == other.firstUid && isInRoot == other.isInRoot;
    }
  };

  struct ResolverTrackIndex {
    // Index of the currently played back track
    // in the context page
    size_t currentTrackIndex;
    size_t currentTrackPageIndex;

    // Index of the last track in the next tracks list
    size_t tailTrackIndex;
    size_t tailTrackPageIndex;
  };

  std::string currentTrackUid;

  std::optional<ResolverTrackIndex> trackIndex;

  std::string rootContextUrl;
  std::vector<ResolvedContextPage> resolvedContextPages;

  std::vector<cspot_proto::ProvidedTrack> previousTracks;
  std::vector<cspot_proto::ProvidedTrack> nextTracks;

  bell::Result<> resolveRootContext(const std::string& contextUrl);

  bell::Result<> resolveContextPage(ResolvedContextPage& page);

  void iterateContextPage(const tao::json::value::array_t& tracks,
                          ResolvedContextPage& page);
};
}  // namespace cspot
