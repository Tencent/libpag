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

#include "JPAGView.h"
#include <ace/xcomponent/native_interface_xcomponent.h>
#include <cstdint>
#include "base/utils/UniqueID.h"
#include "platform/ohos/GPUDrawable.h"
#include "platform/ohos/JPAGLayerHandle.h"
#include "platform/ohos/JsHelper.h"
#include "platform/ohos/XComponentHandler.h"

namespace pag {

static std::unordered_map<std::string, std::shared_ptr<JPAGView>> ViewMap = {};

static napi_value Flush(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);

  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  bool flushResult = false;
  if (view != nullptr) {
    auto player = view->getPlayer();
    if (player != nullptr) {
      flushResult = player->flush();
    }
  }
  napi_value result;
  napi_get_boolean(env, flushResult, &result);
  return result;
}

static napi_value Update(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);

  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));

  if (view != nullptr) {
    auto animator = view->getAnimator();
    if (animator != nullptr) {
      animator->update();
    }
  }
  return nullptr;
}

static napi_value SetProgress(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  if (argc == 0) {
    return nullptr;
  }
  double progress = 0;
  napi_get_value_double(env, args[0], &progress);
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    auto animator = view->getAnimator();
    auto player = view->getPlayer();
    if (player != nullptr && animator != nullptr) {
      player->setProgress(progress);
      animator->setProgress(progress);
      animator->update();
    }
  }
  return nullptr;
}

static napi_value GetProgress(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);

  double progress = 0;
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    auto animator = view->getAnimator();
    if (animator != nullptr) {
      progress = animator->progress();
    }
  }
  napi_value result;
  napi_create_double(env, progress, &result);
  return result;
}

static napi_value SetComposition(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  if (argc == 0) {
    return nullptr;
  }
  auto layer = JPAGLayerHandle::FromJs(env, args[0]);
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    auto player = view->getPlayer();
    auto animator = view->getAnimator();
    if (player == nullptr || animator == nullptr) {
      return nullptr;
    }
    if (layer != nullptr) {
      if (layer->layerType() == LayerType::PreCompose) {
        player->setComposition(std::static_pointer_cast<PAGComposition>(layer));
        animator->setProgress(player->getProgress());
        animator->setDuration(layer->duration());
      }
    } else {
      player->setComposition(nullptr);
      animator->setDuration(0);
    }
  }
  return nullptr;
}

static napi_value SetRepeatCount(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  if (argc == 0) {
    return nullptr;
  }
  int repeatCount = 0;
  napi_get_value_int32(env, args[0], &repeatCount);
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    auto animator = view->getAnimator();
    if (animator) {
      animator->setRepeatCount(repeatCount);
    }
  }
  return nullptr;
}

static napi_value RepeatCount(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);

  int repeatCount = 0;
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    auto animator = view->getAnimator();
    if (animator != nullptr) {
      repeatCount = animator->repeatCount();
    }
  }
  napi_value result;
  napi_create_int32(env, repeatCount, &result);
  return result;
}

static napi_value Play(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));

  if (view != nullptr) {
    auto player = view->getPlayer();
    auto animator = view->getAnimator();
    if (player == nullptr || animator == nullptr) {
      return nullptr;
    }
    player->prepare();
    animator->start();
  }
  return nullptr;
}

static napi_value Pause(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    auto animator = view->getAnimator();
    if (animator) {
      animator->cancel();
    }
  }
  return nullptr;
}

static napi_value IsPlaying(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);

  bool isPlaying = false;
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    auto animator = view->getAnimator();
    if (animator != nullptr) {
      isPlaying = animator->isRunning();
    }
  }
  napi_value result;
  napi_get_boolean(env, isPlaying, &result);
  return result;
}

static napi_value UniqueID(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view == nullptr) {
    return nullptr;
  }
  napi_value result;
  napi_create_string_utf8(env, view->id.c_str(), view->id.length(), &result);
  return result;
}

