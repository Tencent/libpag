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

#pragma once

#ifdef PAG_USE_PAGX_LAYOUT

#include <string>
#include <vector>

namespace pagx {

struct BidiRun {
  size_t start = 0;
  size_t length = 0;
  bool isRTL = false;
};

class BidiResolver {
 public:
  /**
   * Resolves bidirectional runs for the given UTF-8 text. Returns runs in visual (display) order.
   */
  static std::vector<BidiRun> Resolve(const std::string& text);
};

}  // namespace pagx

#endif
