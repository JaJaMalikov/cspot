#pragma once

#include "SessionContext.h"
#include "api/SpClient.h"
#include "proto/SpotifyId.h"

namespace cspot {

class TrackProvider {
 public:
  TrackProvider(std::shared_ptr<SessionContext> sessionContext,
                std::shared_ptr<SpClient> spClient);

  bell::Result<> provideTrack(const SpotifyId& trackId);

 private:
  const char* LOG_TAG = "TrackProvider";
};
}  // namespace cspot
