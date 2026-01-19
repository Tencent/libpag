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

#include "JsHelper.h"
#include <multimedia/image_framework/image/pixelmap_native.h>
#include <multimedia/image_framework/image_pixel_map_mdk.h>
#include "base/utils/Log.h"
#include "tgfx/core/ImageInfo.h"

namespace pag {

static std::unordered_map<std::string, napi_ref> ConstructorRefMap;

bool SetConstructor(napi_env env, napi_value constructor, const std::string& name) {
  if (env == nullptr || constructor == nullptr || name.empty()) {
    return false;
  }
  if (ConstructorRefMap.find(name) != ConstructorRefMap.end()) {
    napi_delete_reference(env, ConstructorRefMap.at(name));
    ConstructorRefMap.erase(name);
  }
  napi_ref ref;
  napi_create_reference(env, constructor, 1, &ref);
  ConstructorRefMap[name] = ref;
  return true;
}

napi_value GetConstructor(napi_env env, const std::string& name) {
  if (env == nullptr || name.empty()) {
    return nullptr;
  }
  napi_value result = nullptr;
  if (ConstructorRefMap.find(name) != ConstructorRefMap.end()) {
    napi_get_reference_value(env, ConstructorRefMap.at(name), &result);
  }
  return result;
}

napi_status ExtendClass(napi_env env, napi_value constructor, const std::string& parentName) {
  if (env == nullptr || constructor == nullptr || parentName.empty()) {
    return napi_status::napi_invalid_arg;
  }
  napi_value baseConstructor = GetConstructor(env, parentName);
  if (baseConstructor == nullptr) {
    LOGE("ExtendClass get baseConstructor failed status");
    return napi_status::napi_invalid_arg;
  }
  napi_value basePrototype;
  napi_status statusCode =
      napi_get_named_property(env, baseConstructor, "prototype", &basePrototype);
  if (statusCode != napi_status::napi_ok) {
    LOGE("ExtendClass get baseConstructor's prototype  failed status :%d", statusCode);
    return statusCode;
  }
  napi_value derivedPrototype;
  statusCode = napi_get_named_property(env, constructor, "prototype", &derivedPrototype);
  if (statusCode != napi_status::napi_ok) {
    LOGE("ExtendClass get constructor's prototype  failed status :%d", statusCode);
    return statusCode;
  }
  return napi_set_named_property(env, derivedPrototype, "__proto__", basePrototype);
}

napi_status DefineClass(napi_env env, napi_value exports, const std::string& utf8name,
                        size_t propertyCount, const napi_property_descriptor* properties,
                        napi_callback constructor, const std::string& parentName) {
  napi_value classConstructor;
  auto status = napi_define_class(env, utf8name.c_str(), utf8name.length(), constructor, nullptr,
                                  propertyCount, properties, &classConstructor);
  if (status != napi_status::napi_ok) {
    LOGE("DefineClass napi_define_class failed:%d", status);
    return status;
  }
  status = napi_set_named_property(env, exports, utf8name.c_str(), classConstructor);
  if (status != napi_status::napi_ok) {
    LOGE("DefineClass napi_set_named_property failed:%d", status);
    return status;
  }
  SetConstructor(env, classConstructor, utf8name);
  if (parentName.empty()) {
    return status;
  }
  status = ExtendClass(env, classConstructor, parentName);
  if (status != napi_status::napi_ok) {
    LOGE("DefineClass ExtendClass failed:%d", status);
    return status;
  }
  return status;
}

napi_value NewInstance(napi_env env, const std::string& name, void* handler) {
  if (env == nullptr || handler == nullptr || name.empty()) {
    return nullptr;
  }
  napi_value constructor = GetConstructor(env, name);
  if (constructor == nullptr) {
    return nullptr;
  }
  napi_value result = nullptr;
  napi_value external[1];
  auto status = napi_create_external(env, handler, nullptr, nullptr, &external[0]);
  if (status != napi_ok) {
    LOGE("NewInstance napi_create_external failed :%d", status);
    return nullptr;
  }
  status = napi_new_instance(env, constructor, 1, external, &result);
  if (status != napi_ok) {
    LOGE("NewInstance napi_new_instance failed :%d", status);
    return nullptr;
  }
  return result;
}

napi_value CreateMarker(napi_env env, const Marker* marker) {
  if (env == nullptr || marker == nullptr) {
    return nullptr;
  }
  napi_value result = nullptr;
  auto status = napi_create_object(env, &result);
  if (status != napi_ok) {
    LOGE("CreateMarker napi_create_object failed :%d", status);
    return nullptr;
  }
  napi_value startTime;
  status = napi_create_int64(env, marker->startTime, &startTime);
  if (status != napi_ok) {
    LOGE("CreateMarker napi_create_int64 failed :%d", status);
    return nullptr;
  }
  status = napi_set_named_property(env, result, "startTime", startTime);
  if (status != napi_ok) {
    LOGE("CreateMarker napi_set_named_property failed :%d", status);
    return nullptr;
  }
  napi_value duration;
  status = napi_create_int64(env, marker->duration, &duration);
  if (status != napi_ok) {
    LOGE("CreateMarker napi_create_int64 failed :%d", status);
    return nullptr;
  }
  status = napi_set_named_property(env, result, "duration", duration);
  if (status != napi_ok) {
    LOGE("CreateMarker napi_set_named_property failed :%d", status);
    return nullptr;
  }
  napi_value comment;
  status =
      napi_create_string_utf8(env, marker->comment.c_str(), marker->comment.length(), &comment);
  if (status != napi_ok) {
    LOGE("CreateMarker napi_create_string_utf8 failed :%d", status);
    return nullptr;
  }
  status = napi_set_named_property(env, result, "comment", comment);
  if (status != napi_ok) {
    LOGE("CreateMarker napi_set_named_property failed :%d", status);
    return nullptr;
  }
  return result;
}

napi_value CreateMarkers(napi_env env, const std::vector<const Marker*>& markers) {
  napi_value result;
  auto status = napi_create_array_with_length(env, markers.size(), &result);
  if (status != napi_ok) {
    return nullptr;
  }
  for (size_t i = 0; i < markers.size(); i++) {
    auto jsMarker = CreateMarker(env, markers[i]);
    if (jsMarker == nullptr) {
      return nullptr;
    }
    napi_set_element(env, result, i, jsMarker);
  }
  return result;
}

napi_value CreateRect(napi_env env, const Rect& rect) {
  if (env == nullptr) {
    return nullptr;
  }
  napi_value result = nullptr;
  auto status = napi_create_array_with_length(env, 4, &result);
  if (status != napi_ok) {
    LOGE("CreateRect napi_create_array_with_length failed :%d", status);
    return nullptr;
  }
  napi_value l;
  status = napi_create_double(env, rect.left, &l);
  if (status != napi_ok) {
    LOGE("CreateRect napi_create_int64 failed :%d", status);
    return nullptr;
  }
  napi_value t;
  status = napi_create_double(env, rect.top, &t);
  if (status != napi_ok) {
    LOGE("CreateRect napi_create_double failed :%d", status);
    return nullptr;
  }
  napi_value r;
  status = napi_create_double(env, rect.right, &r);
  if (status != napi_ok) {
    LOGE("CreateRect napi_create_double failed :%d", status);
    return nullptr;
  }

  napi_value b;
  status = napi_create_double(env, rect.bottom, &b);
  if (status != napi_ok) {
    LOGE("CreateRect napi_create_double failed :%d", status);
    return nullptr;
  }

  napi_value valueArray[] = {l, t, r, b};
  for (uint32_t i = 0; i < 4; i++) {
    napi_set_element(env, result, i, valueArray[i]);
  }
  return result;
}

Rect GetRect(napi_env env, napi_value value) {
  uint32_t length = 0;
  auto status = napi_get_array_length(env, value, &length);
  if (status != napi_ok) {
    LOGE("GetRect napi_get_array_length failed :%d", status);
    return {};
  }
  if (length != 4) {
    LOGE("GetRect array length != 4");
    return {};
  }
  double array[] = {0.0, 0.0, 0.0, 0.0};
  for (uint32_t i = 0; i < 4; i++) {
    napi_value jsValue = nullptr;
    status = napi_get_element(env, value, i, &jsValue);
    if (status != napi_ok) {
      LOGE("GetRect get values[%u] failed :%d", i, status);
      return {};
    }
    status = napi_get_value_double(env, jsValue, &array[i]);
    if (status != napi_ok) {
      LOGE("GetRect get values[%u] value failed :%d", i, status);
      return {};
    }
  }
  return Rect::MakeXYWH(array[0], array[1], array[2], array[3]);
}

napi_value CreateMatrix(napi_env env, const Matrix& matrix) {
  if (env == nullptr) {
    return nullptr;
  }
  float buffer[9];
  matrix.get9(buffer);
  napi_value result = nullptr;
  auto status = napi_create_array_with_length(env, 9, &result);
  if (status != napi_ok) {
    LOGE("CreateMatrix napi_create_array_with_length failed :%d", status);
    return nullptr;
  }
  for (size_t i = 0; i < 9; i++) {
    napi_value ele;
    status = napi_create_double(env, buffer[i], &ele);
    if (status != napi_ok) {
      LOGE("CreateMatrix napi_create_int64 failed :%d", status);
      return nullptr;
    }
    status = napi_set_element(env, result, i, ele);
    if (status != napi_ok) {
      LOGE("CreateMatrix napi_set_element failed :%d", status);
      return nullptr;
    }
  }
  return result;
}

Matrix GetMatrix(napi_env env, napi_value value) {
  if (env == nullptr || value == nullptr) {
    return {};
  }

  Matrix result;
  for (size_t i = 0; i < 9; i++) {
    napi_value ele;
    auto status = napi_get_element(env, value, i, &ele);
    if (status != napi_ok) {
      LOGE("GetMatrix napi_get_element failed :%d", status);
      return {};
    }
    double val = 0;
    status = napi_get_value_double(env, ele, &val);
    if (status != napi_ok) {
      LOGE("GetMatrix napi_get_value_double failed :%d", status);
      return {};
    }
    result.set(i, val);
  }
  return result;
}

napi_value CreateTextDocument(napi_env, TextDocumentHandle) {
  return nullptr;
}

TextDocumentHandle GetTextDocument(napi_env, napi_value) {
  return nullptr;
}

napi_value MakeSnapshot(napi_env env, PAGSurface* surface) {
  if (surface == nullptr) {
    return nullptr;
  }
  tgfx::ImageInfo imageInfo =
      tgfx::ImageInfo::Make(surface->width(), surface->height(), tgfx::ColorType::BGRA_8888);

  auto pixels = ByteData::Make(imageInfo.byteSize());
  if (pixels == nullptr) {
    return nullptr;
  }
  if (!surface->readPixels(ColorType::BGRA_8888, AlphaType::Premultiplied, pixels->data(),
                           imageInfo.rowBytes())) {
    return nullptr;
  }

  OhosPixelMapCreateOps ops;
  ops.width = imageInfo.width();
  ops.height = imageInfo.height();
  ops.pixelFormat = PIXEL_FORMAT_BGRA_8888;
  ops.alphaType = OHOS_PIXEL_MAP_ALPHA_TYPE_PREMUL;
  ops.editable = true;
  napi_value pixelMap;
  auto status = OH_PixelMap_CreatePixelMapWithStride(env, ops, pixels->data(), imageInfo.byteSize(),
                                                     imageInfo.rowBytes(), &pixelMap);
  if (status == napi_ok) {
    return pixelMap;
  }
  return nullptr;
}

int MakeColorInt(uint32_t red, uint32_t green, uint32_t blue) {
  int color = (255 << 24) | (red << 16) | (green << 8) | (blue << 0);
  return color;
}

Color ToColor(int value) {
  auto color = static_cast<uint32_t>(value);
  auto red = (((color) >> 16) & 0xFF);
  auto green = (((color) >> 8) & 0xFF);
  auto blue = (((color) >> 0) & 0xFF);
  return {static_cast<uint8_t>(red), static_cast<uint8_t>(green), static_cast<uint8_t>(blue)};
}

napi_value CreateVideoRange(napi_env env, const PAGVideoRange& videoRange) {
  if (env == nullptr) {
    return nullptr;
  }
  napi_value result = nullptr;
  auto status = napi_create_object(env, &result);
  if (status != napi_ok) {
    LOGI("CreateVideoRange napi_create_object failed :%d", status);
    return nullptr;
  }
  napi_value startTime;
  status = napi_create_int64(env, videoRange.startTime(), &startTime);
  if (status != napi_ok) {
    LOGI("CreateVideoRange napi_create_int64 failed :%d", status);
    return nullptr;
  }
  status = napi_set_named_property(env, result, "startTime", startTime);
  if (status != napi_ok) {
    LOGI("CreateVideoRange napi_set_named_property failed :%d", status);
    return nullptr;
  }
  napi_value endTime;
  status = napi_create_int64(env, videoRange.endTime(), &endTime);
  if (status != napi_ok) {
    LOGI("CreateVideoRange napi_create_int64 failed :%d", status);
    return nullptr;
  }
  status = napi_set_named_property(env, result, "endTime", endTime);
  if (status != napi_ok) {
    LOGI("CreateVideoRange napi_set_named_property failed :%d", status);
    return nullptr;
  }
  napi_value playDuration;
  status = napi_create_int64(env, videoRange.playDuration(), &playDuration);
  if (status != napi_ok) {
    LOGI("CreateVideoRange napi_create_int64 failed :%d", status);
    return nullptr;
  }
  status = napi_set_named_property(env, result, "playDuration", playDuration);
  if (status != napi_ok) {
    LOGI("CreateVideoRange napi_set_named_property failed :%d", status);
    return nullptr;
  }
  napi_value reversed;
  status = napi_get_boolean(env, videoRange.reversed(), &reversed);
  if (status != napi_ok) {
    LOGI("CreateVideoRange napi_get_boolean failed :%d", status);
    return nullptr;
  }
  status = napi_set_named_property(env, result, "reversed", reversed);
  if (status != napi_ok) {
    LOGI("CreateVideoRange napi_set_named_property failed :%d", status);
    return nullptr;
  }
  return result;
}

napi_value CreateVideoRanges(napi_env env, const std::vector<PAGVideoRange>& videoRanges) {
  napi_value result;
  auto status = napi_create_array_with_length(env, videoRanges.size(), &result);
  if (status != napi_ok) {
    return nullptr;
  }
  for (size_t i = 0; i < videoRanges.size(); i++) {
    auto jsVideoRange = CreateVideoRange(env, videoRanges[i]);
    if (jsVideoRange == nullptr) {
      return nullptr;
    }
    napi_set_element(env, result, i, jsVideoRange);
  }
  return result;
}

std::shared_ptr<ByteData> LoadDataFromAsset(NativeResourceManager* mNativeResMgr, char* srcBuf) {
  RawFile* rawFile = OH_ResourceManager_OpenRawFile(mNativeResMgr, srcBuf);
  if (rawFile == NULL) {
    return nullptr;
  }
  long len = OH_ResourceManager_GetRawFileSize(rawFile);
  auto data = ByteData::Make(len);
  if (!data) {
    OH_ResourceManager_CloseRawFile(rawFile);
    return nullptr;
  }
  OH_ResourceManager_ReadRawFile(rawFile, data->data(), len);
  OH_ResourceManager_CloseRawFile(rawFile);
  return data;
}

}  // namespace pag
