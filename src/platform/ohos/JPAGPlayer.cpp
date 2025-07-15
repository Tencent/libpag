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

#include "JPAGPlayer.h"
#include <cstdint>
#include "JPAGLayerHandle.h"
#include "JPAGSurface.h"
#include "JsHelper.h"

namespace pag {

static napi_value GetComposition(napi_env env, napi_callback_info info) {
  napi_value jsPlayer = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsPlayer, nullptr);
  auto player = JPAGPlayer::FromJs(env, jsPlayer);
  if (!player) {
    return nullptr;
  }
  return JPAGLayerHandle::ToJs(env, player->getComposition());
}

static napi_value SetComposition(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_value jsPlayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsPlayer, nullptr);
  auto player = JPAGPlayer::FromJs(env, jsPlayer);
  if (!player) {
    return nullptr;
  }
  auto composition = JPAGLayerHandle::FromJs(env, args[0]);
  if (!composition || composition->layerType() != LayerType::PreCompose) {
    return nullptr;
  }
  player->setComposition(std::static_pointer_cast<PAGComposition>(composition));
  return nullptr;
}

/**
   * Returns the PAGSurface object for PAGPlayer to render onto.
   */
static napi_value GetSurface(napi_env env, napi_callback_info info) {
  napi_value jsPlayer = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsPlayer, nullptr);
  auto player = JPAGPlayer::FromJs(env, jsPlayer);
  if (!player) {
    return nullptr;
  }
  return JPAGSurface::ToJs(env, player->getSurface());
}

static napi_value SetSurface(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_value jsPlayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsPlayer, nullptr);
  auto player = JPAGPlayer::FromJs(env, jsPlayer);
  if (!player) {
    return nullptr;
  }
  player->setSurface(JPAGSurface::FromJs(env, args[0]));
  return nullptr;
}

static napi_value VideoEnabled(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsPlayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsPlayer, nullptr);
  auto player = JPAGPlayer::FromJs(env, jsPlayer);
  if (!player) {
    return nullptr;
  }
  bool videoEnable = player->videoEnabled();
  napi_value result;
  napi_get_boolean(env, videoEnable, &result);
  return result;
}

static napi_value SetVideoEnabled(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_value jsPlayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsPlayer, nullptr);
  auto player = JPAGPlayer::FromJs(env, jsPlayer);
  if (!player) {
    return nullptr;
  }
  bool videoEnabled = false;
  auto status = napi_get_value_bool(env, args[0], &videoEnabled);
  if (status == napi_status::napi_ok) {
    player->setVideoEnabled(videoEnabled);
  }
  return nullptr;
}

static napi_value CacheEnabled(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsPlayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsPlayer, nullptr);
  auto player = JPAGPlayer::FromJs(env, jsPlayer);
  if (!player) {
    return nullptr;
  }
  bool cacheEnabled = player->cacheEnabled();
  napi_value result;
  napi_get_boolean(env, cacheEnabled, &result);
  return result;
}

static napi_value SetCacheEnabled(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_value jsPlayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsPlayer, nullptr);
  auto player = JPAGPlayer::FromJs(env, jsPlayer);
  if (!player) {
    return nullptr;
  }
  bool value = false;
  auto status = napi_get_value_bool(env, args[0], &value);
  if (status == napi_status::napi_ok) {
    player->setCacheEnabled(value);
  }
  return nullptr;
}

static napi_value UseDiskCache(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsPlayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsPlayer, nullptr);
  auto player = JPAGPlayer::FromJs(env, jsPlayer);
  if (!player) {
    return nullptr;
  }
  bool useDiskCache = player->useDiskCache();
  napi_value result;
  napi_get_boolean(env, useDiskCache, &result);
  return result;
}

static napi_value SetUseDiskCache(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_value jsPlayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsPlayer, nullptr);
  auto player = JPAGPlayer::FromJs(env, jsPlayer);
  if (!player) {
    return nullptr;
  }
  bool value = false;
  auto status = napi_get_value_bool(env, args[0], &value);
  if (status == napi_status::napi_ok) {
    player->setUseDiskCache(value);
  }
  return nullptr;
}

