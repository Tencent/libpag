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
#include <emscripten/bind.h>
#include "tgfx/core/FontMetrics.h"
#include "tgfx/core/ImageInfo.h"
#include "tgfx/core/Matrix.h"
#include "tgfx/core/PathTypes.h"
#include "tgfx/core/Stroke.h"

using namespace emscripten;
using namespace tgfx;

EMSCRIPTEN_BINDINGS(tgfx) {
  class_<Matrix>("TGFXMatrix").function("_get", &Matrix::get).function("_set", &Matrix::set);

  value_object<Point>("TGFXPoint").field("x", &Point::x).field("y", &Point::y);

  value_object<Rect>("TGFXRect")
      .field("left", &Rect::left)
      .field("top", &Rect::top)
      .field("right", &Rect::right)
      .field("bottom", &Rect::bottom);

  register_vector<Point>("VectorTGFXPoint");

  class_<ImageInfo>("TGFXImageInfo")
      .property("width", &ImageInfo::width)
      .property("height", &ImageInfo::height)
      .property("rowBytes", &ImageInfo::rowBytes)
      .property("colorType", &ImageInfo::colorType);

  class_<Stroke>("TGFXStroke")
      .property("width", &Stroke::width)
      .property("cap", &Stroke::cap)
      .property("join", &Stroke::join)
      .property("miterLimit", &Stroke::miterLimit);

  value_object<FontMetrics>("TGFXFontMetrics")
      .field("ascent", &FontMetrics::ascent)
      .field("descent", &FontMetrics::descent)
      .field("xHeight", &FontMetrics::xHeight)
      .field("capHeight", &FontMetrics::capHeight);

  enum_<PathFillType>("TGFXPathFillType")
      .value("Winding", PathFillType::Winding)
      .value("EvenOdd", PathFillType::EvenOdd)
      .value("InverseWinding", PathFillType::InverseWinding)
      .value("InverseEvenOdd", PathFillType::InverseEvenOdd);

  enum_<LineCap>("TGFXLineCap")
      .value("Butt", LineCap::Butt)
      .value("Round", LineCap::Round)
      .value("Square", LineCap::Square);

  enum_<LineJoin>("TGFXLineJoin")
      .value("Miter", LineJoin::Miter)
      .value("Round", LineJoin::Round)
      .value("Bevel", LineJoin::Bevel);
}
