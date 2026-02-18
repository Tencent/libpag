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

#include "ToTGFX.h"
#include "tgfx/core/ColorSpace.h"
#include "tgfx/layers/LayerMaskType.h"
#include "tgfx/layers/LayerPaint.h"
#include "tgfx/layers/StrokeAlign.h"
#include "tgfx/layers/vectors/FillStyle.h"
#include "tgfx/layers/vectors/MergePath.h"
#include "tgfx/layers/vectors/Repeater.h"
#include "tgfx/layers/vectors/TextSelector.h"

namespace pagx {

tgfx::Path PathDataToTGFXPath(const PathData& pathData) {
  tgfx::Path path = {};
  pathData.forEach([&path](PathVerb verb, const Point* pts) {
    switch (verb) {
      case PathVerb::Move:
        path.moveTo(pts[0].x, pts[0].y);
        break;
      case PathVerb::Line:
        path.lineTo(pts[0].x, pts[0].y);
        break;
      case PathVerb::Quad:
        path.quadTo(pts[0].x, pts[0].y, pts[1].x, pts[1].y);
        break;
      case PathVerb::Cubic:
        path.cubicTo(pts[0].x, pts[0].y, pts[1].x, pts[1].y, pts[2].x, pts[2].y);
        break;
      case PathVerb::Close:
        path.close();
        break;
    }
  });
  return path;
}

tgfx::Point ToTGFX(const Point& point) {
  return tgfx::Point::Make(point.x, point.y);
}

static tgfx::ColorMatrix33 ComputeP3ToSRGBMatrix() {
  tgfx::ColorMatrix33 matrix = {};
  tgfx::ColorSpace::DisplayP3()->gamutTransformTo(tgfx::ColorSpace::SRGB().get(), &matrix);
  return matrix;
}

tgfx::Color ToTGFX(const Color& c) {
  // tgfx::Color is always in sRGB color space. If source color is in Display P3,
  // we need to convert it to sRGB for correct rendering.
  if (c.colorSpace == ColorSpace::DisplayP3) {
    static const tgfx::ColorMatrix33 p3ToSRGB = ComputeP3ToSRGBMatrix();

    float r = c.red * p3ToSRGB.values[0][0] + c.green * p3ToSRGB.values[0][1] +
              c.blue * p3ToSRGB.values[0][2];
    float g = c.red * p3ToSRGB.values[1][0] + c.green * p3ToSRGB.values[1][1] +
              c.blue * p3ToSRGB.values[1][2];
    float b = c.red * p3ToSRGB.values[2][0] + c.green * p3ToSRGB.values[2][1] +
              c.blue * p3ToSRGB.values[2][2];
    return {r, g, b, c.alpha};
  }
  return {c.red, c.green, c.blue, c.alpha};
}

tgfx::Matrix ToTGFX(const Matrix& m) {
  return tgfx::Matrix::MakeAll(m.a, m.c, m.tx, m.b, m.d, m.ty);
}

tgfx::Rect ToTGFX(const Rect& r) {
  return tgfx::Rect::MakeXYWH(r.x, r.y, r.width, r.height);
}

tgfx::Path ToTGFX(const PathData& pathData) {
  return PathDataToTGFXPath(pathData);
}

tgfx::LineCap ToTGFX(LineCap cap) {
  switch (cap) {
    case LineCap::Butt:
      return tgfx::LineCap::Butt;
    case LineCap::Round:
      return tgfx::LineCap::Round;
    case LineCap::Square:
      return tgfx::LineCap::Square;
  }
  return tgfx::LineCap::Butt;
}

tgfx::LineJoin ToTGFX(LineJoin join) {
  switch (join) {
    case LineJoin::Miter:
      return tgfx::LineJoin::Miter;
    case LineJoin::Round:
      return tgfx::LineJoin::Round;
    case LineJoin::Bevel:
      return tgfx::LineJoin::Bevel;
  }
  return tgfx::LineJoin::Miter;
}

tgfx::BlendMode ToTGFX(BlendMode mode) {
  switch (mode) {
    case BlendMode::Normal:
      return tgfx::BlendMode::SrcOver;
    case BlendMode::Multiply:
      return tgfx::BlendMode::Multiply;
    case BlendMode::Screen:
      return tgfx::BlendMode::Screen;
    case BlendMode::Overlay:
      return tgfx::BlendMode::Overlay;
    case BlendMode::Darken:
      return tgfx::BlendMode::Darken;
    case BlendMode::Lighten:
      return tgfx::BlendMode::Lighten;
    case BlendMode::ColorDodge:
      return tgfx::BlendMode::ColorDodge;
    case BlendMode::ColorBurn:
      return tgfx::BlendMode::ColorBurn;
    case BlendMode::HardLight:
      return tgfx::BlendMode::HardLight;
    case BlendMode::SoftLight:
      return tgfx::BlendMode::SoftLight;
    case BlendMode::Difference:
      return tgfx::BlendMode::Difference;
    case BlendMode::Exclusion:
      return tgfx::BlendMode::Exclusion;
    case BlendMode::Hue:
      return tgfx::BlendMode::Hue;
    case BlendMode::Saturation:
      return tgfx::BlendMode::Saturation;
    case BlendMode::Color:
      return tgfx::BlendMode::Color;
    case BlendMode::Luminosity:
      return tgfx::BlendMode::Luminosity;
    case BlendMode::PlusLighter:
      return tgfx::BlendMode::PlusLighter;
    case BlendMode::PlusDarker:
      return tgfx::BlendMode::PlusDarker;
  }
  return tgfx::BlendMode::SrcOver;
}

tgfx::FillRule ToTGFX(FillRule rule) {
  return rule == FillRule::EvenOdd ? tgfx::FillRule::EvenOdd : tgfx::FillRule::Winding;
}

tgfx::LayerPlacement ToTGFX(LayerPlacement placement) {
  return placement == LayerPlacement::Foreground ? tgfx::LayerPlacement::Foreground
                                                 : tgfx::LayerPlacement::Background;
}

tgfx::StrokeAlign ToTGFX(StrokeAlign align) {
  switch (align) {
    case StrokeAlign::Center:
      return tgfx::StrokeAlign::Center;
    case StrokeAlign::Inside:
      return tgfx::StrokeAlign::Inside;
    case StrokeAlign::Outside:
      return tgfx::StrokeAlign::Outside;
  }
  return tgfx::StrokeAlign::Center;
}

tgfx::MergePathOp ToTGFX(MergePathMode mode) {
  switch (mode) {
    case MergePathMode::Append:
      return tgfx::MergePathOp::Append;
    case MergePathMode::Union:
      return tgfx::MergePathOp::Union;
    case MergePathMode::Intersect:
      return tgfx::MergePathOp::Intersect;
    case MergePathMode::Xor:
      return tgfx::MergePathOp::XOR;
    case MergePathMode::Difference:
      return tgfx::MergePathOp::Difference;
  }
  return tgfx::MergePathOp::Append;
}

tgfx::LayerMaskType ToTGFXMaskType(MaskType type) {
  switch (type) {
    case MaskType::Alpha:
      return tgfx::LayerMaskType::Alpha;
    case MaskType::Luminance:
      return tgfx::LayerMaskType::Luminance;
    case MaskType::Contour:
      return tgfx::LayerMaskType::Contour;
  }
  return tgfx::LayerMaskType::Alpha;
}

tgfx::RepeaterOrder ToTGFX(RepeaterOrder order) {
  switch (order) {
    case RepeaterOrder::BelowOriginal:
      return tgfx::RepeaterOrder::BelowOriginal;
    case RepeaterOrder::AboveOriginal:
      return tgfx::RepeaterOrder::AboveOriginal;
  }
  return tgfx::RepeaterOrder::BelowOriginal;
}

tgfx::SelectorUnit ToTGFX(SelectorUnit unit) {
  switch (unit) {
    case SelectorUnit::Index:
      return tgfx::SelectorUnit::Index;
    case SelectorUnit::Percentage:
      return tgfx::SelectorUnit::Percentage;
  }
  return tgfx::SelectorUnit::Index;
}

tgfx::SelectorShape ToTGFX(SelectorShape shape) {
  switch (shape) {
    case SelectorShape::Square:
      return tgfx::SelectorShape::Square;
    case SelectorShape::RampUp:
      return tgfx::SelectorShape::RampUp;
    case SelectorShape::RampDown:
      return tgfx::SelectorShape::RampDown;
    case SelectorShape::Triangle:
      return tgfx::SelectorShape::Triangle;
    case SelectorShape::Round:
      return tgfx::SelectorShape::Round;
    case SelectorShape::Smooth:
      return tgfx::SelectorShape::Smooth;
  }
  return tgfx::SelectorShape::Square;
}

tgfx::SelectorMode ToTGFX(SelectorMode mode) {
  switch (mode) {
    case SelectorMode::Add:
      return tgfx::SelectorMode::Add;
    case SelectorMode::Subtract:
      return tgfx::SelectorMode::Subtract;
    case SelectorMode::Intersect:
      return tgfx::SelectorMode::Intersect;
    case SelectorMode::Min:
      return tgfx::SelectorMode::Min;
    case SelectorMode::Max:
      return tgfx::SelectorMode::Max;
    case SelectorMode::Difference:
      return tgfx::SelectorMode::Difference;
  }
  return tgfx::SelectorMode::Add;
}

tgfx::TileMode ToTGFX(TileMode mode) {
  switch (mode) {
    case TileMode::Clamp:
      return tgfx::TileMode::Clamp;
    case TileMode::Repeat:
      return tgfx::TileMode::Repeat;
    case TileMode::Mirror:
      return tgfx::TileMode::Mirror;
    case TileMode::Decal:
      return tgfx::TileMode::Decal;
  }
  return tgfx::TileMode::Clamp;
}

tgfx::FilterMode ToTGFX(FilterMode mode) {
  switch (mode) {
    case FilterMode::Nearest:
      return tgfx::FilterMode::Nearest;
    case FilterMode::Linear:
      return tgfx::FilterMode::Linear;
  }
  return tgfx::FilterMode::Linear;
}

tgfx::MipmapMode ToTGFX(MipmapMode mode) {
  switch (mode) {
    case MipmapMode::None:
      return tgfx::MipmapMode::None;
    case MipmapMode::Nearest:
      return tgfx::MipmapMode::Nearest;
    case MipmapMode::Linear:
      return tgfx::MipmapMode::Linear;
  }
  return tgfx::MipmapMode::Linear;
}

tgfx::Matrix3D ToTGFX3D(const Matrix3D& matrix3D) {
  tgfx::Matrix3D m = {};
  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      m.setRowColumn(r, c, matrix3D.getRowColumn(r, c));
    }
  }
  return m;
}

static void ReleasePagxData(const void*, void* context) {
  delete static_cast<std::shared_ptr<Data>*>(context);
}

std::shared_ptr<tgfx::Data> ToTGFXData(const std::shared_ptr<Data>& data) {
  if (data == nullptr) {
    return nullptr;
  }
  auto* ctx = new std::shared_ptr<Data>(data);
  auto result = tgfx::Data::MakeAdopted(data->data(), data->size(), ReleasePagxData, ctx);
  if (!result) {
    delete ctx;
  }
  return result;
}

}  // namespace pagx
