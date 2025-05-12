#include "crypto/Shannon.h"

#include <cstddef>

using namespace cspot;

namespace {
inline uint32_t rotl(uint32_t n, unsigned int c) {
  const unsigned int mask =
      (CHAR_BIT * sizeof(n) - 1);  // assumes width is a power of 2.
  // assert ( (c<=mask) &&"rotate by type width or more");
  c &= mask;
  return (n << c) | (n >> ((-c) & mask));
}
}  // namespace

uint32_t Shannon::sbox1(uint32_t w) {
  w ^= rotl(w, 5) | rotl(w, 7);
  w ^= rotl(w, 19) | rotl(w, 22);
  return w;
}

uint32_t Shannon::sbox2(uint32_t w) {
  w ^= rotl(w, 7) | rotl(w, 22);
  w ^= rotl(w, 5) | rotl(w, 19);
  return w;
}

void Shannon::cycle() {
  uint32_t t;

  /* nonlinear feedback function */
  t = this->R[12] ^ this->R[13] ^ this->konst;
  t = Shannon::sbox1(t) ^ rotl(this->R[0], 1);
  /* shift register */
  for (uint32_t i = 1; i < N; ++i)
    this->R[i - 1] = this->R[i];
  this->R[N - 1] = t;
  t = Shannon::sbox2(this->R[2] ^ this->R[15]);
  this->R[0] ^= t;
  this->sbuf = t ^ this->R[8] ^ this->R[12];
}

void Shannon::crcfunc(uint32_t i) {
  uint32_t t;

  /* Accumulate CRC of input */
  t = this->CRC[0] ^ this->CRC[2] ^ this->CRC[15] ^ i;
  for (uint32_t j = 1; j < N; ++j)
    this->CRC[j - 1] = this->CRC[j];
  this->CRC[N - 1] = t;
}

void Shannon::macfunc(uint32_t i) {
  this->crcfunc(i);
  this->R[KEYP] ^= i;
}

void Shannon::initState() {
  /* Register initialised to Fibonacci numbers; Counter zeroed. */
  this->R[0] = 1;
  this->R[1] = 1;
  for (uint32_t i = 2; i < N; ++i)
    this->R[i] = this->R[i - 1] + this->R[i - 2];
  this->konst = Shannon::INITKONST;
}

void Shannon::saveState() {
  for (uint32_t i = 0; i < Shannon::N; ++i)
    this->initR[i] = this->R[i];
}

void Shannon::reloadState() {
  for (uint32_t i = 0; i < Shannon::N; ++i)
    this->R[i] = this->initR[i];
}

void Shannon::genkonst() {
  this->konst = this->R[0];
}

void Shannon::diffuse() {
  for (uint16_t i = 0; i < Shannon::N; ++i) {
    this->cycle();
  }
}

#define Byte(x, i) ((uint32_t)(((x) >> (8 * (i))) & 0xFF))
#define BYTE2WORD(b)                                                       \
  ((((uint32_t)(b)[3] & 0xFF) << 24) | (((uint32_t)(b)[2] & 0xFF) << 16) | \
   (((uint32_t)(b)[1] & 0xFF) << 8) | (((uint32_t)(b)[0] & 0xFF)))
#define WORD2BYTE(w, b)  \
  {                      \
    (b)[3] = Byte(w, 3); \
    (b)[2] = Byte(w, 2); \
    (b)[1] = Byte(w, 1); \
    (b)[0] = Byte(w, 0); \
  }
#define XORWORD(w, b)     \
  {                       \
    (b)[3] ^= Byte(w, 3); \
    (b)[2] ^= Byte(w, 2); \
    (b)[1] ^= Byte(w, 1); \
    (b)[0] ^= Byte(w, 0); \
  }

#define XORWORD(w, b)     \
  {                       \
    (b)[3] ^= Byte(w, 3); \
    (b)[2] ^= Byte(w, 2); \
    (b)[1] ^= Byte(w, 1); \
    (b)[0] ^= Byte(w, 0); \
  }

/* Load key material into the register
 */
#define ADDKEY(k) this->R[KEYP] ^= (k);

void Shannon::loadKey(const uint8_t* key, size_t keyLen) {
  uint32_t i;
  uint32_t j;
  uint32_t k;
  std::array<uint32_t, 4> xtra{};
  /* start folding in key */
  for (i = 0; i < (keyLen & ~0x3); i += 4) {
    k = BYTE2WORD(&key[i]);
    ADDKEY(k);
    this->cycle();
  }

  /* if there were any extra key bytes, zero pad to a word */
  if (i < keyLen) {
    for (j = 0 /* i unchanged */; i < keyLen; ++i)
      xtra[j++] = key[i];
    for (/* j unchanged */; j < 4; ++j)
      xtra[j] = 0;
    k = BYTE2WORD(xtra);
    ADDKEY(k);
    this->cycle();
  }

  /* also fold in the length of the key */
  ADDKEY(keyLen);
  this->cycle();

  /* save a copy of the register */
  for (i = 0; i < N; ++i)
    this->CRC[i] = this->R[i];

  /* now diffuse */
  this->diffuse();

  /* now xor the copy back -- makes key loading irreversible */
  for (i = 0; i < N; ++i)
    this->R[i] ^= this->CRC[i];
}