static napi_value CacheScale(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsPlayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsPlayer, nullptr);
  auto player = JPAGPlayer::FromJs(env, jsPlayer);
  if (!player) {
    return nullptr;
  }
  napi_value result;
  napi_create_double(env, player->cacheScale(), &result);
  return result;
}

static napi_value SetCacheScale(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_value jsPlayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsPlayer, nullptr);
  auto player = JPAGPlayer::FromJs(env, jsPlayer);
  if (!player) {
    return nullptr;
  }
  double value = 0.0;
  auto status = napi_get_value_double(env, args[0], &value);
  if (status == napi_status::napi_ok) {
    player->setCacheScale(value);
  }
  return nullptr;
}

static napi_value MaxFrameRate(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsPlayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsPlayer, nullptr);
  auto player = JPAGPlayer::FromJs(env, jsPlayer);
  if (!player) {
    return nullptr;
  }
  napi_value result;
  napi_create_double(env, player->maxFrameRate(), &result);
  return result;
}

static napi_value SetMaxFrameRate(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_value jsPlayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsPlayer, nullptr);
  auto player = JPAGPlayer::FromJs(env, jsPlayer);
  if (!player) {
    return nullptr;
  }
  double value = 0.0;
  auto status = napi_get_value_double(env, args[0], &value);
  if (status == napi_status::napi_ok) {
    player->setMaxFrameRate(value);
  }
  return nullptr;
}

static napi_value ScaleMode(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsPlayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsPlayer, nullptr);
  auto player = JPAGPlayer::FromJs(env, jsPlayer);
  if (!player) {
    return nullptr;
  }
  napi_value result;
  napi_create_int32(env, static_cast<int32_t>(player->scaleMode()), &result);
  return result;
}

static napi_value SetScaleMode(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_value jsPlayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsPlayer, nullptr);
  auto player = JPAGPlayer::FromJs(env, jsPlayer);
  if (!player) {
    return nullptr;
  }
  int value = 0;
  auto status = napi_get_value_int32(env, args[0], &value);
  if (status == napi_status::napi_ok) {
    player->setScaleMode(static_cast<PAGScaleMode>(value));
  }
  return nullptr;
}

static napi_value GetMatrix(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsPlayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsPlayer, nullptr);
  auto player = JPAGPlayer::FromJs(env, jsPlayer);
  if (!player) {
    return nullptr;
  }
  return CreateMatrix(env, player->matrix());
}

static napi_value SetMatrix(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_value jsPlayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsPlayer, nullptr);
  auto player = JPAGPlayer::FromJs(env, jsPlayer);
  if (!player) {
    return nullptr;
  }
  player->setMatrix(GetMatrix(env, args[0]));
  return nullptr;
}

static napi_value Duration(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsPlayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsPlayer, nullptr);
  auto player = JPAGPlayer::FromJs(env, jsPlayer);
  if (!player) {
    return nullptr;
  }
  napi_value value;
  napi_create_double(env, player->duration(), &value);
  return value;
}

static napi_value NextFrame(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsPlayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsPlayer, nullptr);
  auto player = JPAGPlayer::FromJs(env, jsPlayer);
  if (!player) {
    return nullptr;
  }
  player->nextFrame();
  return nullptr;
}

static napi_value PreFrame(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsPlayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsPlayer, nullptr);
  auto player = JPAGPlayer::FromJs(env, jsPlayer);
  if (!player) {
    return nullptr;
  }
  player->preFrame();
  return nullptr;
}

static napi_value GetProgress(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsPlayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsPlayer, nullptr);
  auto player = JPAGPlayer::FromJs(env, jsPlayer);
  if (!player) {
    return nullptr;
  }
  napi_value value;
  napi_create_double(env, player->getProgress(), &value);
  return value;
}

static napi_value SetProgress(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_value jsPlayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsPlayer, nullptr);
  auto player = JPAGPlayer::FromJs(env, jsPlayer);
  if (!player) {
    return nullptr;
  }
  double value = 0;
  auto status = napi_get_value_double(env, args[0], &value);
  if (status == napi_status::napi_ok) {
    player->setProgress(value);
  }
  return nullptr;
}

static napi_value CurrentFrame(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsPlayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsPlayer, nullptr);
  auto player = JPAGPlayer::FromJs(env, jsPlayer);
  if (!player) {
    return nullptr;
  }
  napi_value value;
  napi_create_int64(env, player->currentFrame(), &value);
  return value;
}

