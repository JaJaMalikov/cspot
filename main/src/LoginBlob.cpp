#include <LoginBlob.h>

#include <string_view>

#include <bell/Logger.h>
#include <bell/io/BinaryStream.h>
#include <bell/io/MemoryStream.h>
#include <mbedtls/aes.h>
#include <mbedtls/base64.h>
#include <mbedtls/pkcs5.h>
#include <cJSON.h>
#include "bell/net/URIParser.h"

using namespace cspot;

namespace {
// Constants
const std::string deviceIdPrefix = "142137fd329622137a149016";
const std::string protocolVersion = "2.7.1";
const std::string swVersion = "2.0.0";
const std::string brandName = "cspot";
const std::string deviceType = "SPEAKER";
}  // namespace

LoginBlob::LoginBlob(const std::string& deviceName) : deviceName(deviceName) {
  deviceId = fmt::format("{}{:016x}", deviceIdPrefix,
                         std::hash<std::string>{}(deviceName));

  // Cache it, so we don't have to recalculate it
  dhPublicKey = dhPair.getPublicKeyBase64();
}

std::string LoginBlob::getUsername() {
  std::scoped_lock lock(accessMutex);
  return username;
}

std::string LoginBlob::getDeviceName() {
  std::scoped_lock lock(accessMutex);
  return deviceName;
}

std::string LoginBlob::getDeviceId() {
  std::scoped_lock lock(accessMutex);
  return deviceId;
}

bool LoginBlob::isAuthenticated() {
  std::scoped_lock lock(accessMutex);
  return !authBlob.empty();
}

std::string LoginBlob::buildZeroconfJSONResponse() {
  std::scoped_lock lock(accessMutex);

  cJSON* root = cJSON_CreateObject();
  cJSON_AddNumberToObject(root, "status", 101);
  cJSON_AddStringToObject(root, "statusString", "OK");
  cJSON_AddStringToObject(root, "version", protocolVersion.c_str());
  cJSON_AddNumberToObject(root, "spotifyError", 0);
  cJSON_AddStringToObject(root, "libraryVersion", swVersion.c_str());
  cJSON_AddStringToObject(root, "accountReq", "PREMIUM");
  cJSON_AddStringToObject(root, "brandDisplayName", brandName.c_str());
  cJSON_AddStringToObject(root, "modelDisplayName", brandName.c_str());
  cJSON_AddStringToObject(root, "voiceSupport", "NO");
  cJSON_AddNumberToObject(root, "productID", 0);
  cJSON_AddStringToObject(root, "tokenType", "default");
  cJSON_AddStringToObject(root, "groupStatus", "NONE");
  cJSON_AddStringToObject(root, "resolverVersion", "0");
  cJSON_AddStringToObject(root, "scope",
                          "streaming,client-authorization-universal");
  cJSON_AddStringToObject(root, "deviceType", deviceType.c_str());
  cJSON_AddStringToObject(root, "availability", "");

  cJSON_AddStringToObject(root, "deviceID", deviceId.c_str());
  cJSON_AddStringToObject(root, "remoteName", deviceName.c_str());
  cJSON_AddStringToObject(root, "publicKey", dhPublicKey.c_str());
  cJSON_AddStringToObject(root, "activeUser", username.c_str());

  char* str = cJSON_PrintUnformatted(root);
  std::string out(str);
  free(str);
  cJSON_Delete(root);
  return out;
}

bell::Result<> LoginBlob::authenticateZeroconfString(
    std::string_view queryString) {
  std::scoped_lock lock(accessMutex);

  // Parse the query string
  std::unordered_map<std::string, std::string> queryParams;
  size_t pos = 0;
  while (pos < queryString.size()) {
    size_t nextPos = queryString.find('&', pos);
    if (nextPos == std::string::npos) {
      nextPos = queryString.size();
    }

    size_t eqPos = queryString.find('=', pos);
    if (eqPos == std::string::npos || eqPos > nextPos) {
      return std::errc::invalid_argument;
    }

    // Perform URL decoding
    std::string key =
        bell::net::decodeURLEncoded(queryString.substr(pos, eqPos - pos));
    std::string value = bell::net::decodeURLEncoded(
        queryString.substr(eqPos + 1, nextPos - eqPos - 1));

    queryParams[key] = value;
    pos = nextPos + 1;
  }

  if (!queryParams.contains("blob") || queryParams["blob"].empty()) {
    BELL_LOG(error, LOG_TAG, "Blob not found in query string [{}]",
             queryString);
    return std::errc::bad_message;
  }

  if (!queryParams.contains("deviceId") || queryParams["deviceId"].empty()) {
    BELL_LOG(error, LOG_TAG, "Device ID not found in query string [{}]",
             queryString);
    return std::errc::bad_message;
  }

  if (!queryParams.contains("clientKey") || queryParams["clientKey"].empty()) {
    BELL_LOG(error, LOG_TAG, "Client key not found in query string [{}]",
             queryString);
    BELL_LOG(error, LOG_TAG, "Client key not found in query string [{}]",
             queryString);
    return std::errc::bad_message;
  }
  // Holds base64 decoded blob and client key
  std::vector<uint8_t> decodedBlob;
  std::vector<uint8_t> decodedClientKey;

  auto res = base64Decode(queryParams["blob"], decodedBlob);
  if (!res) {
    BELL_LOG(error, LOG_TAG, "Failed to base64 decode blob");
    return res.getError();
  }

  res = base64Decode(queryParams["clientKey"], decodedClientKey);
  if (!res) {
    BELL_LOG(error, LOG_TAG, "Failed to base64 decode client key");
    return res.getError();
  }

  username = queryParams["userName"];
  auto encryptedAuthBlobRes = decodeZeroconfBlob(decodedBlob, decodedClientKey);
  if (!encryptedAuthBlobRes) {
    BELL_LOG(error, LOG_TAG, "Failed to decode zeroconf blob");
    return encryptedAuthBlobRes.getError();
  }

  encryptedAuthBlob = encryptedAuthBlobRes.takeValue();

  // Decode the auth blob
  return decodeEncryptedAuthBlob(username, encryptedAuthBlob);
}

