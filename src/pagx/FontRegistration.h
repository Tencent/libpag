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

#include <memory>
#include <string>
#include <vector>
#include "pagx/FontConfig.h"

namespace tgfx {
class Typeface;
}

namespace pagx {

class PAGXDocument;

/**
 * Resolves one --fallback specifier to a typeface: either a font file path (contains a path
 * separator or ends with a known font extension) or a system font name in "family[,style]"
 * form. Returns null when the path cannot be parsed or the system has no matching font.
 */
std::shared_ptr<tgfx::Typeface> ResolveFallbackTypeface(const std::string& specifier);

/**
 * Registers fallback fonts into the config, in order: file paths register as both primary
 * (exact family match) and fallback entries; names resolve from the system. Returns the
 * specifiers that could not be resolved, in order.
 * @param fontConfig the config to register into.
 * @param fallbacks the --fallback specifiers.
 */
std::vector<std::string> RegisterFallbackFonts(FontConfig* fontConfig,
                                               const std::vector<std::string>& fallbacks);

/**
 * Registers every Font node's `file` declaration into the config (design doc §4.3): each file
 * is a primary registration under its own family/style and is also added to the fallback chain
 * so unicode-range subset files sharing one key stay reachable. Returns the file paths that
 * failed to parse, in document node order.
 * @param document the document whose Font nodes are scanned.
 * @param fontConfig the config to register into.
 */
std::vector<std::string> RegisterDocumentFontFiles(PAGXDocument* document, FontConfig* fontConfig);

}  // namespace pagx
