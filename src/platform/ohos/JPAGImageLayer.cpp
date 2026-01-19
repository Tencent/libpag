/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2024 Tencent. All rights reserved.
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

#include "JPAGImage.h"
#include "JPAGLayerHandle.h"
#include "JsHelper.h"
#include "pag/pag.h"

namespace pag {
static std::shared_ptr<PAGImageLayer> FromJs(napi_env env, napi_value value) {
  auto layer = JPAGLayerHandle::FromJs(env, value);
  if (layer && layer->layerType() == LayerType::Image) {
    return std::static_pointer_cast<PAGImageLayer>(layer);
  }
  return nullptr;
}

static napi_value Make(napi_env env, napi_callback_info info) {
  size_t argc = 3;
  napi_value args[3];
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  int32_t width = 0;
  napi_get_value_int32(env, args[0], &width);
  int32_t height = 0;
  napi_get_value_int32(env, args[1], &height);
  int32_t duration = 0;
  napi_get_value_int32(env, args[2], &duration);
  return JPAGLayerHandle::ToJs(env, PAGImageLayer::Make(width, height, duration));
}

static napi_value GetVideoRanges(napi_env env, napi_callback_info info) {
  napi_value jsLayer = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsLayer, nullptr);
  auto pagImageLayer = FromJs(env, jsLayer);
  if (!pagImageLayer) {
    return nullptr;
  }
  return CreateVideoRanges(env, pagImageLayer->getVideoRanges());
}

static napi_value SetImage(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_value jsLayer;
  napi_get_cb_info(env, info, &argc, args, &jsLayer, nullptr);
  auto pagImageLayer = FromJs(env, jsLayer);
  if (pagImageLayer == nullptr) {
    return nullptr;
  }
  auto image = JPAGImage::FromJs(env, args[0]);
  pagImageLayer->setImage(image);
  return nullptr;
}

static napi_value ContentDuration(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsLayer;
  napi_get_cb_info(env, info, &argc, args, &jsLayer, nullptr);
  auto pagImageLayer = FromJs(env, jsLayer);
  if (pagImageLayer == nullptr) {
    return nullptr;
  }
  napi_value result;
  napi_create_int64(env, pagImageLayer->contentDuration(), &result);
  return result;
}

static napi_value ImageBytes(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsLayer;
  napi_get_cb_info(env, info, &argc, args, &jsLayer, nullptr);
  auto pagImageLayer = FromJs(env, jsLayer);
  if (pagImageLayer == nullptr) {
    return nullptr;
  }
  napi_value arraybuffer;
  void* data = nullptr;
  auto imageBytes = pagImageLayer->imageBytes();
  napi_create_arraybuffer(env, imageBytes->length(), &data, &arraybuffer);
  memcpy(data, imageBytes->data(), imageBytes->length());
  return arraybuffer;
}

bool JPAGLayerHandle::InitPAGImageLayerEnv(napi_env env, napi_value exports) {
  napi_property_descriptor classProp[] = {
      PAG_STATIC_METHOD_ENTRY(Make, Make), PAG_DEFAULT_METHOD_ENTRY(getVideoRanges, GetVideoRanges),
      PAG_DEFAULT_METHOD_ENTRY(setImage, SetImage),
      PAG_DEFAULT_METHOD_ENTRY(contentDuration, ContentDuration),
      PAG_DEFAULT_METHOD_ENTRY(imageBytes, ImageBytes)};

  auto status = DefineClass(env, exports, GetLayerClassName(LayerType::Image),
                            sizeof(classProp) / sizeof(classProp[0]), classProp, Constructor,
                            GetBaseClassName());
  return status == napi_status::napi_ok;
}

}  // namespace pag
