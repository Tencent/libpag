/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include "JNIHelper.h"
#include "rendering/PAGAnimator.h"

namespace pag {
static jfieldID PAGAnimator_nativeContext;
static jmethodID PAGAnimator_onAnimationStart;
static jmethodID PAGAnimator_onAnimationEnd;
static jmethodID PAGAnimator_onAnimationCancel;
static jmethodID PAGAnimator_onAnimationRepeat;
static jmethodID PAGAnimator_onAnimationUpdate;

class AnimatorListener : public pag::PAGAnimator::Listener {
 public:
  AnimatorListener(JNIEnv* env, jobject animatorObject) {
    weakAnimator = env->NewWeakGlobalRef(animatorObject);
  }

  ~AnimatorListener() override {
    JNIEnvironment environment;
    auto env = environment.current();
    if (env == nullptr) {
      return;
    }
    env->DeleteWeakGlobalRef(weakAnimator);
  }

 protected:
  void onAnimationStart(pag::PAGAnimator*) override {
    JNIEnvironment environment;
    auto env = environment.current();
    auto animatorObject = getAnimatorObject(env);
    if (animatorObject == nullptr) {
      return;
    }
    env->CallVoidMethod(animatorObject, PAGAnimator_onAnimationStart);
  }

  void onAnimationEnd(pag::PAGAnimator*) override {
    JNIEnvironment environment;
    auto env = environment.current();
    auto animatorObject = getAnimatorObject(env);
    if (animatorObject == nullptr) {
      return;
    }
    env->CallVoidMethod(animatorObject, PAGAnimator_onAnimationEnd);
  }

  void onAnimationCancel(pag::PAGAnimator*) override {
    JNIEnvironment environment;
    auto env = environment.current();
    auto animatorObject = getAnimatorObject(env);
    if (animatorObject == nullptr) {
      return;
    }
    env->CallVoidMethod(animatorObject, PAGAnimator_onAnimationCancel);
  }

  void onAnimationRepeat(pag::PAGAnimator*) override {
    JNIEnvironment environment;
    auto env = environment.current();
    auto animatorObject = getAnimatorObject(env);
    if (animatorObject == nullptr) {
      return;
    }
    env->CallVoidMethod(animatorObject, PAGAnimator_onAnimationRepeat);
  }

  void onAnimationUpdate(pag::PAGAnimator*) override {
    JNIEnvironment environment;
    auto env = environment.current();
    auto animatorObject = getAnimatorObject(env);
    if (animatorObject == nullptr) {
      return;
    }
    env->CallVoidMethod(animatorObject, PAGAnimator_onAnimationUpdate);
  }

 private:
  jobject weakAnimator = nullptr;

  jobject getAnimatorObject(JNIEnv* env) {
    if (env == nullptr || weakAnimator == nullptr) {
      return nullptr;
    }
    if (env->IsSameObject(weakAnimator, nullptr)) {
      return nullptr;
    }
    return weakAnimator;
  }
};

class JPAGAnimator {
 public:
  JPAGAnimator(JNIEnv* env, jobject animatorObject) {
    listener = std::make_shared<AnimatorListener>(env, animatorObject);
    animator = pag::PAGAnimator::MakeFrom(listener);
  }

  ~JPAGAnimator() {
    clear();
  }

  std::shared_ptr<pag::PAGAnimator> get() {
    std::lock_guard<std::mutex> autoLock(locker);
    return animator;
  }

  void clear() {
    std::shared_ptr<pag::PAGAnimator> tempAnimator;
    std::shared_ptr<pag::AnimatorListener> tempListener;
    {
      std::lock_guard<std::mutex> autoLock(locker);
      tempAnimator = std::move(animator);
      tempListener = std::move(listener);
    }
    // Call cancel outside the lock to avoid deadlock
    if (tempAnimator) {
      tempAnimator->cancel();
    }
  }

 private:
  std::mutex locker;
  std::shared_ptr<pag::AnimatorListener> listener;
  std::shared_ptr<pag::PAGAnimator> animator;
};
}  // namespace pag

using namespace pag;

std::shared_ptr<PAGAnimator> getPAGAnimator(JNIEnv* env, jobject thiz) {
  auto animator =
      reinterpret_cast<JPAGAnimator*>(env->GetLongField(thiz, PAGAnimator_nativeContext));
  if (animator == nullptr) {
    return nullptr;
  }
  return animator->get();
}

void setPAGAnimator(JNIEnv* env, jobject thiz, JPAGAnimator* animator) {
  auto old = reinterpret_cast<JPAGAnimator*>(env->GetLongField(thiz, PAGAnimator_nativeContext));
  delete old;
  env->SetLongField(thiz, PAGAnimator_nativeContext, (jlong)animator);
}

