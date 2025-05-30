#pragma once

#include <cstdint>
#include <string>

namespace cspot {
/**
 * @brief Encodes a byte array to a Base62 string.
 *
 * @param data Pointer to the byte array to encode.
 * @param size Size of the byte array.
 * @return std::string Base62 encoded string.
 */
std::string base62Encode(const uint8_t* data, size_t size);

/**
 * @brief Decodes a Base62 encoded string to a byte array.
 *
 * @param encodedStr Base62 encoded string.
 * @param outData Pointer to the output byte array.
 * @param outSize Reference to the size of the output byte array.
 * @return bool True if decoding was successful, false otherwise.
 */
bool base62Decode(std::string_view encodedStr, uint8_t* outData,
                  size_t& outSize);
}  // namespace cspot