static napi_value AutoClear(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsPlayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsPlayer, nullptr);
  auto player = JPAGPlayer::FromJs(env, jsPlayer);
  if (!player) {
    return nullptr;
  }
  napi_value value;
  napi_get_boolean(env, player->autoClear(), &value);
  return value;
}

static napi_value SetAutoClear(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_value jsPlayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsPlayer, nullptr);
  auto player = JPAGPlayer::FromJs(env, jsPlayer);
  if (!player) {
    return nullptr;
  }
  bool value = false;
  auto status = napi_get_value_bool(env, args[0], &value);
  if (status == napi_status::napi_ok) {
    player->setAutoClear(value);
  }
  return nullptr;
}

static napi_value Prepare(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsPlayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsPlayer, nullptr);
  auto player = JPAGPlayer::FromJs(env, jsPlayer);
  if (!player) {
    return nullptr;
  }
  player->prepare();
  return nullptr;
}

static napi_value Flush(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsPlayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsPlayer, nullptr);
  auto player = JPAGPlayer::FromJs(env, jsPlayer);
  if (!player) {
    return nullptr;
  }
  napi_value value;
  napi_get_boolean(env, player->flush(), &value);
  return value;
}

static napi_value GetBounds(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_value jsPlayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsPlayer, nullptr);
  auto player = JPAGPlayer::FromJs(env, jsPlayer);
  if (!player) {
    return nullptr;
  }
  auto layer = JPAGLayerHandle::FromJs(env, args[0]);
  if (layer == nullptr) {
    return nullptr;
  }
  auto rect = player->getBounds(layer);
  return CreateRect(env, rect);
}

static napi_value GetLayersUnderPoint(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2] = {nullptr};
  napi_value jsPlayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsPlayer, nullptr);
  auto player = JPAGPlayer::FromJs(env, jsPlayer);
  if (!player) {
    return nullptr;
  }
  double surfaceX = 0;
  napi_get_value_double(env, args[0], &surfaceX);
  double surfaceY = 0;
  napi_get_value_double(env, args[1], &surfaceY);
  auto layers = player->getLayersUnderPoint(surfaceX, surfaceY);
  napi_value result;
  napi_create_array_with_length(env, layers.size(), &result);
  for (uint32_t i = 0; i < layers.size(); i++) {
    auto layer = JPAGLayerHandle::ToJs(env, layers[i]);
    if (!layer) {
      break;
    }
    napi_set_element(env, result, i, layer);
  }
  return result;
}

static napi_value HitTestPoint(napi_env env, napi_callback_info info) {
  size_t argc = 3;
  napi_value args[3] = {nullptr};
  napi_value jsPlayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsPlayer, nullptr);
  auto player = JPAGPlayer::FromJs(env, jsPlayer);
  if (!player) {
    napi_value jsResult;
    napi_get_boolean(env, false, &jsResult);
    return jsResult;
  }
  auto layer = JPAGLayerHandle::FromJs(env, args[0]);
  if (layer == nullptr) {
    napi_value jsResult;
    napi_get_boolean(env, false, &jsResult);
    return jsResult;
  }
  double surfaceX = 0;
  napi_get_value_double(env, args[1], &surfaceX);
  double surfaceY = 0;
  napi_get_value_double(env, args[2], &surfaceY);
  bool result = player->hitTestPoint(layer, surfaceX, surfaceY);
  napi_value jsResult;
  napi_get_boolean(env, result, &jsResult);
  return jsResult;
}

static napi_value Release(napi_env env, napi_callback_info info) {
  napi_value jsPlayer = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsPlayer, nullptr);
  void* data = nullptr;
  napi_remove_wrap(env, jsPlayer, &data);
  return nullptr;
}

napi_value JPAGPlayer::Constructor(napi_env env, napi_callback_info info) {
  napi_value jsPlayer = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsPlayer, nullptr);
  void* cPlayer = nullptr;
  if (argc == 0) {
    cPlayer = new JPAGPlayer(std::make_shared<PAGPlayer>());
  } else {
    napi_get_value_external(env, args[0], &cPlayer);
  }
  napi_wrap(
      env, jsPlayer, cPlayer,
      [](napi_env, void* finalize_data, void*) {
        JPAGPlayer* player = static_cast<JPAGPlayer*>(finalize_data);
        delete player;
      },
      nullptr, nullptr);
  return jsPlayer;
}

