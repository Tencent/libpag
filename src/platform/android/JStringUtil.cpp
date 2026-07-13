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

#include "JStringUtil.h"

namespace pag {
jstring SafeConvertToJString(JNIEnv* env, const std::string& text) {
  return env->NewStringUTF(text.c_str());
}

std::string SafeConvertToStdString(JNIEnv* env, jstring jText) {
  if (jText == nullptr) {
    return "";
  }
  const char* chars = env->GetStringUTFChars(jText, nullptr);
  if (chars == nullptr) {
    return "";
  }
  std::string result(chars);
  env->ReleaseStringUTFChars(jText, chars);
  return result;
}
}  // namespace pag