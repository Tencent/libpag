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

#include "JNIHelper.h"
#include "JPAGImage.h"
#include "JPAGLayerHandle.h"

static jfieldID PAGImageLayer_nativeContext;

using namespace pag;

std::shared_ptr<PAGImageLayer> GetPAGImageLayer(JNIEnv* env, jobject thiz) {
  auto nativeContext =
      reinterpret_cast<JPAGLayerHandle*>(env->GetLongField(thiz, PAGImageLayer_nativeContext));
  if (nativeContext == nullptr) {
    return nullptr;
  }
  return std::static_pointer_cast<PAGImageLayer>(nativeContext->get());
}

extern "C" {

JNIEXPORT void Java_org_libpag_PAGImageLayer_nativeInit(JNIEnv* env, jclass clazz) {
  PAGImageLayer_nativeContext = env->GetFieldID(clazz, "nativeContext", "J");
}

JNIEXPORT jlong Java_org_libpag_PAGImageLayer_nativeMake(JNIEnv*, jclass, jint width, jint height,
                                                         jlong duration) {
  if (width <= 0 || height <= 0 || duration <= 0) {
    return 0;
  }
  auto pagImageLayer = PAGImageLayer::Make(width, height, duration);
  if (pagImageLayer == nullptr) {
    return 0;
  }
  return reinterpret_cast<jlong>(new JPAGLayerHandle(pagImageLayer));
}

JNIEXPORT jobjectArray Java_org_libpag_PAGImageLayer_getVideoRanges(JNIEnv* env, jobject thiz) {
  static Global<jclass> PAGVideoRange_Class(env, env->FindClass("org/libpag/PAGVideoRange"));
  auto pagLayer = GetPAGImageLayer(env, thiz);
  if (pagLayer == nullptr) {
    return env->NewObjectArray(0, PAGVideoRange_Class.get(), nullptr);
  }
  auto videoRanges = pagLayer->getVideoRanges();
  if (videoRanges.empty()) {
    return env->NewObjectArray(0, PAGVideoRange_Class.get(), nullptr);
  }
  int size = videoRanges.size();
  jobjectArray rangeArray = env->NewObjectArray(size, PAGVideoRange_Class.get(), nullptr);
  for (int i = 0; i < size; ++i) {
    Local<jobject> jRange = {env, ToPAGVideoRangeObject(env, videoRanges[i])};
    env->SetObjectArrayElement(rangeArray, i, jRange.get());
  }
  return rangeArray;
}

JNIEXPORT void Java_org_libpag_PAGImageLayer_replaceImage(JNIEnv* env, jobject thiz,
                                                          jlong imageObject) {
  auto pagLayer = GetPAGImageLayer(env, thiz);
  if (pagLayer == nullptr) {
    return;
  }
  auto image = reinterpret_cast<JPAGImage*>(imageObject);
  pagLayer->replaceImage(image == nullptr ? nullptr : image->get());
}

JNIEXPORT jlong Java_org_libpag_PAGImageLayer_contentDuration(JNIEnv* env, jobject thiz) {
  auto pagLayer = GetPAGImageLayer(env, thiz);
  if (pagLayer == nullptr) {
    return 0;
  }
  return pagLayer->contentDuration();
}
}