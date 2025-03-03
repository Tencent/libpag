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
#include "ImageBytes.h"
#include "pag/types.h"
#include <AE_GeneralPlug.h>
#include <src/utils/ImageData/ImageRawData.h>
#include <webp/encode.h>
#include "src/utils/StringUtil.h"
#include "src/utils/AEUtils.h"

// 设置render时间
void SetRenderTime(pagexporter::Context* context, const AEGP_RenderOptionsH& renderOptions, float frameRate, int frame) {
  auto& suites = context->suites;

  // 计算render时间
  // frameRate乘以1000是为了frameRate为小数时的精确度（比如：23.976、29.97，以及小于1.0时）
  A_Time currentTime;
  currentTime.scale = (A_u_long)(lround(1000 * frameRate));
  currentTime.value = (A_long)(1000 * frame);

  suites.RenderOptionsSuite3()->AEGP_SetTime(renderOptions, currentTime);
  A_Time getTime;
  suites.RenderOptionsSuite3()->AEGP_GetTime(renderOptions, &getTime);
}

void GetRenderFrame(uint8_t*& rgbaBytes, A_u_long& stride,
                    A_long& width, A_long& height,
                    const AEGP_SuiteHandler& suites,
                    AEGP_RenderOptionsH& renderOptions) {
  suites.RenderOptionsSuite3()->AEGP_SetWorldType(renderOptions, AEGP_WorldType_8);

  suites.RenderOptionsSuite3()->AEGP_SetDownsampleFactor(renderOptions, 1, 1);

  AEGP_FrameReceiptH frameReceipt;
  suites.RenderSuite5()->AEGP_RenderAndCheckoutFrame(renderOptions, nullptr, nullptr, &frameReceipt);

  AEGP_WorldH imageWorld;
  suites.RenderSuite5()->AEGP_GetReceiptWorld(frameReceipt, &imageWorld);
  suites.WorldSuite3()->AEGP_GetSize(imageWorld, &width, &height);

  AEGP_WorldType worldType;
  suites.WorldSuite3()->AEGP_GetType(imageWorld, &worldType);

  PF_Pixel* pixels;
  suites.WorldSuite3()->AEGP_GetBaseAddr8(imageWorld, &pixels);
  A_u_long rowBytes;
  suites.WorldSuite3()->AEGP_GetRowBytes(imageWorld, &rowBytes);

  if (width > 0 && height > 0 && rowBytes > 0) {
    if (rgbaBytes == nullptr) {
      stride = rowBytes;
      rgbaBytes = new uint8_t[stride * height + stride * 2];
    }
    ConvertARGBToRGBA(&(pixels->alpha), width, height, rowBytes, rgbaBytes, stride);
  }

  suites.RenderSuite5()->AEGP_CheckinFrame(frameReceipt);
}

static void GetRenderImageFromImageLayer(uint8_t*& rgbaBytes, A_u_long& rowBytes,
                                         A_long& width, A_long& height,
                                         const pagexporter::Context* context, AEGP_LayerH layerHandle) {
  auto& suites = context->suites;
  auto itemHandle = AEUtils::GetItemFromLayer(layerHandle);
  AEGP_RenderOptionsH renderOptions;
  suites.RenderOptionsSuite3()->AEGP_NewFromItem(context->pluginID, itemHandle, &renderOptions);

  GetRenderFrame(rgbaBytes, rowBytes, width, height, suites, renderOptions);

  suites.RenderOptionsSuite3()->AEGP_Dispose(renderOptions);
}

/**
 * 生成视频黑色占位图信息
 * @param rgbaBytes 获取到的占位图 rgba 信息
 * @param rowBytes 每行所有像素大小
 * @param width 占位图宽
 * @param height 占位图高
 * @param context 上下文
 * @param layerHandle 图层句柄
 */
static void GetRenderImageFromVideoLayer(uint8_t*& rgbaBytes, A_u_long& rowBytes,
                                         A_long& width, A_long& height,
                                         const pagexporter::Context* context, AEGP_LayerH& layerHandle) {
  auto& suites = context->suites;
  AEGP_LayerRenderOptionsH renderOptions;
  suites.LayerRenderOptionsSuite2()->AEGP_NewFromLayer(
      context->pluginID, layerHandle, &renderOptions);

  suites.LayerRenderOptionsSuite2()->AEGP_SetWorldType(renderOptions, AEGP_WorldType_8);

  suites.LayerRenderOptionsSuite2()->AEGP_SetDownsampleFactor(renderOptions, 1, 1);

  AEGP_FrameReceiptH frameReceipt;
  suites.RenderSuite5()->AEGP_RenderAndCheckoutLayerFrame(
      renderOptions, nullptr, nullptr, &frameReceipt);

  AEGP_WorldH imageWorld;
  suites.RenderSuite5()->AEGP_GetReceiptWorld(frameReceipt, &imageWorld);
  suites.WorldSuite3()->AEGP_GetSize(imageWorld, &width, &height);
  suites.WorldSuite3()->AEGP_GetRowBytes(imageWorld, &rowBytes);

  // 生成全黑像素图片数据
  if (width > 0 && height > 0 && rowBytes > 0) {
    rgbaBytes = new uint8_t[rowBytes * height + rowBytes * 2];
    for (int y = 0; y < height; y++) {
      auto src = rgbaBytes + rowBytes * y;
      for (int x = 0; x < width; x++) {
        src[0] = 0; // R
        src[1] = 0; // G
        src[2] = 0; // B
        src[3] = 255; // A
        src += 4;
      }
    }
  }

  suites.RenderSuite5()->AEGP_CheckinFrame(frameReceipt);
}

