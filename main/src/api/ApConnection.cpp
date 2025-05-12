#include "api/ApConnection.h"

#include "NanoPBExtensions.h"
#include "authentication.pb.h"
#include "bell/Logger.h"
#include "bell/utils/DigestCrypto.h"
#include "keyexchange.pb.h"
#include "mbedtls/md.h"

using namespace cspot;

namespace {
const long long SPOTIFY_VERSION = 0x10800000000;
const size_t shannonMacSize = 4;
}  // namespace

ApConnection::ApConnection(const std::string& apAddress) {
  apSock = std::make_unique<bell::net::TCPSocket>();

  // Split the address into hostname and port
  auto colonPos = apAddress.find(':');
  if (colonPos == std::string::npos) {
    throw std::runtime_error("AP address missing port");
  }

  auto hostname = apAddress.substr(0, colonPos);
  auto portStr = apAddress.substr(colonPos + 1);

  // Connect to the AP
  apSock->connect(hostname, std::stoi(portStr));

  // Send the APHello message
  performHandshake();
}

ApConnection::~ApConnection() {
  // Close the socket
  apSock->close();
}

bell::Result<> ApConnection::performHandshake() {
  pbClientHello = ClientHello_init_zero;

  // Prepare the ClientHello message
  pbClientHello.login_crypto_hello.diffie_hellman.server_keys_known = 1;
  pbClientHello.build_info.product = Product_PRODUCT_CLIENT;
  pbClientHello.build_info.platform = Platform2_PLATFORM_LINUX_X86;
  pbClientHello.build_info.version = SPOTIFY_VERSION;
  pbClientHello.feature_set.autoupdate2 = true;
  pbClientHello.cryptosuites_supported[0] = Cryptosuite_CRYPTO_SUITE_SHANNON;
  pbClientHello.padding[0] = 0x1E;
  pbClientHello.has_feature_set = true;
  pbClientHello.login_crypto_hello.has_diffie_hellman = true;
  pbClientHello.has_padding = true;
  pbClientHello.has_feature_set = true;

  // Copy the public key into the ClientHello message
  auto publicKey = dhPair.getPublicKey();
  std::copy(publicKey.begin(), publicKey.end(),
            pbClientHello.login_crypto_hello.diffie_hellman.gc);

  // Fill nonce with random data
  for (unsigned char& nonceByte : pbClientHello.client_nonce) {
    nonceByte = rand() % 256;
  }

  std::array<uint8_t, ClientHello_size> helloPacket;

  // Encode the ClientHello message
  auto encodedSize = pbEncodeMessage(helloPacket.data(), helloPacket.size(),
                                     ClientHello_fields, &pbClientHello);

  if (!encodedSize) {
    return encodedSize.getError();
  }

  // Send the packet
  auto res = sendPlainPacket(helloPacket.data(), encodedSize.getValue(), 0x04);
  if (!res) {
    return res.getError();
  }

  BELL_LOG(info, LOG_TAG, "Sent ClientHello {}", encodedSize.getValue());

  return solveHelloChallenge(helloPacket.data() + 2, encodedSize.getValue());
}