static void StateChangeCallback(napi_env env, napi_value callback, void*, void* data) {
  uint8_t state = *static_cast<uint8_t*>(data);
  size_t argc = 1;
  napi_value argv[1] = {0};
  napi_create_uint32(env, static_cast<uint32_t>(state), &argv[0]);
  napi_value undefined;
  napi_get_undefined(env, &undefined);
  napi_call_function(env, undefined, callback, argc, argv, nullptr);
}

static napi_value SetStateChangeCallback(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  if (argc == 0) {
    return nullptr;
  }
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));

  napi_value resourceName = nullptr;
  napi_create_string_utf8(env, "PAGViewStateChangeCallback", NAPI_AUTO_LENGTH, &resourceName);

  napi_create_threadsafe_function(env, args[0], nullptr, resourceName, 0, 1, nullptr, nullptr, view,
                                  StateChangeCallback, &view->playingStateCallback);
  return nullptr;
}

static void ProgressCallback(napi_env env, napi_value callback, void*, void*) {
  napi_value undefined;
  napi_get_undefined(env, &undefined);
  napi_call_function(env, undefined, callback, 0, nullptr, nullptr);
}

static napi_value SetProgressUpdateCallback(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  if (argc == 0) {
    return nullptr;
  }
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  napi_value resourceName = nullptr;
  napi_create_string_utf8(env, "PAGViewProgressCallback", NAPI_AUTO_LENGTH, &resourceName);
  napi_create_threadsafe_function(env, args[0], nullptr, resourceName, 0, 1, nullptr, nullptr,
                                  nullptr, ProgressCallback, &view->progressCallback);
  return nullptr;
}

static napi_value SetSync(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  if (argc == 0) {
    return nullptr;
  }
  bool value = false;
  napi_get_value_bool(env, args[0], &value);
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    auto animator = view->getAnimator();
    if (animator) {
      animator->setSync(value);
    }
  }
  return nullptr;
}

static napi_value IsSync(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);

  bool isSync = false;
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    auto animator = view->getAnimator();
    if (animator != nullptr) {
      isSync = animator->isSync();
    }
  }
  napi_value result;
  napi_get_boolean(env, isSync, &result);
  return result;
}

static napi_value SetVideoEnabled(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  if (argc == 0) {
    return nullptr;
  }
  bool value = false;
  napi_get_value_bool(env, args[0], &value);
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    auto player = view->getPlayer();
    if (player) {
      player->setVideoEnabled(value);
    }
  }
  return nullptr;
}

static napi_value VideoEnabled(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);

  bool videoEnabled = false;
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    auto player = view->getPlayer();
    if (player != nullptr) {
      videoEnabled = player->videoEnabled();
    }
  }
  napi_value result;
  napi_get_boolean(env, videoEnabled, &result);
  return result;
}

static napi_value SetCacheEnabled(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  if (argc == 0) {
    return nullptr;
  }
  bool value = false;
  napi_get_value_bool(env, args[0], &value);
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    auto player = view->getPlayer();
    if (player) {
      player->setCacheEnabled(value);
    }
  }
  return nullptr;
}

static napi_value CacheEnabled(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);

  bool cacheEnabled = false;
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    auto player = view->getPlayer();
    if (player != nullptr) {
      cacheEnabled = player->cacheEnabled();
    }
  }
  napi_value result;
  napi_get_boolean(env, cacheEnabled, &result);
  return result;
}

static napi_value SetCacheScale(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  if (argc == 0) {
    return nullptr;
  }
  double value = 1.0f;
  napi_get_value_double(env, args[0], &value);
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    auto player = view->getPlayer();
    if (player) {
      player->setCacheScale(value);
    }
  }
  return nullptr;
}

static napi_value CacheScale(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);

  float cacheScale = 1.0;
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    auto player = view->getPlayer();
    if (player != nullptr) {
      cacheScale = player->cacheScale();
    }
  }
  napi_value result;
  napi_create_double(env, cacheScale, &result);
  return result;
}

