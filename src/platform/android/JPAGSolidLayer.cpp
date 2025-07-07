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
#include "JPAGLayerHandle.h"

static jfieldID PAGSolidLayer_nativeContext;

using namespace pag;

std::shared_ptr<PAGSolidLayer> GetPAGSolidLayer(JNIEnv* env, jobject thiz) {
  auto nativeContext =
      reinterpret_cast<JPAGLayerHandle*>(env->GetLongField(thiz, PAGSolidLayer_nativeContext));
  if (nativeContext == nullptr) {
    return nullptr;
  }
  return std::static_pointer_cast<PAGSolidLayer>(nativeContext->get());
}

extern "C" {

PAG_API void Java_org_libpag_PAGSolidLayer_nativeInit(JNIEnv* env, jclass clazz) {
  PAGSolidLayer_nativeContext = env->GetFieldID(clazz, "nativeContext", "J");
}

PAG_API jint Java_org_libpag_PAGSolidLayer_solidColor(JNIEnv* env, jobject thiz) {
  auto pagLayer = GetPAGSolidLayer(env, thiz);
  if (pagLayer == nullptr) {
    return 0;
  }
  auto color = pagLayer->solidColor();
  return MakeColorInt(env, color.red, color.green, color.blue);
}

PAG_API void Java_org_libpag_PAGSolidLayer_setSolidColor(JNIEnv* env, jobject thiz, jint color) {
  auto pagLayer = GetPAGSolidLayer(env, thiz);
  if (pagLayer == nullptr) {
    return;
  }
  pagLayer->setSolidColor(ToColor(env, color));
}
}
