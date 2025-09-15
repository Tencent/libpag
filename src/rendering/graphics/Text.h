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

#include "rendering/caches/TextBlock.h"
#include "rendering/graphics/Graphic.h"
#include "tgfx/core/Paint.h"

namespace pag {
class TextRun {
 public:
  ~TextRun() {
    delete paints[0];
    delete paints[1];
  }

  tgfx::Matrix matrix = tgfx::Matrix::I();
  tgfx::Paint* paints[2] = {nullptr, nullptr};
  tgfx::Font textFont = {};
  std::vector<tgfx::GlyphID> glyphIDs;
  std::vector<tgfx::Point> positions;
};

class Text : public Graphic {
 public:
  /**
   * Creates a text Graphic with specified glyphs. Returns nullptr if glyphs is empty or all of them
   * are invisible.
   */
  static std::shared_ptr<Graphic> MakeFrom(const std::vector<GlyphHandle>& glyphs,
                                           std::shared_ptr<TextBlock> textBlock,
                                           const tgfx::Rect* calculatedBounds = nullptr);

  ~Text() override;

  GraphicType type() const override {
    return GraphicType::Text;
  }

  void measureBounds(tgfx::Rect* rect) const override;
  bool hitTest(RenderCache* cache, float x, float y) override;
  bool getPath(tgfx::Path* path) const override;
  void prepare(RenderCache* cache) const override;
  void draw(Canvas* canvas) const override;

 private:
  Text(std::vector<GlyphHandle> glyphs, std::vector<TextRun*> textRuns, const tgfx::Rect& bounds,
       bool hasAlpha, std::shared_ptr<TextBlock> textBlock);

  void drawTextRuns(Canvas* canvas, int paintIndex) const;

  std::vector<GlyphHandle> glyphs;
  std::vector<TextRun*> textRuns;
  tgfx::Rect bounds = tgfx::Rect::MakeEmpty();
  bool hasAlpha = false;
  std::shared_ptr<TextBlock> textBlock = nullptr;
};
}  // namespace pag
