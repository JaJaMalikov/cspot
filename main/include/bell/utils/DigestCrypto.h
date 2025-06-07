#pragma once
#include <mbedtls/md.h>
#include <vector>

namespace bell {
namespace utils {
class DigestCrypto {
 public:
  DigestCrypto(mbedtls_md_type_t type, bool hmac) {
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(type), hmac);
  }
  ~DigestCrypto() { mbedtls_md_free(&ctx); }
  void getHmac(const uint8_t* key, size_t keyLen, const uint8_t* data,
               size_t dataLen, uint8_t* out) {
    mbedtls_md_hmac_starts(&ctx, key, keyLen);
    mbedtls_md_hmac_update(&ctx, data, dataLen);
    mbedtls_md_hmac_finish(&ctx, out);
  }
 private:
  mbedtls_md_context_t ctx;
};
}  // namespace utils
}  // namespace bell
