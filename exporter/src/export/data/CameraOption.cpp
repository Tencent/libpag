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

#include "CameraOption.h"
#include "export/stream/StreamProperty.h"

namespace exporter {

pag::CameraOption* GetCameraOption(const AEGP_LayerH& layerHandle) {
  auto cameraOption = new pag::CameraOption();

  cameraOption->zoom = GetProperty(layerHandle, AEGP_LayerStream_ZOOM, AEStreamParser::FloatParser);
  cameraOption->depthOfField =
      GetProperty(layerHandle, AEGP_LayerStream_DEPTH_OF_FIELD, AEStreamParser::BooleanParser);
  cameraOption->focusDistance =
      GetProperty(layerHandle, AEGP_LayerStream_FOCUS_DISTANCE, AEStreamParser::FloatParser);
  cameraOption->aperture =
      GetProperty(layerHandle, AEGP_LayerStream_APERTURE, AEStreamParser::FloatParser);
  cameraOption->blurLevel =
      GetProperty(layerHandle, AEGP_LayerStream_BLUR_LEVEL, AEStreamParser::PercentParser);
  cameraOption->irisShape =
      GetProperty(layerHandle, AEGP_LayerStream_IRIS_SHAPE, AEStreamParser::IrisShapeTypeParser);
  cameraOption->irisRotation =
      GetProperty(layerHandle, AEGP_LayerStream_IRIS_ROTATION, AEStreamParser::FloatParser);
  cameraOption->irisRoundness =
      GetProperty(layerHandle, AEGP_LayerStream_IRIS_ROUNDNESS, AEStreamParser::PercentParser);
  cameraOption->irisAspectRatio =
      GetProperty(layerHandle, AEGP_LayerStream_IRIS_ASPECT_RATIO, AEStreamParser::FloatParser);
  cameraOption->irisDiffractionFringe = GetProperty(
      layerHandle, AEGP_LayerStream_IRIS_DIFFRACTION_FRINGE, AEStreamParser::FloatParser);
  cameraOption->highlightGain =
      GetProperty(layerHandle, AEGP_LayerStream_IRIS_HIGHLIGHT_GAIN, AEStreamParser::FloatParser);
  cameraOption->highlightThreshold = GetProperty(
      layerHandle, AEGP_LayerStream_IRIS_HIGHLIGHT_THRESHOLD, AEStreamParser::FloatM255Parser);
  cameraOption->highlightSaturation = GetProperty(
      layerHandle, AEGP_LayerStream_IRIS_HIGHLIGHT_SATURATION, AEStreamParser::FloatParser);
  return cameraOption;
}

}  // namespace exporter
