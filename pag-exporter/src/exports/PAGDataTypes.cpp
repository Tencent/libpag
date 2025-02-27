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

#include "PAGDataTypes.h"
#include <A.h>
#include <AE_GeneralPlug.h>
#include "AEUtils.h"
#include "Context.h"

pag::Ratio ExportRatio(A_Ratio ratio) {
  pag::Ratio pagRatio = {};
  pagRatio.numerator = ratio.num;
  pagRatio.denominator = ratio.den;
  return pagRatio;
}

pag::Frame ExportTime(A_Time time, pagexporter::Context* context) {
  return ExportTime(time, context->frameRate);
}

pag::Frame ExportTime(A_Time time, float frameRate) {
  return static_cast<pag::Frame>(round(time.value * frameRate / time.scale));
}

pag::Point ExportPoint(AEGP_TwoDVal point) {
  pag::Point pagPoint = {};
  pagPoint.x = static_cast<float>(point.x);
  pagPoint.y = static_cast<float>(point.y);
  return pagPoint;
}

pag::Point3D ExportPoint3D(AEGP_ThreeDVal point) {
  pag::Point3D pagPoint = {};
  pagPoint.x = static_cast<float>(point.x);
  pagPoint.y = static_cast<float>(point.y);
  pagPoint.z = static_cast<float>(point.z);
  return pagPoint;
}

pag::Color ExportColor(AEGP_ColorVal color) {
  pag::Color pagColor = {};
  pagColor.red = static_cast<uint8_t>(color.redF * 255 + 0.5);
  pagColor.green = static_cast<uint8_t>(color.greenF * 255 + 0.5);
  pagColor.blue = static_cast<uint8_t>(color.blueF * 255 + 0.5);
  return pagColor;
}

