#pragma once
#include <string>
#include <string_view>
#include <vector>
#include "bell/Result.h"

namespace bell {
namespace http {
class HTTPReader {
 public:
  HTTPReader(int status, std::string body)
      : status(status), body(std::move(body)) {}
  bell::Result<int> getStatusCode() { return status; }
  bell::Result<std::string_view> getBodyStringView() { return body; }
  bell::Result<const char*> getBodyBytesPtr() { return body.c_str(); }
  bell::Result<size_t> getBodyBytesLength() { return body.size(); }
 private:
  int status;
  std::string body;
};

enum class Method { GET, POST, PUT };

bell::Result<HTTPReader> request(Method method, const std::string& url,
                                 const std::vector<std::pair<std::string, std::string>>& headers = {});
bell::Result<HTTPReader> requestWithBody(Method method, const std::string& url,
                                         const std::vector<std::pair<std::string, std::string>>& headers,
                                         const std::vector<std::byte>& body);
bell::Result<HTTPReader> requestWithBodyPtr(Method method, const std::string& url,
                                            const std::vector<std::pair<std::string, std::string>>& headers,
                                            const std::byte* body, size_t size);
}  // namespace http
}  // namespace bell