extern "C" {

PAG_API void Java_org_libpag_PAGAnimator_nativeInit(JNIEnv* env, jclass clazz) {
  PAGAnimator_nativeContext = env->GetFieldID(clazz, "nativeContext", "J");
  PAGAnimator_onAnimationStart = env->GetMethodID(clazz, "onAnimationStart", "()V");
  PAGAnimator_onAnimationEnd = env->GetMethodID(clazz, "onAnimationEnd", "()V");
  PAGAnimator_onAnimationCancel = env->GetMethodID(clazz, "onAnimationCancel", "()V");
  PAGAnimator_onAnimationRepeat = env->GetMethodID(clazz, "onAnimationRepeat", "()V");
  PAGAnimator_onAnimationUpdate = env->GetMethodID(clazz, "onAnimationUpdate", "()V");
}

PAG_API void Java_org_libpag_PAGAnimator_nativeSetup(JNIEnv* env, jobject thiz) {
  setPAGAnimator(env, thiz, new JPAGAnimator(env, thiz));
}

PAG_API void Java_org_libpag_PAGAnimator_nativeRelease(JNIEnv* env, jobject thiz) {
  auto jPlayer =
      reinterpret_cast<JPAGAnimator*>(env->GetLongField(thiz, PAGAnimator_nativeContext));
  if (jPlayer != nullptr) {
    jPlayer->clear();
  }
}

PAG_API void Java_org_libpag_PAGAnimator_nativeFinalize(JNIEnv* env, jobject thiz) {
  setPAGAnimator(env, thiz, nullptr);
}

PAG_API jboolean Java_org_libpag_PAGAnimator_isSync(JNIEnv* env, jobject thiz) {
  auto animator = getPAGAnimator(env, thiz);
  if (animator == nullptr) {
    return JNI_FALSE;
  }
  return animator->isSync() ? JNI_TRUE : JNI_FALSE;
}

PAG_API void Java_org_libpag_PAGAnimator_setSync(JNIEnv* env, jobject thiz, jboolean sync) {
  auto animator = getPAGAnimator(env, thiz);
  if (animator == nullptr) {
    return;
  }
  animator->setSync(sync == JNI_TRUE);
}

PAG_API jlong Java_org_libpag_PAGAnimator_duration(JNIEnv* env, jobject thiz) {
  auto animator = getPAGAnimator(env, thiz);
  if (animator == nullptr) {
    return 0;
  }
  return animator->duration();
}

PAG_API void Java_org_libpag_PAGAnimator_setDuration(JNIEnv* env, jobject thiz, jlong duration) {
  auto animator = getPAGAnimator(env, thiz);
  if (animator == nullptr) {
    return;
  }
  animator->setDuration(duration);
}

PAG_API jint Java_org_libpag_PAGAnimator_repeatCount(JNIEnv* env, jobject thiz) {
  auto animator = getPAGAnimator(env, thiz);
  if (animator == nullptr) {
    return 0;
  }
  return animator->repeatCount();
}

PAG_API void Java_org_libpag_PAGAnimator_setRepeatCount(JNIEnv* env, jobject thiz, jint count) {
  auto animator = getPAGAnimator(env, thiz);
  if (animator == nullptr) {
    return;
  }
  animator->setRepeatCount(count);
}

PAG_API jdouble Java_org_libpag_PAGAnimator_progress(JNIEnv* env, jobject thiz) {
  auto animator = getPAGAnimator(env, thiz);
  if (animator == nullptr) {
    return 0;
  }
  return animator->progress();
}

PAG_API void Java_org_libpag_PAGAnimator_setProgress(JNIEnv* env, jobject thiz, jdouble progress) {
  auto animator = getPAGAnimator(env, thiz);
  if (animator == nullptr) {
    return;
  }
  animator->setProgress(progress);
}

PAG_API jboolean Java_org_libpag_PAGAnimator_isRunning(JNIEnv* env, jobject thiz) {
  auto animator = getPAGAnimator(env, thiz);
  if (animator == nullptr) {
    return JNI_FALSE;
  }
  return animator->isRunning() ? JNI_TRUE : JNI_FALSE;
}

PAG_API void Java_org_libpag_PAGAnimator_doStart(JNIEnv* env, jobject thiz) {
  auto animator = getPAGAnimator(env, thiz);
  if (animator == nullptr) {
    return;
  }
  animator->start();
}

PAG_API void Java_org_libpag_PAGAnimator_cancel(JNIEnv* env, jobject thiz) {
  auto animator = getPAGAnimator(env, thiz);
  if (animator == nullptr) {
    return;
  }
  animator->cancel();
}

PAG_API void Java_org_libpag_PAGAnimator_update(JNIEnv* env, jobject thiz) {
  auto animator = getPAGAnimator(env, thiz);
  if (animator == nullptr) {
    return;
  }
  animator->update();
}
}
