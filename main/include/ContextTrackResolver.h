#pragma once

#include <string>
#include "api/SpClient.h"
#include "proto/ConnectPb.h"

namespace cspot {
class ContextTrackResolver {
 public:
  ContextTrackResolver(std::shared_ptr<SpClient> spClient,
                       std::string rootContextUrl, std::string currentTrackUid,
                       uint32_t maxPreviousTracksCount = 16,
                       uint32_t maxNextTracksCount = 16,
                       uint32_t trackUpdateThreshold = 8);

  bell::Result<cspot_proto::ContextTrack> getCurrentTrack();

  std::span<cspot_proto::ContextTrack> previousTracks();
  std::span<cspot_proto::ContextTrack> nextTracks();

  bell::Result<cspot_proto::ContextTrack> skipForward(
      const cspot_proto::ContextTrack& track);

  bell::Result<cspot_proto::ContextTrack> skipBackward(
      const cspot_proto::ContextTrack& track);

  bell::Result<cspot_proto::ContextTrack> next();
  bell::Result<cspot_proto::ContextTrack> previous();

  // Represents a resolved context page, can either link to a page URL or be a root context
  struct ResolvedContextPage {
    int pageIndex = 0;  // Index of this page in the root context pages
    std::optional<std::string> pageUrl = std::nullopt;
    std::optional<std::string> lastUid = std::nullopt;
    std::optional<std::string> firstUid = std::nullopt;
    std::optional<std::string> nextPageUrl = std::nullopt;

    bool isInRoot = false;

    // Implement comparison operator
    bool operator==(const ResolvedContextPage& other) const {
      return pageUrl == other.pageUrl && lastUid == other.lastUid &&
             firstUid == other.firstUid && isInRoot == other.isInRoot;
    }
  };

  // Enum to define the context fetch window type
  enum class ContextFetchWindow { BeforeID, AfterID, AroundID };

  // Struct to hold the state of the context track parsing
  struct ContextTrackParseState {
    // Fetch window configuration
    ContextFetchWindow fetchWindow;

    // Target track UID to find
    std::string targetTrackUid;

    // Resolved context pages
    std::vector<cspot_proto::ContextTrack> tracks;
    std::optional<uint32_t> foundTrackIndex = std::nullopt;

    // Window size config
    uint32_t maxPreviousTracksCount = 0;
    uint32_t maxNextTracksCount = 0;
  };

 private:
  const char* LOG_TAG = "ContextTrackResolver";

  std::shared_ptr<SpClient> spClient;

  // Root context URL, without "context://" prefix
  std::string rootContextUrl;

  // Config
  std::string currentTrackUid;
  uint32_t maxPreviousTracksCount;
  uint32_t maxNextTracksCount;
  uint32_t trackUpdateThreshold;

  // Contains state for the context track parser
  ContextTrackParseState contextParseState;
  std::vector<ResolvedContextPage> resolvedContextPages;

  std::vector<cspot_proto::ContextTrack> trackCache;
  std::optional<uint32_t> currentTrackInCacheIndex;

  void resetContextParseState();

  void updateTracksFromParseState();

  bell::Result<> ensureContextTracks();

  bell::Result<> resolveRootContext();

  bell::Result<> resolveContextPage(ResolvedContextPage& page);
};
}  // namespace cspot
