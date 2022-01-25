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

#include "rendering/graphics/Graphic.h"

namespace pag {
enum class ShapeFillType { Solid, Gradient };

class ShapeFill {
 public:
  virtual ~ShapeFill() = default;

  virtual ShapeFillType type() const = 0;
};

class SolidFill : public ShapeFill {
 public:
  ShapeFillType type() const override {
    return ShapeFillType::Solid;
  }

  Color color;
};

class GradientFill : public ShapeFill {
 public:
  ShapeFillType type() const override {
    return ShapeFillType::Gradient;
  }

  GradientPaint gradient = {};
};

class Shape : public Graphic {
 public:
  /**
   * Creates a shape Graphic with solid color fill. Returns nullptr if path is empty.
   */
  static std::shared_ptr<Graphic> MakeFrom(const Path& path, Color color);

  /**
   * Creates a shape Graphic with gradient color fill. Returns nullptr if path is empty.
   */
  static std::shared_ptr<Graphic> MakeFrom(const Path& path, const GradientPaint& gradient);

  ~Shape() override;

  GraphicType type() const override {
    return GraphicType::Shape;
  }

  void measureBounds(Rect* bounds) const override;
  bool hitTest(RenderCache* cache, float x, float y) override;
  bool getPath(Path* result) const override;
  void prepare(RenderCache* cache) const override;
  void draw(Canvas* canvas, RenderCache* cache) const override;

 private:
  Path path = {};
  ShapeFill* fill = nullptr;

  Shape(Path path, ShapeFill* fill);
};
}  // namespace pag
