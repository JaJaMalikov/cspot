#pragma once

#include <iostream>
#include <string>
#include "api/SpClient.h"
#include "proto/ConnectPb.h"
#include "tcb/span.hpp"

namespace cspot {
class ContextTrackResolver {
 public:
  ContextTrackResolver(std::shared_ptr<SpClient> spClient,
                       uint32_t maxPreviousTracksCount = 16,
                       uint32_t maxNextTracksCount = 16,
                       uint32_t trackUpdateThreshold = 8);

  /**
   * @brief Sets the context for the resolver
   */
  void updateContext(const std::string& rootContextUrl,
                     std::optional<std::string>& currentTrackUid,
                     std::optional<std::string>& currentTrackUri);

  bell::Result<cspot_proto::ContextTrack> getCurrentTrack();

  tcb::span<cspot_proto::ContextTrack> previousTracks();
  tcb::span<cspot_proto::ContextTrack> nextTracks();

  bell::Result<cspot_proto::ContextTrack> skipForward(
      const cspot_proto::ContextTrack& track);

  bell::Result<cspot_proto::ContextTrack> skipBackward(
      const cspot_proto::ContextTrack& track);

  bell::Result<cspot_proto::ContextTrack> next();
  bell::Result<cspot_proto::ContextTrack> previous();

  // Context tracks IDs or URIs can sometimes be missing or invalid
  struct TrackId {
    std::optional<std::string> uid = std::nullopt;
    std::optional<std::string> uri = std::nullopt;

    TrackId() = default;

    TrackId(const std::string& uid, const std::string& uri) {
      if (!uid.empty()) {
        this->uid = uid;
      }
      if (!uri.empty()) {
        this->uri = uri;
      }
    }

    bool operator==(const TrackId& other) const {
      if (uid.has_value() && other.uid.has_value()) {
        return uid.value() == other.uid.value();
      }

      if (uri.has_value() && other.uri.has_value()) {
        return uri.value() == other.uri.value();
      }

      return false;
    }

    bool operator==(const cspot_proto::ContextTrack& other) const {
      if (!other.uid.empty() && uid.has_value()) {
        return uid.value() == other.uid;
      }
      if (!other.uri.empty() && uri.has_value()) {
        return uri.value() == other.uri;
      }
      return false;
    }
  };

  // Represents a resolved context page, can either link to a page URL or be a root context
  struct ResolvedContextPage {
    int pageIndex = 0;  // Index of this page in the root context pages
    std::optional<std::string> pageUrl = std::nullopt;
    std::optional<TrackId> lastId = std::nullopt;
    std::optional<TrackId> firstId = std::nullopt;
    std::optional<std::string> nextPageUrl = std::nullopt;

    bool isInRoot = false;

    // Implement comparison operator
    bool operator==(const ResolvedContextPage& other) const {
      return pageUrl == other.pageUrl && lastId == other.lastId &&
             firstId == other.firstId && isInRoot == other.isInRoot;
    }
  };

  // Enum to define the context fetch window type
  enum class ContextFetchWindow { BeforeID, AfterID, AroundID };

  // Struct to hold the state of the context track parsing
  struct ContextTrackParseState {
    // Fetch window configuration
    ContextFetchWindow fetchWindow;

    // Target track ID
    TrackId targetTrackId;

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
  TrackId currentTrackId;

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
