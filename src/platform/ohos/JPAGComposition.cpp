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

#include <cstddef>
#include <cstdint>
#include "JPAGLayerHandle.h"
#include "JsHelper.h"

namespace pag {

static std::shared_ptr<PAGComposition> FromJs(napi_env env, napi_value value) {
  auto layer = JPAGLayerHandle::FromJs(env, value);
  if (layer && layer->layerType() == LayerType::PreCompose) {
    return std::static_pointer_cast<PAGComposition>(layer);
  }
  return nullptr;
}

static napi_value Make(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2] = {nullptr};
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  int width = 0;
  int height = 0;
  napi_get_value_int32(env, args[0], &width);
  napi_get_value_int32(env, args[1], &height);
  return JPAGLayerHandle::ToJs(env, PAGComposition::Make(width, height));
}

static napi_value Width(napi_env env, napi_callback_info info) {
  napi_value jsComposition = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsComposition, nullptr);
  auto composition = FromJs(env, jsComposition);
  if (!composition) {
    napi_value value;
    napi_create_int32(env, 0, &value);
    return value;
  }
  napi_value value;
  napi_create_int32(env, composition->width(), &value);
  return value;
}

static napi_value Height(napi_env env, napi_callback_info info) {
  napi_value jsComposition = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsComposition, nullptr);
  auto composition = FromJs(env, jsComposition);
  if (!composition) {
    return nullptr;
  }
  napi_value value;
  napi_create_int32(env, composition->height(), &value);
  return value;
}

static napi_value SetContentSize(napi_env env, napi_callback_info info) {
  napi_value jsComposition = nullptr;
  size_t argc = 2;
  napi_value args[2] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsComposition, nullptr);
  auto composition = FromJs(env, jsComposition);
  if (!composition) {
    return nullptr;
  }
  int32_t width = 0;
  napi_get_value_int32(env, args[0], &width);
  int32_t height = 0;
  napi_get_value_int32(env, args[1], &height);
  composition->setContentSize(width, height);
  return nullptr;
}

static napi_value NumChildren(napi_env env, napi_callback_info info) {
  napi_value jsComposition = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsComposition, nullptr);
  auto composition = FromJs(env, jsComposition);
  if (!composition) {
    return nullptr;
  }
  napi_value value;
  napi_create_int32(env, composition->numChildren(), &value);
  return value;
}

static napi_value GetLayerAt(napi_env env, napi_callback_info info) {
  napi_value jsComposition = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsComposition, nullptr);
  auto composition = FromJs(env, jsComposition);
  if (!composition) {
    return nullptr;
  }
  int32_t index = -1;
  napi_get_value_int32(env, args[0], &index);
  return JPAGLayerHandle::ToJs(env, composition->getLayerAt(index));
}

static napi_value GetLayerIndex(napi_env env, napi_callback_info info) {
  napi_value jsComposition = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsComposition, nullptr);
  auto composition = FromJs(env, jsComposition);
  if (!composition) {
    return nullptr;
  }
  auto layer = JPAGLayerHandle::FromJs(env, args[0]);
  napi_value value;
  napi_create_int32(env, composition->getLayerIndex(layer), &value);
  return value;
}

static napi_value SetLayerIndex(napi_env env, napi_callback_info info) {
  napi_value jsComposition = nullptr;
  size_t argc = 2;
  napi_value args[2] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsComposition, nullptr);
  auto composition = FromJs(env, jsComposition);
  if (!composition) {
    return nullptr;
  }
  auto layer = JPAGLayerHandle::FromJs(env, args[0]);
  if (!layer) {
    return nullptr;
  }
  int32_t index = -1;
  napi_get_value_int32(env, args[1], &index);
  if (index < 0) {
    return nullptr;
  }
  composition->setLayerIndex(layer, index);
  return nullptr;
}

static napi_value AddLayer(napi_env env, napi_callback_info info) {
  napi_value jsComposition = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsComposition, nullptr);
  auto composition = FromJs(env, jsComposition);
  napi_value value;
  if (!composition) {
    napi_get_boolean(env, false, &value);
    return value;
  }
  auto layer = JPAGLayerHandle::FromJs(env, args[0]);
  if (!layer) {
    napi_get_boolean(env, false, &value);
  } else {
    napi_get_boolean(env, composition->addLayer(layer), &value);
  }
  return value;
}

