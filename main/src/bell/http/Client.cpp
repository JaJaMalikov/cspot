#include "bell/http/Reader.h"
#include <esp_http_client.h>
#include <cstring>

namespace bell {
namespace http {

static esp_err_t event_handler(esp_http_client_event_t*) { return ESP_OK; }

static bell::Result<HTTPReader> perform(Method method, const std::string& url,
                                        const std::vector<std::pair<std::string, std::string>>& headers,
                                        const std::byte* body = nullptr,
                                        size_t bodyLen = 0) {
  esp_http_client_config_t cfg = {};
  cfg.url = url.c_str();
  cfg.event_handler = event_handler;
  esp_http_client_handle_t client = esp_http_client_init(&cfg);
  if (!client) return std::errc::not_enough_memory;
  switch (method) {
    case Method::GET:
      esp_http_client_set_method(client, HTTP_METHOD_GET);
      break;
    case Method::POST:
      esp_http_client_set_method(client, HTTP_METHOD_POST);
      break;
    case Method::PUT:
      esp_http_client_set_method(client, HTTP_METHOD_PUT);
      break;
  }
  for (auto& h : headers) {
    esp_http_client_set_header(client, h.first.c_str(), h.second.c_str());
  }
  if (body && bodyLen) {
    esp_http_client_set_post_field(client, reinterpret_cast<const char*>(body), bodyLen);
  }
  esp_err_t err = esp_http_client_perform(client);
  if (err != ESP_OK) {
    esp_http_client_cleanup(client);
    return std::errc::io_error;
  }
  int status = esp_http_client_get_status_code(client);
  int len = esp_http_client_get_content_length(client);
  std::string response;
  if (len > 0) {
    response.resize(len);
    esp_http_client_read(client, response.data(), len);
  }
  esp_http_client_cleanup(client);
  return HTTPReader(status, std::move(response));
}

bell::Result<HTTPReader> request(Method method, const std::string& url,
                                 const std::vector<std::pair<std::string, std::string>>& headers) {
  return perform(method, url, headers, nullptr, 0);
}

bell::Result<HTTPReader> requestWithBody(Method method, const std::string& url,
                                         const std::vector<std::pair<std::string, std::string>>& headers,
                                         const std::vector<std::byte>& body) {
  return perform(method, url, headers, body.data(), body.size());
}

bell::Result<HTTPReader> requestWithBodyPtr(Method method, const std::string& url,
                                            const std::vector<std::pair<std::string, std::string>>& headers,
                                            const std::byte* body, size_t size) {
  return perform(method, url, headers, body, size);
}

}  // namespace http
}  // namespace bell
