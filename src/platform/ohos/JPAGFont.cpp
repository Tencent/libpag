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

#include "JPAGFont.h"
#include <rawfile/raw_file_manager.h>
#include <cstdint>
#include "base/utils/Log.h"
#include "platform/ohos/JsHelper.h"

#define JPAGFONT_FONT_FAMILY "fontFamily"
#define JPAGFONT_FONT_STYLE "fontStyle"

namespace pag {

static napi_value RegisterFontFromPath(napi_env env, napi_callback_info info) {
  size_t argc = 4;
  napi_value args[4] = {nullptr};
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  char fontPath[1024];
  size_t pathLength = 0;
  napi_get_value_string_utf8(env, args[0], fontPath, 1024, &pathLength);

  int index = 0;
  if (argc > 1) {
    napi_get_value_int32(env, args[1], &index);
  }

  char fontFamily[1024];
  size_t familyLength = 0;
  if (argc > 2) {
    napi_get_value_string_utf8(env, args[2], fontFamily, 1024, &familyLength);
  }

  char fontStyle[1024];
  size_t styleLength = 0;
  if (argc > 3) {
    napi_get_value_string_utf8(env, args[3], fontStyle, 1024, &styleLength);
  }

  auto font = PAGFont::RegisterFont({fontPath, pathLength}, index, {fontFamily, familyLength},
                                    {fontStyle, styleLength});
  return JPAGFont::ToJs(env, font);
}

static napi_value RegisterFontFromAsset(napi_env env, napi_callback_info info) {
  size_t argc = 5;
  napi_value args[5] = {nullptr};
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

  NativeResourceManager* mNativeResMgr = OH_ResourceManager_InitNativeResourceManager(env, args[0]);

  size_t nameLength;
  char name[1024];
  napi_get_value_string_utf8(env, args[1], name, sizeof(name), &nameLength);
  RawFile* rawFile = OH_ResourceManager_OpenRawFile(mNativeResMgr, name);
  if (rawFile == NULL) {
    return nullptr;
  }
  auto data = LoadDataFromAsset(mNativeResMgr, name);
  if (data == nullptr) {
    return nullptr;
  }

  int index = 0;
  if (argc > 2) {
    napi_get_value_int32(env, args[2], &index);
  }

  char fontFamily[1024];
  size_t familyLength = 0;
  if (argc > 3) {
    napi_get_value_string_utf8(env, args[3], fontFamily, 1024, &familyLength);
  }

  char fontStyle[1024];
  size_t styleLength = 0;
  if (argc > 4) {
    napi_get_value_string_utf8(env, args[4], fontStyle, 1024, &styleLength);
  }

  auto font = PAGFont::RegisterFont(data->data(), data->length(), index, {fontFamily, familyLength},
                                    {fontStyle, styleLength});
  return JPAGFont::ToJs(env, font);
}

static napi_value UnregisterFont(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  PAGFont::UnregisterFont(JPAGFont::FromJs(env, args[0]));
  return nullptr;
}

static napi_value SetFallbackFontPaths(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

  uint32_t length = 0;
  napi_get_array_length(env, args[0], &length);
  std::vector<std::string> fontPaths;
  std::vector<int> ttcIndices;
  for (uint32_t i = 0; i < length; i++) {
    napi_value pathValue;
    napi_get_element(env, args[0], i, &pathValue);
    char path[1024];
    size_t pathLength = 0;
    napi_get_value_string_utf8(env, pathValue, path, 1024, &pathLength);
    fontPaths.push_back({path, pathLength});
    ttcIndices.push_back(0);
  }
  PAGFont::SetFallbackFontPaths(fontPaths, ttcIndices);
  return nullptr;
}

bool JPAGFont::Init(napi_env env, napi_value exports) {
  napi_property_descriptor classProp[] = {
      PAG_STATIC_METHOD_ENTRY(RegisterFontFromPath, RegisterFontFromPath),
      PAG_STATIC_METHOD_ENTRY(RegisterFontFromAsset, RegisterFontFromAsset),
      PAG_STATIC_METHOD_ENTRY(UnregisterFont, UnregisterFont),
      PAG_STATIC_METHOD_ENTRY(SetFallbackFontPaths, SetFallbackFontPaths)};
  auto status = DefineClass(env, exports, ClassName(), sizeof(classProp) / sizeof(classProp[0]),
                            classProp, Constructor, "");
  return status == napi_ok;
}

napi_value JPAGFont::ToJs(napi_env env, const PAGFont& font) {
  napi_value fontFamily;
  napi_create_string_utf8(env, font.fontFamily.c_str(), font.fontFamily.length(), &fontFamily);

  napi_value fontStyle;
  napi_create_string_utf8(env, font.fontStyle.c_str(), font.fontStyle.length(), &fontStyle);

  napi_value arg[] = {fontFamily, fontStyle};

  napi_value result;
  auto status = napi_new_instance(env, GetConstructor(env, ClassName()), 2, arg, &result);
  if (status != napi_ok) {
    LOGE("JPAGFont::ToJs napi_new_instance failed :%d", status);
    return nullptr;
  }
  return result;
}

PAGFont JPAGFont::FromJs(napi_env env, napi_value value) {
  napi_value fontFamily;
  napi_get_named_property(env, value, JPAGFONT_FONT_FAMILY, &fontFamily);
  char fontFamilyBuf[1024];
  size_t familyLength = 0;
  napi_get_value_string_utf8(env, fontFamily, fontFamilyBuf, 1024, &familyLength);

  napi_value fontStyle;
  napi_get_named_property(env, value, JPAGFONT_FONT_STYLE, &fontStyle);
  char fontStyleBuf[1024];
  size_t styleLength = 0;
  napi_get_value_string_utf8(env, fontStyle, fontStyleBuf, 1024, &styleLength);

  return PAGFont{{fontFamilyBuf, familyLength}, {fontStyleBuf, styleLength}};
}

napi_value JPAGFont::Constructor(napi_env env, napi_callback_info info) {
  napi_value result = nullptr;
  size_t argc = 2;
  napi_value args[2];
  napi_get_cb_info(env, info, &argc, args, &result, nullptr);
  napi_set_named_property(env, result, JPAGFONT_FONT_FAMILY, args[0]);
  napi_set_named_property(env, result, JPAGFONT_FONT_STYLE, args[1]);
  return result;
}

}  // namespace pag
