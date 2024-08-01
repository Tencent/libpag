//
// Created on 2024/7/16.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#include <cstddef>
#include <cstdint>
#include <rawfile/raw_file.h>
#include <rawfile/raw_file_manager.h>
#include "JsPAGLayerHandle.h"
#include "JsHelper.h"
#include "platform/ohos/JsPAGImage.h"

namespace pag {

static std::shared_ptr<PAGFile> FromJs(napi_env env, napi_value value) {
  auto layer = JsPAGLayerHandle::FromJs(env, value);
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
  return JsPAGLayerHandle::ToJs(env, PAGFile::Load(std::string(filePath, length)));
}

static napi_value LoadFromBytes(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2] = {nullptr};
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  size_t length;
  void* data;
  auto code = napi_get_arraybuffer_info(env, args[1], &data, &length);
  if (code != napi_ok) {
    return nullptr;
  }
  std::string path = "";
  if (argc == 2) {
    char filePath[1024] = {0};
    size_t pathLength = 0;
    napi_get_value_string_utf8(env, args[0], filePath, 1024, &pathLength);
    path = std::string(filePath, length);
  }
  return JsPAGLayerHandle::ToJs(env, PAGFile::Load(data, length, path));
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
  RawFile* rawFile = OH_ResourceManager_OpenRawFile(mNativeResMgr, srcBuf);
  if (rawFile != NULL) {
    long len = OH_ResourceManager_GetRawFileSize(rawFile);
    uint8_t* data = static_cast<uint8_t*>(malloc(len));
    OH_ResourceManager_ReadRawFile(rawFile, data, len);
    return JsPAGLayerHandle::ToJs(env, PAGFile::Load(data, len, "asset://" + fileName));
  }
  return nullptr;
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
  return CreateTextDocument(env, file->getTextData(index));
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
  auto textData = GetTextDocument(env, args[1]);
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
  auto image = JsPAGImage::FromJs(env, args[1]);
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
  auto image = JsPAGImage::FromJs(env, args[1]);
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
    napi_set_element(env, array, i, JsPAGLayerHandle::ToJs(env, layers[i]));
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
  napi_create_int32(env, file->timeStretchMode(), &value);
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
  file->setTimeStretchMode(mode);
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
  return JsPAGLayerHandle::ToJs(env, file->copyOriginal());
}

bool JsPAGLayerHandle::InitPAGFileEnv(napi_env env, napi_value exports) {

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
  auto status = DefineClass(
      env, exports, GetFileClassName(), sizeof(classProp) / sizeof(classProp[0]), classProp,
      ConstructorWithHandler<JsPAGLayerHandle>, GetLayerClassName(LayerType::PreCompose));
  return status == napi_ok;
}

}  // namespace pag