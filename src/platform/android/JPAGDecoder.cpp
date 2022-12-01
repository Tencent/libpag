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

#include "JPAGDecoder.h"
#include "JNIHelper.h"

namespace pag {
static jfieldID PAGDecoder_nativeHandler;
}

void setPAGDecoder(JNIEnv* env, jobject thiz, JPAGDecoder* decoder) {
  auto old = reinterpret_cast<JPAGDecoder*>(env->GetLongField(thiz, pag::PAGDecoder_nativeHandler));
  if (old != nullptr) {
    delete old;
  }
  env->SetLongField(thiz, pag::PAGDecoder_nativeHandler, (jlong)decoder);
}

std::shared_ptr<pag::PAGDecoder> getPAGDecoder(JNIEnv* env, jobject thiz) {
  auto jPAGDecoder =
      reinterpret_cast<JPAGDecoder*>(env->GetLongField(thiz, pag::PAGDecoder_nativeHandler));
  if (jPAGDecoder == nullptr) {
    return nullptr;
  }
  return jPAGDecoder->get();
}

extern "C" {
JNIEXPORT jlong JNICALL Java_org_libpag_PAGDecoder_MakeNative(JNIEnv* env, jclass,
                                                              jobject pag_composition, jint width,
                                                              jint height) {
  auto decoder =
      pag::PAGDecoder::Make(ToPAGCompositionNativeObject(env, pag_composition), width, height);
  if (!decoder) {
    return 0;
  }
  return reinterpret_cast<jlong>(new JPAGDecoder(decoder));
}

JNIEXPORT jobject JNICALL Java_org_libpag_PAGDecoder_decode(JNIEnv* env, jobject thiz,
                                                            jdouble value) {
  auto decoder = getPAGDecoder(env, thiz);
  if (decoder) {
    return decoder->decode(value);
  }
  return nullptr;
}

JNIEXPORT void JNICALL Java_org_libpag_PAGDecoder_nativeFinalize(JNIEnv* env, jobject thiz) {
  auto jPAGDecoder =
      reinterpret_cast<JPAGDecoder*>(env->GetLongField(thiz, pag::PAGDecoder_nativeHandler));
  if (jPAGDecoder != nullptr) {
    jPAGDecoder->reset();
  }
}

JNIEXPORT void JNICALL Java_org_libpag_PAGDecoder_nativeInit(JNIEnv* env, jclass clazz) {
  pag::PAGDecoder_nativeHandler = env->GetFieldID(clazz, "nativeHandler", "J");
  pag::PAGDecoder::initJNI(env);
}
}