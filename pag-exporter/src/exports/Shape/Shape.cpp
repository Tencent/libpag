/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 THL A29 Limited, a Tencent company. All rights reserved.
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
#include "Shape.h"
#include <unordered_map>
#include "src/exports/Stream/StreamProperty.h"
#include "src/exports/Stream/StreamFlags.h"

static const std::unordered_map<std::string, pag::ShapeType> shapeTypeMap = {
    {"ADBE Vector Group",              pag::ShapeType::ShapeGroup},
    {"ADBE Vector Shape - Rect",       pag::ShapeType::Rectangle},
    {"ADBE Vector Shape - Ellipse",    pag::ShapeType::Ellipse},
    {"ADBE Vector Shape - Star",       pag::ShapeType::PolyStar},
    {"ADBE Vector Shape - Group",      pag::ShapeType::ShapePath},
    {"ADBE Vector Graphic - Fill",     pag::ShapeType::Fill},
    {"ADBE Vector Graphic - Stroke",   pag::ShapeType::Stroke},
    {"ADBE Vector Graphic - G-Fill",   pag::ShapeType::GradientFill},
    {"ADBE Vector Graphic - G-Stroke", pag::ShapeType::GradientStroke},
    {"ADBE Vector Filter - Merge",     pag::ShapeType::MergePaths},
    {"ADBE Vector Filter - Trim",      pag::ShapeType::TrimPaths},
    {"ADBE Vector Filter - Repeater",  pag::ShapeType::Repeater},
    {"ADBE Vector Filter - RC",        pag::ShapeType::RoundCorners}
};

static pag::ShapeType GetShapeType(pagexporter::Context* context, AEGP_StreamRefH elementStream) {
  auto& suites = context->suites;
  static char matchName[200];
  suites.DynamicStreamSuite4()->AEGP_GetMatchName(elementStream, matchName);
  auto result = shapeTypeMap.find(matchName);
  if (result == shapeTypeMap.end()) {
    return pag::ShapeType::Unknown;
  }
  return result->second;
}

static pag::ShapeTransform* ExportShapeTransform(pagexporter::Context* context, AEGP_StreamRefH elementStream) {
  auto element = new pag::ShapeTransform();
  element->anchorPoint = ExportProperty(context, elementStream, "ADBE Vector Anchor", Parsers::Point);
  element->position = ExportProperty(context, elementStream, "ADBE Vector Position", Parsers::Point);
  element->scale = ExportProperty(context, elementStream, "ADBE Vector Scale", Parsers::Scale, 2);
  element->skew = ExportProperty(context, elementStream, "ADBE Vector Skew", Parsers::Float);
  element->skewAxis = ExportProperty(context, elementStream, "ADBE Vector Skew Axis", Parsers::Float);
  element->rotation = ExportProperty(context, elementStream, "ADBE Vector Rotation", Parsers::Float);
  element->opacity = ExportProperty(context, elementStream, "ADBE Vector Group Opacity", Parsers::Opacity0_100);
  return element;
}

pag::ShapeElement* ExportShape(pagexporter::Context* context, AEGP_StreamRefH elementStream);

static pag::ShapeGroupElement* ExportShapeGroup(pagexporter::Context* context, AEGP_StreamRefH elementStream) {
  auto& suites = context->suites;
  auto element = new pag::ShapeGroupElement();
  element->blendMode = ExportValue(context, elementStream, "ADBE Vector Blend Mode", Parsers::ShapeBlendMode);
  AEGP_StreamRefH transform;
  suites.DynamicStreamSuite4()->AEGP_GetNewStreamRefByMatchname(context->pluginID, elementStream,
                                                                "ADBE Vector Transform Group", &transform);
  element->transform = ExportShapeTransform(context, transform);
  suites.StreamSuite4()->AEGP_DisposeStream(transform);
  AEGP_StreamRefH contents;
  suites.DynamicStreamSuite4()->AEGP_GetNewStreamRefByMatchname(context->pluginID, elementStream,
                                                                "ADBE Vectors Group", &contents);
  A_long numElements;
  suites.DynamicStreamSuite4()->AEGP_GetNumStreamsInGroup(contents, &numElements);
  for (int i = 0; i < numElements; i++) {
    AEGP_StreamRefH stream;
    suites.DynamicStreamSuite4()->AEGP_GetNewStreamRefByIndex(context->pluginID, contents, i, &stream);
    auto shape = ExportShape(context, stream);
    if (shape != nullptr) {
      element->elements.push_back(shape);
    }
    suites.StreamSuite4()->AEGP_DisposeStream(stream);
  }
  suites.StreamSuite4()->AEGP_DisposeStream(contents);
  return element;
}