std::vector<uint8_t> LoginBlob::getStoredAuthBlob() {
  std::scoped_lock lock(accessMutex);
  return authBlob;
}

bell::Result<std::vector<uint8_t>> LoginBlob::decodeZeroconfBlob(
    const std::vector<uint8_t>& blob, const std::vector<uint8_t>& clientKey) {
  std::scoped_lock lock(accessMutex);
  // 0:16 - iv; 17:-20 - blob; -20:0 - checksum
  auto iv = std::span(blob.data(), 16);
  auto encryptedData = std::span(blob.data() + 16, blob.size() - 20 - 16);
  auto checksum = std::span(blob.data() + blob.size() - 20, 20);

  std::array<uint8_t, 96> sharedKey{};
  // Calculate the shared key
  dhPair.computeSharedKey(clientKey.data(), clientKey.size(), sharedKey.data());

  // Base key = sha1(sharedKey) 0:16
  std::vector<uint8_t> baseKey(20);
  sha1Context.getDigest(reinterpret_cast<const uint8_t*>(sharedKey.data()),
                        sharedKey.size(), baseKey.data());
  // Only use the first 16 bytes
  baseKey.resize(16);

  std::string checksumMessage = "checksum";
  std::vector<uint8_t> checksumKey(20);
  // Calculate the checksum hmac
  sha1Context.getHmac(baseKey.data(), baseKey.size(),
                      reinterpret_cast<const uint8_t*>(checksumMessage.data()),
                      checksumMessage.size(), checksumKey.data());

  std::string encryptionMessage = "encryption";
  std::vector<uint8_t> encryptionKey(20);
  // Calculate the encryption hmac
  sha1Context.getHmac(
      baseKey.data(), baseKey.size(),
      reinterpret_cast<const uint8_t*>(encryptionMessage.data()),
      encryptionMessage.size(), encryptionKey.data());

  std::vector<uint8_t> mac(20);
  // Calculate the mac
  sha1Context.getHmac(checksumKey.data(), checksumKey.size(),
                      reinterpret_cast<const uint8_t*>(encryptedData.data()),
                      encryptedData.size(), mac.data());

  if (!std::equal(mac.begin(), mac.end(), checksum.begin())) {
    BELL_LOG(error, LOG_TAG, "Encryption and checksum keys do not match");
    return std::errc::bad_message;
  }

  // Initialize the aes context
  mbedtls_aes_context aesCtx;
  mbedtls_aes_init(&aesCtx);

  // needed for AES internal cache
  size_t off = 0;
  std::array<uint8_t, 16> streamBlock;

  // set IV
  if (mbedtls_aes_setkey_enc(&aesCtx, encryptionKey.data(), 128) != 0) {
    BELL_LOG(error, LOG_TAG, "Failed to set AES key");
    mbedtls_aes_free(&aesCtx);
    return std::errc::bad_message;
  }

  std::array<uint8_t, 16> nounceCounter;
  std::copy(iv.begin(), iv.end(), nounceCounter.begin());

  std::vector<uint8_t> authBlob;
  authBlob.resize(encryptedData.size());

  // Perform decrypt
  if (mbedtls_aes_crypt_ctr(
          &aesCtx, encryptedData.size(), &off, nounceCounter.data(),
          streamBlock.data(),
          reinterpret_cast<const uint8_t*>(encryptedData.data()),
          authBlob.data()) != 0) {
    BELL_LOG(error, LOG_TAG, "Failed to aes decrypt auth data");
    mbedtls_aes_free(&aesCtx);
    return std::errc::bad_message;
  }

  mbedtls_aes_free(&aesCtx);

  BELL_LOG(info, LOG_TAG, "Decoded auth data: {}",
           std::string(authBlob.begin(), authBlob.end()));

  return authBlob;
}

