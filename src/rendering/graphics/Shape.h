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

#include "rendering/graphics/GradientPaint.h"
#include "rendering/graphics/Graphic.h"
#include "rendering/graphics/Snapshot.h"

namespace pag {
class Shape : public Graphic {
 public:
  /**
   * Creates a shape Graphic with solid color fill. Returns nullptr if path is empty.
   */
  static std::shared_ptr<Graphic> MakeFrom(ID assetID, const tgfx::Path& path, tgfx::Color color);

  /**
   * Creates a shape Graphic with gradient color fill. Returns nullptr if path is empty.
   */
  static std::shared_ptr<Graphic> MakeFrom(ID assetID, const tgfx::Path& path,
                                           const GradientPaint& gradient);

  GraphicType type() const override {
    return GraphicType::Shape;
  }

  void measureBounds(tgfx::Rect* bounds) const override;
  bool hitTest(RenderCache* cache, float x, float y) override;
  bool getPath(tgfx::Path* result) const override;
  void prepare(RenderCache* cache) const override;
  void draw(tgfx::Canvas* canvas, RenderCache* cache) const override;

 private:
  Shape(ID assetID, tgfx::Path path, std::shared_ptr<tgfx::Shader> shader);

  std::unique_ptr<Snapshot> makeSnapshot(RenderCache* cache, float scaleFactor) const;

  ID assetID = 0;
  tgfx::Path path = {};
  std::shared_ptr<tgfx::Shader> shader;

  friend class RenderCache;
};
}  // namespace pag
