#pragma once
#include <functional>
#include <string>
struct mg_connection;
namespace bell {
class BellHTTPServer {
 public:
  explicit BellHTTPServer(int) {}
  using Handler = std::function<std::string(struct mg_connection*)>;
  void registerGet(const std::string&, Handler) {}
  void registerPost(const std::string&, Handler) {}
  std::string makeJsonResponse(const std::string& data) { return data; }
};
}
