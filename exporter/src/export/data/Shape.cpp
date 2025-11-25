/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 Tencent. All rights reserved.
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
#include "export/stream/StreamProperty.h"
#include "export/stream/StreamValue.h"

namespace exporter {

static const std::unordered_map<std::string, pag::ShapeType> ShapeTypeMap = {
    {"ADBE Vector Group", pag::ShapeType::ShapeGroup},
    {"ADBE Vector Shape - Rect", pag::ShapeType::Rectangle},
    {"ADBE Vector Shape - Ellipse", pag::ShapeType::Ellipse},
    {"ADBE Vector Shape - Star", pag::ShapeType::PolyStar},
    {"ADBE Vector Shape - Group", pag::ShapeType::ShapePath},
    {"ADBE Vector Graphic - Fill", pag::ShapeType::Fill},
    {"ADBE Vector Graphic - Stroke", pag::ShapeType::Stroke},
    {"ADBE Vector Graphic - G-Fill", pag::ShapeType::GradientFill},
    {"ADBE Vector Graphic - G-Stroke", pag::ShapeType::GradientStroke},
    {"ADBE Vector Filter - Merge", pag::ShapeType::MergePaths},
    {"ADBE Vector Filter - Trim", pag::ShapeType::TrimPaths},
    {"ADBE Vector Filter - Repeater", pag::ShapeType::Repeater},
    {"ADBE Vector Filter - RC", pag::ShapeType::RoundCorners}};

static pag::ShapeType GetShapeType(const AEGP_StreamRefH& streamHandle) {
  std::string matchName = GetStreamMatchName(streamHandle);
  auto result = ShapeTypeMap.find(matchName);
  if (result == ShapeTypeMap.end()) {
    return pag::ShapeType::Unknown;
  }
  return result->second;
}

static pag::ShapeElement* GetShape(const AEGP_StreamRefH& streamHandle, int& gradientIndex,
                                   float frameRate);

static pag::ShapeElement* GetShapeGroup(const AEGP_StreamRefH& streamHandle, float frameRate) {
  const auto& Suites = GetSuites();
  const auto& PluginID = GetPluginID();
  auto element = new pag::ShapeGroupElement();

  QVariantMap map = {};
  map["frameRate"] = frameRate;

  element->blendMode =
      GetValue(streamHandle, "ADBE Vector Blend Mode", AEStreamParser::ShapeBlendModeParser);

  AEGP_StreamRefH transformStreamHandle = nullptr;
  Suites->DynamicStreamSuite4()->AEGP_GetNewStreamRefByMatchname(
      PluginID, streamHandle, "ADBE Vector Transform Group", &transformStreamHandle);
  auto transform = new pag::ShapeTransform();
  transform->anchorPoint =
      GetProperty(transformStreamHandle, "ADBE Vector Anchor", AEStreamParser::PointParser, map);
  transform->position =
      GetProperty(transformStreamHandle, "ADBE Vector Position", AEStreamParser::PointParser, map);
  transform->scale =
      GetProperty(transformStreamHandle, "ADBE Vector Scale", AEStreamParser::ScaleParser, map);
  transform->skew =
      GetProperty(transformStreamHandle, "ADBE Vector Skew", AEStreamParser::FloatParser, map);
  transform->skewAxis =
      GetProperty(transformStreamHandle, "ADBE Vector Skew Axis", AEStreamParser::FloatParser, map);
  transform->rotation =
      GetProperty(transformStreamHandle, "ADBE Vector Rotation", AEStreamParser::FloatParser, map);
  transform->opacity = GetProperty(transformStreamHandle, "ADBE Vector Group Opacity",
                                   AEStreamParser::Opacity0_100Parser, map);
  element->transform = transform;
  Suites->StreamSuite4()->AEGP_DisposeStream(transformStreamHandle);

  int gradientIndex = 0;
  AEGP_StreamRefH contents = nullptr;
  Suites->DynamicStreamSuite4()->AEGP_GetNewStreamRefByMatchname(PluginID, streamHandle,
                                                                 "ADBE Vectors Group", &contents);
  A_long numElements = 0;
  Suites->DynamicStreamSuite4()->AEGP_GetNumStreamsInGroup(contents, &numElements);
  for (A_long index = 0; index < numElements; index++) {
    AEGP_StreamRefH childStreamHandle = nullptr;
    Suites->DynamicStreamSuite4()->AEGP_GetNewStreamRefByIndex(PluginID, contents, index,
                                                               &childStreamHandle);
    auto shape = GetShape(childStreamHandle, gradientIndex, frameRate);
    if (shape != nullptr) {
      element->elements.push_back(shape);
    }
    Suites->StreamSuite4()->AEGP_DisposeStream(childStreamHandle);
  }
  Suites->StreamSuite4()->AEGP_DisposeStream(contents);
  return element;
}

static pag::ShapeElement* GetRectangle(const AEGP_StreamRefH& streamHandle, float frameRate) {
  auto element = new pag::RectangleElement();
  QVariantMap map = {};
  map["frameRate"] = frameRate;

  element->reversed = GetValue(streamHandle, "ADBE Vector Shape Direction",
                               AEStreamParser::ShapeDirectionReversedParser);
  element->size =
      GetProperty(streamHandle, "ADBE Vector Rect Size", AEStreamParser::PointParser, map, 2);
  element->position =
      GetProperty(streamHandle, "ADBE Vector Rect Position", AEStreamParser::PointParser, map);
  element->roundness =
      GetProperty(streamHandle, "ADBE Vector Rect Roundness", AEStreamParser::FloatParser, map);
  return element;
}

static pag::ShapeElement* GetEllipse(const AEGP_StreamRefH& streamHandle, float frameRate) {
  auto element = new pag::EllipseElement();
  QVariantMap map = {};
  map["frameRate"] = frameRate;

  element->reversed = GetValue(streamHandle, "ADBE Vector Shape Direction",
                               AEStreamParser::ShapeDirectionReversedParser);
  element->size =
      GetProperty(streamHandle, "ADBE Vector Ellipse Size", AEStreamParser::PointParser, map, 2);
  element->position =
      GetProperty(streamHandle, "ADBE Vector Ellipse Position", AEStreamParser::PointParser, map);
  return element;
}

static pag::ShapeElement* GetPolyStar(const AEGP_StreamRefH& streamHandle, float frameRate) {
  auto element = new pag::PolyStarElement();
  QVariantMap map = {};
  map["frameRate"] = frameRate;

  element->reversed = GetValue(streamHandle, "ADBE Vector Shape Direction",
                               AEStreamParser::ShapeDirectionReversedParser);
  element->polyType =
      GetValue(streamHandle, "ADBE Vector Star Type", AEStreamParser::PolyStarTypeParser);
  element->points =
      GetProperty(streamHandle, "ADBE Vector Star Points", AEStreamParser::FloatParser, map);
  element->position =
      GetProperty(streamHandle, "ADBE Vector Star Position", AEStreamParser::PointParser, map);
  element->rotation =
      GetProperty(streamHandle, "ADBE Vector Star Rotation", AEStreamParser::FloatParser, map);
  element->innerRadius =
      GetProperty(streamHandle, "ADBE Vector Star Inner Radius", AEStreamParser::FloatParser, map);
  element->outerRadius =
      GetProperty(streamHandle, "ADBE Vector Star Outer Radius", AEStreamParser::FloatParser, map);
  element->innerRoundness = GetProperty(streamHandle, "ADBE Vector Star Inner Roundess",
                                        AEStreamParser::PercentParser, map);
  element->outerRoundness = GetProperty(streamHandle, "ADBE Vector Star Outer Roundess",
                                        AEStreamParser::PercentParser, map);
  return element;
}

static pag::ShapeElement* GetShapePath(const AEGP_StreamRefH& streamHandle, float frameRate) {
  auto element = new pag::ShapePathElement();
  QVariantMap map = {};
  map["frameRate"] = frameRate;

  auto reversed = GetValue(streamHandle, "ADBE Vector Shape Direction",
                           AEStreamParser::ShapeDirectionReversedParser);
  element->shapePath =
      GetProperty(streamHandle, "ADBE Vector Shape", AEStreamParser::PathParser, map);
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

static pag::ShapeElement* GetFill(const AEGP_StreamRefH& streamHandle, float frameRate) {
  auto element = new pag::FillElement();
  QVariantMap map = {};
  map["frameRate"] = frameRate;

  element->blendMode =
      GetValue(streamHandle, "ADBE Vector Blend Mode", AEStreamParser::ShapeBlendModeParser);
  element->composite =
      GetValue(streamHandle, "ADBE Vector Composite Order", AEStreamParser::CompositeOrderParser);
  element->fillRule =
      GetValue(streamHandle, "ADBE Vector Fill Rule", AEStreamParser::FillRuleParser);
  element->color =
      GetProperty(streamHandle, "ADBE Vector Fill Color", AEStreamParser::ColorParser, map);
  element->opacity = GetProperty(streamHandle, "ADBE Vector Fill Opacity",
                                 AEStreamParser::Opacity0_100Parser, map);
  return element;
}

static void GetDashes(const AEGP_StreamRefH& streamHandle, pag::ShapeElement* element) {
  const auto& Suites = GetSuites();
  const auto& PluginID = GetPluginID();

  AEGP_StreamRefH dashStreamHandle = nullptr;
  Suites->DynamicStreamSuite4()->AEGP_GetNewStreamRefByMatchname(
      PluginID, streamHandle, "ADBE Vector Stroke Dashes", &dashStreamHandle);
  A_long numStreams = 0;
  Suites->DynamicStreamSuite4()->AEGP_GetNumStreamsInGroup(dashStreamHandle, &numStreams);
  if (numStreams > 0 && IsStreamActive(dashStreamHandle)) {
    pag::StrokeElement* strokeElement = nullptr;
    pag::GradientStrokeElement* gradientStrokeElement = nullptr;
    if (element->type() == pag::ShapeType::Stroke) {
      strokeElement = static_cast<pag::StrokeElement*>(element);
    } else {
      gradientStrokeElement = static_cast<pag::GradientStrokeElement*>(element);
    }
    auto& dashes = strokeElement != nullptr ? strokeElement->dashes : gradientStrokeElement->dashes;
    auto& dashOffset =
        strokeElement != nullptr ? strokeElement->dashOffset : gradientStrokeElement->dashOffset;
    for (A_long index = 0; index < numStreams; index++) {
      AEGP_StreamRefH childStreamHandle = nullptr;
      Suites->DynamicStreamSuite4()->AEGP_GetNewStreamRefByIndex(PluginID, dashStreamHandle, index,
                                                                 &childStreamHandle);
      if (!IsStreamHidden(childStreamHandle)) {
        auto dash = GetProperty(childStreamHandle, index, AEStreamParser::FloatParser);
        dashes.push_back(dash);
      }
      Suites->StreamSuite4()->AEGP_DisposeStream(childStreamHandle);
    }
    if (!dashes.empty()) {
      dashOffset = GetProperty(dashStreamHandle, numStreams - 1, AEStreamParser::FloatParser);
    }
  }
  Suites->StreamSuite4()->AEGP_DisposeStream(dashStreamHandle);
}

static pag::ShapeElement* GetStroke(const AEGP_StreamRefH& streamHandle, float frameRate) {
  auto element = new pag::StrokeElement();
  QVariantMap map = {};
  map["frameRate"] = frameRate;

  element->blendMode =
      GetValue(streamHandle, "ADBE Vector Blend Mode", AEStreamParser::ShapeBlendModeParser);
  element->composite =
      GetValue(streamHandle, "ADBE Vector Composite Order", AEStreamParser::CompositeOrderParser);
  element->color =
      GetProperty(streamHandle, "ADBE Vector Stroke Color", AEStreamParser::ColorParser, map);
  element->opacity = GetProperty(streamHandle, "ADBE Vector Stroke Opacity",
                                 AEStreamParser::Opacity0_100Parser, map);
  element->strokeWidth =
      GetProperty(streamHandle, "ADBE Vector Stroke Width", AEStreamParser::FloatParser, map);
  element->lineCap =
      GetValue(streamHandle, "ADBE Vector Stroke Line Cap", AEStreamParser::LineCapParser);
  element->lineJoin =
      GetValue(streamHandle, "ADBE Vector Stroke Line Join", AEStreamParser::LineJoinParser);
  element->miterLimit =
      GetProperty(streamHandle, "ADBE Vector Stroke Miter Limit", AEStreamParser::FloatParser, map);
  GetDashes(streamHandle, element);
  return element;
}

static pag::ShapeElement* GetGradientFill(const AEGP_StreamRefH& streamHandle, int& gradientIndex,
                                          float frameRate) {
  auto element = new pag::GradientFillElement();
  QVariantMap map = {};
  map["frameRate"] = frameRate;
  map["index"] = gradientIndex;

  element->blendMode =
      GetValue(streamHandle, "ADBE Vector Blend Mode", AEStreamParser::ShapeBlendModeParser);
  element->composite =
      GetValue(streamHandle, "ADBE Vector Composite Order", AEStreamParser::CompositeOrderParser);
  element->fillRule =
      GetValue(streamHandle, "ADBE Vector Fill Rule", AEStreamParser::FillRuleParser);
  element->fillType =
      GetValue(streamHandle, "ADBE Vector Grad Type", AEStreamParser::GradientFillTypeParser);
  element->startPoint =
      GetProperty(streamHandle, "ADBE Vector Grad Start Pt", AEStreamParser::PointParser, map);
  element->endPoint =
      GetProperty(streamHandle, "ADBE Vector Grad End Pt", AEStreamParser::PointParser, map);
  element->colors = GetProperty(streamHandle, "ADBE Vector Grad Colors",
                                AEStreamParser::GradientColorParser, map);

  element->opacity = GetProperty(streamHandle, "ADBE Vector Fill Opacity",
                                 AEStreamParser::Opacity0_100Parser, map);
  gradientIndex++;
  return element;
}

static pag::ShapeElement* GetGradientStroke(const AEGP_StreamRefH& streamHandle, int& gradientIndex,
                                            float frameRate) {
  auto element = new pag::GradientStrokeElement();
  QVariantMap map = {};
  map["frameRate"] = frameRate;

  element->blendMode =
      GetValue(streamHandle, "ADBE Vector Blend Mode", AEStreamParser::ShapeBlendModeParser);
  element->composite =
      GetValue(streamHandle, "ADBE Vector Composite Order", AEStreamParser::CompositeOrderParser);
  element->fillType =
      GetValue(streamHandle, "ADBE Vector Grad Type", AEStreamParser::GradientFillTypeParser);
  element->startPoint =
      GetProperty(streamHandle, "ADBE Vector Grad Start Pt", AEStreamParser::PointParser, map);
  element->endPoint =
      GetProperty(streamHandle, "ADBE Vector Grad End Pt", AEStreamParser::PointParser, map);
  element->colors = GetProperty(streamHandle, "ADBE Vector Grad Colors",
                                AEStreamParser::GradientColorParser, map);
  element->opacity = GetProperty(streamHandle, "ADBE Vector Stroke Opacity",
                                 AEStreamParser::Opacity0_100Parser, map);
  element->strokeWidth =
      GetProperty(streamHandle, "ADBE Vector Stroke Width", AEStreamParser::FloatParser, map);
  element->lineCap =
      GetValue(streamHandle, "ADBE Vector Stroke Line Cap", AEStreamParser::LineCapParser);
  element->lineJoin =
      GetValue(streamHandle, "ADBE Vector Stroke Line Join", AEStreamParser::LineJoinParser);
  element->miterLimit =
      GetProperty(streamHandle, "ADBE Vector Stroke Miter Limit", AEStreamParser::FloatParser, map);
  GetDashes(streamHandle, element);
  gradientIndex++;
  return element;
}

static pag::ShapeElement* GetMergePaths(const AEGP_StreamRefH& streamHandle) {
  auto element = new pag::MergePathsElement();
  element->mode =
      GetValue(streamHandle, "ADBE Vector Merge Type", AEStreamParser::MergePathsModeParser);
  return element;
}

static pag::ShapeElement* GetTrimPaths(const AEGP_StreamRefH& streamHandle, float frameRate) {
  auto element = new pag::TrimPathsElement();
  QVariantMap map = {};
  map["frameRate"] = frameRate;

  element->start =
      GetProperty(streamHandle, "ADBE Vector Trim Start", AEStreamParser::PercentParser, map);
  element->end =
      GetProperty(streamHandle, "ADBE Vector Trim End", AEStreamParser::PercentParser, map);
  element->offset =
      GetProperty(streamHandle, "ADBE Vector Trim Offset", AEStreamParser::FloatParser, map);
  element->trimType =
      GetValue(streamHandle, "ADBE Vector Trim Type", AEStreamParser::TrimPathsTypeParser);
  return element;
}

static pag::RepeaterTransform* GetRepeaterTransform(const AEGP_StreamRefH& streamHandle,
                                                    float frameRate) {
  auto transform = new pag::RepeaterTransform();
  QVariantMap map = {};
  map["frameRate"] = frameRate;

  transform->anchorPoint =
      GetProperty(streamHandle, "ADBE Vector Repeater Anchor", AEStreamParser::PointParser, map);
  transform->position =
      GetProperty(streamHandle, "ADBE Vector Repeater Position", AEStreamParser::PointParser, map);
  transform->scale =
      GetProperty(streamHandle, "ADBE Vector Repeater Scale", AEStreamParser::ScaleParser, map, 2);
  transform->rotation =
      GetProperty(streamHandle, "ADBE Vector Repeater Rotation", AEStreamParser::FloatParser, map);
  transform->startOpacity = GetProperty(streamHandle, "ADBE Vector Repeater Opacity 1",
                                        AEStreamParser::Opacity0_100Parser, map);
  transform->endOpacity = GetProperty(streamHandle, "ADBE Vector Repeater Opacity 2",
                                      AEStreamParser::Opacity0_100Parser, map);
  return transform;
}

static pag::ShapeElement* GetRepeater(const AEGP_StreamRefH& streamHandle, float frameRate) {
  const auto& Suites = GetSuites();
  const auto& PluginID = GetPluginID();
  auto element = new pag::RepeaterElement();
  QVariantMap map = {};
  map["frameRate"] = frameRate;

  element->copies =
      GetProperty(streamHandle, "ADBE Vector Repeater Copies", AEStreamParser::FloatParser, map);
  element->offset =
      GetProperty(streamHandle, "ADBE Vector Repeater Offset", AEStreamParser::FloatParser, map);
  element->composite =
      GetValue(streamHandle, "ADBE Vector Repeater Order", AEStreamParser::RepeaterOrderParser);
  AEGP_StreamRefH transform = nullptr;
  Suites->DynamicStreamSuite4()->AEGP_GetNewStreamRefByMatchname(
      PluginID, streamHandle, "ADBE Vector Repeater Transform", &transform);
  element->transform = GetRepeaterTransform(transform, frameRate);
  Suites->StreamSuite4()->AEGP_DisposeStream(transform);
  return element;
}

static pag::ShapeElement* GetRoundCorners(const AEGP_StreamRefH& streamHandle, float frameRate) {
  auto element = new pag::RoundCornersElement();
  QVariantMap map = {};
  map["frameRate"] = frameRate;

  element->radius =
      GetProperty(streamHandle, "ADBE Vector RoundCorner Radius", AEStreamParser::FloatParser, map);
  return element;
}

static pag::ShapeElement* GetShape(const AEGP_StreamRefH& streamHandle, int& gradientIndex,
                                   float frameRate) {
  if (!IsStreamActive(streamHandle)) {
    return nullptr;
  }

  pag::ShapeElement* element = nullptr;
  auto type = GetShapeType(streamHandle);
  switch (type) {
    case pag::ShapeType::ShapeGroup:
      element = GetShapeGroup(streamHandle, frameRate);
      break;
    case pag::ShapeType::Rectangle:
      element = GetRectangle(streamHandle, frameRate);
      break;
    case pag::ShapeType::Ellipse:
      element = GetEllipse(streamHandle, frameRate);
      break;
    case pag::ShapeType::PolyStar:
      element = GetPolyStar(streamHandle, frameRate);
      break;
    case pag::ShapeType::ShapePath:
      element = GetShapePath(streamHandle, frameRate);
      break;
    case pag::ShapeType::Fill:
      element = GetFill(streamHandle, frameRate);
      break;
    case pag::ShapeType::Stroke:
      element = GetStroke(streamHandle, frameRate);
      break;
    case pag::ShapeType::GradientFill:
      element = GetGradientFill(streamHandle, gradientIndex, frameRate);
      break;
    case pag::ShapeType::GradientStroke:
      element = GetGradientStroke(streamHandle, gradientIndex, frameRate);
      break;
    case pag::ShapeType::MergePaths:
      element = GetMergePaths(streamHandle);
      break;
    case pag::ShapeType::TrimPaths:
      element = GetTrimPaths(streamHandle, frameRate);
      break;
    case pag::ShapeType::Repeater:
      element = GetRepeater(streamHandle, frameRate);
      break;
    case pag::ShapeType::RoundCorners:
      element = GetRoundCorners(streamHandle, frameRate);
      break;
    default:
      break;
  }
  return element;
}

std::vector<pag::ShapeElement*> GetShapes(const AEGP_LayerH& layerHandle,
                                          std::shared_ptr<PAGExportSession> session) {
  const auto& Suites = GetSuites();
  const auto& PluginID = GetPluginID();
  std::vector<pag::ShapeElement*> contents = {};

  int gradientIndex = 0;
  AEGP_StreamRefH layerStreamHandle = nullptr;
  Suites->DynamicStreamSuite4()->AEGP_GetNewStreamRefForLayer(PluginID, layerHandle,
                                                              &layerStreamHandle);
  AEGP_StreamRefH rootStreamHandle = nullptr;
  Suites->DynamicStreamSuite4()->AEGP_GetNewStreamRefByMatchname(
      PluginID, layerStreamHandle, "ADBE Root Vectors Group", &rootStreamHandle);
  Suites->StreamSuite4()->AEGP_DisposeStream(layerStreamHandle);
  A_long numStreams = 0;
  Suites->DynamicStreamSuite4()->AEGP_GetNumStreamsInGroup(rootStreamHandle, &numStreams);
  for (A_long index = 0; index < numStreams; index++) {
    AEGP_StreamRefH streamHandle = nullptr;
    Suites->DynamicStreamSuite4()->AEGP_GetNewStreamRefByIndex(PluginID, rootStreamHandle, index,
                                                               &streamHandle);
    auto element = GetShape(streamHandle, gradientIndex, session->frameRate);
    if (element != nullptr) {
      contents.push_back(element);
    }
    Suites->StreamSuite4()->AEGP_DisposeStream(streamHandle);
  }
  Suites->StreamSuite4()->AEGP_DisposeStream(rootStreamHandle);
  return contents;
}

}  // namespace exporter