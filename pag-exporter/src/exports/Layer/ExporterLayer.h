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
#ifndef EXPORTERLAYER_H
#define EXPORTERLAYER_H
#include "src/exports/PAGDataTypes.h"

#include "pag/file.h"

/**
 * 除 Video 外，其他都对应 pag 的图层类型。
 * 因为导出的时候有个视频转图片占位图的操作，内部多定义一个 Video 类型，便于标记转化成视频占位图，
 * 导出之后也是对应 pag 里 Image 类型，所以没必要修改 pag 里面的图层样式。
 */
enum class ExportLayerType {
  Unknown,
  Null,
  Solid,
  Text,
  Shape,
  Image,
  PreCompose,
  Video,
  Audio,
  Camera
};

std::vector<pag::Layer*> ExportLayers(pagexporter::Context* context, AEGP_CompH compHandle);
static std::unordered_map<pag::Layer*,AEGP_LayerH> pagToAELayer;
static std::unordered_map<AEGP_LayerH,pag::Layer*> aeToPagLayer;
static bool ifCopyMatlayer=false;
#endif //EXPORTERLAYER_H
