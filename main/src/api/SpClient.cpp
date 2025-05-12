#include "api/SpClient.h"

#include <memory>
#include "NanoPBExtensions.h"
#include "bell/Logger.h"
#include "bell/http/Client.h"
#include "bell/http/Common.h"
#include "connect.pb.h"

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

  return {};
}