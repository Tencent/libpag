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

#include <rawfile/raw_file.h>
#include <rawfile/raw_file_manager.h>
#include <cstddef>
#include <cstdint>
#include "JPAGLayerHandle.h"
#include "JsHelper.h"
#include "platform/ohos/JPAGImage.h"
#include "platform/ohos/JPAGText.h"

namespace pag {

static std::shared_ptr<PAGFile> FromJs(napi_env env, napi_value value) {
  auto layer = JPAGLayerHandle::FromJs(env, value);
  if (layer && layer->isPAGFile()) {
    return std::static_pointer_cast<PAGFile>(layer);
  }
  return nullptr;
}

static napi_value MaxSupportedTagLevel(napi_env env, napi_callback_info) {
  napi_value result;
  napi_create_uint32(env, PAGFile::MaxSupportedTagLevel(), &result);
  return result;
}

static napi_value LoadFromPath(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  char filePath[1024] = {0};
  size_t length = 0;
  napi_get_value_string_utf8(env, args[0], filePath, 1024, &length);
  return JPAGLayerHandle::ToJs(env, PAGFile::Load(std::string(filePath, length)));
}

static napi_value LoadFromBytes(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2] = {nullptr};
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  size_t length;
  void* data;
  auto code = napi_get_arraybuffer_info(env, args[0], &data, &length);
  if (code != napi_ok) {
    return nullptr;
  }
  std::string path = "";
  if (argc == 2) {
    char filePath[1024] = {0};
    size_t pathLength = 0;
    napi_get_value_string_utf8(env, args[1], filePath, 1024, &pathLength);
    path = std::string(filePath, pathLength);
  }
  return JPAGLayerHandle::ToJs(env, PAGFile::Load(data, length, path));
}

static napi_value LoadFromAssets(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2] = {nullptr};
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  NativeResourceManager* mNativeResMgr = OH_ResourceManager_InitNativeResourceManager(env, args[0]);
  size_t strSize;
  char srcBuf[1024];
  napi_get_value_string_utf8(env, args[1], srcBuf, sizeof(srcBuf), &strSize);
  std::string fileName(srcBuf, strSize);
  auto data = LoadDataFromAsset(mNativeResMgr, srcBuf);
  if (data == NULL) {
    return nullptr;
  }
  return JPAGLayerHandle::ToJs(env,
                               PAGFile::Load(data->data(), data->length(), "asset://" + fileName));
}

static napi_value TagLevel(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsFile;
  napi_get_cb_info(env, info, &argc, args, &jsFile, nullptr);
  auto file = FromJs(env, jsFile);
  if (file == nullptr) {
    return nullptr;
  }
  napi_value result;
  napi_create_int32(env, file->tagLevel(), &result);
  return result;
}

static napi_value NumTexts(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsFile;
  napi_get_cb_info(env, info, &argc, args, &jsFile, nullptr);
  auto file = FromJs(env, jsFile);
  if (file == nullptr) {
    return nullptr;
  }
  napi_value result;
  napi_create_int32(env, file->numTexts(), &result);
  return result;
}

static napi_value NumImages(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsFile;
  napi_get_cb_info(env, info, &argc, args, &jsFile, nullptr);
  auto file = FromJs(env, jsFile);
  if (file == nullptr) {
    return nullptr;
  }
  napi_value result;
  napi_create_int32(env, file->numImages(), &result);
  return result;
}

static napi_value NumVideos(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsFile;
  napi_get_cb_info(env, info, &argc, args, &jsFile, nullptr);
  auto file = FromJs(env, jsFile);
  if (file == nullptr) {
    return nullptr;
  }
  napi_value result;
  napi_create_int32(env, file->numVideos(), &result);
  return result;
}

static napi_value Path(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsFile;
  napi_get_cb_info(env, info, &argc, args, &jsFile, nullptr);
  auto file = FromJs(env, jsFile);
  if (file == nullptr) {
    return nullptr;
  }
  napi_value result;
  auto path = file->path();
  napi_create_string_utf8(env, path.c_str(), path.length(), &result);
  return result;
}

static napi_value GetTextData(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_value jsFile;
  napi_get_cb_info(env, info, &argc, args, &jsFile, nullptr);
  auto file = FromJs(env, jsFile);
  if (file == nullptr) {
    return nullptr;
  }
  int index = 0;
  napi_get_value_int32(env, args[0], &index);
  return JPAGText::ToJs(env, file->getTextData(index));
}

static napi_value ReplaceText(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2] = {nullptr};
  napi_value jsFile;
  napi_get_cb_info(env, info, &argc, args, &jsFile, nullptr);
  auto file = FromJs(env, jsFile);
  if (file == nullptr) {
    return nullptr;
  }
  int index = 0;
  napi_get_value_int32(env, args[0], &index);
  auto textData = JPAGText::FromJs(env, args[1]);
  file->replaceText(index, textData);
  return nullptr;
}

static napi_value ReplaceImage(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2] = {nullptr};
  napi_value jsFile;
  napi_get_cb_info(env, info, &argc, args, &jsFile, nullptr);
  auto file = FromJs(env, jsFile);
  if (file == nullptr) {
    return nullptr;
  }
  int index = 0;
  napi_get_value_int32(env, args[0], &index);
  auto image = JPAGImage::FromJs(env, args[1]);
  file->replaceImage(index, image);
  return nullptr;
}

