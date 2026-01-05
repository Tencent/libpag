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

#include "JPAGImage.h"
#include <js_native_api.h>
#include <rawfile/raw_file.h>
#include <rawfile/raw_file_manager.h>
#include "JsHelper.h"
#include "rendering/editing/StillImage.h"
#include "tgfx/platform/ohos/OHOSPixelMap.h"

namespace pag {

static napi_value FromPath(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  char path[1024];
  size_t pathLength = 0;
  napi_get_value_string_utf8(env, args[0], path, 1024, &pathLength);
  return JPAGImage::ToJs(env, PAGImage::FromPath(path));
}

static napi_value FromBytes(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  size_t length;
  void* data;
  auto code = napi_get_arraybuffer_info(env, args[0], &data, &length);
  if (code != napi_ok) {
    return nullptr;
  }
  return JPAGImage::ToJs(env, PAGImage::FromBytes(data, length));
  ;
}

static napi_value FromPixelMap(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  auto bitmap = tgfx::OHOSPixelMap::CopyBitmap(env, args[0]);
  auto image = tgfx::Image::MakeFrom(bitmap);
  if (image == nullptr) {
    return nullptr;
  }
  auto pagImage = pag::StillImage::MakeFrom(std::move(image));
  return JPAGImage::ToJs(env, pagImage);
}

static napi_value LoadFromAssets(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2] = {nullptr};
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  NativeResourceManager* mNativeResMgr = OH_ResourceManager_InitNativeResourceManager(env, args[0]);
  size_t strSize;
  char srcBuf[1024];
  napi_get_value_string_utf8(env, args[1], srcBuf, sizeof(srcBuf), &strSize);
  auto data = LoadDataFromAsset(mNativeResMgr, srcBuf);
  if (data == NULL) {
    return nullptr;
  }
  return JPAGImage::ToJs(env, PAGImage::FromBytes(data->data(), data->length()));
}

static napi_value Width(napi_env env, napi_callback_info info) {
  napi_value jsPAGImage = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsPAGImage, nullptr);
  auto pagImage = JPAGImage::FromJs(env, jsPAGImage);
  if (!pagImage) {
    napi_value value;
    napi_create_int32(env, 0, &value);
    return value;
  }
  napi_value value;
  napi_create_int32(env, pagImage->width(), &value);
  return value;
}

static napi_value Height(napi_env env, napi_callback_info info) {
  napi_value jsPAGImage = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsPAGImage, nullptr);
  auto pagImage = JPAGImage::FromJs(env, jsPAGImage);
  if (!pagImage) {
    napi_value value;
    napi_create_int32(env, 0, &value);
    return value;
  }
  napi_value value;
  napi_create_int32(env, pagImage->height(), &value);
  return value;
}

static napi_value Matrix(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsPAGImage = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsPAGImage, nullptr);
  auto pagImage = JPAGImage::FromJs(env, jsPAGImage);
  if (!pagImage) {
    return nullptr;
  }
  return CreateMatrix(env, pagImage->matrix());
}

static napi_value SetMatrix(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_value jsPAGImage = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsPAGImage, nullptr);
  auto pagImage = JPAGImage::FromJs(env, jsPAGImage);
  if (!pagImage) {
    return nullptr;
  }
  pagImage->setMatrix(GetMatrix(env, args[0]));
  return nullptr;
}

static napi_value ScaleMode(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsPAGImage = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsPAGImage, nullptr);
  auto pagImage = JPAGImage::FromJs(env, jsPAGImage);
  if (!pagImage) {
    return nullptr;
  }
  napi_value result;
  napi_create_int32(env, static_cast<int32_t>(pagImage->scaleMode()), &result);
  return result;
}

static napi_value SetScaleMode(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_value jsPAGImage = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsPAGImage, nullptr);
  auto pagImage = JPAGImage::FromJs(env, jsPAGImage);
  if (!pagImage) {
    return nullptr;
  }
  int value = 0;
  auto status = napi_get_value_int32(env, args[0], &value);
  if (status == napi_status::napi_ok) {
    pagImage->setScaleMode(static_cast<PAGScaleMode>(value));
  }
  return nullptr;
}

napi_value JPAGImage::Constructor(napi_env env, napi_callback_info info) {
  napi_value result = nullptr;
  size_t argc = 1;
  napi_value args[1];
  napi_get_cb_info(env, info, &argc, args, &result, nullptr);
  if (argc == 0) {
    return nullptr;
  }
  void* image = nullptr;
  napi_get_value_external(env, args[0], &image);
  napi_wrap(
      env, result, image,
      [](napi_env, void* finalize_data, void*) {
        JPAGImage* pagImage = static_cast<JPAGImage*>(finalize_data);
        delete pagImage;
      },
      nullptr, nullptr);
  return result;
}

bool JPAGImage::Init(napi_env env, napi_value exports) {
  napi_property_descriptor classProp[] = {PAG_STATIC_METHOD_ENTRY(FromPath, FromPath),
                                          PAG_STATIC_METHOD_ENTRY(FromBytes, FromBytes),
                                          PAG_STATIC_METHOD_ENTRY(FromPixelMap, FromPixelMap),
                                          PAG_STATIC_METHOD_ENTRY(LoadFromAssets, LoadFromAssets),
                                          PAG_DEFAULT_METHOD_ENTRY(width, Width),
                                          PAG_DEFAULT_METHOD_ENTRY(height, Height),
                                          PAG_DEFAULT_METHOD_ENTRY(matrix, Matrix),
                                          PAG_DEFAULT_METHOD_ENTRY(setMatrix, SetMatrix),
                                          PAG_DEFAULT_METHOD_ENTRY(scaleMode, ScaleMode),
                                          PAG_DEFAULT_METHOD_ENTRY(setScaleMode, SetScaleMode)};
  auto status = DefineClass(env, exports, ClassName(), sizeof(classProp) / sizeof(classProp[0]),
                            classProp, Constructor, "");
  return status == napi_ok;
}

std::shared_ptr<PAGImage> JPAGImage::FromJs(napi_env env, napi_value value) {
  JPAGImage* image = nullptr;
  auto status = napi_unwrap(env, value, (void**)&image);
  if (status == napi_ok) {
    return image->get();
  } else {
    return nullptr;
  }
}

napi_value JPAGImage::ToJs(napi_env env, std::shared_ptr<PAGImage> pagImage) {
  if (!pagImage) {
    return nullptr;
  }
  JPAGImage* handler = new JPAGImage(pagImage);
  napi_value result = NewInstance(env, ClassName(), handler);
  if (result == nullptr) {
    delete handler;
  }
  return result;
}
}  // namespace pag
