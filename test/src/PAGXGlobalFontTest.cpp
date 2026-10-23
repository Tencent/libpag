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

#include <gtest/gtest.h>
#include <cstdint>
#include <cstdio>
#include <vector>
#include "base/PAGTest.h"
#include "pagx/FontConfig.h"
#include "pagx/LayoutContext.h"
#include "pagx/PAGFont.h"
#include "utils/ProjectPath.h"

// The process-wide registry is src-internal; tests include it to reset the global state so it
// does not leak into later tests of the same binary.
#include "pagx/GlobalFontRegistry.h"
#include "tgfx/core/Typeface.h"

namespace pagx {
namespace {

using pag::ProjectPath;

std::vector<uint8_t> ReadFontBytes(const std::string& path) {
  FILE* file = fopen(path.c_str(), "rb");
  if (file == nullptr) {
    return {};
  }
  fseek(file, 0, SEEK_END);
  auto size = static_cast<size_t>(ftell(file));
  fseek(file, 0, SEEK_SET);
  std::vector<uint8_t> bytes(size);
  if (size > 0) {
    fread(bytes.data(), 1, size, file);
  }
  fclose(file);
  return bytes;
}

}  // namespace

// Registering a font file makes its family resolvable through a plain LayoutContext, with the
// registration key read from the file itself. Invalid inputs return false.
PAGX_TEST(PAGXGlobalFontTest, RegisterFontFromPathAndBytes) {
  GlobalFontRegistry::ResetForTesting();
  EXPECT_FALSE(PAGFont::RegisterFont(""));
  EXPECT_FALSE(PAGFont::RegisterFont("/tmp/definitely_missing_font_file.ttf"));
  const char* garbage = "not a font at all";
  EXPECT_FALSE(PAGFont::RegisterFont(garbage, 14));
  EXPECT_FALSE(PAGFont::RegisterFont(nullptr, 0));

  auto emojiFont = ProjectPath::Absolute("resources/font/NotoColorEmoji.ttf");
  EXPECT_TRUE(PAGFont::RegisterFont(emojiFont));

  FontConfig emptyConfig;
  LayoutContext context(&emptyConfig);
  auto typeface = context.findTypeface("Noto Color Emoji", "");
  ASSERT_TRUE(typeface != nullptr);
  // The emoji face covers U+1F389 no matter which tier resolved it.
  EXPECT_NE(typeface->getGlyphID(0x1F389), 0);
  // A style miss within the family falls back to the registered Regular face (stage 2).
  EXPECT_TRUE(context.findTypeface("Noto Color Emoji", "Bold") != nullptr);
  // The bytes overload registers independently.
  auto hebrewBytes =
      ReadFontBytes(ProjectPath::Absolute("resources/font/NotoSansHebrew-Regular.ttf"));
  ASSERT_FALSE(hebrewBytes.empty());
  EXPECT_TRUE(PAGFont::RegisterFont(hebrewBytes.data(), hebrewBytes.size()));
  EXPECT_TRUE(context.findTypeface("Noto Sans Hebrew", "") != nullptr);
  // The global tier applies without any document FontConfig as well.
  LayoutContext nullContext(nullptr);
  EXPECT_TRUE(nullContext.findTypeface("Noto Sans Hebrew", "") != nullptr);
  GlobalFontRegistry::ResetForTesting();
}

// SetFallbackFontNames replaces the process-wide fallback list; names resolve from the system
// and are tried in order, before system fallback fonts.
PAGX_TEST(PAGXGlobalFontTest, SetFallbackFontNamesReplacesList) {
  GlobalFontRegistry::ResetForTesting();
  PAGFont::SetFallbackFontNames({"Courier New", "Helvetica"});
  LayoutContext nullContext(nullptr);
  auto first = nullContext.fallbackTypeface('A', nullptr);
  ASSERT_TRUE(first != nullptr);
  EXPECT_EQ(first->fontFamily(), "Courier New");

  PAGFont::SetFallbackFontNames({"Helvetica"});
  auto second = nullContext.fallbackTypeface('A', nullptr);
  ASSERT_TRUE(second != nullptr);
  EXPECT_EQ(second->fontFamily(), "Helvetica");
  // An empty name list clears the global fallback tier: whatever resolves next, it is no
  // longer the globally registered Helvetica face.
  PAGFont::SetFallbackFontNames({});
  auto third = nullContext.fallbackTypeface('A', nullptr);
  if (third != nullptr) {
    EXPECT_NE(third->fontFamily(), "Helvetica");
  }
  GlobalFontRegistry::ResetForTesting();
}

// Document-level FontConfig entries win over the process-wide registry, mirroring the per-doc
// override semantics of the old global font API.
PAGX_TEST(PAGXGlobalFontTest, DocumentConfigWinsOverGlobalRegistry) {
  GlobalFontRegistry::ResetForTesting();
  EXPECT_TRUE(PAGFont::RegisterFont(ProjectPath::Absolute("resources/font/NotoColorEmoji.ttf")));
  FontConfig docConfig;
  docConfig.registerFont(ProjectPath::Absolute("resources/font/NotoSansHebrew-Regular.ttf"), 0,
                         "Noto Color Emoji", "");
  LayoutContext docContext(&docConfig);
  auto typeface = docContext.findTypeface("Noto Color Emoji", "");
  ASSERT_TRUE(typeface != nullptr);
  // The doc-registered Hebrew face wins over the globally registered emoji face.
  EXPECT_EQ(typeface->getGlyphID(0x1F389), 0);

  // Document fallbacks are tried before the global fallback list.
  docConfig.clearFallbackFonts();
  docConfig.addFallbackSystemFont("Helvetica", "");
  PAGFont::SetFallbackFontNames({"Courier New"});
  auto fallback = docContext.fallbackTypeface('A', nullptr);
  ASSERT_TRUE(fallback != nullptr);
  EXPECT_EQ(fallback->fontFamily(), "Helvetica");
  GlobalFontRegistry::ResetForTesting();
}

}  // namespace pagx
