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
//  or required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <mutex>
#include <string>
#include <vector>
#include "pagx/FontConfig.h"

namespace pagx {

/**
 * Process-wide font registry backing pagx::PAGFont::RegisterFont / SetFallbackFontNames
 * (interface draft §3). LayoutContext consults it after the document's own FontConfig and
 * before system fonts, so hosts only need the global calls to feed every document. All access
 * is serialized by an internal mutex.
 */
class GlobalFontRegistry {
 public:
  /**
   * Registers a font file by path. The registration key is read from the file itself.
   */
  static bool RegisterFontFromPath(const std::string& fontPath);

  /**
   * Registers a font from an in-memory buffer. The registration key is read from the data.
   */
  static bool RegisterFontFromBytes(const void* data, size_t length);

  /**
   * Replaces the global fallback list with system-resolved family names, tried in order.
   * Unknown names are skipped, matching the legacy SetFallbackFontNames semantics.
   */
  static void SetFallbackFontNames(const std::vector<std::string>& names);

  /**
   * The mutex serializing all registry access. LayoutContext takes it while reading the font
   * config; registration APIs take it while writing.
   */
  static std::mutex& GetMutex();

  /**
   * The registry contents. The FontConfig object itself never gets replaced, so a reference is
   * stable for the process lifetime; mutations only happen under GetMutex().
   */
  static FontConfig& GetFontConfig();

  /**
   * Empties the registry. Test-only: keeps the process-global tier from leaking into later tests
   * running in the same binary.
   */
  static void ResetForTesting();
};

}  // namespace pagx
