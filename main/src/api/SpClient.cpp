#include "api/SpClient.h"

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

bell::Result<> SpClient::putConnectState(const PutStateRequest& stateRequest,
                                         int retryCount) {
  auto encodeRes =
      pbCalculateEncodedSize(PutStateRequest_fields, &stateRequest);
  if (!encodeRes) {
    BELL_LOG(error, LOG_TAG, "Error while calculating encoded size: {}",
             encodeRes.errorMessage());
    return encodeRes.getError();
  }

  if (requestBuffer.size() < encodeRes.getValue()) {
    requestBuffer.resize(encodeRes.getValue());
  }

  encodeRes = pbEncodeMessage(reinterpret_cast<uint8_t*>(requestBuffer.data()),
                              requestBuffer.size(), PutStateRequest_fields,
                              &stateRequest);
  if (!encodeRes) {
    BELL_LOG(error, LOG_TAG, "Error while encoding message: {}",
             encodeRes.errorMessage());
    return encodeRes.getError();
  }

  logDataBase64(reinterpret_cast<uint8_t*>(requestBuffer.data()), encodeRes.getValue());

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

  size_t encodedLength = encodeRes.getValue();

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
      requestBuffer.data(), encodedLength);

  if (!response) {
    BELL_LOG(error, LOG_TAG, "Error while sending request: {}",
             response.errorMessage());
    return response.getError();
  }

  auto httpResponse = response.takeValue();
  if (httpResponse.getStatusCode().unwrap() != 200) {
    BELL_LOG(error, LOG_TAG, "Error while sending request: {}",
             httpResponse.getStatusCode().unwrap());
    return std::errc::bad_message;
  }

  // logDataBase64(
  //     reinterpret_cast<const uint8_t*>(httpResponse.getBodyBytesPtr().unwrap()),
  //     httpResponse.getBodyBytesLength().unwrap());
  return {};
}

bell::Result<tao::json::value> SpClient::contextResolve(
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
          {"X-Spotify-Connection-Id", sessionContext->sessionId},
          {"Authorization", fmt::format("Bearer {}", accessToken)},
      });

  if (!response) {
    BELL_LOG(error, LOG_TAG, "Error while sending request: {}",
             response.errorMessage());
    return response.getError();
  }

  auto httpResponse = response.takeValue();

  if (httpResponse.getStatusCode().unwrap() != 200) {
    BELL_LOG(error, LOG_TAG, "Error while sending request: {}",
             httpResponse.getStatusCode().unwrap());
    return std::errc::bad_message;
  }

  tao::json::value jsonResponse;

  try {
    jsonResponse =
        tao::json::from_string(httpResponse.getBodyStringView().getValue());
  } catch (const std::exception& e) {
    BELL_LOG(error, LOG_TAG, "Error parsing JSON response: {}", e.what());
    return std::errc::bad_message;
  }

  return jsonResponse;
}

bell::Result<tao::json::value> SpClient::hmRequest(const std::string& request) {
  // Request needs to be in hermes format
  if (!request.starts_with("hm://")) {
    return std::errc::invalid_argument;
  }

  std::string requestUrl = request.substr(5);

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
      fmt::format("https://{}/{}", spClientAddress, requestUrl),
      {
          {"Client-Token", clientToken},
          {"Authorization", fmt::format("Bearer {}", accessToken)},
      });

  if (!response) {
    BELL_LOG(error, LOG_TAG, "Error while sending request: {}",
             response.errorMessage());
    return response.getError();
  }

  auto httpResponse = response.takeValue();

  if (httpResponse.getStatusCode().unwrap() != 200) {
    BELL_LOG(error, LOG_TAG, "Error while sending request: {}",
             httpResponse.getStatusCode().unwrap());
    return std::errc::bad_message;
  }

  tao::json::value jsonResponse;

  try {
    jsonResponse =
        tao::json::from_string(httpResponse.getBodyStringView().getValue());
  } catch (const std::exception& e) {
    BELL_LOG(error, LOG_TAG, "Error parsing JSON response: {}", e.what());
    return std::errc::bad_message;
  }

  return jsonResponse;
}

bell::Result<tao::json::value> SpClient::radioApollo(
    const std::string& scope, const std::string& contextUri, bool autoplay,
    int pageSize) {
  return hmRequest(
      fmt::format("hm://radio-apollo/v3/{}/{}/?autoplay={}&count={}", scope,
                  contextUri, autoplay, pageSize));
}
