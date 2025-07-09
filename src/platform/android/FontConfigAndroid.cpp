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

#include "FontConfigAndroid.h"
#include "JNIHelper.h"

namespace pag {
static Global<jclass> PAGFontClass;
static jmethodID PAGFont_RegisterFallbackFonts;

extern "C" {

void FontConfigAndroid::InitJNI(JNIEnv* env) {
  PAGFontClass = env->FindClass("org/libpag/PAGFont");
  if (PAGFontClass.get() == nullptr) {
    LOGE("Could not run PAGFont.RegisterFallbackFonts(), class is not found!");
    return;
  }
  PAGFont_RegisterFallbackFonts =
      env->GetStaticMethodID(PAGFontClass.get(), "RegisterFallbackFonts", "()V");
}

bool FontConfigAndroid::RegisterFallbackFonts() {
  JNIEnvironment environment;
  auto env = environment.current();
  if (env == nullptr) {
    return false;
  }
  if (PAGFontClass.get() == nullptr) {
    LOGE("PAGFontClass is null");
    return false;
  }
  env->CallStaticVoidMethod(PAGFontClass.get(), PAGFont_RegisterFallbackFonts);
  return true;
}
}
}  // namespace pag
