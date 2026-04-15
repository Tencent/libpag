/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "MD5.h"

namespace pagx {

namespace {
inline uint32_t F(uint32_t x, uint32_t y, uint32_t z) {
  return (x & y) | (~x & z);
}
inline uint32_t G(uint32_t x, uint32_t y, uint32_t z) {
  return (x & z) | (y & ~z);
}
inline uint32_t H(uint32_t x, uint32_t y, uint32_t z) {
  return x ^ y ^ z;
}
inline uint32_t I(uint32_t x, uint32_t y, uint32_t z) {
  return y ^ (x | ~z);
}
inline uint32_t rotate_left(uint32_t x, uint32_t n) {
  return (x << n) | (x >> (32 - n));
}

const uint32_t k[64] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};
const uint32_t r[] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
                      5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
                      4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
                      6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};

class MD5Impl {
 public:
  MD5Impl() {
    init();
  }

  void update(const uint8_t* input, size_t length) {
    size_t index = (count / 8) % 64;
    count += length * 8;
    size_t partLen = 64 - index;
    size_t i = 0;
    if (length >= partLen) {
      std::memcpy(&buffer[index], input, partLen);
      transform(buffer);
      for (i = partLen; i + 63 < length; i += 64) {
        transform(&input[i]);
      }
      index = 0;
    }
    std::memcpy(&buffer[index], &input[i], length - i);
  }

  MD5::Digest finalize() {
    MD5::Digest digest = {};
    static const uint8_t PADDING[64] = {0x80};
    uint8_t bits[8];
    for (int i = 0; i < 8; ++i) bits[i] = (count >> (8 * i)) & 0xff;
    size_t index = (count / 8) % 64;
    size_t padLen = (index < 56) ? (56 - index) : (120 - index);
    update(PADDING, padLen);
    update(bits, 8);
    for (uint32_t i = 0; i < 4; ++i) {
      digest[(static_cast<size_t>(i) * 4) + 0] = static_cast<uint8_t>((state[i]) & 0xff);
      digest[(static_cast<size_t>(i) * 4) + 1] = static_cast<uint8_t>((state[i] >> 8) & 0xff);
      digest[(static_cast<size_t>(i) * 4) + 2] = static_cast<uint8_t>((state[i] >> 16) & 0xff);
      digest[(static_cast<size_t>(i) * 4) + 3] = static_cast<uint8_t>((state[i] >> 24) & 0xff);
    }
    return digest;
  }

 private:
  void init() {
    count = 0;
    state[0] = 0x67452301;
    state[1] = 0xefcdab89;
    state[2] = 0x98badcfe;
    state[3] = 0x10325476;
    std::memset(buffer, 0, sizeof(buffer));
  }

  void transform(const uint8_t block[64]) {
    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t m[16];
    for (uint32_t i = 0; i < 16; ++i) {
      m[i] = static_cast<uint32_t>(block[(i * 4) + 0]) |
             (static_cast<uint32_t>(block[(i * 4) + 1]) << 8) |
             (static_cast<uint32_t>(block[(i * 4) + 2]) << 16) |
             (static_cast<uint32_t>(block[(i * 4) + 3]) << 24);
    }
    for (uint32_t i = 0; i < 64; ++i) {
      uint32_t f;
      uint32_t g;
      if (i < 16) {
        f = F(b, c, d);
        g = i;
      } else if (i < 32) {
        f = G(b, c, d);
        g = static_cast<uint32_t>((5 * i + 1) % 16);
      } else if (i < 48) {
        f = H(b, c, d);
        g = static_cast<uint32_t>((3 * i + 5) % 16);
      } else {
        f = I(b, c, d);
        g = static_cast<uint32_t>((7 * i) % 16);
      }
      uint32_t temp = d;
      d = c;
      c = b;
      b = b + rotate_left(a + f + k[i] + m[g], r[i]);
      a = temp;
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
  }

  uint32_t state[4];
  uint64_t count;
  uint8_t buffer[64];
};
}  // namespace

MD5::Digest MD5::Calculate(const void* bytes, size_t size) {
  Digest digest = {};
  if (bytes == nullptr || size == 0) {
    return digest;
  }
  MD5Impl impl;
  impl.update(static_cast<const uint8_t*>(bytes), size);
  return impl.finalize();
}

}  // namespace pagx
