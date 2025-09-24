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

#pragma once

#include <AE_GeneralPlug.h>
#include <pag/file.h>
#include <QJsonArray>
#include <QString>

namespace exporter {

pag::Color AEColorToColor(AEGP_ColorVal color);

pag::BlendMode AEXferToBlendMode(int value);

pag::TrackMatteType AETrackMatteToTrackMatteType(int value);

pag::MaskMode AEMaskModeToMaskMode(int value);

pag::Frame AEDurationToFrame(A_Time time, float frameRate);

pag::ParagraphJustification AEJustificationToJustification(int value);

pag::BlendMode AEShapeBlendModeToBlendMode(int value);

pag::BlendMode AEStyleBlendModeToBlendMode(int value);

pag::GradientFillType AEGradientOverlayTypeToGradientFillType(int value);

pag::StrokePosition AEStrokePositionToStrokePosition(int value);

pag::PolyStarType AEPolyStarTypeToPolyStarType(int value);

pag::CompositeOrder AECompositeOrderToCompositeOrder(int value);

pag::FillRule AEFillRuleToFillRule(int value);

pag::TrimPathsType AETrimPathsTypeToTrimPathsType(int value);

pag::LineCap AELineCapToLineCap(int value);

pag::RepeaterOrder AERepeaterOrderToRepeaterOrder(int value);

pag::LineJoin AELineJoinToLineJoin(int value);

pag::GradientFillType AEGradientFillTypeToGradientFillType(int value);

pag::MergePathsMode AEMergePathsModeToMergePathsMode(int value);

pag::AnchorPointGrouping AEAnchorPointGroupingToAnchorPointGrouping(int value);

pag::PAGScaleMode AEScaleModeToScaleMode(int value);

pag::BlurDimensionsDirection AEBlurDimensionsDirectionToBlurDimensionsDirection(int value);

pag::DisplacementMapSource AEDisplacementMapSourceToDisplacementMapSource(int value);

pag::DisplacementMapBehavior AEDisplacementMapBehaviorToDisplacementMapBehavior(int value);

pag::RadialBlurMode AERadialBlurModeToRadialBlurMode(int value);

pag::RadialBlurAntialias AERadialBlurAntialiasToRadialBlurAntialias(int value);

pag::TextAnimatorTrackingType AETextAnimatorTrackingTypeToTextAnimatorTrackingType(int value);

pag::TextRangeSelectorUnits AETextRangeSelectorUnitsToTextRangeSelectorUnits(int value);

pag::TextRangeSelectorShape AETextRangeSelectorShapeToTextRangeSelectorShape(int value);

pag::TextSelectorBasedOn AETextSelectorBasedOnToTextSelectorBasedOn(int value);

pag::TextSelectorMode AETextSelectorModeToTextSelectorMode(int value);

pag::ChannelControlType AEChannelControlTypeToChannelControlType(int value);

pag::IrisShapeType AEIrisShapeTypeToIrisShapeType(int value);

pag::GlowColorType AEGlowColorTypeToGlowColorType(int value);

pag::GlowTechniqueType AEGlowTechniqueTypeToGlowTechniqueType(int value);

pag::TextDirection AETextDirectionToTextDirection(int value);

float AEStringToTextFirstBaseLine(const QJsonArray& array, float lineHeight, float baseLineShift,
                                  bool isVertical);

pag::GradientColorHandle GetDefaultGradientColors();

pag::GradientColorHandle XmlToGradientColor(const std::string& xml);

}  // namespace exporter