static pag::RectangleElement* ExportRectangle(pagexporter::Context* context, AEGP_StreamRefH elementStream) {
  auto element = new pag::RectangleElement();
  element->reversed = ExportValue(context, elementStream, "ADBE Vector Shape Direction", Parsers::ShapeDirection);
  element->size = ExportProperty(context, elementStream, "ADBE Vector Rect Size", Parsers::Point, 2);
  element->position = ExportProperty(context, elementStream, "ADBE Vector Rect Position", Parsers::Point);
  element->roundness = ExportProperty(context, elementStream, "ADBE Vector Rect Roundness", Parsers::Float);
  return element;
}

static pag::EllipseElement* ExportEllipse(pagexporter::Context* context, AEGP_StreamRefH elementStream) {
  auto element = new pag::EllipseElement();
  element->reversed = ExportValue(context, elementStream, "ADBE Vector Shape Direction", Parsers::ShapeDirection);
  element->size = ExportProperty(context, elementStream, "ADBE Vector Ellipse Size", Parsers::Point, 2);
  element->position = ExportProperty(context, elementStream, "ADBE Vector Ellipse Position", Parsers::Point);
  return element;
}

static pag::PolyStarElement* ExportPolyStar(pagexporter::Context* context, AEGP_StreamRefH elementStream) {
  auto element = new pag::PolyStarElement();
  element->reversed = ExportValue(context, elementStream, "ADBE Vector Shape Direction", Parsers::ShapeDirection);
  element->polyType = ExportValue(context, elementStream, "ADBE Vector Star Type", Parsers::PolyStarType);
  element->points = ExportProperty(context, elementStream, "ADBE Vector Star Points", Parsers::Float);
  element->position = ExportProperty(context, elementStream, "ADBE Vector Star Position", Parsers::Point);
  element->rotation = ExportProperty(context, elementStream, "ADBE Vector Star Rotation", Parsers::Float);
  element->innerRadius = ExportProperty(context, elementStream, "ADBE Vector Star Inner Radius", Parsers::Float);
  element->outerRadius = ExportProperty(context, elementStream, "ADBE Vector Star Outer Radius", Parsers::Float);
  element->innerRoundness = ExportProperty(context, elementStream, "ADBE Vector Star Inner Roundess",
                                           Parsers::Percent);
  element->outerRoundness = ExportProperty(context, elementStream, "ADBE Vector Star Outer Roundess",
                                           Parsers::Percent);
  return element;
}

static pag::ShapePathElement* ExportShapePath(pagexporter::Context* context, AEGP_StreamRefH elementStream) {
  auto element = new pag::ShapePathElement();
  auto reversed = ExportValue(context, elementStream, "ADBE Vector Shape Direction", Parsers::ShapeDirection);
  element->shapePath = ExportProperty(context, elementStream, "ADBE Vector Shape", Parsers::Path);
  if (reversed) {
    if (element->shapePath->animatable()) {
      auto property = static_cast<pag::AnimatableProperty<pag::PathHandle>*>(element->shapePath);
      for (auto& keyframe : property->keyframes) {
        keyframe->startValue->reverse();
        keyframe->endValue->reverse();
      }
    } else {
      element->shapePath->value->reverse();
    }
  }
  return element;
}

static pag::FillElement* ExportFill(pagexporter::Context* context, AEGP_StreamRefH elementStream) {
  auto element = new pag::FillElement();
  element->blendMode = ExportValue(context, elementStream, "ADBE Vector Blend Mode", Parsers::ShapeBlendMode);
  element->composite = ExportValue(context, elementStream, "ADBE Vector Composite Order", Parsers::CompositeOrder);
  element->fillRule = ExportValue(context, elementStream, "ADBE Vector Fill Rule", Parsers::FillRule);
  element->color = ExportProperty(context, elementStream, "ADBE Vector Fill Color", Parsers::Color);
  element->opacity = ExportProperty(context, elementStream, "ADBE Vector Fill Opacity", Parsers::Opacity0_100);
  return element;
}

