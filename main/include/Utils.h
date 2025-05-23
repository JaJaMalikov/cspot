#pragma once

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include "fmt/core.h"
#include "mbedtls/base64.h"

namespace cspot {

inline void logDataBase64(const uint8_t* data, size_t dataSize) {
  std::string outputStr;
  size_t outputSize = 0;
  int res = mbedtls_base64_encode(nullptr, 0, &outputSize, data, dataSize);
  if (outputSize == 0) {
    throw std::runtime_error(
        fmt::format("Failed to calculate base64 encoded public key size"));
  }

  outputStr.resize(outputSize);
  res = mbedtls_base64_encode(reinterpret_cast<uint8_t*>(outputStr.data()),
                              outputStr.size(), &outputSize, data, dataSize);
  if (res != 0) {
    throw std::runtime_error("Failed to calculate base64 encoded public key");
  }

  std::cout << outputStr << std::endl;
}
};  // namespace cspot
