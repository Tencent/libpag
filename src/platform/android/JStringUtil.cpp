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
#include <cstring>

namespace pag {
/**
 * There is a known bug in env->NewStringUTF which will lead to crash in some devices.
 * So we use this method instead.
 */
jstring SafeConvertToJString(JNIEnv* env, const std::string& text) {
  static Global<jclass> StringClass = env->FindClass("java/lang/String");
  static jmethodID StringConstructID =
      env->GetMethodID(StringClass.get(), "<init>", "([BLjava/lang/String;)V");
  auto array = env->NewByteArray(text.size());
  env->SetByteArrayRegion(array, 0, text.size(), reinterpret_cast<const jbyte*>(text.data()));
  auto stringUTF = env->NewStringUTF("UTF-8");
  auto result = (jstring)env->NewObject(StringClass.get(), StringConstructID, array, stringUTF);
  env->DeleteLocalRef(array);
  env->DeleteLocalRef(stringUTF);
  return result;
}

std::string SafeConvertToStdString(JNIEnv* env, jstring jText) {
  if (jText == nullptr) {
    return "";
  }
  std::string result;
  static Global<jclass> StringClass = env->FindClass("java/lang/String");
  static jmethodID GetBytesID =
      env->GetMethodID(StringClass.get(), "getBytes", "(Ljava/lang/String;)[B");
  auto encoding = env->NewStringUTF("utf-8");
  auto jBytes = (jbyteArray)env->CallObjectMethod(jText, GetBytesID, encoding);
  env->DeleteLocalRef(encoding);
  auto textLength = env->GetArrayLength(jBytes);
  if (textLength > 0) {
    char* bytes = new char[textLength];
    env->GetByteArrayRegion(jBytes, 0, textLength, (jbyte*)bytes);
    result = std::string(bytes, (unsigned)textLength);
    delete[] bytes;
  }
  env->DeleteLocalRef(jBytes);
  return result;
}
}  // namespace pag