static napi_value SetMaxFrameRate(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  if (argc == 0) {
    return nullptr;
  }
  double value = 60.0f;
  napi_get_value_double(env, args[0], &value);
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    auto player = view->getPlayer();
    if (player) {
      player->setMaxFrameRate(value);
    }
  }
  return nullptr;
}

static napi_value MaxFrameRate(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);

  float maxFrameRate = 60.0;
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    auto player = view->getPlayer();
    if (player != nullptr) {
      maxFrameRate = player->maxFrameRate();
    }
  }
  napi_value result;
  napi_create_double(env, maxFrameRate, &result);
  return result;
}

static napi_value SetScaleMode(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  if (argc == 0) {
    return nullptr;
  }
  int value = 0;
  napi_get_value_int32(env, args[0], &value);
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    auto player = view->getPlayer();
    if (player) {
      player->setScaleMode(static_cast<PAGScaleMode>(value));
    }
  }
  return nullptr;
}

static napi_value ScaleMode(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);

  PAGScaleMode scaleMode = PAGScaleMode::LetterBox;
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    auto player = view->getPlayer();
    if (player != nullptr) {
      scaleMode = player->scaleMode();
    }
  }
  napi_value result;
  napi_create_int32(env, static_cast<int32_t>(scaleMode), &result);
  return result;
}

static napi_value SetMatrix(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  if (argc == 0) {
    return nullptr;
  }
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    auto player = view->getPlayer();
    if (player) {
      player->setMatrix(GetMatrix(env, args[0]));
    }
  }
  return nullptr;
}

static napi_value Matrix(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);

  auto matrix = Matrix::I();
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    auto player = view->getPlayer();
    if (player != nullptr) {
      matrix = player->matrix();
    }
  }
  return CreateMatrix(env, matrix);
}

static napi_value CurrentFrame(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view == nullptr) {
    return nullptr;
  }
  auto player = view->getPlayer();
  if (!player) {
    return nullptr;
  }
  napi_value result;
  napi_create_int32(env, player->currentFrame(), &result);
  return result;
}

static napi_value GetLayersUnderPoint(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2] = {nullptr};
  napi_value jsView = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (!view) {
    return nullptr;
  }
  auto player = view->getPlayer();
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

static napi_value GetBounds(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_value jsView = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  Rect rect = Rect::MakeEmpty();
  if (!view) {
    return CreateRect(env, rect);
  }
  auto layer = JPAGLayerHandle::FromJs(env, args[0]);
  if (!layer) {
    return CreateRect(env, rect);
  }
  auto player = view->getPlayer();
  if (player) {
    rect = player->getBounds(layer);
  }
  return CreateRect(env, rect);
}

static napi_value FreeCache(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view == nullptr) {
    return nullptr;
  }
  auto player = view->getPlayer();
  if (!player) {
    return nullptr;
  }
  auto surface = player->getSurface();
  if (surface) {
    surface->freeCache();
  }
  return nullptr;
}

static napi_value MakeSnapshot(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view == nullptr) {
    return nullptr;
  }
  auto player = view->getPlayer();
  if (!player) {
    return nullptr;
  }
  auto surface = player->getSurface();
  return MakeSnapshot(env, surface.get());
}

static napi_value UseDiskCache(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);

  bool useDiskCache = false;
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    auto player = view->getPlayer();
    if (player != nullptr) {
      useDiskCache = player->useDiskCache();
    }
  }
  napi_value result;
  napi_get_boolean(env, useDiskCache, &result);
  return result;
}

static napi_value SetUseDiskCache(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view == nullptr) {
    return nullptr;
  }
  auto player = view->getPlayer();
  if (player) {
    bool useDiskCache = false;
    napi_get_value_bool(env, args[0], &useDiskCache);
    player->setUseDiskCache(useDiskCache);
  }
  return nullptr;
}

static napi_value Release(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  JPAGView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view == nullptr) {
    return nullptr;
  }
  view->release();
  return nullptr;
}

