#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

// Library includes
#include "bell/Result.h"
#include "bell/net/TCPSocket.h"

// Own includes
#include "crypto/DiffieHellman.h"
#include "crypto/Shannon.h"

// Protobufs
#include "authentication.pb.h"
#include "keyexchange.pb.h"

namespace cspot {
class ApConnection {
 public:
  ApConnection(const std::string& apAddress);
  ~ApConnection();

  /**
   * @brief Sends a shannon encrypted packet to the AP
   *
   * @param cmd Packet command
   * @param packetData Buffer containing the packet data
   * @param packetSize Size of the packet data
   */
  bell::Result<> sendPacket(uint8_t cmd, const uint8_t* packetData,
                            uint16_t packetSize);

  /**
   * @brief Receives a shannon encrypted packet from the AP
   *
   * @param cmd Reference to the packet command byte
   * @param packetSize Reference to the packet size
   * @return uint8_t* Buffer containing the received packet data
   */
  bell::Result<uint8_t*> receivePacket(uint8_t& cmd, uint16_t& packetSize);

  /**
   * @brief Authenticates the connection with the AP
   *
   * @param authBlobBuffer Buffer containing the authentication blob
   * @param authBlobSize Size of the authentication blob
   * @param authType Authentication type
   * @param username Username
   * @param deviceId Device ID
   */
  bell::Result<> authenticate(const uint8_t* authBlobBuffer,
                              size_t authBlobSize, uint32_t authType,
                              const std::string& username,
                              const std::string& deviceId);

 private:
  const char* LOG_TAG = "ApConnection";
  const static uint32_t operationTimeout = 3000;

  std::shared_ptr<bell::net::TCPSocket> apSock;
  DH dhPair;

  // Nonce counters for Shannon ciphers
  uint32_t shanRecvNonce = 0;
  uint32_t shanSendNonce = 0;

  Shannon recvCipher{};
  Shannon sendCipher{};

  // Set to true after handshake is completed
  bool shanonAuthenticated = false;

  // Protobufs
  ClientHello pbClientHello{};
  APResponseMessage pbApResponse{};
  ClientResponsePlaintext pbClientResponse{};
  ClientResponseEncrypted pbClientResponseEncrypted{};

  std::vector<uint8_t> connectionBuffer;

  bell::Result<> performHandshake();
  bell::Result<> solveHelloChallenge(const uint8_t* helloPacket,
                                     size_t helloPacketSize);

  bell::Result<> sendPlainPacket(const uint8_t* data, size_t len,
                                 std::optional<uint16_t> cmd);

  bell::Result<size_t> receivePlainPacket();

  static void updateShannonNonce(uint32_t& nonce, Shannon& cipher);
};
}  // namespace cspot