void GetRenderImageFromLayer(uint8_t*& rgbaBytes, A_u_long& rowBytes,
                             A_long& width, A_long& height,
                             pagexporter::Context& context, AEGP_LayerH layerHandle,
                             bool isVideo) {
  // 区分是视频占位图层还是普通的图片图层
  if (isVideo) {
    GetRenderImageFromVideoLayer(rgbaBytes, rowBytes, width, height, &context, layerHandle);
  } else {
    GetRenderImageFromImageLayer(rgbaBytes, rowBytes, width, height, &context, layerHandle);
  }
}

void GetRenderImageFromComposition(uint8_t*& rgbaBytes, A_u_long& rowBytes,
                                   A_long& width, A_long& height,
                                   pagexporter::Context& context, AEGP_ItemH itemH,
                                   float frameRate, int frame) {
  auto& suites = context.suites;
  auto pluginID = context.pluginID;
  AEGP_RenderOptionsH renderOptions = nullptr;
  suites.RenderOptionsSuite3()->AEGP_NewFromItem(pluginID, itemH, &renderOptions);
  // 设置输出格式为8比特位图，避免出现16比特/32比特位深的情况
  suites.RenderOptionsSuite3()->AEGP_SetWorldType(renderOptions, AEGP_WorldType_8);
  SetRenderTime(&context, renderOptions, frameRate, frame); // 设置render时间
  GetRenderFrame(rgbaBytes, rowBytes, width, height, suites, renderOptions);
  suites.RenderOptionsSuite3()->AEGP_Dispose(renderOptions);
}

pag::ImageBytes* ExportImageBytes(pagexporter::Context* context, const AEGP_LayerH& layerHandle, bool isVideo) {
  auto itemHandle = AEUtils::GetItemFromLayer(layerHandle);
  auto imageID = AEUtils::GetItemId(itemHandle);
  for (auto& bytes : context->imageBytesList) {
    if (bytes->id == imageID) {
      return bytes;
    }
  }

  // 暂时不导出位图，仅记录id和handle，延后再导出
  auto imageBytes = new pag::ImageBytes();
  imageBytes->id = static_cast<pag::ID>(imageID);
  context->imageBytesList.push_back(imageBytes);
  context->reFactorList.push_back(1);
  std::pair<AEGP_LayerH, bool> pair(layerHandle, isVideo);
  context->imageLayerHList.push_back(pair);
  return imageBytes;
}

void ReExportImageBytes(pagexporter::Context* context, int index, float factor) {
  auto layerHandlePair = context->imageLayerHList[index];
  auto layerHandle = layerHandlePair.first;
  auto imageBytes = context->imageBytesList[index];

  A_long width = 0;
  A_long height = 0;
  uint8_t* rgbaBytes = nullptr;
  A_u_long rowBytes = 0;
  GetRenderImageFromLayer(rgbaBytes, rowBytes, width, height,
                          *context, layerHandle, layerHandlePair.second);

  if (width > 0 && height > 0 && rowBytes > 0) {
    ImageRect rect = {0, 0, width, height};
    if (context->exportTagLevel >= static_cast<uint16_t>(pag::TagCode::ImageBytesV3)) {
      ClipTransparentEdge(rect, rgbaBytes, width, height, rowBytes);
    }

    uint8_t* output = nullptr;
    size_t size = 0;

    auto scaledWidth = (int)ceil(rect.width * factor);
    auto scaledHeight = (int)ceil(rect.height * factor);
    if (scaledWidth == rect.width && scaledHeight == rect.height) {
      size = WebPEncodeRGBA(rgbaBytes + rect.yPos * rowBytes + rect.xPos * 4, scaledWidth, scaledHeight, rowBytes,
                            context->imageQuality,
                            &output);
    } else {
      int stride = scaledWidth * 4;
      auto scaledRGBA = new uint8_t[stride * scaledHeight + stride * 2];
      ScaleCoreGraphics(scaledRGBA, stride, rgbaBytes + rect.yPos * rowBytes + rect.xPos * 4, rowBytes, scaledWidth,
                        scaledHeight, rect.width, rect.height); // 缩放

      size = WebPEncodeRGBA(scaledRGBA, scaledWidth, scaledHeight, stride, context->imageQuality, &output);

      delete[] scaledRGBA;
    }

    if (size > 0 && output != nullptr) {
      auto bytes = new uint8_t[size];
      memcpy(bytes, output, size);

      imageBytes->width = width;
      imageBytes->height = height;
      imageBytes->anchorX = -rect.xPos;
      imageBytes->anchorY = -rect.yPos;
      imageBytes->scaleFactor = factor;
      imageBytes->fileBytes = pag::ByteData::MakeAdopted(bytes, size).release();
    }
    WebPFree(output);
    delete[] rgbaBytes;
  }
}
