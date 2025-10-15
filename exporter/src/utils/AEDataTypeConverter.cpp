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

#include "AEDataTypeConverter.h"
#include <AE_EffectCB.h>
#include <src/base/utils/Log.h>
#include <tinyxml2.h>
#include <QStringList>

namespace exporter {

pag::Color AEColorToColor(AEGP_ColorVal color) {
  pag::Color pagColor = {};
  pagColor.red = static_cast<uint8_t>(color.redF * 255 + 0.5);
  pagColor.green = static_cast<uint8_t>(color.greenF * 255 + 0.5);
  pagColor.blue = static_cast<uint8_t>(color.blueF * 255 + 0.5);
  return pagColor;
}

pag::BlendMode AEXferToBlendMode(int value) {
  switch (value) {
    case PF_Xfer_MULTIPLY:
      return pag::BlendMode::Multiply;
    case PF_Xfer_SCREEN:
      return pag::BlendMode::Screen;
    case PF_Xfer_OVERLAY:
      return pag::BlendMode::Overlay;
    case PF_Xfer_DARKEN:
      return pag::BlendMode::Darken;
    case PF_Xfer_LIGHTEN:
      return pag::BlendMode::Lighten;
    case PF_Xfer_COLOR_DODGE:
    case PF_Xfer_COLOR_DODGE2:
      return pag::BlendMode::ColorDodge;
    case PF_Xfer_COLOR_BURN:
    case PF_Xfer_COLOR_BURN2:
      return pag::BlendMode::ColorBurn;
    case PF_Xfer_HARD_LIGHT:
      return pag::BlendMode::HardLight;
    case PF_Xfer_SOFT_LIGHT:
      return pag::BlendMode::SoftLight;
    case PF_Xfer_DIFFERENCE:
    case PF_Xfer_DIFFERENCE2:
      return pag::BlendMode::Difference;
    case PF_Xfer_EXCLUSION:
      return pag::BlendMode::Exclusion;
    case PF_Xfer_HUE:
      return pag::BlendMode::Hue;
    case PF_Xfer_SATURATION:
      return pag::BlendMode::Saturation;
    case PF_Xfer_COLOR:
      return pag::BlendMode::Color;
    case PF_Xfer_LUMINOSITY:
      return pag::BlendMode::Luminosity;
    case PF_Xfer_ADD:
      return pag::BlendMode::Add;
    default:
      return pag::BlendMode::Normal;
  }
}

pag::TrackMatteType AETrackMatteToTrackMatteType(int value) {
  switch (value) {
    case AEGP_TrackMatte_ALPHA:
      return pag::TrackMatteType::Alpha;
    case AEGP_TrackMatte_NOT_ALPHA:
      return pag::TrackMatteType::AlphaInverted;
    case AEGP_TrackMatte_LUMA:
      return pag::TrackMatteType::Luma;
    case AEGP_TrackMatte_NOT_LUMA:
      return pag::TrackMatteType::LumaInverted;
    default:
      return pag::TrackMatteType::None;
  }
}

pag::MaskMode AEMaskModeToMaskMode(int value) {
  switch (value) {
    case PF_MaskMode_NONE:
      return pag::MaskMode::None;
    case PF_MaskMode_SUBTRACT:
      return pag::MaskMode::Subtract;
    case PF_MaskMode_INTERSECT:
      return pag::MaskMode::Intersect;
    case PF_MaskMode_LIGHTEN:
      return pag::MaskMode::Lighten;
    case PF_MaskMode_DARKEN:
      return pag::MaskMode::Darken;
    case PF_MaskMode_DIFFERENCE:
      return pag::MaskMode::Difference;
    case PF_MaskMode_ACCUM:
      return pag::MaskMode::Accum;
    default:
      return pag::MaskMode::Add;
  }
}

pag::Frame AEDurationToFrame(A_Time time, float frameRate) {
  return static_cast<pag::Frame>(round(time.value * frameRate / time.scale));
}

pag::ParagraphJustification AEJustificationToJustification(int value) {
  switch (value) {
    case 7413:
      return pag::ParagraphJustification::RightJustify;
    case 7414:
      return pag::ParagraphJustification::LeftJustify;
    case 7415:
      return pag::ParagraphJustification::CenterJustify;
    case 7416:
      return pag::ParagraphJustification::FullJustifyLastLineLeft;
    case 7417:
      return pag::ParagraphJustification::FullJustifyLastLineRight;
    case 7418:
      return pag::ParagraphJustification::FullJustifyLastLineCenter;
    case 7419:
      return pag::ParagraphJustification::FullJustifyLastLineFull;
    default:
      return pag::ParagraphJustification::LeftJustify;
  }
}

pag::BlendMode AEShapeBlendModeToBlendMode(int value) {
  switch (value) {
    case 4:
      return pag::BlendMode::Multiply;
    case 10:
      return pag::BlendMode::Screen;
    case 15:
      return pag::BlendMode::Overlay;
    case 3:
      return pag::BlendMode::Darken;
    case 9:
      return pag::BlendMode::Lighten;
    case 11:
      return pag::BlendMode::ColorDodge;
    case 5:
      return pag::BlendMode::ColorBurn;
    case 17:
      return pag::BlendMode::HardLight;
    case 16:
      return pag::BlendMode::SoftLight;
    case 23:
      return pag::BlendMode::Difference;
    case 24:
      return pag::BlendMode::Exclusion;
    case 26:
      return pag::BlendMode::Hue;
    case 27:
      return pag::BlendMode::Saturation;
    case 28:
      return pag::BlendMode::Color;
    case 29:
      return pag::BlendMode::Luminosity;
    default:
      return pag::BlendMode::Normal;
  }
}

pag::BlendMode AEStyleBlendModeToBlendMode(int value) {
  switch (value) {
    case 5:
      return pag::BlendMode::Multiply;
    case 11:
      return pag::BlendMode::Screen;
    case 16:
      return pag::BlendMode::Overlay;
    case 4:
      return pag::BlendMode::Darken;
    case 10:
      return pag::BlendMode::Lighten;
    case 12:
      return pag::BlendMode::ColorDodge;
    case 6:
      return pag::BlendMode::ColorBurn;
    case 18:
      return pag::BlendMode::HardLight;
    case 17:
      return pag::BlendMode::SoftLight;
    case 24:
      return pag::BlendMode::Difference;
    case 25:
      return pag::BlendMode::Exclusion;
    case 27:
      return pag::BlendMode::Hue;
    case 28:
      return pag::BlendMode::Saturation;
    case 29:
      return pag::BlendMode::Color;
    case 30:
      return pag::BlendMode::Luminosity;
    default:
      return pag::BlendMode::Normal;
  }
}

pag::GradientFillType AEGradientOverlayTypeToGradientFillType(int value) {
  switch (value) {
    case 2:
      return pag::GradientFillType::Radial;
    case 3:
      return pag::GradientFillType::Angle;
    case 4:
      return pag::GradientFillType::Reflected;
    case 5:
      return pag::GradientFillType::Diamond;
    default:
      return pag::GradientFillType::Linear;
  }
}

pag::StrokePosition AEStrokePositionToStrokePosition(int value) {
  switch (value) {
    case 2:
      return pag::StrokePosition::Inside;
    case 3:
      return pag::StrokePosition::Center;
    default:
      return pag::StrokePosition::Outside;
  }
}

pag::PolyStarType AEPolyStarTypeToPolyStarType(int value) {
  if (value == 2) {
    return pag::PolyStarType::Polygon;
  }

  return pag::PolyStarType::Star;
}

pag::CompositeOrder AECompositeOrderToCompositeOrder(int value) {
  if (value == 2) {
    return pag::CompositeOrder::AbovePreviousInSameGroup;
  }

  return pag::CompositeOrder::BelowPreviousInSameGroup;
}

pag::FillRule AEFillRuleToFillRule(int value) {
  if (value == 2) {
    return pag::FillRule::EvenOdd;
  }

  return pag::FillRule::NonZeroWinding;
}

pag::TrimPathsType AETrimPathsTypeToTrimPathsType(int value) {
  if (value == 2) {
    return pag::TrimPathsType::Individually;
  }

  return pag::TrimPathsType::Simultaneously;
}

pag::LineCap AELineCapToLineCap(int value) {
  switch (value) {
    case 2:
      return pag::LineCap::Round;
    case 3:
      return pag::LineCap::Square;
    default:
      return pag::LineCap::Butt;
  }
}

pag::RepeaterOrder AERepeaterOrderToRepeaterOrder(int value) {
  if (value == 2) {
    return pag::RepeaterOrder::Above;
  }

  return pag::RepeaterOrder::Below;
}

pag::LineJoin AELineJoinToLineJoin(int value) {
  switch (value) {
    case 2:
      return pag::LineJoin::Round;
    case 3:
      return pag::LineJoin::Bevel;
    default:
      return pag::LineJoin::Miter;
  }
}

pag::GradientFillType AEGradientFillTypeToGradientFillType(int value) {
  if (value == 2) {
    return pag::GradientFillType::Radial;
  }

  return pag::GradientFillType::Linear;
}

pag::MergePathsMode AEMergePathsModeToMergePathsMode(int value) {
  switch (value) {
    case 2:
      return pag::MergePathsMode::Add;
    case 3:
      return pag::MergePathsMode::Subtract;
    case 4:
      return pag::MergePathsMode::Intersect;
    case 5:
      return pag::MergePathsMode::ExcludeIntersections;
    default:
      return pag::MergePathsMode::Merge;
  }
}

pag::AnchorPointGrouping AEAnchorPointGroupingToAnchorPointGrouping(int value) {
  switch (value) {
    case 2:
      return pag::AnchorPointGrouping::Word;
    case 3:
      return pag::AnchorPointGrouping::Line;
    case 4:
      return pag::AnchorPointGrouping::All;
    default:
      return pag::AnchorPointGrouping::Character;
  }
}

pag::PAGScaleMode AEScaleModeToScaleMode(int value) {
  switch (value) {
    case 0:
      return pag::PAGScaleMode::None;
    case 2:
      return pag::PAGScaleMode::Stretch;
    case 3:
      return pag::PAGScaleMode::LetterBox;
    case 4:
      return pag::PAGScaleMode::Zoom;
    default:
      return pag::PAGScaleMode::None;
  }
}

pag::BlurDimensionsDirection AEBlurDimensionsDirectionToBlurDimensionsDirection(int value) {
  switch (value) {
    case 2:
      return pag::BlurDimensionsDirection::Horizontal;
    case 3:
      return pag::BlurDimensionsDirection::Vertical;
    default:
      return pag::BlurDimensionsDirection::All;
  }
}

pag::DisplacementMapSource AEDisplacementMapSourceToDisplacementMapSource(int value) {
  switch (value) {
    case 1:
      return pag::DisplacementMapSource::Red;
    case 2:
      return pag::DisplacementMapSource::Green;
    case 3:
      return pag::DisplacementMapSource::Blue;
    case 4:
      return pag::DisplacementMapSource::Alpha;
    case 5:
      return pag::DisplacementMapSource::Luminance;
    case 6:
      return pag::DisplacementMapSource::Hue;
    case 7:
      return pag::DisplacementMapSource::Lightness;
    case 8:
      return pag::DisplacementMapSource::Saturation;
    case 9:
      return pag::DisplacementMapSource::Full;
    case 10:
      return pag::DisplacementMapSource::Half;
    case 11:
      return pag::DisplacementMapSource::Off;
    default:
      return pag::DisplacementMapSource::Off;
  }
}

pag::DisplacementMapBehavior AEDisplacementMapBehaviorToDisplacementMapBehavior(int value) {
  switch (value) {
    case 1:
      return pag::DisplacementMapBehavior::CenterMap;
    case 2:
      return pag::DisplacementMapBehavior::StretchMapToFit;
    case 3:
      return pag::DisplacementMapBehavior::TileMap;
    default:
      return pag::DisplacementMapBehavior::CenterMap;
  }
}

pag::RadialBlurMode AERadialBlurModeToRadialBlurMode(int value) {
  switch (value) {
    case 1:
      return pag::RadialBlurMode::Spin;
    case 2:
      return pag::RadialBlurMode::Zoom;
    default:
      return pag::RadialBlurMode::Spin;
  }
}

pag::RadialBlurAntialias AERadialBlurAntialiasToRadialBlurAntialias(int value) {
  switch (value) {
    case 1:
      return pag::RadialBlurAntialias::Low;
    case 2:
      return pag::RadialBlurAntialias::High;
    default:
      return pag::RadialBlurAntialias::Low;
  }
}

pag::TextAnimatorTrackingType AETextAnimatorTrackingTypeToTextAnimatorTrackingType(int value) {
  switch (value) {
    case 1:
      return pag::TextAnimatorTrackingType::BeforeAndAfter;
    case 2:
      return pag::TextAnimatorTrackingType::Before;
    case 3:
      return pag::TextAnimatorTrackingType::After;
    default:
      return pag::TextAnimatorTrackingType::BeforeAndAfter;
  }
}

pag::TextRangeSelectorUnits AETextRangeSelectorUnitsToTextRangeSelectorUnits(int value) {
  switch (value) {
    case 1:
      return pag::TextRangeSelectorUnits::Percentage;
    case 2:
      return pag::TextRangeSelectorUnits::Index;
    default:
      return pag::TextRangeSelectorUnits::Percentage;
  }
}

pag::TextRangeSelectorShape AETextRangeSelectorShapeToTextRangeSelectorShape(int value) {
  switch (value) {
    case 1:
      return pag::TextRangeSelectorShape::Square;
    case 2:
      return pag::TextRangeSelectorShape::RampUp;
    case 3:
      return pag::TextRangeSelectorShape::RampDown;
    case 4:
      return pag::TextRangeSelectorShape::Triangle;
    case 5:
      return pag::TextRangeSelectorShape::Round;
    case 6:
      return pag::TextRangeSelectorShape::Smooth;
    default:
      return pag::TextRangeSelectorShape::Square;
  }
}

pag::TextSelectorBasedOn AETextSelectorBasedOnToTextSelectorBasedOn(int value) {
  switch (value) {
    case 1:
      return pag::TextSelectorBasedOn::Characters;
    case 2:
      return pag::TextSelectorBasedOn::CharactersExcludingSpaces;
    case 3:
      return pag::TextSelectorBasedOn::Words;
    case 4:
      return pag::TextSelectorBasedOn::Lines;
    default:
      return pag::TextSelectorBasedOn::Characters;
  }
}

pag::TextSelectorMode AETextSelectorModeToTextSelectorMode(int value) {
  switch (value) {
    case 0:
      return pag::TextSelectorMode::None;
    case 1:
      return pag::TextSelectorMode::Add;
    case 2:
      return pag::TextSelectorMode::Subtract;
    case 3:
      return pag::TextSelectorMode::Intersect;
    case 4:
      return pag::TextSelectorMode::Min;
    case 5:
      return pag::TextSelectorMode::Max;
    case 6:
      return pag::TextSelectorMode::Difference;
    default:
      return pag::TextSelectorMode::Add;
  }
}

pag::ChannelControlType AEChannelControlTypeToChannelControlType(int value) {
  switch (value) {
    case 1:
      return pag::ChannelControlType::Master;
    case 2:
      return pag::ChannelControlType::Reds;
    case 3:
      return pag::ChannelControlType::Yellows;
    case 4:
      return pag::ChannelControlType::Greens;
    case 5:
      return pag::ChannelControlType::Cyans;
    case 6:
      return pag::ChannelControlType::Blues;
    case 7:
      return pag::ChannelControlType::Magentas;
    default:
      return pag::ChannelControlType::Master;
  }
}

pag::IrisShapeType AEIrisShapeTypeToIrisShapeType(int value) {
  switch (value) {
    case 3:
      return pag::IrisShapeType::Triangle;
    case 4:
      return pag::IrisShapeType::Square;
    case 5:
      return pag::IrisShapeType::Pentagon;
    case 6:
      return pag::IrisShapeType::Hexagon;
    case 7:
      return pag::IrisShapeType::Heptagon;
    case 8:
      return pag::IrisShapeType::Octagon;
    case 9:
      return pag::IrisShapeType::Nonagon;
    case 10:
      return pag::IrisShapeType::Decagon;
    default:
      return pag::IrisShapeType::FastRectangle;
  }
}

pag::GlowColorType AEGlowColorTypeToGlowColorType(int value) {
  if (value == 2) {
    return pag::GlowColorType::Gradient;
  }

  return pag::GlowColorType::SingleColor;
}

pag::GlowTechniqueType AEGlowTechniqueTypeToGlowTechniqueType(int value) {
  if (value == 2) {
    return pag::GlowTechniqueType::Precise;
  }

  return pag::GlowTechniqueType::Softer;
}

pag::TextDirection AETextDirectionToTextDirection(int value) {
  /*
   * 13212: Horizontal
   * 13213: Vertical
   */
  if (value == 13213) {
    return pag::TextDirection::Vertical;
  }

  return pag::TextDirection::Horizontal;
}

float AEStringToTextFirstBaseLine(const QJsonArray& array, float lineHeight, float baseLineShift,
                                  bool isVertical) {
  if (array.size() < 4) {
    return 0;
  }

  int lineNum = static_cast<int>(array.size()) / 4;
  int index = (lineNum - 1) * 4;
  float firstBaseLine = 0;
  if (isVertical) {
    firstBaseLine = array[0].toString().toFloat();
    if (fabsf(firstBaseLine) > 100000000.0f) {
      auto lastBaseLine = array[index + 0].toString().toFloat();
      firstBaseLine = lastBaseLine + lineHeight * static_cast<float>((lineNum - 1));
    }
  } else {
    firstBaseLine = array[1].toString().toFloat();
    if (fabsf(firstBaseLine) > 100000000.0f) {
      auto lastBaseLine = array[index + 1].toString().toFloat();
      firstBaseLine = lastBaseLine - lineHeight * static_cast<float>((lineNum - 1));
    }
  }
  if (fabsf(firstBaseLine) > 100000000.0f) {
    firstBaseLine = 0;
  }

  return isVertical ? firstBaseLine - baseLineShift : firstBaseLine + baseLineShift;
}

pag::GradientColorHandle GetDefaultGradientColors() {
  auto gradientColor = std::make_shared<pag::GradientColor>();
  pag::AlphaStop stop = {};
  stop.position = 0.0f;
  gradientColor->alphaStops.emplace_back(stop);
  stop = {};
  stop.position = 1.0f;
  gradientColor->alphaStops.emplace_back(stop);
  pag::ColorStop colorStop = {};
  colorStop.position = 0.0f;
  colorStop.color = pag::White;
  gradientColor->colorStops.emplace_back(colorStop);
  colorStop = {};
  colorStop.position = 1.0f;
  colorStop.color = pag::Black;
  gradientColor->colorStops.emplace_back(colorStop);
  return gradientColor;
}

static std::vector<std::vector<float>> ExtractFloatArraysByKey(const std::string& xmlContent,
                                                               const std::string& keyName) {
  using namespace tinyxml2;

  std::vector<std::vector<float>> result = {};
  XMLDocument doc;
  if (doc.Parse(xmlContent.c_str()) != XML_SUCCESS) {
    LOGE("XML parsing failed: %s", doc.ErrorStr());

    return result;
  }

  auto traverse = [&](XMLElement* element, const auto& traverseRef) {
    if (!element) return;

    for (XMLElement* child = element->FirstChildElement(); child != nullptr;
         child = child->NextSiblingElement()) {
      if (std::string(child->Value()) == "prop.pair") {
        XMLElement* key = child->FirstChildElement("key");
        if (key && key->GetText()) {
          if (std::string(key->GetText()) == keyName) {
            XMLElement* array = child->FirstChildElement("array");
            if (array) {
              XMLElement* arrayType = array->FirstChildElement("array.type");
              if (arrayType && arrayType->FirstChildElement("float")) {
                std::vector<float> floatList = {};
                for (XMLElement* floatVal = array->FirstChildElement("float"); floatVal != nullptr;
                     floatVal = floatVal->NextSiblingElement("float")) {
                  if (floatVal->GetText()) {
                    try {
                      floatList.emplace_back(std::stof(floatVal->GetText()));
                    } catch (const std::exception& e) {
                      LOGE("Error converting float value: %s", e.what());
                    }
                  }
                }
                result.emplace_back(floatList);
              }
            }
          }
        }
      }
      traverseRef(child, traverseRef);
    }
  };

  traverse(doc.RootElement(), traverse);

  return result;
}

static std::vector<pag::AlphaStop> ParseAlphaStops(const std::string& xml) {
  std::vector<pag::AlphaStop> list = {};

  auto alphaStopList = ExtractFloatArraysByKey(xml, "Stops Alpha");
  for (const auto& alphaStop : alphaStopList) {
    if (alphaStop.size() < 3) {
      return {};
    }
    pag::AlphaStop stop = {};
    stop.position = alphaStop[0];
    stop.midpoint = alphaStop[1];
    stop.opacity = static_cast<uint8_t>(alphaStop[2] * 255);
    list.emplace_back(stop);
  }
  return list;
}

static std::vector<pag::ColorStop> ParseColorStops(const std::string& xml) {
  std::vector<pag::ColorStop> list = {};
  auto colorStopList = ExtractFloatArraysByKey(xml, "Stops Color");
  for (const auto& colorStop : colorStopList) {
    if (colorStop.size() < 6) {
      return {};
    }
    pag::Color color = {};
    color.red = static_cast<uint8_t>(255 * colorStop[2]);
    color.green = static_cast<uint8_t>(255 * colorStop[3]);
    color.blue = static_cast<uint8_t>(255 * colorStop[4]);
    pag::ColorStop stop = {};
    stop.position = colorStop[0];
    stop.midpoint = colorStop[1];
    stop.color = color;
    list.push_back(stop);
  }

  return list;
}

template <typename T>
static void ResortGradientStops(std::vector<T>& list) {
  std::sort(list.begin(), list.end(),
            [](const T& a, const T& b) { return a.position < b.position; });
}

pag::GradientColorHandle XmlToGradientColor(const std::string& xml) {
  if (xml.empty()) {
    return GetDefaultGradientColors();
  }
  auto gradientColor = std::make_shared<pag::GradientColor>();
  gradientColor->alphaStops = ParseAlphaStops(xml);
  gradientColor->colorStops = ParseColorStops(xml);
  if (gradientColor->alphaStops.size() < 2 || gradientColor->colorStops.size() < 2) {
    return GetDefaultGradientColors();
  }
  ResortGradientStops(gradientColor->alphaStops);
  ResortGradientStops(gradientColor->colorStops);
  return gradientColor;
}

}  // namespace exporter
