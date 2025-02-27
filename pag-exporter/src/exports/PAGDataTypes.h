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
#ifndef PAGDATATYPES_H
#define PAGDATATYPES_H
#include <pag/file.h>
#include <pag/pag.h>
#include "AEGP_SuiteHandler.h"
#include "Context.h"

pag::Ratio ExportRatio(A_Ratio ratio);
pag::Frame ExportTime(A_Time time, float frameRate);
pag::Frame ExportTime(A_Time time, pagexporter::Context* context);
pag::Point ExportPoint(AEGP_TwoDVal point);
pag::Point3D ExportPoint3D(AEGP_ThreeDVal point);
pag::Color ExportColor(AEGP_ColorVal color);
pag::Enum ExportLayerBlendMode(PF_TransferMode mode);
pag::Enum ExportShapeBlendMode(int mode);
pag::Enum ExportStyleBlendMode(int mode);
pag::Enum ExportGradientOverlayType(int type);
pag::Enum ExportTrackMatte(AEGP_TrackMatte trackMatte);
pag::Enum ExportKeyframeInterpolationType(AEGP_KeyframeInterpolationType type);
pag::Enum ExportMaskMode(PF_MaskMode mode);
pag::Enum ExportPolyStarType(int value);
pag::Enum ExportCompositeOrder(int value);
pag::Enum ExportFillRule(int value);
pag::Enum ExportTrimType(int value);
pag::Enum ExportRepeaterOrder(int value);
pag::Enum ExportLineCap(int value);
pag::Enum ExportLineJoin(int value);
pag::Enum ExportGradientFillType(int value);
pag::Enum ExportMergePathsMode(int value);
pag::Enum ExportAnchorPointGrouping(int style);
pag::Enum ExportParagraphJustification(int value);
pag::Enum ExportScaleMode(int value);
pag::Enum ExportBlurDimensionsDirection(int value);
pag::Enum ExportDisplacementMapSource(int value);
pag::Enum ExportDisplacementBehavior(int value);
pag::Enum ExportRadialBlurMode(int value);
pag::Enum ExportRadialBlurAntialias(int value);
pag::Enum ExportTextAnimatorTrackingType(int value);
pag::Enum ExportTextRangeSelectorUnits(int value);
pag::Enum ExportTextRangeSelectorBasedOn(int value);
pag::Enum ExportTextRangeSelectorMode(int value);
pag::Enum ExportTextRangeSelectorShape(int value);
pag::Enum ExportChannelControlType(int value);
pag::Enum ExportIrisShapeType(int value);
pag::Enum ExportStrokePosition(int value);
pag::Enum ExportGlowColorType(int value);
pag::Enum ExportGlowTechniqueType(int value);
pag::Layer* ExportLayerID(AEGP_LayerH layerH, const AEGP_SuiteHandler& suites);
pag::MaskData* ExportMaskID(AEGP_MaskRefH maskRefH, const AEGP_SuiteHandler& suites);
pag::Composition* ExportCompositionID(AEGP_ItemH itemHandle, const AEGP_SuiteHandler& suites);

#endif  //PAGDATATYPES_H
