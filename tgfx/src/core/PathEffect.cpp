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

#include "tgfx/core/PathEffect.h"
#include "core/PathRef.h"
#include "tgfx/core/Stroke.h"

namespace tgfx {
using namespace pk;

class PkPathEffect : public PathEffect {
 public:
  explicit PkPathEffect(sk_sp<SkPathEffect> effect) : pathEffect(std::move(effect)) {
  }

  bool applyTo(Path* path) const override {
    if (path == nullptr) {
      return false;
    }
    auto& skPath = PathRef::WriteAccess(*path);
    SkStrokeRec rec(SkStrokeRec::kHairline_InitStyle);
    return pathEffect->filterPath(&skPath, skPath, &rec, nullptr);
  }

 private:
  sk_sp<SkPathEffect> pathEffect = nullptr;
};

class StrokePathEffect : public PathEffect {
 public:
  explicit StrokePathEffect(SkPaint paint) : paint(std::move(paint)) {
  }

  bool applyTo(Path* path) const override {
    if (path == nullptr) {
      return false;
    }
    auto& skPath = PathRef::WriteAccess(*path);
    return paint.getFillPath(skPath, &skPath);
  }

 private:
  SkPaint paint = {};
};

static SkPaint::Cap ToSkLineCap(LineCap cap) {
  switch (cap) {
    case LineCap::Round:
      return SkPaint::kRound_Cap;
    case LineCap::Square:
      return SkPaint::kSquare_Cap;
    default:
      return SkPaint::kButt_Cap;
  }
}

static SkPaint::Join ToSkLineJoin(LineJoin join) {
  switch (join) {
    case LineJoin::Round:
      return SkPaint::kRound_Join;
    case LineJoin::Bevel:
      return SkPaint::kBevel_Join;
    default:
      return SkPaint::kMiter_Join;
  }
}

std::unique_ptr<PathEffect> PathEffect::MakeStroke(const Stroke& stroke) {
  if (stroke.width <= 0) {
    return nullptr;
  }
  SkPaint paint = {};
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(stroke.width);
  paint.setStrokeCap(ToSkLineCap(stroke.cap));
  paint.setStrokeJoin(ToSkLineJoin(stroke.join));
  paint.setStrokeMiter(stroke.miterLimit);
  return std::unique_ptr<PathEffect>(new StrokePathEffect(std::move(paint)));
}

std::unique_ptr<PathEffect> PathEffect::MakeDash(const float* intervals, int count, float phase) {
  auto effect = SkDashPathEffect::Make(intervals, count, phase);
  if (effect == nullptr) {
    return nullptr;
  }
  return std::unique_ptr<PathEffect>(new PkPathEffect(std::move(effect)));
}

std::unique_ptr<PathEffect> PathEffect::MakeCorner(float radius) {
  auto effect = SkCornerPathEffect::Make(radius);
  if (effect == nullptr) {
    return nullptr;
  }
  return std::unique_ptr<PathEffect>(new PkPathEffect(std::move(effect)));
}
}  // namespace tgfx