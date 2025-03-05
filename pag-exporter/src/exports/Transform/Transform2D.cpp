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
#include "Transform2D.h"
#include "src/exports/Stream/StreamProperty.h"

pag::Transform2D* ExportTransform2D(pagexporter::Context* context, const AEGP_LayerH& layerHandle) {
  auto transform = new pag::Transform2D();
  auto& suites = context->suites;
  AEGP_StreamRefH position;
  RECORD_ERROR(suites.StreamSuite4()->AEGP_GetNewLayerStream(context->pluginID, layerHandle,
                                                             AEGP_LayerStream_POSITION, &position));
  A_Boolean dimensionSeparated = 0;
  RECORD_ERROR(
      suites.DynamicStreamSuite4()->AEGP_AreDimensionsSeparated(position, &dimensionSeparated));
  if (dimensionSeparated != 0) {
    AEGP_StreamRefH xPosition;
    RECORD_ERROR(suites.DynamicStreamSuite4()->AEGP_GetSeparationFollower(position, 0, &xPosition));
    transform->xPosition = ExportProperty(context, xPosition, Parsers::Float);
    RECORD_ERROR(suites.StreamSuite4()->AEGP_DisposeStream(xPosition));
    AEGP_StreamRefH yPosition;
    RECORD_ERROR(suites.DynamicStreamSuite4()->AEGP_GetSeparationFollower(position, 1, &yPosition));
    transform->yPosition = ExportProperty(context, yPosition, Parsers::Float);
    RECORD_ERROR(suites.StreamSuite4()->AEGP_DisposeStream(yPosition));
  } else {
    transform->position = ExportProperty(context, position, Parsers::Point);
  }
  suites.StreamSuite4()->AEGP_DisposeStream(position);
  transform->anchorPoint =
      ExportProperty(context, layerHandle, AEGP_LayerStream_ANCHORPOINT, Parsers::Point);
  transform->scale =
      ExportProperty(context, layerHandle, AEGP_LayerStream_SCALE, Parsers::Scale, 2);
  transform->rotation =
      ExportProperty(context, layerHandle, AEGP_LayerStream_ROTATION, Parsers::Float);
  transform->opacity =
      ExportProperty(context, layerHandle, AEGP_LayerStream_OPACITY, Parsers::Opacity0_100);
  return transform;
}