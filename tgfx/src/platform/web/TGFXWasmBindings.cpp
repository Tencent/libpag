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
#include <emscripten/val.h>
#include "tgfx/core/Color.h"
#include "tgfx/core/FontMetrics.h"
#include "tgfx/core/ImageInfo.h"
#include "tgfx/core/Mask.h"
#include "tgfx/core/Matrix.h"
#include "tgfx/core/Paint.h"
#include "tgfx/core/PathTypes.h"
#include "tgfx/core/Stroke.h"
#include "tgfx/gpu/Surface.h"
#include "tgfx/opengl/GLDefines.h"
#include "tgfx/opengl/webgl/WebGLWindow.h"
#include "tgfx/utils/UTF.h"

using namespace emscripten;
using namespace tgfx;

EMSCRIPTEN_BINDINGS(tgfx) {
// TGFX binding function
#ifdef TGFX_BIND_FOR_WEB
  class_<tgfx::Matrix>("_Matrix")
      .class_function("_MakeAll", Matrix::MakeAll)
      .class_function("_MakeScale", optional_override([](float sx, float sy) {
                        return Matrix::MakeScale(sx, sy);
                      }))
      .class_function("_MakeScale",
                      optional_override([](float scale) { return Matrix::MakeScale(scale); }))
      .class_function("_MakeTrans", Matrix::MakeTrans)
      .function("_get", &Matrix::get)
      .function("_set", &Matrix::set)
      .function("_setAll", &Matrix::setAll)
      .function("_setAffine", &Matrix::setAffine)
      .function("_reset", &Matrix::reset)
      .function("_setTranslate", &Matrix::setTranslate)
      .function("_setScale",
                optional_override([](Matrix& matrix, float sx, float sy, float px, float py) {
                  return matrix.setScale(sx, sy, px, py);
                }))
      .function("_setRotate",
                optional_override([](Matrix& matrix, float degrees, float px, float py) {
                  return matrix.setRotate(degrees, px, py);
                }))
      .function("_setSinCos",
                optional_override([](Matrix& matrix, float sinV, float cosV, float px, float py) {
                  return matrix.setSinCos(sinV, cosV, px, py);
                }))
      .function("_setSkew",
                optional_override([](Matrix& matrix, float kx, float ky, float px, float py) {
                  return matrix.setSkew(kx, ky, px, py);
                }))
      .function("_setConcat", &Matrix::setConcat)
      .function("_preTranslate", &Matrix::preTranslate)
      .function("_preScale",
                optional_override([](Matrix& matrix, float sx, float sy, float px, float py) {
                  return matrix.preScale(sx, sy, px, py);
                }))
      .function("_preRotate",
                optional_override([](Matrix& matrix, float degrees, float px, float py) {
                  return matrix.preRotate(degrees, px, py);
                }))
      .function("_preSkew",
                optional_override([](Matrix& matrix, float kx, float ky, float px, float py) {
                  return matrix.preSkew(kx, ky, px, py);
                }))
      .function("_preConcat", &Matrix::preConcat)
      .function("_postTranslate", &Matrix::postTranslate)
      .function("_postScale",
                optional_override([](Matrix& matrix, float sx, float sy, float px, float py) {
                  return matrix.postScale(sx, sy, px, py);
                }))
      .function("_postRotate",
                optional_override([](Matrix& matrix, float degrees, float px, float py) {
                  return matrix.postRotate(degrees, px, py);
                }))
      .function("_postSkew",
                optional_override([](Matrix& matrix, float kx, float ky, float px, float py) {
                  return matrix.postSkew(kx, ky, px, py);
                }))
      .function("_postConcat", &Matrix::postConcat);

  value_object<tgfx::Rect>("Rect")
      .field("left", &Rect::left)
      .field("top", &Rect::top)
      .field("right", &Rect::right)
      .field("bottom", &Rect::bottom);

  value_object<tgfx::Point>("Point").field("x", &Point::x).field("y", &Point::y);

  value_object<tgfx::Color>("Color")
      .field("red", &Color::red)
      .field("green", &Color::green)
      .field("blue", &Color::blue);
#else
  // PAG binding function
  class_<tgfx::Matrix>("TGFXMatrix")
      .function("_get", &tgfx::Matrix::get)
      .function("_set", &tgfx::Matrix::set);

  value_object<tgfx::Point>("TGFXPoint").field("x", &tgfx::Point::x).field("y", &tgfx::Point::y);

  value_object<tgfx::Rect>("TGFXRect")
      .field("left", &tgfx::Rect::left)
      .field("top", &tgfx::Rect::top)
      .field("right", &tgfx::Rect::right)
      .field("bottom", &tgfx::Rect::bottom);
#endif
  // Public binding function
  register_vector<int>("VectorInt");
  register_vector<tgfx::Point>("VectorTGFXPoint");

  register_vector<std::string>("VectorString");

  class_<tgfx::ImageInfo>("TGFXImageInfo")
      .property("width", &ImageInfo::width)
      .property("height", &ImageInfo::height)
      .property("rowBytes", &ImageInfo::rowBytes)
      .property("colorType", &ImageInfo::colorType);

  class_<tgfx::Stroke>("TGFXStroke")
      .property("width", &Stroke::width)
      .property("cap", &Stroke::cap)
      .property("join", &Stroke::join)
      .property("miterLimit", &Stroke::miterLimit);

  value_object<tgfx::FontMetrics>("TGFXFontMetrics")
      .field("ascent", &FontMetrics::ascent)
      .field("descent", &FontMetrics::descent)
      .field("xHeight", &FontMetrics::xHeight)
      .field("capHeight", &FontMetrics::capHeight);

  enum_<tgfx::PathFillType>("TGFXPathFillType")
      .value("Winding", PathFillType::Winding)
      .value("EvenOdd", PathFillType::EvenOdd)
      .value("InverseWinding", PathFillType::InverseWinding)
      .value("InverseEvenOdd", PathFillType::InverseEvenOdd);

  enum_<tgfx::LineCap>("TGFXLineCap")
      .value("Butt", LineCap::Butt)
      .value("Round", LineCap::Round)
      .value("Square", LineCap::Square);

  enum_<tgfx::LineJoin>("TGFXLineJoin")
      .value("Miter", LineJoin::Miter)
      .value("Round", LineJoin::Round)
      .value("Bevel", LineJoin::Bevel);
}