bell::Result<> ApConnection::solveHelloChallenge(const uint8_t* helloPacket,
                                                 size_t helloPacketSize) {
  // Receive the AP challenge
  auto packetResult = receivePlainPacket();
  if (!packetResult) {
    return packetResult.getError();
  }

  size_t apResponseLen = packetResult.getValue();

  auto res = pbDecodeMessage(connectionBuffer.data(), apResponseLen,
                             APResponseMessage_fields, &pbApResponse);
  if (!res) {
    return res.getError();
  }

  std::array<uint8_t, 96> sharedKey{};

  // Compute the diffie hellman shared key based on the response
  dhPair.computeSharedKey(
      pbApResponse.challenge.login_crypto_challenge.diffie_hellman.gs, 96,
      sharedKey.data());

  // Init client packet + Init server packets are required for the hmac challenge
  std::vector<uint8_t> challengeData(apResponseLen + helloPacketSize + 1);
  std::copy(helloPacket, helloPacket + helloPacketSize, challengeData.begin());
  std::copy(connectionBuffer.data(), connectionBuffer.data() + apResponseLen,
            challengeData.begin() + helloPacketSize);

  bell::utils::DigestCrypto sha1Context{MBEDTLS_MD_SHA1, true};

  std::array<uint8_t, 100> challengeResult{};
  // Solve the hmac challenge
  for (size_t x = 0; x < 5; x++) {
    challengeData[challengeData.size() - 1] = x + 1;

    // Calculate the hmac
    sha1Context.getHmac(sharedKey.data(), sharedKey.size(),
                        challengeData.data(), challengeData.size(),
                        &challengeResult[x * 20]);
  }

  std::array<uint8_t, 20> responseHmac{};
  sha1Context.getHmac(challengeResult.data(), 20, challengeData.data(),
                      challengeData.size() - 1, responseHmac.data());

  pbClientResponse.login_crypto_response.has_diffie_hellman = true;
  std::copy(responseHmac.begin(), responseHmac.end(),
            pbClientResponse.login_crypto_response.diffie_hellman.hmac);

  std::array<uint8_t, 32> shanSendKey{};
  std::array<uint8_t, 32> shanRecvKey{};

  // Shan send key = [0x14:0x34]
  std::copy(challengeResult.begin() + 0x14, challengeResult.begin() + 0x34,
            shanSendKey.begin());

  // Shan recv key = [0x34:0x54]
  std::copy(challengeResult.begin() + 0x14, challengeResult.begin() + 0x34,
            shanRecvKey.begin());

  // Reuse the challengeData buffer for the response
  challengeData.resize(ClientResponsePlaintext_size);
  auto encodeRes =
      pbEncodeMessage(challengeData.data(), challengeData.size(),
                      ClientResponsePlaintext_fields, &pbClientResponse);
  if (!encodeRes) {
    return encodeRes.getError();
  }

  size_t responseSize = encodeRes.getValue();

  // Send the response
  res = sendPlainPacket(challengeData.data(), responseSize, std::nullopt);
  if (!res) {
    return res.getError();
  }

  // At this point, the handshake is complete, and the connection is no longer plaintext
  BELL_LOG(info, LOG_TAG, "Handshake complete");

  // Initialize the Shannon ciphers
  recvCipher.key(shanRecvKey.data(), shanRecvKey.size());
  sendCipher.key(shanSendKey.data(), shanSendKey.size());

  shanonAuthenticated = true;

  // Reset the nonce for the Shannon ciphers
  shanRecvNonce = 0;
  shanSendNonce = 0;
  updateShannonNonce(shanRecvNonce, recvCipher);
  updateShannonNonce(shanSendNonce, sendCipher);

  return {};
}

bell::Result<> ApConnection::sendPlainPacket(const uint8_t* data, size_t len,
                                             std::optional<uint16_t> cmd) {
  // Send the packet size
  uint32_t packetSize = htonl(len + 4 + (cmd.has_value() ? 2 : 0));
  if (cmd.has_value()) {
    uint32_t prefix = htons(cmd.value());
    auto res = apSock->write(reinterpret_cast<const uint8_t*>(&prefix),
                             sizeof(uint16_t));
    if (!res) {
      return res.getError();
    }
  }
  auto res = apSock->write(reinterpret_cast<const uint8_t*>(&packetSize),
                           sizeof(packetSize));
  if (!res) {
    return res.getError();
  }

  // Send the packet data
  res = apSock->write(data, len);
  if (!res) {
    return res.getError();
  }

  return {};
}

bell::Result<> ApConnection::authenticate(const uint8_t* authBlobBuffer,
                                          size_t authBlobSize,
                                          uint32_t authType,
                                          const std::string& username,
                                          const std::string& deviceId) {
  // Prepare the authentication request
  std::vector<uint8_t> authBlob(authBlobBuffer, authBlobBuffer + authBlobSize);
  std::string authUsername = username;

  pbClientResponseEncrypted.login_credentials.auth_data.funcs.encode =
      cspot::pbEncodeUint8Vector;
  pbClientResponseEncrypted.login_credentials.auth_data.arg = &authBlob;

  pbClientResponseEncrypted.login_credentials.typ =
      static_cast<AuthenticationType>(authType);
  pbClientResponseEncrypted.login_credentials.username.funcs.encode =
      pbEncodeString;
  pbClientResponseEncrypted.login_credentials.username.arg = &authUsername;

  pbClientResponseEncrypted.system_info.cpu_family = CpuFamily_CPU_UNKNOWN;
  pbClientResponseEncrypted.system_info.os = Os_OS_UNKNOWN;
  std::string infoStr = "cspot-player";
  pbClientResponseEncrypted.system_info.system_information_string.funcs.encode =
      pbEncodeString;
  pbClientResponseEncrypted.system_info.system_information_string.arg =
      &infoStr;

  std::string deviceIdStr = deviceId;
  pbClientResponseEncrypted.system_info.device_id.funcs.encode = pbEncodeString;
  pbClientResponseEncrypted.system_info.device_id.arg = &deviceIdStr;

  std::string versionStr = "cspot-1.1";
  pbClientResponseEncrypted.version_string.funcs.encode = pbEncodeString;
  pbClientResponseEncrypted.version_string.arg = &versionStr;

  // Get encoded size
  //
  return {};
}

