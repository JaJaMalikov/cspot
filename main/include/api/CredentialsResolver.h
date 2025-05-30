#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "bell/Result.h"

#include "LoginBlob.h"

namespace cspot {
class CredentialsResolver {
 public:
  CredentialsResolver(std::shared_ptr<LoginBlob> loginBlob);

  // Enumeration of the endpoint types
  enum class AddressType {
    AccessPoint,
    Dealer,
    SpClient,
  };

  /**
   * @brief Resolve the address of the access point, dealer, or spClient.
   *
   * @note This function will fetch the addresses from the Spotify API if they are not already cached, or are expired.
   *
   * @param type The type of address to resolve
   * @return std::string The resolved address
   */
  bell::Result<std::string> getApAddress(AddressType type);

  /**
   * @brief Retrieve the Spotify client token, using the "clienttoken.spotify.com" endpoint, caching the token for subsequent calls.
   *
   * @return std::string retrieved token
   */
  bell::Result<std::string> getClientToken();

  /**
  * @brief Fetches a new access key from the Spotify API, caching the key for subsequent calls.
  *
  * @returns std::string access key
  */
  bell::Result<std::string> getAccessKey();

  /**
   * @brief Forces a refresh of the addresses.
   */
  bell::Result<> updateAddresses();

  /**
   * @brief Forces a refresh of the client token.
   */
  bell::Result<> updateClientToken();

  /**
   * @brief Forces a refresh of the access key.
   */
  bell::Result<> updateAccessKey();

  std::string getSessionId() { return this->sessionId; }

  void setSessionId(const std::string& sessionId) {
    this->sessionId = sessionId;
  }

 private:
  const char* LOG_TAG = "CredentialsResolver";

  std::shared_ptr<LoginBlob> loginBlob;

  // Cached
  std::vector<std::string> apAddresses;
  std::vector<std::string> dealerAddresses;
  std::vector<std::string> spClientAddresses;
  std::string clientToken;
  std::string accessKey;
  std::string sessionId;

  // Expiry times
  using sysclock_timepoint = std::chrono::time_point<std::chrono::system_clock>;
  sysclock_timepoint addressesExpiresAt;
  sysclock_timepoint clientTokenExpiresAt;
  sysclock_timepoint accessKeyExpiresAt;

  std::recursive_mutex accessMutex;
};
}  // namespace cspot