void Shannon::key(const uint8_t* key, size_t keyLen) {
  this->initState();
  this->loadKey(key, keyLen);
  this->genkonst(); /* in case we proceed to stream generation */
  this->saveState();
  this->nbuf = 0;
}

void Shannon::nonce(const uint8_t* nonce, size_t nonceLen) {
  this->reloadState();
  this->konst = Shannon::INITKONST;
  this->loadKey(nonce, nonceLen);
  this->genkonst();
  this->nbuf = 0;
}

void Shannon::encrypt(uint8_t* buffer, size_t bufferLen) {
  uint8_t* endbuf;
  uint32_t t = 0;

  /* handle any previously buffered bytes */
  if (this->nbuf != 0) {
    while (this->nbuf != 0 && bufferLen != 0) {
      this->mbuf ^= *buffer << (32 - this->nbuf);
      *buffer ^= (this->sbuf >> (32 - this->nbuf)) & 0xFF;
      ++buffer;
      this->nbuf -= 8;
      --bufferLen;
    }
    if (this->nbuf != 0) /* not a whole word yet */
      return;
    /* LFSR already cycled */
    this->macfunc(this->mbuf);
  }

  /* handle whole words */
  endbuf = &buffer[bufferLen & ~((uint32_t)0x03)];
  while (buffer < endbuf) {
    this->cycle();
    t = BYTE2WORD(buffer);
    this->macfunc(t);
    t ^= this->sbuf;
    WORD2BYTE(t, buffer);
    buffer += 4;
  }

  /* handle any trailing bytes */
  bufferLen &= 0x03;
  if (bufferLen != 0) {
    this->cycle();
    this->mbuf = 0;
    this->nbuf = 32;
    while (this->nbuf != 0 && bufferLen != 0) {
      this->mbuf ^= *buffer << (32 - this->nbuf);
      *buffer ^= (this->sbuf >> (32 - this->nbuf)) & 0xFF;
      ++buffer;
      this->nbuf -= 8;
      --bufferLen;
    }
  }
}

void Shannon::decrypt(uint8_t* buffer, size_t bufferLen) {
  uint8_t* endbuf;
  uint32_t t = 0;

  /* handle any previously buffered bytes */
  if (this->nbuf != 0) {
    while (this->nbuf != 0 && bufferLen != 0) {
      *buffer ^= (this->sbuf >> (32 - this->nbuf)) & 0xFF;
      this->mbuf ^= *buffer << (32 - this->nbuf);
      ++buffer;
      this->nbuf -= 8;
      --bufferLen;
    }
    if (this->nbuf != 0) /* not a whole word yet */
      return;
    /* LFSR already cycled */
    this->macfunc(this->mbuf);
  }

  /* handle whole words */
  endbuf = &buffer[bufferLen & ~((uint32_t)0x03)];
  while (buffer < endbuf) {
    this->cycle();
    t = BYTE2WORD(buffer) ^ this->sbuf;
    this->macfunc(t);
    WORD2BYTE(t, buffer);
    buffer += 4;
  }

  /* handle any trailing bytes */
  bufferLen &= 0x03;
  if (bufferLen != 0) {
    this->cycle();
    this->mbuf = 0;
    this->nbuf = 32;
    while (this->nbuf != 0 && bufferLen != 0) {
      *buffer ^= (this->sbuf >> (32 - this->nbuf)) & 0xFF;
      this->mbuf ^= *buffer << (32 - this->nbuf);
      ++buffer;
      this->nbuf -= 8;
      --bufferLen;
    }
  }
}

void Shannon::finish(uint8_t* macBuffer, size_t macBufferLen) {
  /* handle any previously buffered bytes */
  if (this->nbuf != 0) {
    /* LFSR already cycled */
    this->macfunc(this->mbuf);
  }

  /* perturb the MAC to mark end of input.
     * Note that only the stream register is updated, not the CRC. This is an
     * action that can't be duplicated by passing in plaintext, hence
     * defeating any kind of extension attack.
     */
  this->cycle();
  ADDKEY(INITKONST ^ (this->nbuf << 3));
  this->nbuf = 0;

  /* now add the CRC to the stream register and diffuse it */
  for (uint32_t i = 0; i < N; ++i)
    this->R[i] ^= this->CRC[i];
  this->diffuse();

  /* produce output from the stream buffer */
  while (macBufferLen > 0) {
    this->cycle();
    if (macBufferLen >= 4) {
      WORD2BYTE(this->sbuf, macBuffer);
      macBufferLen -= 4;
      macBuffer += 4;
    } else {
      for (uint32_t i = 0; i < macBufferLen; ++i)
        macBuffer[i] = Byte(this->sbuf, i);
      break;
    }
  }
}
