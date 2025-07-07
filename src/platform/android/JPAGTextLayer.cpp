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

static jfieldID PAGTextLayer_nativeContext;

using namespace pag;

std::shared_ptr<PAGTextLayer> GetPAGTextLayer(JNIEnv* env, jobject thiz) {
  auto nativeContext =
      reinterpret_cast<JPAGLayerHandle*>(env->GetLongField(thiz, PAGTextLayer_nativeContext));
  if (nativeContext == nullptr) {
    return nullptr;
  }
  return std::static_pointer_cast<PAGTextLayer>(nativeContext->get());
}

extern "C" {

PAG_API void Java_org_libpag_PAGTextLayer_nativeInit(JNIEnv* env, jclass clazz) {
  PAGTextLayer_nativeContext = env->GetFieldID(clazz, "nativeContext", "J");
}

PAG_API void Java_org_libpag_PAGTextLayer_setFont(JNIEnv* env, jobject thiz, jstring fontFamily,
                                                  jstring fontStyle) {
  auto pagLayer = GetPAGTextLayer(env, thiz);
  if (pagLayer == nullptr) {
    return;
  }
  pagLayer->setFont(
      PAGFont(SafeConvertToStdString(env, fontFamily), SafeConvertToStdString(env, fontStyle)));
}

PAG_API jobject Java_org_libpag_PAGTextLayer_font(JNIEnv* env, jobject thiz) {
  auto pagLayer = GetPAGTextLayer(env, thiz);
  if (pagLayer == nullptr) {
    return nullptr;
  }
  return MakePAGFontObject(env, pagLayer->font().fontFamily, pagLayer->font().fontStyle);
}

PAG_API jint Java_org_libpag_PAGTextLayer_fillColor(JNIEnv* env, jobject thiz) {
  auto pagLayer = GetPAGTextLayer(env, thiz);
  if (pagLayer == nullptr) {
    return 0;
  }
  auto color = pagLayer->fillColor();
  return MakeColorInt(env, color.red, color.green, color.blue);
}

PAG_API void Java_org_libpag_PAGTextLayer_setFillColor(JNIEnv* env, jobject thiz, jint color) {
  auto pagLayer = GetPAGTextLayer(env, thiz);
  if (pagLayer == nullptr) {
    return;
  }
  pagLayer->setFillColor(ToColor(env, color));
}

PAG_API jint Java_org_libpag_PAGTextLayer_strokeColor(JNIEnv* env, jobject thiz) {
  auto pagLayer = GetPAGTextLayer(env, thiz);
  if (pagLayer == nullptr) {
    return 0;
  }
  auto color = pagLayer->strokeColor();
  return MakeColorInt(env, color.red, color.green, color.blue);
}

PAG_API void Java_org_libpag_PAGTextLayer_setStrokeColor(JNIEnv* env, jobject thiz, jint color) {
  auto pagLayer = GetPAGTextLayer(env, thiz);
  if (pagLayer == nullptr) {
    return;
  }
  pagLayer->setStrokeColor(ToColor(env, color));
}

PAG_API void Java_org_libpag_PAGTextLayer_setText(JNIEnv* env, jobject thiz, jstring text) {
  auto pagLayer = GetPAGTextLayer(env, thiz);
  if (pagLayer == nullptr) {
    return;
  }
  pagLayer->setText(SafeConvertToStdString(env, text));
}

PAG_API void Java_org_libpag_PAGTextLayer_reset(JNIEnv* env, jobject thiz) {
  auto pagLayer = GetPAGTextLayer(env, thiz);
  if (pagLayer == nullptr) {
    return;
  }
  pagLayer->reset();
}

PAG_API jstring Java_org_libpag_PAGTextLayer_text(JNIEnv* env, jobject thiz) {
  auto pagLayer = GetPAGTextLayer(env, thiz);
  if (pagLayer == nullptr) {
    std::string empty = "";
    return SafeConvertToJString(env, empty);
  }
  return SafeConvertToJString(env, pagLayer->text());
}

PAG_API void Java_org_libpag_PAGTextLayer_setFontSize(JNIEnv* env, jobject thiz, jfloat fontSize) {
  auto pagLayer = GetPAGTextLayer(env, thiz);
  if (pagLayer == nullptr) {
    return;
  }
  pagLayer->setFontSize(fontSize);
}

PAG_API jfloat Java_org_libpag_PAGTextLayer_fontSize(JNIEnv* env, jobject thiz) {
  auto pagLayer = GetPAGTextLayer(env, thiz);
  if (pagLayer == nullptr) {
    return 0;
  }
  return pagLayer->fontSize();
}
}
