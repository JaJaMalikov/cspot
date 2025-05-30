#pragma once

#include "proto/SpotifyId.h"

namespace cspot {
struct CurrentTrackMetadata {
  SpotifyId trackId;
  std::string name;
  int32_t durationMs = 0;
};
};  // namespace cspot