static napi_value ReplaceImageByName(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2] = {nullptr};
  napi_value jsFile;
  napi_get_cb_info(env, info, &argc, args, &jsFile, nullptr);
  auto file = FromJs(env, jsFile);
  if (file == nullptr) {
    return nullptr;
  }
  char str[2048];
  size_t length = 0;
  napi_get_value_string_utf8(env, args[0], str, 2048, &length);
  auto image = JPAGImage::FromJs(env, args[1]);
  file->replaceImageByName({str, length}, image);
  return nullptr;
}

static napi_value GetLayersByEditableIndex(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2] = {nullptr};
  napi_value jsFile;
  napi_get_cb_info(env, info, &argc, args, &jsFile, nullptr);
  auto file = FromJs(env, jsFile);
  if (file == nullptr) {
    return nullptr;
  }
  int editableIndex = 0;
  napi_get_value_int32(env, args[0], &editableIndex);
  int layerType = 0;
  napi_get_value_int32(env, args[1], &layerType);
  auto layers = file->getLayersByEditableIndex(editableIndex, static_cast<LayerType>(layerType));
  napi_value array;
  napi_create_array_with_length(env, layers.size(), &array);
  for (size_t i = 0; i < layers.size(); i++) {
    napi_set_element(env, array, i, JPAGLayerHandle::ToJs(env, layers[i]));
  }
  return array;
}

static napi_value GetEditableIndices(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_value jsFile;
  napi_get_cb_info(env, info, &argc, args, &jsFile, nullptr);
  auto file = FromJs(env, jsFile);
  if (file == nullptr) {
    return nullptr;
  }
  int layerType = 0;
  napi_get_value_int32(env, args[0], &layerType);
  auto editableIndices = file->getEditableIndices(static_cast<LayerType>(layerType));
  napi_value array;
  napi_create_array_with_length(env, editableIndices.size(), &array);
  for (size_t i = 0; i < editableIndices.size(); i++) {
    napi_value value;
    napi_create_int32(env, editableIndices[i], &value);
    napi_set_element(env, array, i, value);
  }
  return array;
}
static napi_value TimeStretchMode(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsFile;
  napi_get_cb_info(env, info, &argc, args, &jsFile, nullptr);
  auto file = FromJs(env, jsFile);
  if (file == nullptr) {
    return nullptr;
  }
  napi_value value;
  napi_create_int32(env, static_cast<int32_t>(file->timeStretchMode()), &value);
  return value;
}

static napi_value SetTimeStretchMode(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_value jsFile;
  napi_get_cb_info(env, info, &argc, args, &jsFile, nullptr);
  auto file = FromJs(env, jsFile);
  if (file == nullptr) {
    return nullptr;
  }
  int mode = 0;
  napi_get_value_int32(env, args[0], &mode);
  file->setTimeStretchMode(static_cast<PAGTimeStretchMode>(mode));
  return nullptr;
}
static napi_value SetDuration(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_value jsFile;
  napi_get_cb_info(env, info, &argc, args, &jsFile, nullptr);
  auto file = FromJs(env, jsFile);
  if (file == nullptr) {
    return nullptr;
  }
  int64_t duration = 0;
  napi_get_value_int64(env, args[0], &duration);
  file->setDuration(duration);
  return nullptr;
}

static napi_value CopyOriginal(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsFile;
  napi_get_cb_info(env, info, &argc, args, &jsFile, nullptr);
  auto file = FromJs(env, jsFile);
  if (file == nullptr) {
    return nullptr;
  }
  return JPAGLayerHandle::ToJs(env, file->copyOriginal());
}

bool JPAGLayerHandle::InitPAGFileEnv(napi_env env, napi_value exports) {

  napi_property_descriptor classProp[] = {
      PAG_STATIC_METHOD_ENTRY(MaxSupportedTagLevel, MaxSupportedTagLevel),
      PAG_STATIC_METHOD_ENTRY(LoadFromPath, LoadFromPath),
      PAG_STATIC_METHOD_ENTRY(LoadFromBytes, LoadFromBytes),
      PAG_STATIC_METHOD_ENTRY(LoadFromAssets, LoadFromAssets),

      PAG_DEFAULT_METHOD_ENTRY(tagLevel, TagLevel),
      PAG_DEFAULT_METHOD_ENTRY(numTexts, NumTexts),
      PAG_DEFAULT_METHOD_ENTRY(numImages, NumImages),
      PAG_DEFAULT_METHOD_ENTRY(numVideos, NumVideos),
      PAG_DEFAULT_METHOD_ENTRY(path, Path),
      PAG_DEFAULT_METHOD_ENTRY(getTextData, GetTextData),
      PAG_DEFAULT_METHOD_ENTRY(replaceText, ReplaceText),
      PAG_DEFAULT_METHOD_ENTRY(replaceImage, ReplaceImage),
      PAG_DEFAULT_METHOD_ENTRY(replaceImageByName, ReplaceImageByName),
      PAG_DEFAULT_METHOD_ENTRY(getLayersByEditableIndex, GetLayersByEditableIndex),
      PAG_DEFAULT_METHOD_ENTRY(getEditableIndices, GetEditableIndices),
      PAG_DEFAULT_METHOD_ENTRY(timeStretchMode, TimeStretchMode),
      PAG_DEFAULT_METHOD_ENTRY(setTimeStretchMode, SetTimeStretchMode),
      PAG_DEFAULT_METHOD_ENTRY(setDuration, SetDuration),
      PAG_DEFAULT_METHOD_ENTRY(copyOriginal, CopyOriginal)};
  auto status =
      DefineClass(env, exports, GetFileClassName(), sizeof(classProp) / sizeof(classProp[0]),
                  classProp, Constructor, GetLayerClassName(LayerType::PreCompose));
  return status == napi_ok;
}

}  // namespace pag
