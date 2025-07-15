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

#include "ShapeTag.h"
#include "base/utils/EnumClassHash.h"
#include "codec/tags/shapes/ShapePath.h"
#include "shapes/Ellipse.h"
#include "shapes/Fill.h"
#include "shapes/Gradient.h"
#include "shapes/MergePaths.h"
#include "shapes/PolyStar.h"
#include "shapes/Rectangle.h"
#include "shapes/Repeater.h"
#include "shapes/RoundCorners.h"
#include "shapes/ShapeGroup.h"
#include "shapes/Stroke.h"
#include "shapes/TrimPaths.h"

namespace pag {
using ReadShapeHandler = ShapeElement*(DecodeStream* stream);

template <class T>
static std::function<ReadShapeHandler> MakeReadShapeHandler(
    std::unique_ptr<BlockConfig> (*ConfigMaker)(T*)) {
  return [ConfigMaker](DecodeStream* stream) { return ReadTagBlock(stream, ConfigMaker); };
}

static const std::unordered_map<TagCode, std::function<ReadShapeHandler>, EnumClassHash>
    readShapeHandlers = {
        {TagCode::ShapeGroup, MakeReadShapeHandler<ShapeGroupElement>(ShapeGroupTag)},
        {TagCode::Rectangle, MakeReadShapeHandler<RectangleElement>(RectangleTag)},
        {TagCode::Ellipse, MakeReadShapeHandler<EllipseElement>(EllipseTag)},
        {TagCode::PolyStar, MakeReadShapeHandler<PolyStarElement>(PolyStarTag)},
        {TagCode::ShapePath, MakeReadShapeHandler<ShapePathElement>(ShapePathTag)},
        {TagCode::Fill, MakeReadShapeHandler<FillElement>(FillTag)},
        {TagCode::Stroke, MakeReadShapeHandler<StrokeElement>(StrokeTag)},
        {TagCode::GradientFill, MakeReadShapeHandler<GradientFillElement>(GradientFillTag)},
        {TagCode::GradientStroke, MakeReadShapeHandler<GradientStrokeElement>(GradientStrokeTag)},
        {TagCode::MergePaths, MakeReadShapeHandler<MergePathsElement>(MergePathsTag)},
        {TagCode::TrimPaths, MakeReadShapeHandler<TrimPathsElement>(TrimPathsTag)},
        {TagCode::Repeater, MakeReadShapeHandler<RepeaterElement>(RepeaterTag)},
        {TagCode::RoundCorners, MakeReadShapeHandler<RoundCornersElement>(RoundCornersTag)},
};

bool ReadShape(DecodeStream* stream, TagCode code, std::vector<ShapeElement*>* contents) {
  auto iter = readShapeHandlers.find(code);
  if (iter == readShapeHandlers.end()) {
    return false;
  }
  auto shape = iter->second(stream);
  if (shape == nullptr) {
    return false;
  }
  contents->push_back(shape);
  return true;
}

using WriteShapeHandler = void(EncodeStream* stream, ShapeElement* shape);

template <class T>
static std::function<WriteShapeHandler> MakeWriteShapeHandler(
    std::unique_ptr<BlockConfig> (*ConfigMaker)(T*)) {
  return [ConfigMaker](EncodeStream* stream, ShapeElement* shape) {
    WriteTagBlock(stream, static_cast<T*>(shape), ConfigMaker);
  };
}

static const std::unordered_map<ShapeType, std::function<WriteShapeHandler>, EnumClassHash>
    writeShapeHandlers = {
        {ShapeType::ShapeGroup, MakeWriteShapeHandler<ShapeGroupElement>(ShapeGroupTag)},
        {ShapeType::Rectangle, MakeWriteShapeHandler<RectangleElement>(RectangleTag)},
        {ShapeType::Ellipse, MakeWriteShapeHandler<EllipseElement>(EllipseTag)},
        {ShapeType::PolyStar, MakeWriteShapeHandler<PolyStarElement>(PolyStarTag)},
        {ShapeType::ShapePath, MakeWriteShapeHandler<ShapePathElement>(ShapePathTag)},
        {ShapeType::Fill, MakeWriteShapeHandler<FillElement>(FillTag)},
        {ShapeType::Stroke, MakeWriteShapeHandler<StrokeElement>(StrokeTag)},
        {ShapeType::GradientFill, MakeWriteShapeHandler<GradientFillElement>(GradientFillTag)},
        {ShapeType::GradientStroke,
         MakeWriteShapeHandler<GradientStrokeElement>(GradientStrokeTag)},
        {ShapeType::MergePaths, MakeWriteShapeHandler<MergePathsElement>(MergePathsTag)},
        {ShapeType::TrimPaths, MakeWriteShapeHandler<TrimPathsElement>(TrimPathsTag)},
        {ShapeType::Repeater, MakeWriteShapeHandler<RepeaterElement>(RepeaterTag)},
        {ShapeType::RoundCorners, MakeWriteShapeHandler<RoundCornersElement>(RoundCornersTag)},
};

void WriteShape(EncodeStream* stream, const std::vector<ShapeElement*>* contents) {
  auto handleShape = [stream](ShapeElement* shape) {
    auto iter = writeShapeHandlers.find(shape->type());
    if (iter != writeShapeHandlers.end()) {
      iter->second(stream, shape);
    }
  };
  std::for_each(contents->begin(), contents->end(), handleShape);
}
}  // namespace pag
