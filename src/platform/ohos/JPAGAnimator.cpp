/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2024 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "JPAGAnimator.h"
#include <cstdint>
#include "base/utils/UniqueID.h"
#include "platform/ohos/JsHelper.h"

namespace pag {
static int PAGAnimatorStateStart = 0;
static int PAGAnimatorStateCancel = 1;
static int PAGAnimatorStateEnd = 2;
static int PAGAnimatorStateRepeat = 3;

static std::unordered_map<std::string, std::shared_ptr<JPAGAnimator>> AnimatorMap = {};

static napi_value Duration(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsAnimator = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsAnimator, nullptr);
  auto animator = JPAGAnimator::FromJs(env, jsAnimator);
  if (!animator) {
    return nullptr;
  }
  napi_value value;
  napi_create_double(env, animator->duration(), &value);
  return value;
}

static napi_value SetDuration(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_value jsAnimator = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsAnimator, nullptr);
  auto animator = JPAGAnimator::FromJs(env, jsAnimator);
  if (!animator) {
    return nullptr;
  }
  int64_t value = 0;
  auto status = napi_get_value_int64(env, args[0], &value);
  if (status == napi_status::napi_ok) {
    animator->setDuration(value);
  }
  return nullptr;
}

static napi_value Progress(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsAnimator = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsAnimator, nullptr);
  auto animator = JPAGAnimator::FromJs(env, jsAnimator);
  if (!animator) {
    return nullptr;
  }
  napi_value value;
  napi_create_double(env, animator->progress(), &value);
  return value;
}

static napi_value SetProgress(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_value jsAnimator = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsAnimator, nullptr);
  auto animator = JPAGAnimator::FromJs(env, jsAnimator);
  if (!animator) {
    return nullptr;
  }
  double value = 0;
  auto status = napi_get_value_double(env, args[0], &value);
  if (status == napi_status::napi_ok) {
    animator->setProgress(value);
  }
  return nullptr;
}

static napi_value RepeatCount(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsAnimator = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsAnimator, nullptr);
  auto animator = JPAGAnimator::FromJs(env, jsAnimator);
  if (!animator) {
    return nullptr;
  }
  napi_value value;
  napi_create_double(env, animator->repeatCount(), &value);
  return value;
}

static napi_value SetRepeatCount(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_value jsAnimator = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsAnimator, nullptr);
  auto animator = JPAGAnimator::FromJs(env, jsAnimator);
  if (!animator) {
    return nullptr;
  }
  int32_t value = 0;
  auto status = napi_get_value_int32(env, args[0], &value);
  if (status == napi_status::napi_ok) {
    animator->setRepeatCount(value);
  }
  return nullptr;
}

static napi_value IsRunning(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsAnimator = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsAnimator, nullptr);
  auto animator = JPAGAnimator::FromJs(env, jsAnimator);
  if (!animator) {
    return nullptr;
  }
  napi_value value;
  napi_get_boolean(env, animator->isRunning(), &value);
  return value;
}

static napi_value Start(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsAnimator = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsAnimator, nullptr);
  auto animator = JPAGAnimator::FromJs(env, jsAnimator);
  if (!animator) {
    return nullptr;
  }
  animator->start();
  return nullptr;
}

static napi_value Cancel(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsAnimator = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsAnimator, nullptr);
  auto animator = JPAGAnimator::FromJs(env, jsAnimator);
  if (!animator) {
    return nullptr;
  }
  animator->cancel();
  return nullptr;
}

static napi_value Update(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsAnimator = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsAnimator, nullptr);
  auto animator = JPAGAnimator::FromJs(env, jsAnimator);
  if (!animator) {
    return nullptr;
  }
  animator->update();
  return nullptr;
}

static void StateChangeCallback(napi_env env, napi_value callback, void*, void* data) {
  int state = *static_cast<int*>(data);
  size_t argc = 1;
  napi_value argv[1] = {0};
  napi_create_uint32(env, state, &argv[0]);
  napi_value undefined;
  napi_get_undefined(env, &undefined);
  napi_call_function(env, undefined, callback, argc, argv, nullptr);
}

static napi_value SetStateChangeCallback(napi_env env, napi_callback_info info) {
  napi_value jsAnimator = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsAnimator, nullptr);
  if (argc == 0) {
    return nullptr;
  }
  JPAGAnimator* animator = nullptr;
  napi_unwrap(env, jsAnimator, reinterpret_cast<void**>(&animator));

  napi_value resourceName = nullptr;
  napi_create_string_utf8(env, "PAGAnimatorStateChangeCallback", NAPI_AUTO_LENGTH, &resourceName);

  napi_create_threadsafe_function(env, args[0], nullptr, resourceName, 0, 1, nullptr, nullptr,
                                  animator, StateChangeCallback, &animator->playingStateCallback);
  return nullptr;
}

static void ProgressCallback(napi_env env, napi_value callback, void*, void* data) {
  double* progressPtr = static_cast<double*>(data);
  size_t argc = 1;
  napi_value argv[1] = {0};
  napi_create_double(env, *progressPtr, &argv[0]);
  napi_value undefined;
  napi_get_undefined(env, &undefined);
  napi_call_function(env, undefined, callback, argc, argv, nullptr);
  delete progressPtr;
}

