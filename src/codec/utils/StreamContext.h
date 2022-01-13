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

#pragma once

#include <string>
#include <vector>
#include "base/utils/Log.h"

namespace pag {
class StreamContext {
 public:
  virtual ~StreamContext() = default;

  void throwException(const std::string& message) {
    errorMessages.push_back(message);
  }

  bool hasException() {
    return !errorMessages.empty();
  }

  std::vector<std::string> errorMessages;
};

#ifdef DEBUG

#define Throw(context, message)                                              \
  do {                                                                       \
    (context)->throwException(message);                                      \
    LOGE("PAG Decode Failed: \"%s\" at %s:%d", message, __FILE__, __LINE__); \
  } while (false)

#else

#define Throw(context, message)         \
  do {                                  \
    (context)->throwException(message); \
  } while (false)

#endif
}  // namespace pag