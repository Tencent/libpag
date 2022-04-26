/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "tgfx/core/UTF.h"

namespace tgfx {
static int utf8_byte_type(uint8_t c) {
  if (c < 0x80) {
    return 1;
  } else if (c < 0xC0) {
    return 0;
  } else if (c >= 0xF5 || (c & 0xFE) == 0xC0) {
    return -1;
  } else {
    int value = (((0xe5 << 24) >> ((unsigned)c >> 4 << 1)) & 3) + 1;
    return value;
  }
}

static bool utf8_type_is_valid_leading_byte(int type) {
  return type > 0;
}

static bool utf8_byte_is_continuation(uint8_t c) {
  return utf8_byte_type(c) == 0;
}

int UTF::CountUTF8(const char* utf8, size_t byteLength) {
  if (!utf8) {
    return -1;
  }
  int count = 0;
  const char* stop = utf8 + byteLength;
  while (utf8 < stop) {
    int type = utf8_byte_type(*(const uint8_t*)utf8);
    if (!utf8_type_is_valid_leading_byte(type) || utf8 + type > stop) {
      return -1;
    }
    while (type-- > 1) {
      ++utf8;
      if (!utf8_byte_is_continuation(*(const uint8_t*)utf8)) {
        return -1;
      }
    }
    ++utf8;
    ++count;
  }
  return count;
}

template <typename T>
static int32_t next_fail(const T** ptr, const T* end) {
  *ptr = end;
  return -1;
}

static constexpr inline int32_t left_shift(int32_t value, int32_t shift) {
  return (int32_t)((uint32_t)value << shift);
}

int32_t UTF::NextUTF8(const char** ptr, const char* end) {
  if (!ptr || !end) {
    return -1;
  }
  const uint8_t* p = (const uint8_t*)*ptr;
  if (!p || p >= (const uint8_t*)end) {
    return next_fail(ptr, end);
  }
  int c = *p;
  int hic = c << 24;

  if (!utf8_type_is_valid_leading_byte(utf8_byte_type(c))) {
    return next_fail(ptr, end);
  }
  if (hic < 0) {
    uint32_t mask = (uint32_t)~0x3F;
    hic = left_shift(hic, 1);
    do {
      ++p;
      if (p >= (const uint8_t*)end) {
        return next_fail(ptr, end);
      }
      uint8_t nextByte = *p;
      if (!utf8_byte_is_continuation(nextByte)) {
        return next_fail(ptr, end);
      }
      c = (c << 6) | (nextByte & 0x3F);
      mask <<= 5;
    } while ((hic = left_shift(hic, 1)) < 0);
    c &= ~mask;
  }
  *ptr = (char*)p + 1;
  return c;
}
}  // namespace tgfx
