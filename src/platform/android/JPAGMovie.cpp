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

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <assert.h>
#include "JNIHelper.h"
#include "JPAGImage.h"

namespace pag {
static jfieldID PAGImage_nativeContext;
}

using namespace pag;

std::shared_ptr<PAGMovie> getPAGMovie(JNIEnv* env, jobject thiz) {
  auto pagImage = reinterpret_cast<JPAGImage*>(env->GetLongField(thiz, PAGImage_nativeContext));
  if (pagImage == nullptr) {
    return nullptr;
  }
  return std::static_pointer_cast<pag::PAGMovie>(pagImage->get());
}

extern "C" {

JNIEXPORT void Java_org_libpag_PAGMovie_nativeInit(JNIEnv* env, jclass clazz) {
  PAGImage_nativeContext = env->GetFieldID(clazz, "nativeContext", "J");
}

JNIEXPORT jlong Java_org_libpag_PAGMovie_MakeFromComposition(JNIEnv* env, jclass,
                                                             jobject composition) {
  auto pagComposition = ToPAGCompositionNativeObject(env, composition);
  auto movie = PAGMovie::FromComposition(pagComposition);
  if (movie == nullptr) {
    return 0;
  }
  auto pagImage = new JPAGImage(movie);
  return reinterpret_cast<jlong>(pagImage);
}

JNIEXPORT jlong JNICALL Java_org_libpag_PAGMovie_MakeFromVideoPath__Ljava_lang_String_2(
    JNIEnv* env, jclass, jstring pathObj) {
  if (pathObj == nullptr) {
    LOGE("PAGMovie.FromVideoPath() Invalid path specified.");
    return 0;
  }
  auto path = SafeConvertToStdString(env, pathObj);
  if (path.empty()) {
    return 0;
  }
  auto movie = PAGMovie::FromVideoPath(path);
  if (movie == nullptr) {
    return 0;
  }
  auto pagImage = new JPAGImage(movie);
  return reinterpret_cast<jlong>(pagImage);
}

JNIEXPORT jlong JNICALL Java_org_libpag_PAGMovie_MakeFromVideoPath__Ljava_lang_String_2JJ(
    JNIEnv* env, jclass, jstring pathObj, jlong startTime, jlong duration) {
  if (pathObj == nullptr) {
    LOGE("PAGMovie.FromVideoPath() Invalid path specified.");
    return 0;
  }
  auto path = SafeConvertToStdString(env, pathObj);
  if (path.empty()) {
    return 0;
  }
  auto movie = PAGMovie::FromVideoPath(path, startTime, duration);
  if (movie == nullptr) {
    return 0;
  }
  auto pagImage = new JPAGImage(movie);
  return reinterpret_cast<jlong>(pagImage);
}

JNIEXPORT jlong Java_org_libpag_PAGMovie_duration(JNIEnv* env, jobject thiz) {
  auto movie = getPAGMovie(env, thiz);
  if (movie == nullptr) {
    return 0;
  }
  return movie->duration();
}
}
