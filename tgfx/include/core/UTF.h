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

#include <cstddef>
#include <cstdint>

namespace tgfx {
class UTF {
 public:
  /**
   * Given a sequence of UTF-8 bytes, return the number of unicode codepoints. If the sequence is
   * invalid UTF-8, return -1.
   */
  static int CountUTF8(const char* utf8, size_t byteLength);

  /**
   * Given a sequence of UTF-8 bytes, return the first unicode codepoint. The pointer will be
   * incremented to point at the next codepoint's start.  If invalid UTF-8 is encountered, set *ptr
   * to end and return -1.
   */
  static int32_t NextUTF8(const char** ptr, const char* end);
};

}  // namespace tgfx
