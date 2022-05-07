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

#include "GLDrawer.h"
#include "gpu/CanvasState.h"
#include "tgfx/core/BlendMode.h"
#include "tgfx/core/Mask.h"
#include "tgfx/gpu/Canvas.h"

namespace tgfx {
class GLCanvas : public Canvas {
 public:
  explicit GLCanvas(Surface* surface);

  void clear() override;
  void drawTexture(const Texture* texture, const Texture* mask, bool inverted) override;
  void drawTexture(const Texture* texture, const RGBAAALayout* layout) override;
  void drawPath(const Path& path, const Paint& paint) override;
  void drawGlyphs(const GlyphID glyphIDs[], const Point positions[], size_t glyphCount,
                  const Font& font, const Paint& paint) override;
  void drawAtlas(const Texture* atlas, const Matrix matrix[], const Rect tex[],
                 const Color colors[], size_t count) override;

 protected:
  void onSave() override {
  }
  void onRestore() override {
  }
  void onSetMatrix(const Matrix&) override {
  }
  void onClipPath(const Path&) override {
  }

 private:
  std::shared_ptr<Surface> _clipSurface = nullptr;
  uint32_t clipID = kDefaultClipID;
  std::shared_ptr<GLDrawer> _drawer = nullptr;

  GLDrawer* getDrawer();

  Texture* getClipTexture();

  Matrix getViewMatrix();

  std::unique_ptr<FragmentProcessor> getClipMask(const Rect& deviceBounds, Rect* scissorRect);

  Rect clipLocalBounds(Rect localBounds);

  void drawTexture(const Texture* texture, const RGBAAALayout* layout, const Texture* mask,
                   bool inverted);

  void drawMask(const Rect& bounds, const Texture* mask, const Shader* shader);

  void drawColorGlyphs(const GlyphID glyphIDs[], const Point positions[], size_t glyphCount,
                       const Font& font, const Paint& paint);

  void drawMaskGlyphs(TextBlob* textBlob, const Paint& paint);

  void fillPath(const Path& path, const Shader* shader);

  void draw(std::unique_ptr<GLDrawOp> op, std::unique_ptr<FragmentProcessor> color,
            std::unique_ptr<FragmentProcessor> mask = nullptr, bool aa = false);
};
}  // namespace tgfx
