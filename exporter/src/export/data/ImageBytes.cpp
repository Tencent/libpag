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

#include "ImageBytes.h"
#include "export/stream/StreamProperty.h"
#include "utils/ImageData.h"
#include "utils/PAGExportSession.h"

namespace exporter {

static void GetVideoLayerRenderImageSize(const AEGP_LayerH& layerHandle, A_u_long& srcStride,
                                         A_long& width, A_long& height) {
  const auto& Suites = GetSuites();
  const auto& PluginID = GetPluginID();

  AEGP_LayerRenderOptionsH renderOptions = nullptr;
  Suites->LayerRenderOptionsSuite2()->AEGP_NewFromLayer(PluginID, layerHandle, &renderOptions);
  if (renderOptions == nullptr) {
    return;
  }
  // AEGP_NewFromLayer uses the current time indicator position which may be outside the layer's
  // visible range. Explicitly set the time to the layer's in point in layer time to ensure the
  // first visible frame is rendered correctly (including footage slip).
  A_Time inPointLayer = {};
  Suites->LayerSuite6()->AEGP_GetLayerInPoint(layerHandle, AEGP_LTimeMode_LayerTime, &inPointLayer);
  Suites->LayerRenderOptionsSuite2()->AEGP_SetTime(renderOptions, inPointLayer);
  GetLayerRenderFrameSize(renderOptions, srcStride, width, height);
  Suites->LayerRenderOptionsSuite2()->AEGP_Dispose(renderOptions);
}

static void GetVideoLayerRenderImage(uint8* rgbaBytes, const AEGP_LayerH& layerHandle,
                                     A_u_long srcStride, A_u_long dstStride, A_long width,
                                     A_long height) {
  const auto& Suites = GetSuites();
  const auto& PluginID = GetPluginID();

  AEGP_LayerRenderOptionsH renderOptions = nullptr;
  Suites->LayerRenderOptionsSuite2()->AEGP_NewFromLayer(PluginID, layerHandle, &renderOptions);
  if (renderOptions == nullptr) {
    return;
  }
  A_Time inPointLayer = {};
  Suites->LayerSuite6()->AEGP_GetLayerInPoint(layerHandle, AEGP_LTimeMode_LayerTime, &inPointLayer);
  Suites->LayerRenderOptionsSuite2()->AEGP_SetTime(renderOptions, inPointLayer);
  GetLayerRenderFrame(rgbaBytes, srcStride, dstStride, width, height, renderOptions);
  Suites->LayerRenderOptionsSuite2()->AEGP_Dispose(renderOptions);
}

static void GetImageLayerRenderImageSize(const AEGP_LayerH& layerHandle, A_u_long& srcStride,
                                         A_long& width, A_long& height) {
  const auto& Suites = GetSuites();
  const auto& PluginID = GetPluginID();

  AEGP_ItemH itemHandle = GetLayerItemH(layerHandle);
  AEGP_RenderOptionsH renderOptions = nullptr;
  Suites->RenderOptionsSuite3()->AEGP_NewFromItem(PluginID, itemHandle, &renderOptions);
  if (renderOptions == nullptr) {
    return;
  }
  GetRenderFrameSize(renderOptions, srcStride, width, height);
  Suites->RenderOptionsSuite3()->AEGP_Dispose(renderOptions);
}

static void GetImageLayerRenderImage(uint8* rgbaBytes, const AEGP_LayerH& layerHandle,
                                     A_u_long srcStride, A_u_long dstStride, A_long width,
                                     A_long height) {
  const auto& Suites = GetSuites();
  const auto& PluginID = GetPluginID();

  AEGP_ItemH itemHandle = GetLayerItemH(layerHandle);
  AEGP_RenderOptionsH renderOptions = nullptr;
  Suites->RenderOptionsSuite3()->AEGP_NewFromItem(PluginID, itemHandle, &renderOptions);
  if (renderOptions == nullptr) {
    return;
  }
  GetRenderFrame(rgbaBytes, srcStride, dstStride, width, height, renderOptions);
  Suites->RenderOptionsSuite3()->AEGP_Dispose(renderOptions);
}

// Returns the masked bounds offset of a video layer in layer space. When a video layer has masks,
// AEGP_RenderAndCheckoutLayerFrame returns a world cropped to the mask bounding box. This function
// retrieves that bounding box so we can record the correct offset in the exported imageBytes.
static void GetVideoLayerMaskedOffset(const AEGP_LayerH& layerHandle, A_long& offsetX,
                                      A_long& offsetY) {
  const auto& Suites = GetSuites();
  A_Time inPoint = {};
  Suites->LayerSuite6()->AEGP_GetLayerInPoint(layerHandle, AEGP_LTimeMode_CompTime, &inPoint);
  A_FloatRect maskedBounds = {};
  Suites->LayerSuite8()->AEGP_GetLayerMaskedBounds(layerHandle, AEGP_LTimeMode_CompTime, &inPoint,
                                                   &maskedBounds);
  // Floor toward negative infinity so masks with negative fractional bounds match AE's pixel-floor
  // crop origin.
  offsetX = static_cast<A_long>(floor(maskedBounds.left));
  offsetY = static_cast<A_long>(floor(maskedBounds.top));
}

void GetImageBytes(std::shared_ptr<PAGExportSession> session, pag::ImageBytes* imageBytes,
                   const AEGP_LayerH& layerHandle, bool isVideo, float factor) {
  A_long width = 0;
  A_long height = 0;
  A_u_long srcStride = 0;
  A_u_long dstStride = 0;
  uint8* rgbaBytes = nullptr;
  if (isVideo) {
    GetVideoLayerRenderImageSize(layerHandle, srcStride, width, height);
  } else {
    GetImageLayerRenderImageSize(layerHandle, srcStride, width, height);
  }
  if (width < 0 || height < 0) {
    return;
  }
  dstStride = width * 4;
  rgbaBytes = new uint8[dstStride * height];
  if (isVideo) {
    GetVideoLayerRenderImage(rgbaBytes, layerHandle, srcStride, dstStride, width, height);
  } else {
    GetImageLayerRenderImage(rgbaBytes, layerHandle, srcStride, dstStride, width, height);
  }

  ImageRect rect = {0, 0, width, height};
  if (session->configParam.isTagCodeSupport(pag::TagCode::ImageBytesV3)) {
    ClipTransparentEdge(rect, rgbaBytes, width, height, dstStride);
  }

  uint8_t* data = nullptr;
  uint8_t* scaledRGBA = nullptr;
  int theStride = 0;
  auto scaledWidth = static_cast<int>(ceil(rect.width * factor));
  auto scaledHeight = static_cast<int>(ceil(rect.height * factor));
  if (scaledWidth == rect.width && scaledHeight == rect.height) {
    data = rgbaBytes + rect.yPos * dstStride + rect.xPos * 4;
    theStride = dstStride;
  } else {
    theStride = scaledWidth * 4;
    scaledRGBA = new uint8_t[theStride * scaledHeight + theStride * 2];
    ScalePixels(scaledRGBA, theStride, scaledWidth, scaledHeight,
                rgbaBytes + rect.yPos * dstStride + rect.xPos * 4, dstStride, rect.width,
                rect.height);
    data = scaledRGBA;
  }

  auto byteData = EncodeImageData(data, scaledWidth, scaledHeight, theStride,
                                  session->configParam.imageQuality);
  if (byteData != nullptr) {
    A_long maskedOffsetX = 0;
    A_long maskedOffsetY = 0;
    A_long sourceWidth = width;
    A_long sourceHeight = height;
    if (isVideo) {
      // For video layers, Layer Render may crop the output to the mask bounding box. We need to
      // use the source item dimensions as the logical content size and record the mask offset so
      // the player can position the placeholder image correctly in layer space.
      AEGP_ItemH itemHandle = GetLayerItemH(layerHandle);
      auto dimensions = GetItemDimensions(itemHandle);
      sourceWidth = dimensions.width();
      sourceHeight = dimensions.height();
      if (sourceWidth != width || sourceHeight != height) {
        GetVideoLayerMaskedOffset(layerHandle, maskedOffsetX, maskedOffsetY);
      }
    }
    imageBytes->width = sourceWidth;
    imageBytes->height = sourceHeight;
    imageBytes->anchorX = -(rect.xPos + maskedOffsetX);
    imageBytes->anchorY = -(rect.yPos + maskedOffsetY);
    imageBytes->scaleFactor = factor;
    imageBytes->fileBytes = byteData;
  }

  if (scaledRGBA != nullptr) {
    delete[] scaledRGBA;
  }
  if (rgbaBytes != nullptr) {
    delete[] rgbaBytes;
  }
}

}  // namespace exporter