bell::Result<size_t> ApConnection::receivePlainPacket() {
  uint32_t packetSize = 0;

  // Not using a BinaryStream here, as we only really need to read a single uint32_t
  auto res =
      apSock->read(reinterpret_cast<uint8_t*>(&packetSize), sizeof(packetSize));

  if (!res) {
    return res.getError();
  }
  packetSize = ntohl(packetSize);

  // TODO: Verify maximum packet size
  if (packetSize > connectionBuffer.size()) {
    connectionBuffer.resize(packetSize);
  }

  // Already read the packet size, so subtract it from the total size
  packetSize -= 4;

  res = apSock->read(connectionBuffer.data(), packetSize);
  if (!res) {
    return res.getError();
  }
  return packetSize;
}

void ApConnection::updateShannonNonce(uint32_t& nonce, Shannon& cipher) {
  std::array<uint8_t, 4> nonceData{};
  uint32_t packedNonce = htonl(nonce);

  std::copy(reinterpret_cast<uint8_t*>(&packedNonce),
            reinterpret_cast<uint8_t*>(&packedNonce) + 4, nonceData.begin());

  cipher.nonce(nonceData.data(), nonceData.size());
}

bell::Result<> ApConnection::sendPacket(uint8_t cmd, const uint8_t* packetData,
                                        uint16_t packetSize) {
  if (!shanonAuthenticated) {
    return std::errc::operation_not_permitted;
  }

  // Packet + size + cmd
  size_t totalSize = packetSize + sizeof(uint16_t) + 1 + shannonMacSize;

  if (connectionBuffer.size() < totalSize) {
    connectionBuffer.resize(totalSize);
  }

  // Set the command byte
  connectionBuffer[0] = cmd;

  // Encode the packet size
  packetSize = htons(packetSize);
  std::copy(reinterpret_cast<uint8_t*>(&packetSize),
            reinterpret_cast<uint8_t*>(&packetSize) + sizeof(uint16_t),
            &connectionBuffer[1]);

  // Copy the packet data
  std::copy(packetData, packetData + packetSize,
            &connectionBuffer[sizeof(uint16_t) + 1]);

  // Encrypt the packet
  sendCipher.encrypt(connectionBuffer.data(), totalSize - shannonMacSize);

  // Generate mac
  sendCipher.finish(&connectionBuffer[totalSize - shannonMacSize],
                    shannonMacSize);

  // Update the nonce
  shanSendNonce += 1;
  updateShannonNonce(shanSendNonce, sendCipher);

  // Send the packet
  auto res = apSock->write(connectionBuffer.data(), totalSize);
  if (!res) {
    return res.getError();
  }

  return {};
}

bell::Result<uint8_t*> ApConnection::receivePacket(uint8_t& cmd,
                                                   uint16_t& packetSize) {
  if (!shanonAuthenticated) {
    return std::errc::operation_not_permitted;
  }

  // Receive 3 bytes, cmd + size
  auto res = apSock->read(connectionBuffer.data(), 3);
  if (!res) {
    return res.getError();
  }

  // Extract the command byte
  cmd = connectionBuffer[0];

  std::copy(&connectionBuffer[1], &connectionBuffer[3],
            reinterpret_cast<uint8_t*>(&packetSize));
  packetSize = ntohs(packetSize);

  if (packetSize + shannonMacSize > connectionBuffer.size()) {
    connectionBuffer.resize(packetSize + shannonMacSize);
  }

  // Read the packet data + 4 byte mac
  res = apSock->read(connectionBuffer.data(), packetSize + shannonMacSize);
  if (!res) {
    return res.getError();
  }

  // Decrypt the packet
  recvCipher.decrypt(connectionBuffer.data(), packetSize);

  // Generate mac
  std::array<uint8_t, shannonMacSize> mac{};
  recvCipher.finish(mac.data(), mac.size());

  // Compare the received mac with the calculated mac
  if (std::memcmp(mac.data(), &connectionBuffer[packetSize], shannonMacSize) !=
      0) {
    throw std::runtime_error("MAC mismatch in the received packet");
  }

  // Update the nonce
  shanRecvNonce += 1;
  updateShannonNonce(shanRecvNonce, recvCipher);

  return connectionBuffer.data();
}
