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
#include "JPAGLayerHandle.h"

static jfieldID PAGShapeLayer_nativeContext;

using namespace pag;

std::shared_ptr<PAGShapeLayer> GetPAGShapeLayer(JNIEnv* env, jobject thiz) {
  auto nativeContext =
      reinterpret_cast<JPAGLayerHandle*>(env->GetLongField(thiz, PAGShapeLayer_nativeContext));
  if (nativeContext == nullptr) {
    return nullptr;
  }
  return std::static_pointer_cast<PAGShapeLayer>(nativeContext->get());
}

extern "C" {

PAG_API void Java_org_libpag_PAGShapeLayer_nativeInit(JNIEnv* env, jclass clazz) {
  PAGShapeLayer_nativeContext = env->GetFieldID(clazz, "nativeContext", "J");
}

PAG_API jint Java_org_libpag_PAGShapeLayer_getTintColor(JNIEnv* env, jobject thiz) {
  auto pagShapeLayer = GetPAGShapeLayer(env, thiz);
  if (pagShapeLayer == nullptr) {
      return 0;
  }
  auto color = pagShapeLayer->getTintColor();
  if (color == nullptr) {
    return 0;
  } else {
    return MakeColorInt(env, color->red, color->green, color->blue);
  }
}


PAG_API void Java_org_libpag_PAGShapeLayer_setTintColor(JNIEnv* env, jobject thiz, jint color) {
  auto pagShapeLayer = GetPAGShapeLayer(env, thiz);
  if (pagShapeLayer == nullptr) {
    return;
  }
  pag::Color pagColor = ToColor(env, color);
  pagShapeLayer->setTintColor(pagColor);
}

PAG_API void Java_org_libpag_PAGShapeLayer_clearTintColor(JNIEnv* env, jobject thiz) {
  auto pagShapeLayer = GetPAGShapeLayer(env, thiz);
  if (pagShapeLayer == nullptr) {
    return;
  }
  pagShapeLayer->clearTintColor();
}
}