static void ExportDashes(pagexporter::Context* context, AEGP_StreamRefH elementStream, pag::ShapeElement* shape) {
  auto& suites = context->suites;
  AEGP_StreamRefH dashStream;
  suites.DynamicStreamSuite4()->AEGP_GetNewStreamRefByMatchname(context->pluginID, elementStream,
                                                                "ADBE Vector Stroke Dashes", &dashStream);
  A_long numDashes;
  suites.DynamicStreamSuite4()->AEGP_GetNumStreamsInGroup(dashStream, &numDashes);
  if (numDashes > 0 && IsStreamActive(suites, dashStream)) {
    auto stroke = shape->type() == pag::ShapeType::Stroke ? static_cast<pag::StrokeElement*>(shape) : nullptr;
    auto gradientStroke =
        shape->type() == pag::ShapeType::GradientStroke ? static_cast<pag::GradientStrokeElement*>(shape) : nullptr;
    auto& dashes = stroke != nullptr ? stroke->dashes : gradientStroke->dashes;
    for (int i = 0; i < numDashes - 1; i++) {
      AEGP_StreamRefH stream;
      suites.DynamicStreamSuite4()->AEGP_GetNewStreamRefByIndex(context->pluginID, dashStream, i, &stream);
      auto hidden = IsStreamHidden(suites, stream);
      suites.StreamSuite4()->AEGP_DisposeStream(stream);
      if (hidden) {
        break;
      }
      auto dash = ExportProperty(context, dashStream, i, Parsers::Float);
      dashes.push_back(dash);

    }
    if (!dashes.empty()) {
      auto dashOffset = ExportProperty(context, dashStream, numDashes - 1, Parsers::Float);
      if (stroke != nullptr) {
        stroke->dashOffset = dashOffset;
      } else {
        gradientStroke->dashOffset = dashOffset;
      }
    }
  }
  suites.StreamSuite4()->AEGP_DisposeStream(dashStream);
}

static pag::StrokeElement* ExportStroke(pagexporter::Context* context, AEGP_StreamRefH elementStream) {
  auto element = new pag::StrokeElement();
  element->blendMode = ExportValue(context, elementStream, "ADBE Vector Blend Mode", Parsers::ShapeBlendMode);
  element->composite = ExportValue(context, elementStream, "ADBE Vector Composite Order", Parsers::CompositeOrder);
  element->color = ExportProperty(context, elementStream, "ADBE Vector Stroke Color", Parsers::Color);
  element->opacity = ExportProperty(context, elementStream, "ADBE Vector Stroke Opacity", Parsers::Opacity0_100);
  element->strokeWidth = ExportProperty(context, elementStream, "ADBE Vector Stroke Width", Parsers::Float);
  element->lineCap = ExportValue(context, elementStream, "ADBE Vector Stroke Line Cap", Parsers::LineCap);
  element->lineJoin = ExportValue(context, elementStream, "ADBE Vector Stroke Line Join", Parsers::LineJoin);
  element->miterLimit = ExportProperty(context, elementStream, "ADBE Vector Stroke Miter Limit", Parsers::Float);
  ExportDashes(context, elementStream, element);
  return element;
}

static pag::GradientFillElement* ExportGradientFill(pagexporter::Context* context, AEGP_StreamRefH elementStream) {
  auto element = new pag::GradientFillElement();
  element->blendMode = ExportValue(context, elementStream, "ADBE Vector Blend Mode", Parsers::ShapeBlendMode);
  element->composite = ExportValue(context, elementStream, "ADBE Vector Composite Order", Parsers::CompositeOrder);
  element->fillRule = ExportValue(context, elementStream, "ADBE Vector Fill Rule", Parsers::FillRule);
  element->fillType = ExportValue(context, elementStream, "ADBE Vector Grad Type", Parsers::GradientFillType);
  element->startPoint = ExportProperty(context, elementStream, "ADBE Vector Grad Start Pt", Parsers::Point);
  element->endPoint = ExportProperty(context, elementStream, "ADBE Vector Grad End Pt", Parsers::Point);
  element->colors = ExportProperty(context, elementStream, "ADBE Vector Grad Colors", Parsers::GradientColor);
  element->opacity = ExportProperty(context, elementStream, "ADBE Vector Fill Opacity", Parsers::Opacity0_100);
  context->gradientIndex++;
  return element;
}