bell::Result<> LoginBlob::decodeEncryptedAuthBlob(
    const std::string& username,
    const std::vector<uint8_t>& encryptedAuthBlob) {
  std::scoped_lock lock(accessMutex);

  std::vector<uint8_t> base64DecodedAuthData;
  auto decodeRes = base64Decode(
      std::string_view(reinterpret_cast<const char*>(encryptedAuthBlob.data()),
                       encryptedAuthBlob.size()),
      base64DecodedAuthData);

  if (!decodeRes) {
    BELL_LOG(error, LOG_TAG, "Failed to base64 decode auth blob");
    return decodeRes.getError();
  }

  // Calculate the pbkdf2 hmac
  std::array<uint8_t, 20> deviceIdDigest;
  std::array<uint8_t, 20> pbkdf2Hmac;
  sha1Context.reset();
  sha1Context.getDigest(reinterpret_cast<const uint8_t*>(deviceId.data()),
                        deviceId.size(), deviceIdDigest.data());

  int res = mbedtls_pkcs5_pbkdf2_hmac_ext(
      MBEDTLS_MD_SHA1, deviceIdDigest.data(), deviceIdDigest.size(),
      reinterpret_cast<const uint8_t*>(username.data()), username.size(), 256,
      20, pbkdf2Hmac.data());
  if (res != 0) {
    BELL_LOG(error, LOG_TAG, "Failed to calculate pbkdf2, mbedtls error: {}",
             res);
    return std::errc::bad_message;
  }

  // Calculate the sha1 hmac
  std::array<uint8_t, 20 + 4> baseKeyHashed{};
  // First 4 bytes are the length of the base key, which is 20 bytes
  // The rest is the sha1 hash of the base key
  baseKeyHashed[23] = 0x14;

  sha1Context.reset();
  sha1Context.getDigest(pbkdf2Hmac.data(), pbkdf2Hmac.size(),
                        baseKeyHashed.data());

  // Initialize the aes context
  mbedtls_aes_context aesCtx;
  mbedtls_aes_init(&aesCtx);

  // Set the key
  if (mbedtls_aes_setkey_dec(&aesCtx, baseKeyHashed.data(), 192) != 0) {
    BELL_LOG(error, LOG_TAG, "Failed to set AES key");
    mbedtls_aes_free(&aesCtx);
    return std::errc::bad_message;
  }

  for (uint64_t x = 0; x < base64DecodedAuthData.size() / 16; x++) {
    // Temporary buffer for decrypted 16 bytes
    std::array<uint8_t, 16> decryptedChunk{};

    if (mbedtls_aes_crypt_ecb(&aesCtx, MBEDTLS_AES_DECRYPT,
                              &base64DecodedAuthData[x * 16],
                              decryptedChunk.data()) != 0) {
      BELL_LOG(error, LOG_TAG, "Failed to aes decrypt auth data");
      mbedtls_aes_free(&aesCtx);
      return std::errc::bad_message;
    }

    std::copy(decryptedChunk.begin(), decryptedChunk.end(),
              &base64DecodedAuthData[x * 16]);
  }

  mbedtls_aes_free(&aesCtx);

  auto l = base64DecodedAuthData.size();
  for (size_t i = 0; i < l - 16; i++) {
    base64DecodedAuthData[l - i - 1] ^= base64DecodedAuthData[l - i - 17];
  }

  bell::io::IMemoryStream blobMemoryStream(
      reinterpret_cast<const std::byte*>(base64DecodedAuthData.data()),
      base64DecodedAuthData.size());

  // Construct the binary stream reader
  bell::io::BinaryStream blobBinaryStream(&blobMemoryStream);

  blobBinaryStream.skip(1);  // Skip the first byte
  // Skip the next uvarint of bytes
  blobBinaryStream.skip(readUvarint(blobBinaryStream) + 1);

  uint32_t authType = readUvarint(blobBinaryStream);
  blobBinaryStream.skip(1);  // Skip the next byte

  uint32_t authDataSize = readUvarint(blobBinaryStream);
  authBlob.resize(authDataSize);
  blobMemoryStream.read(reinterpret_cast<char*>(authBlob.data()), authDataSize);

  BELL_LOG(info, LOG_TAG, "Auth type: {}", authType);
  BELL_LOG(info, LOG_TAG, "Auth blob len: {}", authBlob.size());

  return {};
}

uint32_t LoginBlob::readUvarint(bell::io::BinaryStream& stream) {
  uint8_t lo;
  stream >> lo;
  if ((int)(lo & 0x80) == 0) {
    return lo;
  }

  uint8_t hi;
  stream >> hi;

  return (uint32_t)((lo & 0x7f) | (hi << 7));
}

bell::Result<> LoginBlob::base64Decode(std::string_view encoded,
                                       std::vector<uint8_t>& targetBuffer) {
  size_t outputSize = 0;
  int res = mbedtls_base64_decode(
      nullptr, 0, &outputSize, reinterpret_cast<const uint8_t*>(encoded.data()),
      encoded.size());
  if (outputSize == 0) {
    return std::errc::bad_message;
  }

  targetBuffer.resize(outputSize);
  res = mbedtls_base64_decode(reinterpret_cast<uint8_t*>(targetBuffer.data()),
                              targetBuffer.size(), &outputSize,
                              reinterpret_cast<const uint8_t*>(encoded.data()),
                              encoded.size());
  if (res != 0) {
    return std::errc::bad_message;
  }

  return {};
}