static napi_value SetProgressUpdateCallback(napi_env env, napi_callback_info info) {
  napi_value jsAnimator = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsAnimator, nullptr);
  if (argc == 0) {
    return nullptr;
  }
  JPAGAnimator* animator = nullptr;
  napi_unwrap(env, jsAnimator, reinterpret_cast<void**>(&animator));
  napi_value resourceName = nullptr;
  napi_create_string_utf8(env, "PAGAnimatorProgressCallback", NAPI_AUTO_LENGTH, &resourceName);
  napi_create_threadsafe_function(env, args[0], nullptr, resourceName, 0, 1, nullptr, nullptr,
                                  nullptr, ProgressCallback, &animator->progressCallback);
  return nullptr;
}

static napi_value SetSync(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_value jsAnimator = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsAnimator, nullptr);
  auto animator = JPAGAnimator::FromJs(env, jsAnimator);
  if (!animator) {
    return nullptr;
  }
  bool value = false;
  napi_get_value_bool(env, args[0], &value);
  animator->setSync(value);
  return nullptr;
}

napi_value JPAGAnimator::Constructor(napi_env env, napi_callback_info info) {
  napi_value jsAnimator = nullptr;
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_get_cb_info(env, info, &argc, args, &jsAnimator, nullptr);
  std::string id = "PAGAnimator" + std::to_string(UniqueID::Next());
  auto cAnimator = std::make_shared<JPAGAnimator>(id);
  cAnimator->pagAnimator = PAGAnimator::MakeFrom(cAnimator);
  cAnimator->pagAnimator->setRepeatCount(-1);
  napi_wrap(
      env, jsAnimator, cAnimator.get(),
      [](napi_env, void* finalize_data, void*) {
        JPAGAnimator* animator = static_cast<JPAGAnimator*>(finalize_data);
        AnimatorMap.erase(animator->id);
        delete animator;
      },
      nullptr, nullptr);
  AnimatorMap.emplace(id, cAnimator);
  return jsAnimator;
}

bool JPAGAnimator::Init(napi_env env, napi_value exports) {
  napi_property_descriptor classProp[] = {
      PAG_DEFAULT_METHOD_ENTRY(duration, Duration),
      PAG_DEFAULT_METHOD_ENTRY(setDuration, SetDuration),
      PAG_DEFAULT_METHOD_ENTRY(progress, Progress),
      PAG_DEFAULT_METHOD_ENTRY(setProgress, SetProgress),
      PAG_DEFAULT_METHOD_ENTRY(repeatCount, RepeatCount),
      PAG_DEFAULT_METHOD_ENTRY(setRepeatCount, SetRepeatCount),
      PAG_DEFAULT_METHOD_ENTRY(isRunning, IsRunning),
      PAG_DEFAULT_METHOD_ENTRY(start, Start),
      PAG_DEFAULT_METHOD_ENTRY(cancel, Cancel),
      PAG_DEFAULT_METHOD_ENTRY(update, Update),
      PAG_DEFAULT_METHOD_ENTRY(setStateChangeCallback, SetStateChangeCallback),
      PAG_DEFAULT_METHOD_ENTRY(setProgressUpdateCallback, SetProgressUpdateCallback),
      PAG_DEFAULT_METHOD_ENTRY(setSync, SetSync),
  };

  auto status = DefineClass(env, exports, ClassName(), sizeof(classProp) / sizeof(classProp[0]),
                            classProp, Constructor, "");
  return status == napi_ok;
}

void JPAGAnimator::onAnimationStart(PAGAnimator*) {

  napi_call_threadsafe_function(playingStateCallback, &PAGAnimatorStateStart,
                                napi_threadsafe_function_call_mode::napi_tsfn_nonblocking);
}

void JPAGAnimator::onAnimationCancel(PAGAnimator*) {
  napi_call_threadsafe_function(playingStateCallback, &PAGAnimatorStateCancel,
                                napi_threadsafe_function_call_mode::napi_tsfn_nonblocking);
}

void JPAGAnimator::onAnimationEnd(PAGAnimator*) {
  napi_call_threadsafe_function(playingStateCallback, &PAGAnimatorStateEnd,
                                napi_threadsafe_function_call_mode::napi_tsfn_nonblocking);
}

void JPAGAnimator::onAnimationRepeat(PAGAnimator*) {
  napi_call_threadsafe_function(playingStateCallback, &PAGAnimatorStateRepeat,
                                napi_threadsafe_function_call_mode::napi_tsfn_nonblocking);
}

void JPAGAnimator::onAnimationUpdate(PAGAnimator* animator) {
  napi_call_threadsafe_function(progressCallback, new double(animator->progress()),
                                napi_threadsafe_function_call_mode::napi_tsfn_nonblocking);
}

std::shared_ptr<PAGAnimator> JPAGAnimator::FromJs(napi_env env, napi_value value) {
  JPAGAnimator* cAnimator = nullptr;
  auto status = napi_unwrap(env, value, reinterpret_cast<void**>(&cAnimator));
  if (status == napi_ok) {
    return cAnimator->animator();
  } else {
    return nullptr;
  }
}
}  // namespace pag