napi_value JPAGView::Constructor(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  std::string id = "PAGView" + std::to_string(UniqueID::Next());
  auto cView = std::make_shared<JPAGView>(id);
  cView->animator = PAGAnimator::MakeFrom(cView);
  XComponentHandler::AddListener(id, cView);
  napi_wrap(
      env, jsView, cView.get(),
      [](napi_env, void* finalize_data, void*) {
        JPAGView* view = static_cast<JPAGView*>(finalize_data);
        XComponentHandler::RemoveListener(view->id);
        ViewMap.erase(view->id);
      },
      nullptr, nullptr);
  ViewMap.emplace(id, cView);
  return jsView;
}

bool JPAGView::Init(napi_env env, napi_value exports) {
  napi_property_descriptor classProp[] = {
      PAG_DEFAULT_METHOD_ENTRY(flush, Flush),
      PAG_DEFAULT_METHOD_ENTRY(update, Update),
      PAG_DEFAULT_METHOD_ENTRY(setProgress, SetProgress),
      PAG_DEFAULT_METHOD_ENTRY(getProgress, GetProgress),
      PAG_DEFAULT_METHOD_ENTRY(setComposition, SetComposition),
      PAG_DEFAULT_METHOD_ENTRY(setRepeatCount, SetRepeatCount),
      PAG_DEFAULT_METHOD_ENTRY(repeatCount, RepeatCount),
      PAG_DEFAULT_METHOD_ENTRY(play, Play),
      PAG_DEFAULT_METHOD_ENTRY(pause, Pause),
      PAG_DEFAULT_METHOD_ENTRY(isPlaying, IsPlaying),
      PAG_DEFAULT_METHOD_ENTRY(setStateChangeCallback, SetStateChangeCallback),
      PAG_DEFAULT_METHOD_ENTRY(setProgressUpdateCallback, SetProgressUpdateCallback),
      PAG_DEFAULT_METHOD_ENTRY(uniqueID, UniqueID),
      PAG_DEFAULT_METHOD_ENTRY(setSync, SetSync),
      PAG_DEFAULT_METHOD_ENTRY(isSync, IsSync),
      PAG_DEFAULT_METHOD_ENTRY(setVideoEnabled, SetVideoEnabled),
      PAG_DEFAULT_METHOD_ENTRY(videoEnabled, VideoEnabled),
      PAG_DEFAULT_METHOD_ENTRY(setCacheEnabled, SetCacheEnabled),
      PAG_DEFAULT_METHOD_ENTRY(cacheEnabled, CacheEnabled),
      PAG_DEFAULT_METHOD_ENTRY(setCacheScale, SetCacheScale),
      PAG_DEFAULT_METHOD_ENTRY(cacheScale, CacheScale),
      PAG_DEFAULT_METHOD_ENTRY(setMaxFrameRate, SetMaxFrameRate),
      PAG_DEFAULT_METHOD_ENTRY(maxFrameRate, MaxFrameRate),
      PAG_DEFAULT_METHOD_ENTRY(setScaleMode, SetScaleMode),
      PAG_DEFAULT_METHOD_ENTRY(scaleMode, ScaleMode),
      PAG_DEFAULT_METHOD_ENTRY(setMatrix, SetMatrix),
      PAG_DEFAULT_METHOD_ENTRY(matrix, Matrix),
      PAG_DEFAULT_METHOD_ENTRY(currentFrame, CurrentFrame),
      PAG_DEFAULT_METHOD_ENTRY(getLayersUnderPoint, GetLayersUnderPoint),
      PAG_DEFAULT_METHOD_ENTRY(getBounds, GetBounds),
      PAG_DEFAULT_METHOD_ENTRY(freeCache, FreeCache),
      PAG_DEFAULT_METHOD_ENTRY(makeSnapshot, MakeSnapshot),
      PAG_DEFAULT_METHOD_ENTRY(useDiskCache, UseDiskCache),
      PAG_DEFAULT_METHOD_ENTRY(setUseDiskCache, SetUseDiskCache),
      PAG_DEFAULT_METHOD_ENTRY(release, Release)};
  auto status = DefineClass(env, exports, ClassName(), sizeof(classProp) / sizeof(classProp[0]),
                            classProp, Constructor, "");
  if (status != napi_ok) {
    return false;
  }
  return true;
}

