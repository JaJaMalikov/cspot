#include "api/SpClient.h"

#include <fmt/format.h>
#include <iostream>
#include <memory>
#include <tao/json.hpp>
#include "NanoPBExtensions.h"
#include "Utils.h"
#include "bell/Logger.h"
#include "bell/http/Client.h"
#include "bell/http/Common.h"

#include "connect.pb.h"
#include "mbedtls/base64.h"

using namespace cspot;

SpClient::SpClient(std::shared_ptr<SessionContext> sessionContext)
    : sessionContext(std::move(sessionContext)) {}

bell::Result<> SpClient::putConnectStateInactive(int retryCount) {
  // PutStateRequest stateRequest = PutStateRequest_init_zero;
  // return putConnectState(stateRequest, retryCount);

  return {};
}

bell::Result<> SpClient::putConnectState(
    cspot_proto::PutStateRequest& stateRequest, int retryCount) {
  bool encodeRes = nanopb_helper::encodeToVector(stateRequest, requestBuffer);
  if (!encodeRes) {
    BELL_LOG(error, LOG_TAG, "Error while encoding message");
    return std::errc::bad_message;
  }

  std::vector<uint8_t> freshBuffer;
  encodeRes = nanopb_helper::encodeToVector(stateRequest, freshBuffer);

  logDataBase64(reinterpret_cast<uint8_t*>(freshBuffer.data()),
                freshBuffer.size());

  auto addrRes = sessionContext->credentialsResolver->getApAddress(
      CredentialsResolver::AddressType::SpClient);

  if (!addrRes) {
    return addrRes.getError();
  }

  std::string spClientAddress = addrRes.takeValue();

  auto keyRes = sessionContext->credentialsResolver->getAccessKey();
  if (!keyRes) {
    return keyRes.getError();
  }
  auto accessToken = keyRes.takeValue();

  // size_t encodedLength = encodeRes;

  auto response = bell::http::requestWithBodyPtr(
      bell::HTTPMethod::PUT,
      fmt::format("https://{}/connect-state/v1/devices/{}", spClientAddress,
                  sessionContext->loginBlob->getDeviceId()),
      {
          {
              "Content-Type",
              "application/x-protobuf",
          },
          {"X-Spotify-Connection-Id", sessionContext->sessionId},
          {"Authorization", fmt::format("Bearer {}", accessToken)},
      },
      reinterpret_cast<const std::byte*>(requestBuffer.data()),
      requestBuffer.size());

  if (!response) {
    BELL_LOG(error, LOG_TAG, "Error while sending request: {}",
             response.errorMessage());
    return response.getError();
  }

  auto httpResponse = response.takeValue();
  if (httpResponse.getStatusCode().unwrap() != 200) {
    BELL_LOG(error, LOG_TAG, "Error while sending request: {}",
             httpResponse.getStatusCode().unwrap());
    std::cout << "Response body: "
              << httpResponse.getBodyStringView().getValue() << std::endl;
    return std::errc::bad_message;
  }

  logDataBase64(
      reinterpret_cast<const uint8_t*>(httpResponse.getBodyBytesPtr().unwrap()),
      httpResponse.getBodyBytesLength().unwrap());
  return {};
}

bell::Result<bell::HTTPReader> SpClient::contextResolve(
    const std::string& contextUri) {
  auto addrRes = sessionContext->credentialsResolver->getApAddress(
      CredentialsResolver::AddressType::SpClient);

  if (!addrRes) {
    return addrRes.getError();
  }

  std::string spClientAddress = addrRes.takeValue();

  auto keyRes = sessionContext->credentialsResolver->getAccessKey();
  if (!keyRes) {
    return keyRes.getError();
  }
  auto accessToken = keyRes.takeValue();

  auto clientTokenRes = sessionContext->credentialsResolver->getClientToken();
  if (!clientTokenRes) {
    return clientTokenRes.getError();
  }
  auto clientToken = clientTokenRes.takeValue();
  auto response = bell::http::request(
      bell::HTTPMethod::GET,
      fmt::format("https://{}/context-resolve/v1/{}", spClientAddress,
                  contextUri),
      {
          {"Client-Token", clientToken},
          {"Authorization", fmt::format("Bearer {}", accessToken)},
      });

  if (!response) {
    BELL_LOG(error, LOG_TAG, "Error while sending request: {}",
             response.errorMessage());
    return response.getError();
  }

  return response;
}

bell::Result<bell::HTTPReader> SpClient::doRequest(
    bell::HTTPMethod method, const std::string& requestUrl) {
  std::cout << requestUrl << std::endl;

  auto addrRes = sessionContext->credentialsResolver->getApAddress(
      CredentialsResolver::AddressType::SpClient);

  if (!addrRes) {
    return addrRes.getError();
  }

  std::string spClientAddress = addrRes.takeValue();

  auto keyRes = sessionContext->credentialsResolver->getAccessKey();
  if (!keyRes) {
    return keyRes.getError();
  }
  auto accessToken = keyRes.takeValue();

  auto clientTokenRes = sessionContext->credentialsResolver->getClientToken();
  if (!clientTokenRes) {
    return clientTokenRes.getError();
  }
  auto clientToken = clientTokenRes.takeValue();

  auto response = bell::http::request(
      method, fmt::format("https://{}/{}", spClientAddress, requestUrl),
      {
          {"Client-Token", clientToken},
          {"Authorization", fmt::format("Bearer {}", accessToken)},
      });

  if (!response) {
    BELL_LOG(error, LOG_TAG, "Error while sending request: {}",
             response.errorMessage());
    return response.getError();
  }

  return response;
}

// bell::Result<tao::json::value> SpClient::radioApollo(
//     const std::string& scope, const std::string& contextUri, bool autoplay,
//     int pageSize) {
//   return hmRequest(
//       fmt::format("hm://radio-apollo/v3/{}/{}/?autoplay={}&count={}", scope,
//                   contextUri, autoplay, pageSize));
// }

bell::Result<cspot_proto::Track> SpClient::trackMetadata(
    const SpotifyId& trackId) {
  if (trackId.type != SpotifyIdType::Track) {
    BELL_LOG(error, LOG_TAG, "Invalid track ID type: expected Track, got {}",
             static_cast<int>(trackId.type));
    return std::errc::invalid_argument;
  }

  auto res = doRequest(bell::http::Method::GET,
                       fmt::format("metadata/4/track/{}", trackId.hexGid()));
  if (!res) {
    return res.getError();
  }

  auto reader = res.takeValue();
  if (reader.getStatusCode().unwrap() != 200) {
    BELL_LOG(error, LOG_TAG, "Error while fetching track metadata: {}",
             reader.getStatusCode().unwrap());
    return std::errc::bad_message;
  }

  auto resultBytes = reader.getBodyBytes();

  cspot_proto::Track trackProto;

  bool decodeRes = nanopb_helper::decodeFromBuffer(
      trackProto,
      reinterpret_cast<const uint8_t*>(reader.getBodyBytesPtr().unwrap()),
      reader.getBodyBytesLength().unwrap());

  if (!decodeRes) {
    BELL_LOG(error, LOG_TAG, "Error while decoding track metadata");
    return std::errc::bad_message;
  }

  return trackProto;
}