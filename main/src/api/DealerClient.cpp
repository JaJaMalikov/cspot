#include "api/DealerClient.h"
#include "SessionContext.h"
#include <cJSON.h>
#include <fmt/format.h>

using namespace cspot;

DealerClient::DealerClient(std::shared_ptr<SessionContext> ctx)
    : sessionContext(std::move(ctx)) {}

bell::Result<> DealerClient::connect() {
  auto accessKey = sessionContext->credentialsResolver->getAccessKey();
  if (!accessKey) return accessKey.getError();
  auto dealerAddr =
      sessionContext->credentialsResolver->getApAddress(CredentialsResolver::AddressType::Dealer);
  if (!dealerAddr) return dealerAddr.getError();

  std::string url = fmt::format("wss://{}/?access_token={}", dealerAddr.getValue(),
                               accessKey.getValue());
  esp_websocket_client_config_t cfg = {};
  cfg.uri = url.c_str();
  wsClient = esp_websocket_client_init(&cfg);
  if (!wsClient) return std::errc::not_enough_memory;
  esp_websocket_register_events(wsClient, WEBSOCKET_EVENT_ANY, websocketHandler,
                                this);
  esp_websocket_client_start(wsClient);
  return {};
}

bell::Result<> DealerClient::replyToRequest(bool success,
                                            const std::string& requestKey) {
  cJSON* root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "type", "reply");
  cJSON_AddStringToObject(root, "key", requestKey.c_str());
  cJSON* payload = cJSON_AddObjectToObject(root, "payload");
  cJSON_AddBoolToObject(payload, "success", success);
  char* str = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  esp_websocket_client_send_text(wsClient, str, strlen(str), portMAX_DELAY);
  free(str);
  return {};
}

void DealerClient::websocketHandler(void* arg, esp_event_base_t, int32_t id,
                                    void* data) {
  auto* self = static_cast<DealerClient*>(arg);
  if (id == WEBSOCKET_EVENT_CONNECTED) {
    self->connectionReady = true;
    BELL_LOG(info, self->LOG_TAG, "Dealer websocket connected");
  } else if (id == WEBSOCKET_EVENT_DATA) {
    auto* event = static_cast<esp_websocket_event_data_t*>(data);
    std::string payload(event->data_ptr, event->data_len);
    self->sessionContext->eventLoop->post(EventLoop::EventType::DEALER_MESSAGE,
                                          payload);
  }
}

void DealerClient::doHousekeeping() {}
