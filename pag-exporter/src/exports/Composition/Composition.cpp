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
#include "Composition.h"
#include <unordered_map>
#include "AEUtils.h"
#include "BitmapComposition.h"
#include "StringUtil.h"
#include "VectorComposition.h"
#include "VideoComposition.h"
#include "src/exports/ImageBytes/ImageBytes.h"
#include "src/utils/CommonMethod.h"
#include "src/utils/ImageData/ImageRawData.h"
// 获取合成的时长
static pag::Frame GetCompositionDuration(pagexporter::Context* context,
                                         AEGP_CompH const& compHandle) {
  auto& suites = context->suites;

  auto itemHandle = AEUtils::GetItemFromComp(compHandle);

  A_Time durationTime = {};
  RECORD_ERROR(suites.ItemSuite6()->AEGP_GetItemDuration(itemHandle, &durationTime));
  auto duration = ExportTime(durationTime, context);

  // 工作区域只对总合成有效，这里先不考虑

  return duration;
}

pag::ID GetCompositionAttributes(pagexporter::Context* context, AEGP_CompH const& compHandle,
                                 pag::Composition* composition) {
  auto& suites = context->suites;
  if (context->frameRate == -1) {
    A_FpLong frameRate = 24;
    RECORD_ERROR(suites.CompSuite6()->AEGP_GetCompFramerate(compHandle, &frameRate));
    composition->frameRate = static_cast<float>(frameRate);
    context->frameRate = composition->frameRate;
  } else {
    composition->frameRate = context->frameRate;
  }

  auto itemHandle = AEUtils::GetItemFromComp(compHandle);
  composition->id = AEUtils::GetItemId(itemHandle);
  RECORD_ERROR(suites.ItemSuite6()->AEGP_GetItemDimensions(itemHandle, &composition->width,
                                                           &composition->height));
  composition->duration = GetCompositionDuration(context, compHandle);
  AEGP_ColorVal bgColor = {};
  RECORD_ERROR(suites.CompSuite6()->AEGP_GetCompBGColor(compHandle, &bgColor));
  composition->backgroundColor = ExportColor(bgColor);

  if (nullptr == context->getCompItemHById(composition->id)) {
    context->compItemHList.insert(std::make_pair(composition->id, itemHandle));
  }

  return composition->id;
}

//判断是否全静止的合成。
static bool IsStaticComposition(pagexporter::Context* context, AEGP_CompH const& compHandle) {
  auto isSame = true;

  auto& suites = context->suites;
  auto itemHandle = AEUtils::GetItemFromComp(compHandle);

  A_FpLong fps = 24;
  RECORD_ERROR(suites.CompSuite6()->AEGP_GetCompFramerate(compHandle, &fps));
  float frameRate = static_cast<float>(fps);

  A_Time durationTime = {};
  RECORD_ERROR(suites.ItemSuite6()->AEGP_GetItemDuration(itemHandle, &durationTime));
  pag::Frame duration = durationTime.value * frameRate / durationTime.scale;

  AEGP_RenderOptionsH renderOptions = nullptr;
  RECORD_ERROR(suites.RenderOptionsSuite3()->AEGP_NewFromItem(context->pluginID, itemHandle,
                                                              &renderOptions));

  uint8_t* curData = nullptr;
  uint8_t* preData = nullptr;
  A_u_long stride = 0;

  for (int frame = 0; frame < duration && !context->bEarlyExit; frame++) {
    SetRenderTime(context, renderOptions, frameRate, frame);  // 设置render时间

    A_long width = 0;
    A_long height = 0;
    GetRenderFrame(curData, stride, width, height, suites, renderOptions);

    if (curData != nullptr && preData != nullptr) {
      if (!ImageIsStatic(curData, preData, width, height, stride)) {
        isSame = false;
        break;
      }
    }

    std::swap(curData, preData);
  }

  if (curData != nullptr) {
    delete curData;
  }
  if (preData != nullptr) {
    delete preData;
  }

  return isSame;
}

// 通过判断composition名字中是否包含"_BMP"来判断是否导出位图序列帧或视频序列帧，或矢量PAG
static pag::CompositionType GetCompositionType(pagexporter::Context* context,
                                               AEGP_CompH const& compHandle) {
  auto compName = AEUtils::GetCompName(compHandle);

  transform(compName.begin(), compName.end(), compName.begin(), ::tolower);
  if (compName.length() >= context->sequenceSuffix.length()) {
    auto pos = compName.find(context->sequenceSuffix,
                             compName.length() - context->sequenceSuffix.length());
    if (pos != std::string::npos) {
      if (context->sequenceType == pag::CompositionType::Video &&
          context->exportTagLevel >= static_cast<uint16_t>(pag::TagCode::VideoSequence)) {
        if (context->enableForceStaticBMP && IsStaticComposition(context, compHandle)) {
          return pag::CompositionType::Bitmap;  // 如果是全静止的预合成，则使用位图序列帧
        }
        return pag::CompositionType::Video;
      }
      if (context->exportTagLevel >= static_cast<uint16_t>(pag::TagCode::BitmapSequence)) {
        return pag::CompositionType::Bitmap;
      }
    }
  }

  return pag::CompositionType::Vector;
}

pag::Composition* ExportComposition(pagexporter::Context* context, const AEGP_ItemH& itemH) {
  auto id = AEUtils::GetItemId(itemH);
  context->compItemHList.insert(std::make_pair(id, itemH));
  AssignRecover<pag::ID> arCI(context->curCompId, id);

  auto compHandle = AEUtils::GetCompFromItem(itemH);
  auto compositionType = GetCompositionType(context, compHandle);
  if (context->bEarlyExit) {
    return nullptr;
  }

  pag::Composition* composition = nullptr;
  switch (compositionType) {
    case pag::CompositionType::Video:
      composition = static_cast<pag::Composition*>(ExportVideoComposition(context, compHandle));
      break;
    case pag::CompositionType::Bitmap:
      composition = static_cast<pag::Composition*>(ExportBitmapComposition(context, compHandle));
      break;
    case pag::CompositionType::Vector:
    default:
      composition = static_cast<pag::Composition*>(ExportVectorComposition(context, compHandle));
      break;
  }
  return composition;
}
