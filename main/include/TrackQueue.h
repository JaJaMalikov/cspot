#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace cspot {
// Represents both a ContextTrack and ProvidedTrack
struct SpotifyTrack {
  std::string uri;
  std::string uid;
  std::vector<std::byte> gid;
  std::unordered_map<std::string, std::string> metadata;
};
}  // namespace cspot