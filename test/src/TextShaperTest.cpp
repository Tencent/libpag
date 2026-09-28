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

#include <string>
#include <vector>
#include "pag/pag.h"
#include "rendering/FontManager.h"
#include "rendering/graphics/Glyph.h"
#include "tgfx/core/UTF.h"
#include "utils/TestUtils.h"

namespace pag {

static std::vector<size_t> CharStartOffsets(const std::string& text) {
  std::vector<size_t> offsets = {};
  const char* ptr = text.data();
  const char* end = ptr + text.size();
  while (ptr < end) {
    offsets.push_back(static_cast<size_t>(ptr - text.data()));
    tgfx::UTF::NextUTF8(&ptr, end);
  }
  return offsets;
}

/**
 * 用例描述: RTL 文本（如阿拉伯文与符号混排）经 HarfBuzz 整形后，每个 glyph 的名字必须与其覆盖的
 * 字符一一对应：名字非空、对齐到字符边界、且全部名字的字节数恰好等于原文（不重叠不遗漏）。
 */
PAG_TEST(TextShaperTest, glyphNamesCoverRtlText) {
  PAGFont::RegisterFont(ProjectPath::Absolute("resources/font/NotoSansSC-Regular.otf"), 0,
                        "TextShaperTest", "Regular");
  auto typeface = FontManager::GetTypefaceWithoutFallback("TextShaperTest", "Regular");
  ASSERT_NE(typeface, nullptr);
  tgfx::Font font(typeface, 24);

  const std::vector<std::string> texts = {"✧٩و✧", "٩و", "و", "abc", "中文测试", "aوb"};
  for (const auto& text : texts) {
    auto glyphList = Glyph::BuildFromText(text, font, TextPaint{});
    ASSERT_FALSE(glyphList.empty()) << "text=[" << text << "]";
    auto offsets = CharStartOffsets(text);
    size_t totalBytes = 0;
    for (auto& glyph : glyphList) {
      auto name = glyph->getName();
      EXPECT_FALSE(name.empty()) << "text=[" << text << "]";
      bool aligned = false;
      for (auto offset : offsets) {
        if (text.compare(offset, name.size(), name) == 0) {
          aligned = true;
          break;
        }
      }
      EXPECT_TRUE(aligned) << "name=[" << name << "] is not char aligned in text=[" << text << "]";
      totalBytes += name.size();
    }
    EXPECT_EQ(totalBytes, text.size()) << "text=[" << text << "]";
  }
}

}  // namespace pag
