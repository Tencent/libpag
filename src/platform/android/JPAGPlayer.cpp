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

#include "JPAGPlayer.h"
#include "JNIHelper.h"
#include "JPAGLayerHandle.h"
#include "JPAGSurface.h"
#include "NativePlatform.h"

#ifdef PAG_USE_FFAVC
#include "ffavc.h"
#endif

namespace pag {
static jfieldID PAGPlayer_nativeContext;
}

using namespace pag;

std::shared_ptr<PAGPlayer> getPAGPlayer(JNIEnv* env, jobject thiz) {
  auto jPlayer = reinterpret_cast<JPAGPlayer*>(env->GetLongField(thiz, PAGPlayer_nativeContext));
  if (jPlayer == nullptr) {
    return nullptr;
  }
  return jPlayer->get();
}

void setPAGPlayer(JNIEnv* env, jobject thiz, JPAGPlayer* player) {
  auto old = reinterpret_cast<JPAGPlayer*>(env->GetLongField(thiz, PAGPlayer_nativeContext));
  if (old != nullptr) {
    delete old;
  }
  env->SetLongField(thiz, PAGPlayer_nativeContext, (jlong)player);
}

extern "C" {

PAG_API void Java_org_libpag_PAGPlayer_nativeInit(JNIEnv* env, jclass clazz) {
  PAGPlayer_nativeContext = env->GetFieldID(clazz, "nativeContext", "J");
}

PAG_API void Java_org_libpag_PAGPlayer_nativeSetup(JNIEnv* env, jobject thiz) {
  auto player = std::make_shared<PAGPlayer>();
  setPAGPlayer(env, thiz, new JPAGPlayer(player));
}

PAG_API void Java_org_libpag_PAGPlayer_nativeRelease(JNIEnv* env, jobject thiz) {
  auto jPlayer = reinterpret_cast<JPAGPlayer*>(env->GetLongField(thiz, PAGPlayer_nativeContext));
  if (jPlayer != nullptr) {
    jPlayer->clear();
  }
}

PAG_API void Java_org_libpag_PAGPlayer_nativeFinalize(JNIEnv* env, jobject thiz) {
  setPAGPlayer(env, thiz, nullptr);
}

PAG_API jobject Java_org_libpag_PAGPlayer_getComposition(JNIEnv* env, jobject thiz) {
  auto player = getPAGPlayer(env, thiz);
  if (player == nullptr) {
    return nullptr;
  }
  // 必须从pagPlayer里返回，不能额外存储一个引用，因为同一个PAGComposition添加到别的PAGPlayer后会从当前的移除。
  auto composition = player->getComposition();
  if (composition == nullptr) {
    return nullptr;
  }
  return ToPAGLayerJavaObject(env, composition);
}

PAG_API void Java_org_libpag_PAGPlayer_setComposition(JNIEnv* env, jobject thiz,
                                                      jobject newComposition) {
  auto player = getPAGPlayer(env, thiz);
  if (player == nullptr) {
    return;
  }
  auto composition = ToPAGCompositionNativeObject(env, newComposition);
  player->setComposition(composition);
}

PAG_API void Java_org_libpag_PAGPlayer_nativeSetSurface(JNIEnv* env, jobject thiz,
                                                        jlong surfaceObject) {
  auto player = getPAGPlayer(env, thiz);
  if (player == nullptr) {
    return;
  }
  auto surface = reinterpret_cast<JPAGSurface*>(surfaceObject);
  if (surface != nullptr) {
    player->setSurface(surface->get());
  } else {
    player->setSurface(nullptr);
  }
}

PAG_API jboolean Java_org_libpag_PAGPlayer_videoEnabled(JNIEnv* env, jobject thiz) {
  auto player = getPAGPlayer(env, thiz);
  if (player == nullptr) {
    return 0;
  }
  return static_cast<jboolean>(player->videoEnabled());
}

PAG_API void Java_org_libpag_PAGPlayer_setVideoEnabled(JNIEnv* env, jobject thiz, jboolean value) {
  auto player = getPAGPlayer(env, thiz);
  if (player == nullptr) {
    return;
  }
  player->setVideoEnabled(value);
}

PAG_API jboolean Java_org_libpag_PAGPlayer_cacheEnabled(JNIEnv* env, jobject thiz) {
  auto player = getPAGPlayer(env, thiz);
  if (player == nullptr) {
    return 0;
  }
  return static_cast<jboolean>(player->cacheEnabled());
}

PAG_API void Java_org_libpag_PAGPlayer_setCacheEnabled(JNIEnv* env, jobject thiz, jboolean value) {
  auto player = getPAGPlayer(env, thiz);
  if (player == nullptr) {
    return;
  }
  player->setCacheEnabled(value);
}

PAG_API jfloat Java_org_libpag_PAGPlayer_cacheScale(JNIEnv* env, jobject thiz) {
  auto player = getPAGPlayer(env, thiz);
  if (player == nullptr) {
    return 0;
  }
  return player->cacheScale();
}

PAG_API void Java_org_libpag_PAGPlayer_setCacheScale(JNIEnv* env, jobject thiz, jfloat value) {
  auto player = getPAGPlayer(env, thiz);
  if (player == nullptr) {
    return;
  }
  player->setCacheScale(value);
}

PAG_API jfloat Java_org_libpag_PAGPlayer_maxFrameRate(JNIEnv* env, jobject thiz) {
  auto player = getPAGPlayer(env, thiz);
  if (player == nullptr) {
    return 0;
  }
  return player->maxFrameRate();
}

PAG_API void Java_org_libpag_PAGPlayer_setMaxFrameRate(JNIEnv* env, jobject thiz, jfloat value) {
  auto player = getPAGPlayer(env, thiz);
  if (player == nullptr) {
    return;
  }
  player->setMaxFrameRate(value);
}

PAG_API jint Java_org_libpag_PAGPlayer_scaleMode(JNIEnv* env, jobject thiz) {
  auto player = getPAGPlayer(env, thiz);
  if (player == nullptr) {
    return 0;
  }
  return static_cast<jint>(player->scaleMode());
}

PAG_API void Java_org_libpag_PAGPlayer_setScaleMode(JNIEnv* env, jobject thiz, jint value) {
  auto player = getPAGPlayer(env, thiz);
  if (player == nullptr) {
    return;
  }
  player->setScaleMode(static_cast<PAGScaleMode>(value));
}

PAG_API void Java_org_libpag_PAGPlayer_nativeGetMatrix(JNIEnv* env, jobject thiz,
                                                       jfloatArray values) {
  auto list = env->GetFloatArrayElements(values, nullptr);
  auto player = getPAGPlayer(env, thiz);
  if (player != nullptr) {
    auto matrix = player->matrix();
    matrix.get9(list);
  } else {
    Matrix matrix = {};
    matrix.setIdentity();
    matrix.get9(list);
  }
  env->SetFloatArrayRegion(values, 0, 9, list);
  env->ReleaseFloatArrayElements(values, list, 0);
}

PAG_API void Java_org_libpag_PAGPlayer_nativeSetMatrix(JNIEnv* env, jobject thiz, jfloat a,
                                                       jfloat b, jfloat c, jfloat d, jfloat tx,
                                                       jfloat ty) {
  auto player = getPAGPlayer(env, thiz);
  if (player == nullptr) {
    return;
  }
  Matrix matrix = {};
  matrix.setAll(a, c, tx, b, d, ty, 0, 0, 1);
  player->setMatrix(matrix);
}

PAG_API jlong Java_org_libpag_PAGPlayer_duration(JNIEnv* env, jobject thiz) {
  auto player = getPAGPlayer(env, thiz);
  if (player == nullptr) {
    return 0;
  }
  return player->duration();
}

PAG_API jdouble Java_org_libpag_PAGPlayer_getProgress(JNIEnv* env, jobject thiz) {
  auto player = getPAGPlayer(env, thiz);
  if (player == nullptr) {
    return 0;
  }
  return player->getProgress();
}

PAG_API void Java_org_libpag_PAGPlayer_setProgress(JNIEnv* env, jobject thiz, jdouble value) {
  auto player = getPAGPlayer(env, thiz);
  if (player == nullptr) {
    return;
  }
  player->setProgress(value);
}

PAG_API jlong Java_org_libpag_PAGPlayer_currentFrame(JNIEnv* env, jobject thiz) {
  auto player = getPAGPlayer(env, thiz);
  if (player == nullptr) {
    return 0;
  }
  return player->currentFrame();
}

PAG_API void Java_org_libpag_PAGPlayer_prepare(JNIEnv* env, jobject thiz) {
  auto player = getPAGPlayer(env, thiz);
  if (player == nullptr) {
    return;
  }
  player->prepare();
}

PAG_API jboolean Java_org_libpag_PAGPlayer_flushAndFenceSync(JNIEnv* env, jobject thiz,
                                                             jlongArray syncArray) {
  auto player = getPAGPlayer(env, thiz);
  if (player == nullptr) {
    return 0;
  }
  if (syncArray == nullptr || env->GetArrayLength(syncArray) == 0) {
    return static_cast<jboolean>(player->flush());
  }
  auto array = env->GetLongArrayElements(syncArray, nullptr);
  if (array == nullptr) {
    return static_cast<jboolean>(player->flush());
  }
  BackendSemaphore semaphore;
  auto result = player->flushAndSignalSemaphore(&semaphore);
  array[0] = semaphore.isInitialized() ? reinterpret_cast<jlong>(semaphore.glSync()) : 0;
  env->ReleaseLongArrayElements(syncArray, array, 0);
  return result;
}

PAG_API jboolean Java_org_libpag_PAGPlayer_waitSync(JNIEnv* env, jobject thiz, jlong sync) {
  auto player = getPAGPlayer(env, thiz);
  if (player == nullptr || sync == 0) {
    return false;
  }
  BackendSemaphore semaphore;
  semaphore.initGL(reinterpret_cast<void*>(sync));
  return player->wait(semaphore);
}

PAG_API jobject Java_org_libpag_PAGPlayer_getBounds(JNIEnv* env, jobject thiz, jobject layer) {
  auto player = getPAGPlayer(env, thiz);
  if (player == nullptr) {
    return MakeRectFObject(env, 0.0f, 0.0f, 0.0f, 0.0f);
  }
  auto pagLayer = ToPAGLayerNativeObject(env, layer);
  auto rect = player->getBounds(pagLayer);
  return MakeRectFObject(env, rect.x(), rect.y(), rect.width(), rect.height());
}

PAG_API jobjectArray Java_org_libpag_PAGPlayer_getLayersUnderPoint(JNIEnv* env, jobject thiz,
                                                                   jfloat x, jfloat y) {
  auto player = getPAGPlayer(env, thiz);
  if (player == nullptr) {
    return ToPAGLayerJavaObjectList(env, {});
  }

  auto layers = player->getLayersUnderPoint(x, y);
  return ToPAGLayerJavaObjectList(env, layers);
}

PAG_API jboolean Java_org_libpag_PAGPlayer_hitTestPoint(JNIEnv* env, jobject thiz, jobject layer,
                                                        jfloat x, jfloat y, jboolean pixelHitTest) {
  auto player = getPAGPlayer(env, thiz);
  if (player == nullptr) {
    return JNI_FALSE;
  }
  auto pagLayer = ToPAGLayerNativeObject(env, layer);
  return (jboolean)player->hitTestPoint(pagLayer, x, y, pixelHitTest);
}

PAG_API void Java_org_libpag_PAGPlayer_setUseDiskCache(JNIEnv* env, jobject thiz, jboolean value) {
  auto player = getPAGPlayer(env, thiz);
  if (player == nullptr) {
    return;
  }
  player->setUseDiskCache(value);
}

PAG_API jboolean Java_org_libpag_PAGPlayer_useDiskCache(JNIEnv* env, jobject thiz) {
  auto player = getPAGPlayer(env, thiz);
  if (player == nullptr) {
    return JNI_FALSE;
  }
  return player->useDiskCache();
}
}
