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

#include "JPAGLayerHandle.h"
#include "platform/ohos/JPAGFont.h"
#include "platform/ohos/JsHelper.h"

namespace pag {

static napi_value FillColor(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsLayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsLayer, nullptr);
  auto layer = JPAGLayerHandle::FromJs(env, jsLayer);
  if (!layer) {
    return nullptr;
  }
  auto textLayer = std::static_pointer_cast<PAGTextLayer>(layer);
  napi_value result;
  auto color = textLayer->fillColor();
  napi_create_int32(env, MakeColorInt(color.red, color.green, color.blue), &result);
  return result;
}

static napi_value SetFillColor(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_value jsLayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsLayer, nullptr);
  auto layer = JPAGLayerHandle::FromJs(env, jsLayer);
  if (!layer) {
    return nullptr;
  }
  int color = 0;
  napi_get_value_int32(env, args[0], &color);
  auto textLayer = std::static_pointer_cast<PAGTextLayer>(layer);
  textLayer->setFillColor(ToColor(color));
  return nullptr;
}

static napi_value Font(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsLayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsLayer, nullptr);
  auto layer = JPAGLayerHandle::FromJs(env, jsLayer);
  if (!layer) {
    return nullptr;
  }
  auto textLayer = std::static_pointer_cast<PAGTextLayer>(layer);
  auto font = textLayer->font();
  return JPAGFont::ToJs(env, font);
}
static napi_value SetFont(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_value jsLayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsLayer, nullptr);
  auto layer = JPAGLayerHandle::FromJs(env, jsLayer);
  if (!layer) {
    return nullptr;
  }
  auto textLayer = std::static_pointer_cast<PAGTextLayer>(layer);
  textLayer->setFont(JPAGFont::FromJs(env, args[0]));
  return nullptr;
}
static napi_value FontSize(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsLayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsLayer, nullptr);
  auto layer = JPAGLayerHandle::FromJs(env, jsLayer);
  if (!layer) {
    return nullptr;
  }
  auto textLayer = std::static_pointer_cast<PAGTextLayer>(layer);
  napi_value result;
  napi_create_double(env, textLayer->fontSize(), &result);
  return result;
}

static napi_value SetFontSize(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_value jsLayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsLayer, nullptr);
  auto layer = JPAGLayerHandle::FromJs(env, jsLayer);
  if (!layer) {
    return nullptr;
  }
  auto textLayer = std::static_pointer_cast<PAGTextLayer>(layer);
  double fontsize = 0;
  napi_get_value_double(env, args[0], &fontsize);
  textLayer->setFontSize(fontsize);
  return nullptr;
}

static napi_value StrokeColor(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsLayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsLayer, nullptr);
  auto layer = JPAGLayerHandle::FromJs(env, jsLayer);
  if (!layer) {
    return nullptr;
  }
  auto textLayer = std::static_pointer_cast<PAGTextLayer>(layer);
  napi_value result;
  auto color = textLayer->strokeColor();
  napi_create_int32(env, MakeColorInt(color.red, color.green, color.blue), &result);
  return result;
}
static napi_value SetStrokeColor(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_value jsLayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsLayer, nullptr);
  auto layer = JPAGLayerHandle::FromJs(env, jsLayer);
  if (!layer) {
    return nullptr;
  }
  auto textLayer = std::static_pointer_cast<PAGTextLayer>(layer);
  int color = 0;
  napi_get_value_int32(env, args[0], &color);
  textLayer->setStrokeColor(ToColor(color));
  return nullptr;
}

static napi_value Text(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsLayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsLayer, nullptr);
  auto layer = JPAGLayerHandle::FromJs(env, jsLayer);
  if (!layer) {
    return nullptr;
  }
  auto textLayer = std::static_pointer_cast<PAGTextLayer>(layer);
  napi_value result;
  auto text = textLayer->text();
  napi_create_string_utf8(env, text.c_str(), text.size(), &result);
  return result;
}

static napi_value SetText(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_value jsLayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsLayer, nullptr);
  auto layer = JPAGLayerHandle::FromJs(env, jsLayer);
  if (!layer) {
    return nullptr;
  }
  auto textLayer = std::static_pointer_cast<PAGTextLayer>(layer);
  char buf[2048];
  size_t length = 0;
  napi_get_value_string_utf8(env, args[0], buf, 2048, &length);
  textLayer->setText({buf, length});
  return nullptr;
}

static napi_value Reset(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsLayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsLayer, nullptr);
  auto layer = JPAGLayerHandle::FromJs(env, jsLayer);
  if (!layer) {
    return nullptr;
  }
  auto textLayer = std::static_pointer_cast<PAGTextLayer>(layer);
  textLayer->reset();
  return nullptr;
}

bool JPAGLayerHandle::InitPAGTextLayerEnv(napi_env env, napi_value exports) {
  napi_property_descriptor classProp[] = {PAG_DEFAULT_METHOD_ENTRY(fillColor, FillColor),
                                          PAG_DEFAULT_METHOD_ENTRY(setFillColor, SetFillColor),
                                          PAG_DEFAULT_METHOD_ENTRY(font, Font),
                                          PAG_DEFAULT_METHOD_ENTRY(setFont, SetFont),
                                          PAG_DEFAULT_METHOD_ENTRY(fontSize, FontSize),
                                          PAG_DEFAULT_METHOD_ENTRY(setFontSize, SetFontSize),
                                          PAG_DEFAULT_METHOD_ENTRY(strokeColor, StrokeColor),
                                          PAG_DEFAULT_METHOD_ENTRY(setStrokeColor, SetStrokeColor),
                                          PAG_DEFAULT_METHOD_ENTRY(text, Text),
                                          PAG_DEFAULT_METHOD_ENTRY(setText, SetText),
                                          PAG_DEFAULT_METHOD_ENTRY(reset, Reset)};
  auto status = DefineClass(env, exports, GetLayerClassName(LayerType::Text),
                            sizeof(classProp) / sizeof(classProp[0]), classProp, Constructor,
                            GetBaseClassName());
  return status == napi_ok;
}

}  // namespace pag
