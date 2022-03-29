/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

JNIEXPORT void Java_org_libpag_PAGPlayer_nativeInit(JNIEnv* env, jclass clazz) {
  PAGPlayer_nativeContext = env->GetFieldID(clazz, "nativeContext", "J");
  // 调用堆栈源头从C++触发而不是Java触发的情况下，FindClass
  // 可能会失败，因此要提前初始化这部分反射方法。
  NativePlatform::InitJNI(env);
}

JNIEXPORT void Java_org_libpag_PAGPlayer_nativeSetup(JNIEnv* env, jobject thiz) {
  auto player = std::make_shared<PAGPlayer>();
  setPAGPlayer(env, thiz, new JPAGPlayer(player));
}

JNIEXPORT void Java_org_libpag_PAGPlayer_nativeRelease(JNIEnv* env, jobject thiz) {
  auto jPlayer = reinterpret_cast<JPAGPlayer*>(env->GetLongField(thiz, PAGPlayer_nativeContext));
  if (jPlayer != nullptr) {
    jPlayer->clear();
  }
}

JNIEXPORT void Java_org_libpag_PAGPlayer_nativeFinalize(JNIEnv* env, jobject thiz) {
  setPAGPlayer(env, thiz, nullptr);
}

JNIEXPORT jobject Java_org_libpag_PAGPlayer_getComposition(JNIEnv* env, jobject thiz) {
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

JNIEXPORT void Java_org_libpag_PAGPlayer_setComposition(JNIEnv* env, jobject thiz,
                                                        jobject newComposition) {
  auto player = getPAGPlayer(env, thiz);
  if (player == nullptr) {
    return;
  }
  auto composition = ToPAGCompositionNativeObject(env, newComposition);
  player->setComposition(composition);
}

JNIEXPORT void Java_org_libpag_PAGPlayer_nativeSetSurface(JNIEnv* env, jobject thiz,
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

JNIEXPORT jboolean Java_org_libpag_PAGPlayer_videoEnabled(JNIEnv* env, jobject thiz) {
  auto player = getPAGPlayer(env, thiz);
  if (player == nullptr) {
    return 0;
  }
  return static_cast<jboolean>(player->videoEnabled());
}

JNIEXPORT void Java_org_libpag_PAGPlayer_setVideoEnabled(JNIEnv* env, jobject thiz,
                                                         jboolean value) {
  auto player = getPAGPlayer(env, thiz);
  if (player == nullptr) {
    return;
  }
  player->setVideoEnabled(value);
}

JNIEXPORT jboolean Java_org_libpag_PAGPlayer_cacheEnabled(JNIEnv* env, jobject thiz) {
  auto player = getPAGPlayer(env, thiz);
  if (player == nullptr) {
    return 0;
  }
  return static_cast<jboolean>(player->cacheEnabled());
}

JNIEXPORT void Java_org_libpag_PAGPlayer_setCacheEnabled(JNIEnv* env, jobject thiz,
                                                         jboolean value) {
  auto player = getPAGPlayer(env, thiz);
  if (player == nullptr) {
    return;
  }
  player->setCacheEnabled(value);
}

JNIEXPORT jfloat Java_org_libpag_PAGPlayer_cacheScale(JNIEnv* env, jobject thiz) {
  auto player = getPAGPlayer(env, thiz);
  if (player == nullptr) {
    return 0;
  }
  return player->cacheScale();
}

JNIEXPORT void Java_org_libpag_PAGPlayer_setCacheScale(JNIEnv* env, jobject thiz, jfloat value) {
  auto player = getPAGPlayer(env, thiz);
  if (player == nullptr) {
    return;
  }
  player->setCacheScale(value);
}

JNIEXPORT jfloat Java_org_libpag_PAGPlayer_maxFrameRate(JNIEnv* env, jobject thiz) {
  auto player = getPAGPlayer(env, thiz);
  if (player == nullptr) {
    return 0;
  }
  return player->maxFrameRate();
}

JNIEXPORT void Java_org_libpag_PAGPlayer_setMaxFrameRate(JNIEnv* env, jobject thiz, jfloat value) {
  auto player = getPAGPlayer(env, thiz);
  if (player == nullptr) {
    return;
  }
  player->setMaxFrameRate(value);
}

JNIEXPORT jint Java_org_libpag_PAGPlayer_scaleMode(JNIEnv* env, jobject thiz) {
  auto player = getPAGPlayer(env, thiz);
  if (player == nullptr) {
    return 0;
  }
  return player->scaleMode();
}

JNIEXPORT void Java_org_libpag_PAGPlayer_setScaleMode(JNIEnv* env, jobject thiz, jint value) {
  auto player = getPAGPlayer(env, thiz);
  if (player == nullptr) {
    return;
  }
  player->setScaleMode(value);
}

JNIEXPORT void Java_org_libpag_PAGPlayer_nativeGetMatrix(JNIEnv* env, jobject thiz,
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

JNIEXPORT void Java_org_libpag_PAGPlayer_nativeSetMatrix(JNIEnv* env, jobject thiz, jfloat a,
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

JNIEXPORT jlong Java_org_libpag_PAGPlayer_duration(JNIEnv* env, jobject thiz) {
  auto player = getPAGPlayer(env, thiz);
  if (player == nullptr) {
    return 0;
  }
  return player->duration();
}

JNIEXPORT jdouble Java_org_libpag_PAGPlayer_getProgress(JNIEnv* env, jobject thiz) {
  auto player = getPAGPlayer(env, thiz);
  if (player == nullptr) {
    return 0;
  }
  return player->getProgress();
}

JNIEXPORT void Java_org_libpag_PAGPlayer_setProgress(JNIEnv* env, jobject thiz, jdouble value) {
  auto player = getPAGPlayer(env, thiz);
  if (player == nullptr) {
    return;
  }
  player->setProgress(value);
}

JNIEXPORT void Java_org_libpag_PAGPlayer_prepare(JNIEnv* env, jobject thiz) {
  auto player = getPAGPlayer(env, thiz);
  if (player == nullptr) {
    return;
  }
  player->prepare();
}

JNIEXPORT jboolean Java_org_libpag_PAGPlayer_flushAndFenceSync(JNIEnv* env, jobject thiz,
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

JNIEXPORT jboolean Java_org_libpag_PAGPlayer_waitSync(JNIEnv* env, jobject thiz, jlong sync) {
  auto player = getPAGPlayer(env, thiz);
  if (player == nullptr || sync == 0) {
    return false;
  }
  BackendSemaphore semaphore;
  semaphore.initGL(reinterpret_cast<void*>(sync));
  return player->wait(semaphore);
}

JNIEXPORT jobject Java_org_libpag_PAGPlayer_getBounds(JNIEnv* env, jobject thiz, jobject layer) {
  auto player = getPAGPlayer(env, thiz);
  if (player == nullptr) {
    return MakeRectFObject(env, 0.0f, 0.0f, 0.0f, 0.0f);
  }
  auto pagLayer = ToPAGLayerNativeObject(env, layer);
  auto rect = player->getBounds(pagLayer);
  return MakeRectFObject(env, rect.x(), rect.y(), rect.width(), rect.height());
}

JNIEXPORT jobjectArray Java_org_libpag_PAGPlayer_getLayersUnderPoint(JNIEnv* env, jobject thiz,
                                                                     jfloat x, jfloat y) {
  auto player = getPAGPlayer(env, thiz);
  if (player == nullptr) {
    return ToPAGLayerJavaObjectList(env, {});
  }

  auto layers = player->getLayersUnderPoint(x, y);
  return ToPAGLayerJavaObjectList(env, layers);
}

JNIEXPORT jboolean Java_org_libpag_PAGPlayer_hitTestPoint(JNIEnv* env, jobject thiz, jobject layer,
                                                          jfloat x, jfloat y,
                                                          jboolean pixelHitTest) {
  auto player = getPAGPlayer(env, thiz);
  if (player == nullptr) {
    return JNI_FALSE;
  }
  auto pagLayer = ToPAGLayerNativeObject(env, layer);
  return (jboolean)player->hitTestPoint(pagLayer, x, y, pixelHitTest);
}
}
