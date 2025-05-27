#pragma once

#include "ConnectDeviceState.h"
#include "SessionContext.h"
#include "api/SpClient.h"

namespace cspot {

class TrackProvider {
 public:
  TrackProvider(std::shared_ptr<SessionContext> sessionContext,
                std::shared_ptr<SpClient> spClient,
                std::shared_ptr<ConnectDeviceState> connectDeviceState);

  bell::Result<> transferContext(
      const cspot_proto::TransferState& transferState);

 private:
  const char* LOG_TAG = "TrackProvider";

  std::shared_ptr<SessionContext> sessionContext;
  std::shared_ptr<SpClient> spClient;
  std::shared_ptr<ConnectDeviceState> connectDeviceState;

  std::vector<cspot_proto::ContextTrack> contextQueue;

  static cspot_proto::ProvidedTrack contextTrackToProvidedTrack(
      const cspot_proto::ContextTrack& contextTrack);
};
}  // namespace cspot
