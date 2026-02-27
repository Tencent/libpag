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

#include <memory>
#include <vector>
#include "tgfx/core/Typeface.h"

namespace pagx::cli {

/**
 * Provides access to system fallback fonts by querying native platform APIs. On macOS, this uses
 * CTFontCopyDefaultCascadeListForLanguages with the user's language preferences. On Linux, this
 * uses fontconfig's FcFontSort to enumerate system fonts in priority order. On Windows, this
 * enumerates the system font collection via DirectWrite.
 */
class SystemFonts {
 public:
  /**
   * Returns the system default fallback typeface list ordered by the user's language preferences.
   * The returned list covers CJK, emoji, and other scripts installed on the system.
   */
  static std::vector<std::shared_ptr<tgfx::Typeface>> FallbackTypefaces();
};

}  // namespace pagx::cli
