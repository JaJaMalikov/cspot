#include "TrackProvider.h"

// #include <bell/Logger.h>
// #include "proto/SpotifyId.h"

using namespace cspot;

TrackProvider::TrackProvider(std::shared_ptr<SessionContext> sessionContext,
                             std::shared_ptr<SpClient> spClient)
    : sessionContext(std::move(sessionContext)), spClient(std::move(spClient)) {
  // this->contextTrackResolver = std::make_shared<ContextTrackResolver>(
  //     this->sessionContext, this->spClient);
}

// void TrackProvider::setQueue(const cspot_proto::Queue& queue) {
//   if (queue.isPlayingQueue) {
    
//   }
// }

// cspot_proto::ProvidedTrack TrackProvider::getCurrentTrack() {
//   return this->currentTrack;
// }

// std::vector<cspot_proto::ProvidedTrack> TrackProvider::getNextTracks() {
//   return this->contextTrackResolver->getNextTracks();
// }
// std::vector<cspot_proto::ProvidedTrack> TrackProvider::getPreviousTracks() {
//   return this->contextTrackResolver->getPreviousTracks();
// }

// bell::Result<> TrackProvider::setTrackContext(
//     const cspot_proto::Context& context, const std::string& currentTrackUid) {
//   auto res =
//       this->contextTrackResolver->resolveContext(context.url, currentTrackUid);

//   if (!res) {
//     BELL_LOG(error, LOG_TAG, "Failed to resolve context: {}",
//              res.errorMessage());
//     return res.getError();
//   }

//   BELL_LOG(info, LOG_TAG, "Context resolved successfully: {}", context.url);

//   auto contextIdx = this->contextTrackResolver->getCurrentContextIndex();

//   this->currentContextIndex.emplace();
//   this->currentContextIndex->page = contextIdx->first;
//   this->currentContextIndex->track = contextIdx->second;

//   // Post state update
//   sessionContext->eventLoop->post(EventLoop::EventType::TRACKPROVIDER_UPDATED,
//                                   std::monostate{});

//   return {};
// };

// bell::Result<> TrackProvider::provideTrack(const SpotifyId& trackId) {
//   if (trackId.type == SpotifyIdType::Track) {
//     auto metadataRes = spClient->trackMetadata(trackId);
//     if (!metadataRes) {
//       BELL_LOG(error, LOG_TAG, "Failed to fetch track metadata: {}",
//                metadataRes.errorMessage());
//       return metadataRes.getError();
//     }

//     auto trackProto = metadataRes.takeValue();
//     sessionContext->eventLoop->post(
//         EventLoop::EventType::CURRENT_TRACK_METADATA,
//         CurrentTrackMetadata{.trackId = trackId,
//                              .name = trackProto.name,
//                              .durationMs = trackProto.durationMs});

//   } else {
//     // TODO: episode
//   }

//   return {};
// }