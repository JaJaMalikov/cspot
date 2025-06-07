#pragma once

#include <array>
#include <memory>

#include <bell/Logger.h>
#include <bell/Result.h>
#include <esp_websocket_client.h>

// Own includes
#include "SessionContext.h"

namespace cspot {
class DealerClient {
 public:
  DealerClient(std::shared_ptr<cspot::SessionContext> sessionContext);

  bell::Result<> connect();

  bell::Result<> replyToRequest(bool success, const std::string& requestKey);

  // Used for keep alive messages
  void doHousekeeping();

 private:
  const char* LOG_TAG = "DealerClient";

  std::mutex accessMutex;

  std::shared_ptr<cspot::SessionContext> sessionContext;

  bool connectionReady = false;
  esp_websocket_client_handle_t wsClient = nullptr;

  static void websocketHandler(void* arg, esp_event_base_t base, int32_t id,
                               void* data);
};
}  // namespace cspot