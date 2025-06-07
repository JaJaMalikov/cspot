#pragma once
#include <cstddef>
#include <cstdint>
#include <istream>

namespace bell::io {
class BinaryStream {
 public:
  explicit BinaryStream(std::istream* in) : stream(in) {}
  BinaryStream& operator>>(uint8_t& v) {
    v = stream->get();
    return *this;
  }
  void skip(size_t n) { stream->ignore(n); }
 private:
  std::istream* stream;
};
}  // namespace bell::io