static pag::GradientStrokeElement* ExportGradientStroke(pagexporter::Context* context, AEGP_StreamRefH elementStream) {
  auto element = new pag::GradientStrokeElement();
  element->blendMode = ExportValue(context, elementStream, "ADBE Vector Blend Mode", Parsers::ShapeBlendMode);
  element->composite = ExportValue(context, elementStream, "ADBE Vector Composite Order", Parsers::CompositeOrder);
  element->fillType = ExportValue(context, elementStream, "ADBE Vector Grad Type", Parsers::GradientFillType);
  element->startPoint = ExportProperty(context, elementStream, "ADBE Vector Grad Start Pt", Parsers::Point);
  element->endPoint = ExportProperty(context, elementStream, "ADBE Vector Grad End Pt", Parsers::Point);
  element->colors = ExportProperty(context, elementStream, "ADBE Vector Grad Colors", Parsers::GradientColor);
  element->opacity = ExportProperty(context, elementStream, "ADBE Vector Stroke Opacity", Parsers::Opacity0_100);
  element->strokeWidth = ExportProperty(context, elementStream, "ADBE Vector Stroke Width", Parsers::Float);
  element->lineCap = ExportValue(context, elementStream, "ADBE Vector Stroke Line Cap", Parsers::LineCap);
  element->lineJoin = ExportValue(context, elementStream, "ADBE Vector Stroke Line Join", Parsers::LineJoin);
  element->miterLimit = ExportProperty(context, elementStream, "ADBE Vector Stroke Miter Limit", Parsers::Float);
  ExportDashes(context, elementStream, element);
  context->gradientIndex++;
  return element;
}

static pag::MergePathsElement* ExportMergePaths(pagexporter::Context* context, AEGP_StreamRefH elementStream) {
  auto element = new pag::MergePathsElement();
  element->mode = ExportValue(context, elementStream, "ADBE Vector Merge Type", Parsers::MergePathsMode);
  return element;
}

static pag::TrimPathsElement* ExportTrimPaths(pagexporter::Context* context, AEGP_StreamRefH elementStream) {
  auto element = new pag::TrimPathsElement();
  element->start = ExportProperty(context, elementStream, "ADBE Vector Trim Start", Parsers::Percent);
  element->end = ExportProperty(context, elementStream, "ADBE Vector Trim End", Parsers::Percent);
  element->offset = ExportProperty(context, elementStream, "ADBE Vector Trim Offset", Parsers::Float);
  element->trimType = ExportValue(context, elementStream, "ADBE Vector Trim Type", Parsers::TrimType);
  return element;
}

static pag::RepeaterTransform* ExportRepeaterTransform(pagexporter::Context* context, AEGP_StreamRefH elementStream) {
  auto transform = new pag::RepeaterTransform();
  transform->anchorPoint = ExportProperty(context, elementStream, "ADBE Vector Repeater Anchor", Parsers::Point);
  transform->position = ExportProperty(context, elementStream, "ADBE Vector Repeater Position", Parsers::Point);
  transform->scale = ExportProperty(context, elementStream, "ADBE Vector Repeater Scale", Parsers::Scale, 2);
  transform->rotation = ExportProperty(context, elementStream, "ADBE Vector Repeater Rotation", Parsers::Float);
  transform->startOpacity = ExportProperty(context, elementStream, "ADBE Vector Repeater Opacity 1",
                                           Parsers::Opacity0_100);
  transform->endOpacity = ExportProperty(context, elementStream, "ADBE Vector Repeater Opacity 2",
                                         Parsers::Opacity0_100);
  return transform;
}

