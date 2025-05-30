#pragma once

#include "ContextTrackResolver.h"
#include "SessionContext.h"
#include "api/SpClient.h"
#include "proto/ConnectPb.h"
#include "proto/SpotifyId.h"

namespace cspot {

/**
 * @brief Manages the current track and context, providing methods to load tracks,
 * skip tracks, and retrieve next/previous tracks.
 */
class TrackProvider {
 public:
  TrackProvider(std::shared_ptr<SessionContext> sessionContext,
                std::shared_ptr<SpClient> spClient);

  /**
   * @brief Sets the queue for the track provider.
   */
  void setQueue(const cspot_proto::Queue& queue);

  /**
   * @brief sets the current track and context. This will completely replace the current context and track, and reset the queue.
   */
  bell::Result<> loadTrackAndContext(const SpotifyId& trackId,
                                     const cspot_proto::Context& context);

  std::optional<cspot_proto::ProvidedTrack> currentTrack();

  std::optional<cspot_proto::ContextIndex> currentContextIndex();

  std::vector<cspot_proto::ProvidedTrack> getNextTracks();
  std::vector<cspot_proto::ProvidedTrack> getPreviousTracks();

  void skipToNextTrack(
      std::optional<cspot_proto::ContextTrack> track = std::nullopt);

  void skipToPreviousTrack(
      std::optional<cspot_proto::ContextTrack> track = std::nullopt);

 private:
  const char* LOG_TAG = "TrackProvider";

  std::shared_ptr<SessionContext> sessionContext;
  std::shared_ptr<SpClient> spClient;
  std::shared_ptr<ContextTrackResolver> contextTrackResolver;

  std::vector<cspot_proto::ProvidedTrack> queueTracks;

  struct ProviderState {
    // Contains information about the current track
    cspot_proto::ProvidedTrack currentTrack;

    // Contains information about index of current track in the context
    cspot_proto::ContextIndex currentContextIndex;

    // Contains the UID of the current track in context
    std::string currentContextUid;

    // Whether we are currently playing a queue
    bool isPlayingQueue = false;

    // Contains manually queued tracks, outside of the context
    std::vector<cspot_proto::ProvidedTrack> trackQueue;

    // Contains next tracks in the context
    std::vector<cspot_proto::ProvidedTrack> nextCtxTracks;
    std::vector<cspot_proto::ProvidedTrack> prevCtxTracks;
  };

  ProviderState providerState;
};
}  // namespace cspot
