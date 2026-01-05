/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#pragma once

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include "base/utils/Log.h"

namespace pag {
static constexpr uint8_t LENGTH_FOR_STORE_NUM_BITS = 5;

class StreamContext {
 public:
  virtual ~StreamContext() = default;

  bool throwException(const std::string& message) {
    if (!errorMessages.empty() && errorMessages.back() == message) {
      return false;
    }
    errorMessages.push_back(message);
    return true;
  }

  bool hasException() {
    return !errorMessages.empty();
  }

  std::vector<std::string> errorMessages;
};

inline size_t BitsToBytes(size_t capacity) {
  return static_cast<size_t>(ceil(capacity * 0.125));
}

#ifdef DEBUG

#define PAGThrowError(context, message)                                          \
  do {                                                                           \
    if ((context)->throwException(message)) {                                    \
      LOGE("PAG Decoding Failed: \"%s\" at %s:%d", message, __FILE__, __LINE__); \
    }                                                                            \
  } while (false)

#else

#define PAGThrowError(context, message) \
  do {                                  \
    (context)->throwException(message); \
  } while (false)

#endif
}  // namespace pag