static napi_value AddLayerAt(napi_env env, napi_callback_info info) {
  napi_value jsComposition = nullptr;
  size_t argc = 2;
  napi_value args[2] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsComposition, nullptr);
  auto composition = FromJs(env, jsComposition);
  napi_value value;
  if (!composition) {
    napi_get_boolean(env, false, &value);
    return value;
  }
  auto layer = JPAGLayerHandle::FromJs(env, args[0]);
  if (!layer) {
    napi_get_boolean(env, false, &value);
    return value;
  }
  int32_t index = -1;
  napi_get_value_int32(env, args[1], &index);
  if (index < 0) {
    napi_get_boolean(env, false, &value);
  } else {
    napi_get_boolean(env, composition->addLayerAt(layer, index), &value);
  }
  return value;
}

static napi_value Contains(napi_env env, napi_callback_info info) {
  napi_value jsComposition = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsComposition, nullptr);
  auto composition = FromJs(env, jsComposition);
  napi_value value;
  if (!composition) {
    napi_get_boolean(env, false, &value);
    return value;
  }
  auto layer = JPAGLayerHandle::FromJs(env, args[0]);
  if (!layer) {
    napi_get_boolean(env, false, &value);
    return value;
  }
  napi_get_boolean(env, composition->contains(layer), &value);
  return value;
}

static napi_value RemoveLayer(napi_env env, napi_callback_info info) {
  napi_value jsComposition = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsComposition, nullptr);
  auto composition = FromJs(env, jsComposition);
  if (!composition) {
    return nullptr;
  }
  auto layer = JPAGLayerHandle::FromJs(env, args[0]);
  if (!layer) {
    return nullptr;
  }
  return JPAGLayerHandle::ToJs(env, composition->removeLayer(layer));
}

static napi_value RemoveLayerAt(napi_env env, napi_callback_info info) {
  napi_value jsComposition = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsComposition, nullptr);
  auto composition = FromJs(env, jsComposition);
  if (!composition) {
    return nullptr;
  }
  int32_t index = -1;
  napi_get_value_int32(env, args[0], &index);
  if (index < 0) {
    return nullptr;
  }
  return JPAGLayerHandle::ToJs(env, composition->removeLayerAt(index));
}

static napi_value RemoveAllLayers(napi_env env, napi_callback_info info) {
  napi_value jsComposition = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsComposition, nullptr);
  auto composition = FromJs(env, jsComposition);
  if (!composition) {
    return nullptr;
  }
  composition->removeAllLayers();
  return nullptr;
}

static napi_value SwapLayer(napi_env env, napi_callback_info info) {
  napi_value jsComposition = nullptr;
  size_t argc = 2;
  napi_value args[2] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsComposition, nullptr);
  auto composition = FromJs(env, jsComposition);
  if (!composition) {
    return nullptr;
  }
  auto layer1 = JPAGLayerHandle::FromJs(env, args[0]);
  if (!layer1) {
    return nullptr;
  }
  auto layer2 = JPAGLayerHandle::FromJs(env, args[1]);
  if (!layer2) {
    return nullptr;
  }
  composition->swapLayer(layer1, layer2);
  return nullptr;
}

static napi_value SwapLayerAt(napi_env env, napi_callback_info info) {
  napi_value jsComposition = nullptr;
  size_t argc = 2;
  napi_value args[2] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsComposition, nullptr);
  auto composition = FromJs(env, jsComposition);
  if (!composition) {
    return nullptr;
  }
  int32_t index1 = -1;
  napi_get_value_int32(env, args[0], &index1);
  if (index1 < 0) {
    return nullptr;
  }
  int32_t index2 = -1;
  napi_get_value_int32(env, args[1], &index2);
  if (index2 < 0) {
    return nullptr;
  }
  composition->swapLayerAt(index1, index2);
  return nullptr;
}

static napi_value AudioBytes(napi_env env, napi_callback_info info) {
  napi_value jsComposition = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsComposition, nullptr);
  auto composition = FromJs(env, jsComposition);
  if (!composition) {
    return nullptr;
  }
  napi_value arraybuffer;
  void* data = nullptr;
  auto audio = composition->audioBytes();
  if (audio == nullptr) {
    return nullptr;
  }
  napi_create_arraybuffer(env, audio->length(), &data, &arraybuffer);
  memcpy(data, audio->data(), audio->length());
  return arraybuffer;
}