JPAGView::~JPAGView() {
  release();
}

void JPAGView::onAnimationStart(PAGAnimator*) {
  std::lock_guard lock_guard(locker);
  if (playingStateCallback) {
    napi_call_threadsafe_function(playingStateCallback,
                                  const_cast<uint8_t*>(&PAGAnimatorState::Start),
                                  napi_threadsafe_function_call_mode::napi_tsfn_nonblocking);
  }
}

void JPAGView::onAnimationCancel(PAGAnimator*) {
  std::lock_guard lock_guard(locker);
  if (playingStateCallback) {
    napi_call_threadsafe_function(playingStateCallback,
                                  const_cast<uint8_t*>(&PAGAnimatorState::Cancel),
                                  napi_threadsafe_function_call_mode::napi_tsfn_nonblocking);
  }
}

void JPAGView::onAnimationEnd(PAGAnimator*) {
  std::lock_guard lock_guard(locker);
  if (playingStateCallback) {
    napi_call_threadsafe_function(playingStateCallback,
                                  const_cast<uint8_t*>(&PAGAnimatorState::End),
                                  napi_threadsafe_function_call_mode::napi_tsfn_nonblocking);
  }
}

void JPAGView::onAnimationRepeat(PAGAnimator*) {
  std::lock_guard lock_guard(locker);
  if (playingStateCallback) {
    napi_call_threadsafe_function(playingStateCallback,
                                  const_cast<uint8_t*>(&PAGAnimatorState::Repeat),
                                  napi_threadsafe_function_call_mode::napi_tsfn_nonblocking);
  }
}

void JPAGView::onAnimationUpdate(PAGAnimator* animator) {
  std::lock_guard lock_guard(locker);
  if (progressCallback) {
    napi_call_threadsafe_function(progressCallback, nullptr,
                                  napi_threadsafe_function_call_mode::napi_tsfn_nonblocking);
  }

  if (player) {
    player->setProgress(animator->progress());
    player->flush();
  }
}

void JPAGView::onSurfaceCreated(NativeWindow* window) {
  std::lock_guard lock_guard(locker);
  auto drawable = pag::GPUDrawable::FromWindow(window);
  if (player == nullptr || animator == nullptr) {
    return;
  }
  player->setSurface(pag::PAGSurface::MakeFrom(drawable));
}

void JPAGView::onSurfaceSizeChanged() {
  std::lock_guard lock_guard(locker);
  if (player == nullptr) {
    return;
  }
  auto surface = player->getSurface();
  if (surface) {
    surface->updateSize();
  }
}

void JPAGView::onSurfaceDestroyed() {
  std::lock_guard lock_guard(locker);
  if (player == nullptr) {
    return;
  }
  player->setSurface(nullptr);
}

void JPAGView::release() {
  // A memory leak may occur if the timer is not cancelled upon release.
  if (animator) {
    animator->cancel();
  }

  std::lock_guard lock_guard(locker);
  if (progressCallback != nullptr) {
    napi_release_threadsafe_function(progressCallback, napi_tsfn_abort);
    progressCallback = nullptr;
  }
  if (playingStateCallback != nullptr) {
    napi_release_threadsafe_function(playingStateCallback, napi_tsfn_abort);
    playingStateCallback = nullptr;
  }

  if (animator) {
    animator = nullptr;
  }

  if (player) {
    player->setSurface(nullptr);
    player = nullptr;
  }
}

std::shared_ptr<PAGAnimator> JPAGView::getAnimator() {
  std::lock_guard lock_guard(locker);
  return animator;
}

std::shared_ptr<PAGPlayer> JPAGView::getPlayer() {
  std::lock_guard lock_guard(locker);
  return player;
}
}  // namespace pag