pag::Enum ExportLayerBlendMode(PF_TransferMode mode) {
  switch (mode) {
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

pag::Enum ExportShapeBlendMode(int mode) {
  switch (mode) {
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

pag::Enum ExportStyleBlendMode(int mode) {
  switch (mode) {
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

pag::Enum ExportGradientOverlayType(int type) {
  switch (type) {
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

pag::Enum ExportTrackMatte(AEGP_TrackMatte trackMatte) {
  switch (trackMatte) {
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

pag::Enum ExportKeyframeInterpolationType(AEGP_KeyframeInterpolationType type) {
  switch (type) {
    case AEGP_KeyInterp_LINEAR:
      return pag::KeyframeInterpolationType::Linear;
    case AEGP_KeyInterp_BEZIER:
      return pag::KeyframeInterpolationType::Bezier;
    case AEGP_KeyInterp_HOLD:
      return pag::KeyframeInterpolationType::Hold;
    default:
      return pag::KeyframeInterpolationType::None;
  }
}

pag::Enum ExportMaskMode(PF_MaskMode mode) {
  switch (mode) {
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

pag::Enum ExportAnchorPointGrouping(int value) {
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

pag::Enum ExportPolyStarType(int value) {
  switch (value) {
    case 2:
      return pag::PolyStarType::Polygon;
    default:
      return pag::PolyStarType::Star;
  }
}

pag::Enum ExportCompositeOrder(int value) {
  switch (value) {
    case 2:
      return pag::CompositeOrder::AbovePreviousInSameGroup;
    default:
      return pag::CompositeOrder::BelowPreviousInSameGroup;
  }
}

pag::Enum ExportFillRule(int value) {
  switch (value) {
    case 2:
      return pag::FillRule::EvenOdd;
    default:
      return pag::FillRule::NonZeroWinding;
  }
}

pag::Enum ExportTrimType(int value) {
  switch (value) {
    case 2:
      return pag::TrimPathsType::Individually;
    default:
      return pag::TrimPathsType::Simultaneously;
  }
}

pag::Enum ExportRepeaterOrder(int value) {
  switch (value) {
    case 2:
      return pag::RepeaterOrder::Above;
    default:
      return pag::RepeaterOrder::Below;
  }
}

pag::Enum ExportLineCap(int value) {
  switch (value) {
    case 2:
      return pag::LineCap::Round;
    case 3:
      return pag::LineCap::Square;
    default:
      return pag::LineCap::Butt;
  }
}

pag::Enum ExportLineJoin(int value) {
  switch (value) {
    case 2:
      return pag::LineJoin::Round;
    case 3:
      return pag::LineJoin::Bevel;
    default:
      return pag::LineJoin::Miter;
  }
}

pag::Enum ExportGradientFillType(int value) {
  switch (value) {
    case 2:
      return pag::GradientFillType::Radial;
    default:
      return pag::GradientFillType::Linear;
  }
}

pag::Enum ExportMergePathsMode(int value) {
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

pag::Enum ExportParagraphJustification(int value) {
  switch (value) {
    case 7414:
      return pag::ParagraphJustification::RightJustify;
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

pag::Enum ExportScaleMode(int value) {
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

pag::Enum ExportBlurDimensionsDirection(int value) {
  switch (value) {
    case 2:
      return pag::BlurDimensionsDirection::Horizontal;
    case 3:
      return pag::BlurDimensionsDirection::Vertical;
    default:
      return pag::BlurDimensionsDirection::All;
  }
}

pag::Enum ExportRadialBlurMode(int value) {
  switch (value) {
    case 1:
      return pag::RadialBlurMode::Spin;
    case 2:
      return pag::RadialBlurMode::Zoom;
    default:
      return pag::RadialBlurMode::Spin;
  }
}

pag::Enum ExportRadialBlurAntialias(int value) {
  switch (value) {
    case 1:
      return pag::RadialBlurAntialias::Low;
    case 2:
      return pag::RadialBlurAntialias::High;
    default:
      return pag::RadialBlurAntialias::Low;
  }
}

pag::Enum ExportTextAnimatorTrackingType(int value) {
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

pag::Enum ExportTextRangeSelectorUnits(int value) {
  switch (value) {
    case 1:
      return pag::TextRangeSelectorUnits::Percentage;
    case 2:
      return pag::TextRangeSelectorUnits::Index;
    default:
      return pag::TextRangeSelectorUnits::Percentage;
  }
}

pag::Enum ExportTextRangeSelectorBasedOn(int value) {
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

pag::Enum ExportTextRangeSelectorMode(int value) {
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

pag::Enum ExportTextRangeSelectorShape(int value) {
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

pag::Layer* ExportLayerID(AEGP_LayerH layerH, const AEGP_SuiteHandler& suites) {
  if (layerH != nullptr) {
    auto layer = new pag::Layer();
    layer->id = AEUtils::GetLayerId(layerH);
    return layer;
  }
  return nullptr;
}

pag::MaskData* ExportMaskID(AEGP_MaskRefH maskRefH, const AEGP_SuiteHandler& suites) {
  if (maskRefH != nullptr) {
    A_long id;
    suites.MaskSuite6()->AEGP_GetMaskID(maskRefH, &id);
    auto mask = new pag::MaskData();
    mask->id = static_cast<pag::ID>(id);
    return mask;
  }
  return nullptr;
}

pag::Composition* ExportCompositionID(AEGP_ItemH itemHandle, const AEGP_SuiteHandler& suites) {
  if (itemHandle != nullptr) {
    auto composition = new pag::Composition();
    composition->id = AEUtils::GetItemId(itemHandle);
    return composition;
  }
  return nullptr;
}

pag::Enum ExportDisplacementMapSource(int value) {
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

pag::Enum ExportDisplacementBehavior(int value) {
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

pag::Enum ExportChannelControlType(int value) {
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

pag::Enum ExportIrisShapeType(int value) {
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

pag::Enum ExportStrokePosition(int value) {
  switch (value) {
    case 2:
      return pag::StrokePosition::Inside;
    case 3:
      return pag::StrokePosition::Center;
    default:
      return pag::StrokePosition::Outside;
  }
}

pag::Enum ExportGlowColorType(int type) {
  switch (type) {
    case 2:
      return pag::GlowColorType::Gradient;
    default:
      return pag::GlowColorType::SingleColor;
  }
}

pag::Enum ExportGlowTechniqueType(int type) {
  switch (type) {
    case 2:
      return pag::GlowTechniqueType::Precise;
    default:
      return pag::GlowTechniqueType::Softer;
  }
}