static pag::RepeaterElement* ExportRepeater(pagexporter::Context* context, AEGP_StreamRefH elementStream) {
  auto& suites = context->suites;
  auto element = new pag::RepeaterElement();
  element->copies = ExportProperty(context, elementStream, "ADBE Vector Repeater Copies", Parsers::Float);
  element->offset = ExportProperty(context, elementStream, "ADBE Vector Repeater Offset", Parsers::Float);
  element->composite = ExportValue(context, elementStream, "ADBE Vector Repeater Order", Parsers::RepeaterOrder);
  AEGP_StreamRefH transform;
  suites.DynamicStreamSuite4()->AEGP_GetNewStreamRefByMatchname(context->pluginID, elementStream,
                                                                "ADBE Vector Repeater Transform", &transform);
  element->transform = ExportRepeaterTransform(context, transform);
  suites.StreamSuite4()->AEGP_DisposeStream(transform);
  return element;
}

static pag::RoundCornersElement* ExportRoundCorners(pagexporter::Context* context, AEGP_StreamRefH elementStream) {
  auto element = new pag::RoundCornersElement();
  element->radius = ExportProperty(context, elementStream, "ADBE Vector RoundCorner Radius", Parsers::Float);
  return element;
}

pag::ShapeElement* ExportShape(pagexporter::Context* context, AEGP_StreamRefH elementStream) {
  auto& suites = context->suites;
  auto type = GetShapeType(context, elementStream);
  pag::ShapeElement* element;
  switch (type) {
    case pag::ShapeType::ShapeGroup:
      element = ExportShapeGroup(context, elementStream);
      break;
    case pag::ShapeType::Rectangle:
      element = ExportRectangle(context, elementStream);
      break;
    case pag::ShapeType::Ellipse:
      element = ExportEllipse(context, elementStream);
      break;
    case pag::ShapeType::PolyStar:
      element = ExportPolyStar(context, elementStream);
      break;
    case pag::ShapeType::ShapePath:
      element = ExportShapePath(context, elementStream);
      break;
    case pag::ShapeType::Fill:
      element = ExportFill(context, elementStream);
      break;
    case pag::ShapeType::Stroke:
      element = ExportStroke(context, elementStream);
      break;
    case pag::ShapeType::GradientFill:
      element = ExportGradientFill(context, elementStream);
      break;
    case pag::ShapeType::GradientStroke:
      element = ExportGradientStroke(context, elementStream);
      break;
    case pag::ShapeType::MergePaths:
      element = ExportMergePaths(context, elementStream);
      break;
    case pag::ShapeType::TrimPaths:
      element = ExportTrimPaths(context, elementStream);
      break;
    case pag::ShapeType::Repeater:
      element = ExportRepeater(context, elementStream);
      break;
    case pag::ShapeType::RoundCorners:
      element = ExportRoundCorners(context, elementStream);
      break;
    default:
      element = nullptr;
  }
  // We don't check this at the beginning because we need to update the gradientIndex in context.
  if (!IsStreamActive(suites, elementStream)) {
    delete element;
    return nullptr;
  }
  return element;
}

std::vector<pag::ShapeElement*> ExportShapes(pagexporter::Context* context, const AEGP_LayerH& layerHandle) {
  context->gradientIndex = 0;
  auto& suites = context->suites;
  std::vector<pag::ShapeElement*> contents;
  AEGP_StreamRefH layerStream;
  suites.DynamicStreamSuite4()->AEGP_GetNewStreamRefForLayer(context->pluginID, layerHandle, &layerStream);
  AEGP_StreamRefH rootStream;
  suites.DynamicStreamSuite4()->AEGP_GetNewStreamRefByMatchname(context->pluginID, layerStream,
                                                                "ADBE Root Vectors Group", &rootStream);
  suites.StreamSuite4()->AEGP_DisposeStream(layerStream);
  A_long numElements = 0;
  suites.DynamicStreamSuite4()->AEGP_GetNumStreamsInGroup(rootStream, &numElements);
  for (int i = 0; i < numElements; i++) {
    AEGP_StreamRefH stream;
    suites.DynamicStreamSuite4()->AEGP_GetNewStreamRefByIndex(context->pluginID, rootStream, i, &stream);
    auto element = ExportShape(context, stream);
    if (element != nullptr) {
      contents.push_back(element);
    }
    suites.StreamSuite4()->AEGP_DisposeStream(stream);
  }
  suites.StreamSuite4()->AEGP_DisposeStream(rootStream);
  return contents;
}
