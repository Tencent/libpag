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
#ifndef IMAGEBYTES_H
#define IMAGEBYTES_H
#include "src/exports/PAGDataTypes.h"

// 设置render时间
void SetRenderTime(pagexporter::Context* context, const AEGP_RenderOptionsH& renderOptions,
                   float frameRate, int frame);

void GetRenderFrame(uint8_t*& rgbaBytes, A_u_long& stride, A_long& width, A_long& height,
                    const AEGP_SuiteHandler& suites, AEGP_RenderOptionsH& renderOptions);

void GetRenderImageFromLayer(uint8_t*& rgbaBytes, A_u_long& rowBytes,
                             A_long& width, A_long& height,
                             pagexporter::Context& context, AEGP_LayerH layerHandle,
                             bool isVideo);

void GetRenderImageFromComposition(uint8_t*& rgbaBytes, A_u_long& rowBytes,
                                   A_long& width, A_long& height,
                                   pagexporter::Context& context, AEGP_ItemH itemH,
                                   float frameRate, int frame);

pag::ImageBytes* ExportImageBytes(pagexporter::Context* context,
                                  const AEGP_LayerH& layerHandle,
                                  bool isVideo = false);

void ReExportImageBytes(pagexporter::Context* context, int index, float factor);
#endif //IMAGEBYTES_H
