#pragma once

#include <cstddef>
#include <string>
#include "SessionContext.h"
#include "api/SpClient.h"
#include "proto/ConnectPb.h"

namespace cspot {

class ContextTrackResolver {
 public:
  ContextTrackResolver(std::shared_ptr<SpClient> spClient,
                       const std::string& rootContextUrl,
                       const std::string& currentTrackUid,
                       int maxPreviousTracksCount = 16,
                       int maxNextTracksCount = 16,
                       int trackUpdateThreshold = 8);
  std::span<cspot_proto::ContextTrack> previousTracks();
  std::span<cspot_proto::ContextTrack> nextTracks();

  bell::Result<cspot_proto::ContextTrack> skipForward(
      const cspot_proto::ContextTrack& track);

  bell::Result<cspot_proto::ContextTrack> skipBackward(
      const cspot_proto::ContextTrack& track);

  bell::Result<cspot_proto::ContextTrack> next();
  bell::Result<cspot_proto::ContextTrack> previous();

 private:
  const char* LOG_TAG = "ContextTrackResolver";

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

  std::string currentTrackUid;

  std::string rootContextUrl;
  std::vector<ResolvedContextPage> resolvedContextPages;

  std::queue<cspot_proto::ContextTrack> contextTrackQueue;
  uint32_t currentTrackIndexInQueue = 0;

  bell::Result<> resolveRootContext(const std::string& contextUrl);

  bell::Result<> resolveContextPage(ResolvedContextPage& page);

  void iterateContextPage(const tao::json::value::array_t& tracks,
                          ResolvedContextPage& page);
};
}  // namespace cspot
