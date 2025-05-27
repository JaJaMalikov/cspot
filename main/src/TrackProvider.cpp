#include "TrackProvider.h"
#include "ConnectDeviceState.h"

using namespace cspot;

TrackProvider::TrackProvider(
    std::shared_ptr<SessionContext> sessionContext,
    std::shared_ptr<SpClient> spClient,
    std::shared_ptr<ConnectDeviceState> connectDeviceState)
    : sessionContext(std::move(sessionContext)),
      spClient(std::move(spClient)),
      connectDeviceState(std::move(connectDeviceState)) {}

bell::Result<> TrackProvider::transferContext(
    const cspot_proto::TransferState& transferState) {
  // Lock the connect device state to ensure thread safety
  auto lock = connectDeviceState->lock();

  // If there are any tracks in the queue, transfer them here
  for (const auto& track : transferState.queue.tracks) {
    contextQueue.push_back(track);
  }

  auto& playerState = connectDeviceState->getPlayerState();

  // Assign the context URI and URL
  playerState.contextUri = transferState.current_session.context.uri;
  playerState.contextUrl = transferState.current_session.context.url;

  auto resolvedContext = spClient->contextResolve(playerState.contextUri);

  return {};
}