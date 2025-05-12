#pragma once

#include <mutex>
#include <unordered_map>
#include <vector>

#include <bell/Result.h>
#include <bell/io/BinaryStream.h>
#include <bell/utils/DigestCrypto.h>

#include "crypto/DiffieHellman.h"

namespace cspot {
class LoginBlob {
 public:
  LoginBlob(const std::string& deviceName);

  bell::Result<> authenticateZeroconfQuery(
      const std::unordered_map<std::string, std::string>& queryParams);

  bell::Result<> authenticateZeroconfString(std::string_view queryStr);

  std::string buildZeroconfJSONResponse();
  std::string getUsername();
  std::string getDeviceName();
  std::string getDeviceId();
  bool isAuthenticated();

  std::vector<uint8_t> getStoredAuthBlob();

  bell::Result<> decodeEncryptedAuthBlob(const std::string& username,
                                         const std::vector<uint8_t>& authBlob);

 private:
  const char* LOG_TAG = "LoginBlob";

  // Device name, set on construction
  std::string deviceName;

  // Derieved from device name
  std::string deviceId;

  // Signed-in username, will be set after authentication
  std::string username;

  // Used for SHA1 computations
  bell::utils::DigestCrypto sha1Context{MBEDTLS_MD_SHA1, true};

  // Used for Diffie-Hellman key exchange
  DH dhPair;

  // Base64 encoded public key
  std::string dhPublicKey;

  std::recursive_mutex accessMutex;

  std::vector<uint8_t> encryptedAuthBlob;

  std::vector<uint8_t> authBlob;

  bell::Result<std::vector<uint8_t>> decodeZeroconfBlob(
      const std::vector<uint8_t>& blob, const std::vector<uint8_t>& clientKey);

  static uint32_t readUvarint(bell::io::BinaryStream& stream);

  static bell::Result<> base64Decode(std::string_view encoded,
                                     std::vector<uint8_t>& targetBuffer);
};
}  // namespace cspot
