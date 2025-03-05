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
#include "CameraOption.h"
#include "pag/file.h"
#include "src/exports/Stream/StreamProperty.h"

pag::CameraOption* ExportCameraOption(pagexporter::Context* context, const AEGP_LayerH& layerHandle) {
  auto cameraOption = new pag::CameraOption();

  cameraOption->zoom = ExportProperty(context, layerHandle, AEGP_LayerStream_ZOOM, Parsers::Float);
  cameraOption->depthOfField = ExportProperty(context, layerHandle, AEGP_LayerStream_DEPTH_OF_FIELD, Parsers::Boolean);
  cameraOption->focusDistance = ExportProperty(context, layerHandle, AEGP_LayerStream_FOCUS_DISTANCE, Parsers::Float);
  cameraOption->aperture = ExportProperty(context, layerHandle, AEGP_LayerStream_APERTURE, Parsers::Float);
  cameraOption->blurLevel = ExportProperty(context, layerHandle, AEGP_LayerStream_BLUR_LEVEL, Parsers::Percent);
  cameraOption->irisShape = ExportProperty(context, layerHandle, AEGP_LayerStream_IRIS_SHAPE, Parsers::IrisShapeType);
  cameraOption->irisRotation = ExportProperty(context, layerHandle, AEGP_LayerStream_IRIS_ROTATION, Parsers::Float);
  cameraOption->irisRoundness = ExportProperty(context, layerHandle, AEGP_LayerStream_IRIS_ROUNDNESS, Parsers::Percent);
  cameraOption->irisAspectRatio = ExportProperty(context, layerHandle,
                                                 AEGP_LayerStream_IRIS_ASPECT_RATIO, Parsers::Float);
  cameraOption->irisDiffractionFringe = ExportProperty(context, layerHandle,
                                                       AEGP_LayerStream_IRIS_DIFFRACTION_FRINGE, Parsers::Float);
  cameraOption->highlightGain = ExportProperty(context, layerHandle,
                                               AEGP_LayerStream_IRIS_HIGHLIGHT_GAIN, Parsers::Float);
  cameraOption->highlightThreshold = ExportProperty(context, layerHandle,
                                                    AEGP_LayerStream_IRIS_HIGHLIGHT_THRESHOLD, Parsers::FloatM255);
  cameraOption->highlightSaturation = ExportProperty(context, layerHandle,
                                                     AEGP_LayerStream_IRIS_HIGHLIGHT_SATURATION, Parsers::Float);
  return cameraOption;
}
