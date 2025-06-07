#pragma once
#include <streambuf>
#include <istream>

namespace bell::io {
class IMemoryStream : public std::streambuf {
 public:
  IMemoryStream(const std::byte* data, size_t len) {
    char* begin = const_cast<char*>(reinterpret_cast<const char*>(data));
    setg(begin, begin, begin + len);
  }
};
}  // namespace bell::io
