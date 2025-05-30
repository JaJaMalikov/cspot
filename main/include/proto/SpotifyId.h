#pragma once

#include <vector>
namespace cspot {
enum class SpotifyIdType { Track, Episode, Playlist };
struct SpotifyId {
  // GID constructor
  SpotifyId(SpotifyIdType type, const std::vector<uint8_t>& gid);

  // Base62 GID constructor
  SpotifyId(SpotifyIdType type, const std::string& base62Gid);

  // URI constructor
  SpotifyId(const std::string& uri);

  std::string trackHex() const;

  SpotifyIdType type;
  std::array<uint8_t, 16> gid;  // GID is always 16 bytes
  std::string base62Gid;        // Base62 GID representation
  std::string uri;              // Full URI representation
};
}  // namespace cspot