static napi_value AudioMarkers(napi_env env, napi_callback_info info) {
  napi_value jsComposition = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsComposition, nullptr);
  auto composition = FromJs(env, jsComposition);
  if (!composition) {
    return nullptr;
  }
  return CreateMarkers(env, composition->audioMarkers());
}

static napi_value AudioStartTime(napi_env env, napi_callback_info info) {
  napi_value jsComposition = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsComposition, nullptr);
  auto composition = FromJs(env, jsComposition);
  if (!composition) {
    napi_value value;
    napi_create_int32(env, 0, &value);
    return value;
  }
  napi_value value;
  napi_create_int32(env, composition->audioStartTime(), &value);
  return value;
}

static napi_value GetLayersByName(napi_env env, napi_callback_info info) {
  napi_value jsComposition = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsComposition, nullptr);
  auto composition = FromJs(env, jsComposition);
  if (!composition) {
    return nullptr;
  }
  char name[1024] = {0};
  size_t length = 0;
  napi_get_value_string_utf8(env, args[0], name, 1024, &length);
  auto layers = composition->getLayersByName(std::string(name, length));
  napi_value array;
  napi_create_array_with_length(env, layers.size(), &array);
  for (size_t i = 0; i < layers.size(); i++) {
    napi_set_element(env, array, i, JPAGLayerHandle::ToJs(env, layers[i]));
  }
  return array;
}

static napi_value GetLayersUnderPoint(napi_env env, napi_callback_info info) {
  napi_value jsComposition = nullptr;
  size_t argc = 2;
  napi_value args[2] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsComposition, nullptr);
  auto composition = FromJs(env, jsComposition);
  if (!composition) {
    return nullptr;
  }
  double x = 0;
  napi_get_value_double(env, args[0], &x);
  double y = 0;
  napi_get_value_double(env, args[1], &y);
  auto layers = composition->getLayersUnderPoint(x, y);
  napi_value array;
  napi_create_array_with_length(env, layers.size(), &array);
  for (size_t i = 0; i < layers.size(); i++) {
    napi_set_element(env, array, i, JPAGLayerHandle::ToJs(env, layers[i]));
  }
  return array;
}

bool JPAGLayerHandle::InitPAGCompositionLayerEnv(napi_env env, napi_value exports) {
  napi_property_descriptor classProp[] = {
      PAG_STATIC_METHOD_ENTRY(Make, Make),

      PAG_DEFAULT_METHOD_ENTRY(width, Width),
      PAG_DEFAULT_METHOD_ENTRY(height, Height),
      PAG_DEFAULT_METHOD_ENTRY(setContentSize, SetContentSize),
      PAG_DEFAULT_METHOD_ENTRY(numChildren, NumChildren),
      PAG_DEFAULT_METHOD_ENTRY(getLayerAt, GetLayerAt),
      PAG_DEFAULT_METHOD_ENTRY(getLayerIndex, GetLayerIndex),
      PAG_DEFAULT_METHOD_ENTRY(setLayerIndex, SetLayerIndex),
      PAG_DEFAULT_METHOD_ENTRY(addLayer, AddLayer),
      PAG_DEFAULT_METHOD_ENTRY(addLayerAt, AddLayerAt),
      PAG_DEFAULT_METHOD_ENTRY(contains, Contains),
      PAG_DEFAULT_METHOD_ENTRY(removeLayer, RemoveLayer),
      PAG_DEFAULT_METHOD_ENTRY(removeLayerAt, RemoveLayerAt),
      PAG_DEFAULT_METHOD_ENTRY(removeAllLayers, RemoveAllLayers),
      PAG_DEFAULT_METHOD_ENTRY(swapLayer, SwapLayer),
      PAG_DEFAULT_METHOD_ENTRY(swapLayerAt, SwapLayerAt),
      PAG_DEFAULT_METHOD_ENTRY(audioBytes, AudioBytes),
      PAG_DEFAULT_METHOD_ENTRY(audioMarkers, AudioMarkers),
      PAG_DEFAULT_METHOD_ENTRY(audioStartTime, AudioStartTime),
      PAG_DEFAULT_METHOD_ENTRY(getLayersByName, GetLayersByName),
      PAG_DEFAULT_METHOD_ENTRY(getLayersUnderPoint, GetLayersUnderPoint)};

  auto status = DefineClass(env, exports, GetLayerClassName(LayerType::PreCompose),
                            sizeof(classProp) / sizeof(classProp[0]), classProp, Constructor,
                            GetBaseClassName());
  return status == napi_status::napi_ok;
}

}  // namespace pag
