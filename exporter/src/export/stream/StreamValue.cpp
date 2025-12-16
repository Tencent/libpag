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

#include "StreamValue.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include "utils/AEDataTypeConverter.h"
#include "utils/FileHelper.h"
#include "utils/PAGExportSession.h"
#include "utils/PAGExportSessionManager.h"

namespace exporter {

static pag::Frame ParseTime(const AEGP_StreamVal2& streamValue, const QVariantMap& map) {
  float frameRate = map.value("frameRate", 24.0f).toFloat();
  return static_cast<pag::Frame>(round(streamValue.one_d * frameRate));
}

static float ParseFloat(const AEGP_StreamVal2& streamValue, const QVariantMap&) {
  return static_cast<float>(streamValue.one_d);
}

static float ParseFloatM255(const AEGP_StreamVal2& streamValue, const QVariantMap&) {
  return static_cast<float>(streamValue.one_d) * 255.0f;
}

static float ParsePercent(const AEGP_StreamVal2& streamValue, const QVariantMap&) {
  return static_cast<float>(streamValue.one_d) / 100.0f;
}

static float ParsePercentD255(const AEGP_StreamVal2& streamValue, const QVariantMap&) {
  return static_cast<float>(streamValue.one_d) / 255.0f;
}

static bool ParseBoolean(const AEGP_StreamVal2& streamValue, const QVariantMap&) {
  return static_cast<bool>(streamValue.one_d);
}

static pag::ID ParseLayerID(const AEGP_StreamVal2& streamValue, const QVariantMap&) {
  return static_cast<pag::ID>(streamValue.layer_id);
}

static pag::ID ParseMaskID(const AEGP_StreamVal2& streamValue, const QVariantMap&) {
  return static_cast<pag::ID>(streamValue.mask_id);
}

static uint8_t ParseUint8(const AEGP_StreamVal2& streamValue, const QVariantMap&) {
  return static_cast<uint8_t>(round(streamValue.one_d));
}

static uint16_t ParseUint16(const AEGP_StreamVal2& streamValue, const QVariantMap&) {
  return static_cast<uint16_t>(round(streamValue.one_d));
}

static int32_t ParseInt32(const AEGP_StreamVal2& streamValue, const QVariantMap&) {
  return static_cast<int32_t>(round(streamValue.one_d));
}

static pag::Opacity ParseOpacity0_100(const AEGP_StreamVal2& streamValue, const QVariantMap&) {
  return static_cast<pag::Opacity>(round(streamValue.one_d * 2.55));
}

static pag::Opacity ParseOpacity0_255(const AEGP_StreamVal2& streamValue, const QVariantMap&) {
  return static_cast<pag::Opacity>(round(streamValue.one_d));
}

static pag::Opacity ParseOpacity0_1(const AEGP_StreamVal2& streamValue, const QVariantMap&) {
  return static_cast<pag::Opacity>(round(streamValue.one_d * 255));
}

static pag::Point ParsePoint(const AEGP_StreamVal2& streamValue, const QVariantMap&) {
  pag::Point point = {};
  point.x = static_cast<float>(streamValue.two_d.x);
  point.y = static_cast<float>(streamValue.two_d.y);
  return point;
}

static pag::Point3D ParsePoint3D(const AEGP_StreamVal2& streamValue, const QVariantMap&) {
  pag::Point3D point = {};
  point.x = static_cast<float>(streamValue.three_d.x);
  point.y = static_cast<float>(streamValue.three_d.y);
  point.z = static_cast<float>(streamValue.three_d.z);
  return point;
}

static pag::Point ParseScale(const AEGP_StreamVal2& streamValue, const QVariantMap&) {
  pag::Point point = {};
  point.x = static_cast<float>(streamValue.two_d.x * 0.01f);
  point.y = static_cast<float>(streamValue.two_d.y * 0.01f);
  return point;
}

static pag::Point3D ParseScale3D(const AEGP_StreamVal2& streamValue, const QVariantMap&) {
  pag::Point3D point = {};
  point.x = static_cast<float>(streamValue.three_d.x * 0.01f);
  point.y = static_cast<float>(streamValue.three_d.y * 0.01f);
  point.z = static_cast<float>(streamValue.three_d.z * 0.01f);
  return point;
}

static pag::Color ParseColor(const AEGP_StreamVal2& streamValue, const QVariantMap&) {
  pag::Color color = {};
  color.red = static_cast<uint8_t>(lround(streamValue.color.redF * 255));
  color.green = static_cast<uint8_t>(lround(streamValue.color.greenF * 255));
  color.blue = static_cast<uint8_t>(lround(streamValue.color.blueF * 255));
  return color;
}

static pag::PathHandle ParsePath(const AEGP_StreamVal2& streamValue, const QVariantMap&) {
  auto stream = streamValue.mask;
  auto path = new pag::PathData();
  A_long numSegment = 0;
  GetSuites()->MaskOutlineSuite3()->AEGP_GetMaskOutlineNumSegments(stream, &numSegment);
  if (numSegment == 0) {
    return pag::PathHandle(path);
  }

  AEGP_MaskVertex lastVertex = {};
  for (int index = 0; index <= numSegment; index++) {
    AEGP_MaskVertex vertex = {};
    GetSuites()->MaskOutlineSuite3()->AEGP_GetMaskOutlineVertexInfo(stream, index, &vertex);
    auto x = static_cast<float>(vertex.x);
    auto y = static_cast<float>(vertex.y);
    if (index == 0) {
      path->moveTo(x, y);
    } else {
      if (lastVertex.tan_out_x != 0 || lastVertex.tan_out_y != 0 || vertex.tan_in_x != 0 ||
          vertex.tan_in_y != 0) {
        auto controlX1 = static_cast<float>(lastVertex.x + lastVertex.tan_out_x);
        auto controlY1 = static_cast<float>(lastVertex.y + lastVertex.tan_out_y);
        auto controlX2 = static_cast<float>(vertex.tan_in_x + vertex.x);
        auto controlY2 = static_cast<float>(vertex.tan_in_y + vertex.y);
        path->cubicTo(controlX1, controlY1, controlX2, controlY2, x, y);
      } else {
        path->lineTo(x, y);
      }
    }
    lastVertex = vertex;
  }

  A_Boolean isOpen = 0;
  GetSuites()->MaskOutlineSuite3()->AEGP_IsMaskOutlineOpen(stream, &isOpen);
  if (isOpen == 0) {
    path->verbs.push_back(pag::PathDataVerb::Close);
  }
  return pag::PathHandle(path);
}

static pag::Point ArraryToPoint(const QJsonArray& array, const pag::Point& defaultValue) {
  if (array.size() != 2) {
    return defaultValue;
  }
  return pag::Point::Make(array[0].toString().toFloat(), array[1].toString().toFloat());
}

static pag::Color ArrayToColor(const QJsonArray& array, const pag::Color& defaultValue) {
  if (array.size() != 3) {
    return defaultValue;
  }
  pag::Color color = {};
  color.red = static_cast<uint8_t>(array[0].toString().toDouble() * 255);
  color.green = static_cast<uint8_t>(array[1].toString().toDouble() * 255);
  color.blue = static_cast<uint8_t>(array[2].toString().toDouble() * 255);
  return color;
}

static pag::TextDocumentHandle ParseTextDocument(const AEGP_StreamVal2&, const QVariantMap& map) {
  bool runJavaScript = map.value("runJavaScript", false).toBool();
  std::string compID = map.value("compID").toString().toStdString();
  std::string layerIndex = map.value("layerIndex").toString().toStdString();
  std::string keyFrame = map.value("keyFrame").toString().toStdString();
  std::string outPath = map.value("outPath", "").toString().toStdString();
  if (!runJavaScript || compID.empty() || layerIndex.empty() || keyFrame.empty()) {
    return std::make_shared<pag::TextDocument>();
  }

  RegisterTextDocumentScript();
  std::string code =
      "PAG.printTextDocuments(" + compID + ", " + layerIndex + ", " + keyFrame + ");";
  std::string result = RunScript(code);
  if (result.empty()) {
    return std::make_shared<pag::TextDocument>();
  }

  QJsonDocument doc = QJsonDocument::fromJson(result.data());
  if (doc.isNull()) {
    return std::make_shared<pag::TextDocument>();
  }
  QJsonObject obj = doc.object();

  auto textDocument = std::make_shared<pag::TextDocument>();

  /* 13212: Horizontal，see function 'AETextDirectionToTextDirection' for detail */
  textDocument->direction =
      AETextDirectionToTextDirection(obj.value("lineOrientation").toInt(13212));
  textDocument->applyFill = obj.value("applyFill").toBool(textDocument->applyFill);
  textDocument->applyStroke = obj.value("applyStroke").toBool(textDocument->applyStroke);
  textDocument->baselineShift =
      static_cast<float>(obj.value("baselineShift").toDouble(textDocument->baselineShift));
  textDocument->boxText = obj.value("boxText").toBool(textDocument->boxText);
  textDocument->boxTextPos =
      ArraryToPoint(obj.value("boxTextPos").toArray(), textDocument->boxTextPos);
  textDocument->boxTextSize =
      ArraryToPoint(obj.value("boxTextSize").toArray(), textDocument->boxTextSize);
  textDocument->fauxBold = obj.value("fauxBold").toBool(textDocument->fauxBold);
  textDocument->fauxItalic = obj.value("fauxItalic").toBool(textDocument->fauxItalic);
  textDocument->fillColor = ArrayToColor(obj.value("fillColor").toArray(), textDocument->fillColor);
  textDocument->fontFamily = obj.value("fontFamily").toString("").toStdString();
  textDocument->fontStyle = obj.value("fontStyle").toString("").toStdString();
  textDocument->fontSize =
      static_cast<float>(obj.value("fontSize").toDouble(textDocument->fontSize));
  textDocument->strokeColor =
      ArrayToColor(obj.value("strokeColor").toArray(), textDocument->strokeColor);
  textDocument->strokeOverFill = obj.value("strokeOverFill").toBool(textDocument->strokeOverFill);
  textDocument->strokeWidth =
      static_cast<float>(obj.value("strokeWidth").toDouble(textDocument->strokeWidth));
  textDocument->text = obj.value("text").toString("").replace("\\n", "\n").toStdString();
  textDocument->justification =
      AEJustificationToJustification(static_cast<float>(obj.value("justification").toInt(0)));
  textDocument->leading = static_cast<float>(obj.value("leading").toDouble(textDocument->leading));
  if (textDocument->leading == 0) {
    textDocument->leading =
        static_cast<float>(obj.value("baselineLocs").toDouble(textDocument->fontSize));
  }
  textDocument->tracking =
      static_cast<float>(obj.value("tracking").toDouble(textDocument->tracking));
  float lineHeight =
      textDocument->leading == 0 ? roundf(textDocument->fontSize * 1.2f) : textDocument->leading;
  textDocument->firstBaseLine = AEStringToTextFirstBaseLine(
      obj.value("baselineLocs").toArray(), lineHeight, textDocument->baselineShift,
      textDocument->direction == pag::TextDirection::Vertical);

  if (!outPath.empty()) {
    std::string fontDstPath = GetDir(outPath);
    fontDstPath =
        JoinPaths(fontDstPath, textDocument->fontFamily + "-" + textDocument->fontStyle + ".ttc");
    std::string fontSrcPath = obj.value("fontLocation").toString("").toStdString();
    CopyFile(fontSrcPath, fontDstPath);
    size_t fontSize = GetFileSize(fontDstPath);
    if (fontSize > 30 * 1024 * 1024) {
      PAGExportSessionManager::GetInstance()->recordWarning(
          AlertInfoType::FontFileTooBig,
          textDocument->fontFamily + ", " + std::to_string(fontSize / 1024 / 1024) + "MB");
    }
  }

  return {textDocument};
}

pag::GradientColorHandle ParseGradientColor(const AEGP_StreamVal2&, const QVariantMap& map) {
  int index = map.value("index", 0).toInt();
  int keyFrameIndex = map.value("keyFrameIndex", 0).toInt();
  return PAGExportSessionManager::GetInstance()->getGradientColors(
      {"ADBE Vector Graphic - G-Fill", "ADBE Vector Graphic - G-Stroke"}, index, keyFrameIndex);
}

pag::GradientColorHandle ParseGradientOverlayColor(const AEGP_StreamVal2&, const QVariantMap& map) {
  int index = map.value("index", 0).toInt();
  int keyFrameIndex = map.value("keyFrameIndex", 0).toInt();
  return PAGExportSessionManager::GetInstance()->getGradientColors({"gradientFill/gradient"}, index,
                                                                   keyFrameIndex);
}

bool ParseShapeDirectionReversed(const AEGP_StreamVal2& streamValue, const QVariantMap&) {
  return static_cast<int>(streamValue.one_d) == 2;
}

pag::BlendMode ParseShapeBlendMode(const AEGP_StreamVal2& streamValue, const QVariantMap&) {
  return AEShapeBlendModeToBlendMode(static_cast<int>(streamValue.one_d));
}

pag::BlendMode ParseStyleBlendMode(const AEGP_StreamVal2& streamValue, const QVariantMap&) {
  return AEStyleBlendModeToBlendMode(static_cast<int>(streamValue.one_d));
}

pag::GradientFillType ParseGradientOverlayType(const AEGP_StreamVal2& streamValue,
                                               const QVariantMap&) {
  return AEGradientOverlayTypeToGradientFillType(static_cast<int>(streamValue.one_d));
}

pag::GradientColorHandle ParseOuterGlowGradientColor(const AEGP_StreamVal2&,
                                                     const QVariantMap& map) {
  int index = map.value("index", 0).toInt();
  int keyFrameIndex = map.value("keyFrameIndex", 0).toInt();
  return PAGExportSessionManager::GetInstance()->getGradientColors({"outerGlow/gradient"}, index,
                                                                   keyFrameIndex);
}

pag::StrokePosition ParseStrokePosition(const AEGP_StreamVal2& streamValue, const QVariantMap&) {
  return AEStrokePositionToStrokePosition(static_cast<int>(streamValue.one_d));
}

pag::PolyStarType ParsePolyStarType(const AEGP_StreamVal2& streamValue, const QVariantMap&) {
  return AEPolyStarTypeToPolyStarType(static_cast<int>(streamValue.one_d));
}

pag::CompositeOrder ParseCompositeOrder(const AEGP_StreamVal2& streamValue, const QVariantMap&) {
  return AECompositeOrderToCompositeOrder(static_cast<int>(streamValue.one_d));
}

pag::FillRule ParseFillRule(const AEGP_StreamVal2& streamValue, const QVariantMap&) {
  return AEFillRuleToFillRule(static_cast<int>(streamValue.one_d));
}

pag::TrimPathsType ParseTrimPathsType(const AEGP_StreamVal2& streamValue, const QVariantMap&) {
  return AETrimPathsTypeToTrimPathsType(static_cast<int>(streamValue.one_d));
}

pag::LineCap ParseLineCap(const AEGP_StreamVal2& streamValue, const QVariantMap&) {
  return AELineCapToLineCap(static_cast<int>(streamValue.one_d));
}

pag::RepeaterOrder ParseRepeaterOrder(const AEGP_StreamVal2& streamValue, const QVariantMap&) {
  return AERepeaterOrderToRepeaterOrder(static_cast<int>(streamValue.one_d));
}

pag::LineJoin ParseLineJoin(const AEGP_StreamVal2& streamValue, const QVariantMap&) {
  return AELineJoinToLineJoin(static_cast<int>(streamValue.one_d));
}

pag::GradientFillType ParseGradientFillType(const AEGP_StreamVal2& streamValue,
                                            const QVariantMap&) {
  return AEGradientFillTypeToGradientFillType(static_cast<int>(streamValue.one_d));
}

pag::MergePathsMode ParseMergePathsMode(const AEGP_StreamVal2& streamValue, const QVariantMap&) {
  return AEMergePathsModeToMergePathsMode(static_cast<int>(streamValue.one_d));
}

pag::AnchorPointGrouping ParseAnchorPointGrouping(const AEGP_StreamVal2& streamValue,
                                                  const QVariantMap&) {
  return AEAnchorPointGroupingToAnchorPointGrouping(static_cast<int>(streamValue.one_d));
}

pag::PAGScaleMode ParseScaleMode(const AEGP_StreamVal2& streamValue, const QVariantMap&) {
  return AEScaleModeToScaleMode(static_cast<int>(streamValue.one_d));
}

pag::BlurDimensionsDirection ParseBlurDimensionsDirection(const AEGP_StreamVal2& streamValue,
                                                          const QVariantMap&) {
  return AEBlurDimensionsDirectionToBlurDimensionsDirection(static_cast<int>(streamValue.one_d));
}

pag::DisplacementMapSource ParseDisplacementMapSource(const AEGP_StreamVal2& streamValue,
                                                      const QVariantMap&) {
  return AEDisplacementMapSourceToDisplacementMapSource(static_cast<int>(streamValue.one_d));
}

pag::DisplacementMapBehavior ParseDisplacementMapBehavior(const AEGP_StreamVal2& streamValue,
                                                          const QVariantMap&) {
  return AEDisplacementMapBehaviorToDisplacementMapBehavior(static_cast<int>(streamValue.one_d));
}

pag::RadialBlurMode ParseRadialBlurMode(const AEGP_StreamVal2& streamValue, const QVariantMap&) {
  return AERadialBlurModeToRadialBlurMode(static_cast<int>(streamValue.one_d));
}

pag::RadialBlurAntialias ParseRadialBlurAntialias(const AEGP_StreamVal2& streamValue,
                                                  const QVariantMap&) {
  return AERadialBlurAntialiasToRadialBlurAntialias(static_cast<int>(streamValue.one_d));
}

pag::TextAnimatorTrackingType ParseTextAnimatorTrackingType(const AEGP_StreamVal2& streamValue,
                                                            const QVariantMap&) {
  return AETextAnimatorTrackingTypeToTextAnimatorTrackingType(static_cast<int>(streamValue.one_d));
}

pag::TextRangeSelectorUnits ParseTextRangeSelectorUnits(const AEGP_StreamVal2& streamValue,
                                                        const QVariantMap&) {
  return AETextRangeSelectorUnitsToTextRangeSelectorUnits(static_cast<int>(streamValue.one_d));
}

pag::TextRangeSelectorShape ParseTextRangeSelectorShape(const AEGP_StreamVal2& streamValue,
                                                        const QVariantMap&) {
  return AETextRangeSelectorShapeToTextRangeSelectorShape(static_cast<int>(streamValue.one_d));
}

pag::TextSelectorBasedOn ParseTextSelectorBasedOn(const AEGP_StreamVal2& streamValue,
                                                  const QVariantMap&) {
  return AETextSelectorBasedOnToTextSelectorBasedOn(static_cast<int>(streamValue.one_d));
}

pag::TextSelectorMode ParseTextSelectorMode(const AEGP_StreamVal2& streamValue,
                                            const QVariantMap&) {
  return AETextSelectorModeToTextSelectorMode(static_cast<int>(streamValue.one_d));
}

pag::ChannelControlType ParseChannelControlType(const AEGP_StreamVal2& streamValue,
                                                const QVariantMap&) {
  return AEChannelControlTypeToChannelControlType(static_cast<int>(streamValue.one_d));
}

pag::IrisShapeType ParseIrisShapeType(const AEGP_StreamVal2& streamValue, const QVariantMap&) {
  return AEIrisShapeTypeToIrisShapeType(static_cast<int>(streamValue.one_d));
}

pag::GlowColorType ParseGlowColorType(const AEGP_StreamVal2& streamValue, const QVariantMap&) {
  return AEGlowColorTypeToGlowColorType(static_cast<int>(streamValue.one_d));
}

pag::GlowTechniqueType ParseGlowTechniqueType(const AEGP_StreamVal2& streamValue,
                                              const QVariantMap&) {
  return AEGlowTechniqueTypeToGlowTechniqueType(static_cast<int>(streamValue.one_d));
}

StreamParser<pag::Frame> AEStreamParser::TimeParser = ParseTime;
StreamParser<float> AEStreamParser::FloatParser = ParseFloat;
StreamParser<float> AEStreamParser::FloatM255Parser = ParseFloatM255;
StreamParser<float> AEStreamParser::PercentParser = ParsePercent;
StreamParser<float> AEStreamParser::PercentD255Parser = ParsePercentD255;
StreamParser<bool> AEStreamParser::BooleanParser = ParseBoolean;
StreamParser<pag::ID> AEStreamParser::LayerIDParser = ParseLayerID;
StreamParser<pag::ID> AEStreamParser::MaskIDParser = ParseMaskID;
StreamParser<uint8_t> AEStreamParser::Uint8Parser = ParseUint8;
StreamParser<uint16_t> AEStreamParser::Uint16Parser = ParseUint16;
StreamParser<int32_t> AEStreamParser::Int32Parser = ParseInt32;
StreamParser<pag::Opacity> AEStreamParser::Opacity0_100Parser = ParseOpacity0_100;
StreamParser<pag::Opacity> AEStreamParser::Opacity0_255Parser = ParseOpacity0_255;
StreamParser<pag::Opacity> AEStreamParser::Opacity0_1Parser = ParseOpacity0_1;
StreamParser<pag::Point> AEStreamParser::PointParser = ParsePoint;
StreamParser<pag::Point3D> AEStreamParser::Point3DParser = ParsePoint3D;
StreamParser<pag::Point> AEStreamParser::ScaleParser = ParseScale;
StreamParser<pag::Point3D> AEStreamParser::Scale3DParser = ParseScale3D;
StreamParser<pag::Color> AEStreamParser::ColorParser = ParseColor;
StreamParser<pag::PathHandle> AEStreamParser::PathParser = ParsePath;
StreamParser<pag::TextDocumentHandle> AEStreamParser::TextDocumentParser = ParseTextDocument;
StreamParser<pag::GradientColorHandle> AEStreamParser::GradientColorParser = ParseGradientColor;
StreamParser<pag::GradientColorHandle> AEStreamParser::GradientOverlayColorParser =
    ParseGradientOverlayColor;
StreamParser<bool> AEStreamParser::ShapeDirectionReversedParser = ParseShapeDirectionReversed;
StreamParser<pag::BlendMode> AEStreamParser::ShapeBlendModeParser = ParseShapeBlendMode;
StreamParser<pag::BlendMode> AEStreamParser::StyleBlendModeParser = ParseStyleBlendMode;
StreamParser<pag::GradientFillType> AEStreamParser::GradientOverlayTypeParser =
    ParseGradientOverlayType;
StreamParser<pag::GradientColorHandle> AEStreamParser::OuterGlowGradientColorParser =
    ParseOuterGlowGradientColor;
StreamParser<pag::StrokePosition> AEStreamParser::StrokePositionParser = ParseStrokePosition;
StreamParser<pag::PolyStarType> AEStreamParser::PolyStarTypeParser = ParsePolyStarType;
StreamParser<pag::CompositeOrder> AEStreamParser::CompositeOrderParser = ParseCompositeOrder;
StreamParser<pag::FillRule> AEStreamParser::FillRuleParser = ParseFillRule;
StreamParser<pag::TrimPathsType> AEStreamParser::TrimPathsTypeParser = ParseTrimPathsType;
StreamParser<pag::LineCap> AEStreamParser::LineCapParser = ParseLineCap;
StreamParser<pag::RepeaterOrder> AEStreamParser::RepeaterOrderParser = ParseRepeaterOrder;
StreamParser<pag::LineJoin> AEStreamParser::LineJoinParser = ParseLineJoin;
StreamParser<pag::GradientFillType> AEStreamParser::GradientFillTypeParser = ParseGradientFillType;
StreamParser<pag::MergePathsMode> AEStreamParser::MergePathsModeParser = ParseMergePathsMode;
StreamParser<pag::AnchorPointGrouping> AEStreamParser::AnchorPointGroupingParser =
    ParseAnchorPointGrouping;
StreamParser<pag::PAGScaleMode> AEStreamParser::ScaleModeParser = ParseScaleMode;
StreamParser<pag::BlurDimensionsDirection> AEStreamParser::BlurDimensionsDirectionParser =
    ParseBlurDimensionsDirection;
StreamParser<pag::DisplacementMapSource> AEStreamParser::DisplacementMapSourceParser =
    ParseDisplacementMapSource;
StreamParser<pag::DisplacementMapBehavior> AEStreamParser::DisplacementMapBehaviorParser =
    ParseDisplacementMapBehavior;
StreamParser<pag::RadialBlurMode> AEStreamParser::RadialBlurModeParser = ParseRadialBlurMode;
StreamParser<pag::RadialBlurAntialias> AEStreamParser::RadialBlurAntialiasParser =
    ParseRadialBlurAntialias;
StreamParser<pag::TextAnimatorTrackingType> AEStreamParser::TextAnimatorTrackingTypeParser =
    ParseTextAnimatorTrackingType;
StreamParser<pag::TextRangeSelectorUnits> AEStreamParser::TextRangeSelectorUnitsParser =
    ParseTextRangeSelectorUnits;
StreamParser<pag::TextRangeSelectorShape> AEStreamParser::TextRangeSelectorShapeParser =
    ParseTextRangeSelectorShape;
StreamParser<pag::TextSelectorBasedOn> AEStreamParser::TextSelectorBasedOnParser =
    ParseTextSelectorBasedOn;
StreamParser<pag::TextSelectorMode> AEStreamParser::TextSelectorModeParser = ParseTextSelectorMode;
StreamParser<pag::ChannelControlType> AEStreamParser::ChannelControlTypeParser =
    ParseChannelControlType;
StreamParser<pag::IrisShapeType> AEStreamParser::IrisShapeTypeParser = ParseIrisShapeType;
StreamParser<pag::GlowColorType> AEStreamParser::GlowColorTypeParser = ParseGlowColorType;
StreamParser<pag::GlowTechniqueType> AEStreamParser::GlowTechniqueTypeParser =
    ParseGlowTechniqueType;

}  // namespace exporter