bool JPAGPlayer::Init(napi_env env, napi_value exports) {
  napi_property_descriptor classProp[] = {
      PAG_DEFAULT_METHOD_ENTRY(getComposition, GetComposition),
      PAG_DEFAULT_METHOD_ENTRY(setComposition, SetComposition),
      PAG_DEFAULT_METHOD_ENTRY(getSurface, GetSurface),
      PAG_DEFAULT_METHOD_ENTRY(setSurface, SetSurface),
      PAG_DEFAULT_METHOD_ENTRY(videoEnabled, VideoEnabled),
      PAG_DEFAULT_METHOD_ENTRY(setVideoEnabled, SetVideoEnabled),
      PAG_DEFAULT_METHOD_ENTRY(cacheEnabled, CacheEnabled),
      PAG_DEFAULT_METHOD_ENTRY(setCacheEnabled, SetCacheEnabled),
      PAG_DEFAULT_METHOD_ENTRY(useDiskCache, UseDiskCache),
      PAG_DEFAULT_METHOD_ENTRY(setUseDiskCache, SetUseDiskCache),
      PAG_DEFAULT_METHOD_ENTRY(cacheScale, CacheScale),
      PAG_DEFAULT_METHOD_ENTRY(setCacheScale, SetCacheScale),
      PAG_DEFAULT_METHOD_ENTRY(maxFrameRate, MaxFrameRate),
      PAG_DEFAULT_METHOD_ENTRY(setMaxFrameRate, SetMaxFrameRate),
      PAG_DEFAULT_METHOD_ENTRY(scaleMode, ScaleMode),
      PAG_DEFAULT_METHOD_ENTRY(setScaleMode, SetScaleMode),
      PAG_DEFAULT_METHOD_ENTRY(matrix, GetMatrix),
      PAG_DEFAULT_METHOD_ENTRY(setMatrix, SetMatrix),
      PAG_DEFAULT_METHOD_ENTRY(duration, Duration),
      PAG_DEFAULT_METHOD_ENTRY(nextFrame, NextFrame),
      PAG_DEFAULT_METHOD_ENTRY(preFrame, PreFrame),
      PAG_DEFAULT_METHOD_ENTRY(getProgress, GetProgress),
      PAG_DEFAULT_METHOD_ENTRY(setProgress, SetProgress),
      PAG_DEFAULT_METHOD_ENTRY(currentFrame, CurrentFrame),
      PAG_DEFAULT_METHOD_ENTRY(autoClear, AutoClear),
      PAG_DEFAULT_METHOD_ENTRY(setAutoClear, SetAutoClear),
      PAG_DEFAULT_METHOD_ENTRY(prepare, Prepare),
      PAG_DEFAULT_METHOD_ENTRY(flush, Flush),
      PAG_DEFAULT_METHOD_ENTRY(getBounds, GetBounds),
      PAG_DEFAULT_METHOD_ENTRY(getLayersUnderPoint, GetLayersUnderPoint),
      PAG_DEFAULT_METHOD_ENTRY(hitTestPoint, HitTestPoint),
      PAG_DEFAULT_METHOD_ENTRY(release, Release)};

  auto status = DefineClass(env, exports, ClassName(), sizeof(classProp) / sizeof(classProp[0]),
                            classProp, Constructor, "");
  return status == napi_ok;
}

std::shared_ptr<PAGPlayer> JPAGPlayer::FromJs(napi_env env, napi_value value) {
  JPAGPlayer* cPlayer = nullptr;
  auto status = napi_unwrap(env, value, (void**)&cPlayer);
  if (status == napi_ok) {
    return cPlayer->get();
  } else {
    return nullptr;
  }
}

napi_value JPAGPlayer::ToJs(napi_env env, std::shared_ptr<PAGPlayer> player) {
  if (!player) {
    return nullptr;
  }
  JPAGPlayer* handler = new JPAGPlayer(player);
  napi_value result = NewInstance(env, ClassName(), handler);
  if (result == nullptr) {
    delete handler;
  }
  return result;
}
}  // namespace pag
