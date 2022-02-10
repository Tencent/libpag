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

#include "gpu/Paint.h"
#include "rendering/graphics/Graphic.h"

namespace pag {
class TextRun {
 public:
  ~TextRun() {
    delete paints[0];
    delete paints[1];
  }

  Matrix matrix = Matrix::I();
  Paint* paints[2] = {nullptr, nullptr};
  Font textFont = {};
  std::vector<GlyphID> glyphIDs;
  std::vector<Point> positions;
};

class Text : public Graphic {
 public:
  /**
   * Creates a text Graphic with specified glyphs. Returns nullptr if glyphs is empty or all of them
   * are invisible.
   */
  static std::shared_ptr<Graphic> MakeFrom(const std::vector<GlyphHandle>& glyphs,
                                           const Rect* calculatedBounds = nullptr);

  ~Text() override;

  GraphicType type() const override {
    return GraphicType::Text;
  }

  void measureBounds(Rect* rect) const override;
  bool hitTest(RenderCache* cache, float x, float y) override;
  bool getPath(Path* path) const override;
  void prepare(RenderCache* cache) const override;
  void draw(Canvas* canvas, RenderCache* cache) const override;

 private:
  std::vector<TextRun*> textRuns;
  Rect bounds = Rect::MakeEmpty();
  bool hasAlpha = false;

  explicit Text(const std::vector<TextRun*>& textRuns, const Rect& bounds, bool hasAlpha);
  void drawTextRuns(Canvas* canvas, int paintIndex) const;
};
}  // namespace pag
