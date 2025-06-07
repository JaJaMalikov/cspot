#include "Session.h"

#include <string>
#include <cJSON.h>
#include "bell/Logger.h"
#include "connect.pb.h"
#include "events/EventLoop.h"

using namespace cspot;

cspot::Session::Session(std::shared_ptr<LoginBlob> loginBlob)
    : loginBlob(std::move(loginBlob)) {
  // Prepare the session context
  sessionContext = std::make_shared<SessionContext>();
  sessionContext->loginBlob = this->loginBlob;
  sessionContext->eventLoop = std::make_shared<cspot::EventLoop>();
  sessionContext->credentialsResolver =
      std::make_shared<CredentialsResolver>(this->loginBlob);

  // Prepare the dealer client
  dealerClient = std::make_shared<DealerClient>(sessionContext);
  spClient = std::make_shared<SpClient>(sessionContext);
  connectStateHandler =
      std::make_shared<ConnectStateHandler>(sessionContext, spClient);

  sessionContext->eventLoop->registerHandler(
      EventLoop::EventType::DEALER_MESSAGE,
      std::bind(&cspot::Session::handleDealerMessage, this,
                std::placeholders::_1));

  sessionContext->eventLoop->registerHandler(
      EventLoop::EventType::DEALER_REQUEST,
      std::bind(&cspot::Session::handleDealerRequest, this,
                std::placeholders::_1));
}

void cspot::Session::handleDealerMessage(EventLoop::Event&& event) {
  auto dealerMessageEvent = std::move(event);
  std::string messageStr = std::get<std::string>(dealerMessageEvent.payload);
  cJSON* messageJson = cJSON_Parse(messageStr.c_str());
  if (!messageJson) {
    BELL_LOG(error, LOG_TAG, "Invalid JSON message");
    return;
  }

  cJSON* uriItem = cJSON_GetObjectItem(messageJson, "uri");
  if (!cJSON_IsString(uriItem)) {
    BELL_LOG(info, LOG_TAG, "Received message without URI");
    cJSON_Delete(messageJson);
    return;
  }
  std::string uri = uriItem->valuestring;

  if (uri.rfind("hm://pusher/v1/connections", 0) == 0) {
    // Extract session ID
    cJSON* headers = cJSON_GetObjectItem(messageJson, "headers");
    cJSON* sessionIdItem =
        cJSON_GetObjectItem(headers, "Spotify-Connection-Id");
    if (!cJSON_IsString(sessionIdItem)) {
      BELL_LOG(info, LOG_TAG, "Received message without session ID");
      cJSON_Delete(messageJson);
      return;
    }

    sessionContext->sessionId = sessionIdItem->valuestring;
    BELL_LOG(info, LOG_TAG, "Session ID: %s", sessionIdItem->valuestring);

    // Announce spotify connect state
    auto res = connectStateHandler->putState(PutStateReason_NEW_CONNECTION);
    if (!res) {
      BELL_LOG(error, LOG_TAG, "Failed to announce connect state: {}",
               res.errorMessage());
      return;
    }
  } else {
    BELL_LOG(info, LOG_TAG, "Received message with URI: %s", uri.c_str());
  }
  cJSON_Delete(messageJson);
}

void cspot::Session::handleDealerRequest(EventLoop::Event&& event) {
  auto dealerRequestEvent = std::move(event);
  std::string messageStr = std::get<std::string>(dealerRequestEvent.payload);
  cJSON* messageJson = cJSON_Parse(messageStr.c_str());
  if (!messageJson) {
    BELL_LOG(error, LOG_TAG, "Invalid JSON request");
    return;
  }

  cJSON* identItem = cJSON_GetObjectItem(messageJson, "message_ident");
  if (!cJSON_IsString(identItem)) {
    BELL_LOG(info, LOG_TAG, "Received message without message_ident");
    cJSON_Delete(messageJson);
    return;
  }
  std::string messageIdent = identItem->valuestring;

  cJSON* keyItem = cJSON_GetObjectItem(messageJson, "key");
  if (!cJSON_IsString(keyItem)) {
    BELL_LOG(info, LOG_TAG, "Received message without request key");
    cJSON_Delete(messageJson);
    return;
  }
  std::string requestKey = keyItem->valuestring;

  bool requestSuccess = false;

  if (messageIdent == "hm://connect-state/v1/player/command") {
    auto res = connectStateHandler->handlePlayerCommand(messageJson);
    if (!res) {
      BELL_LOG(error, LOG_TAG, "Failed to handle player command: {}",
               res.errorMessage());
      requestSuccess = false;
    } else {
      requestSuccess = true;
    }
  }

  auto replyRes = dealerClient->replyToRequest(requestSuccess, requestKey);
  if (!replyRes) {
    BELL_LOG(error, LOG_TAG, "Failed to reply to dealer request: {}",
             replyRes.errorMessage());
  }
  cJSON_Delete(messageJson);
}

bell::Result<> cspot::Session::start() {
  // Start the dealer client
  auto res = dealerClient->connect();
  if (!res) {
    BELL_LOG(error, LOG_TAG, "Failed to connect to dealer client: {}",
             res.errorMessage());
    return res;
  }

  return {};
}
