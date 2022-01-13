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

#include "JStringUtil.h"
#include <string.h>

/**
 * There is a known bug in env->NewStringUTF which will lead to crash in some devices.
 * So we use this method instead.
 */
jstring SafeConvertToJString(JNIEnv* env, const char* text) {
  static Global<jclass> StringClass(env, env->FindClass("java/lang/String"));
  static jmethodID StringConstructID =
      env->GetMethodID(StringClass.get(), "<init>", "([BLjava/lang/String;)V");
  int length = strlen(text);
  jbyteArray array = env->NewByteArray(length);
  env->SetByteArrayRegion(array, 0, length, reinterpret_cast<const jbyte*>(text));
  jstring stringUTF = env->NewStringUTF("UTF-8");
  jstring object = (jstring)env->NewObject(StringClass.get(), StringConstructID, array, stringUTF);
  env->DeleteLocalRef(stringUTF);
  env->DeleteLocalRef(array);
  return object;
}

std::string SafeConvertToStdString(JNIEnv* env, jstring jText) {
  if (jText == NULL) {
    return "";
  }
  std::string result;
  static Global<jclass> StringClass(env, env->FindClass("java/lang/String"));
  static jmethodID GetBytesID =
      env->GetMethodID(StringClass.get(), "getBytes", "(Ljava/lang/String;)[B");
  jstring encoding = env->NewStringUTF("utf-8");

  jbyteArray jBytes = (jbyteArray)env->CallObjectMethod(jText, GetBytesID, encoding);
  env->DeleteLocalRef(encoding);

  jsize textLength = env->GetArrayLength(jBytes);
  if (textLength > 0) {
    char* bytes = new char[textLength];
    env->GetByteArrayRegion(jBytes, 0, textLength, (jbyte*)bytes);
    result = std::string(bytes, (unsigned)textLength);
    delete[] bytes;
  }
  env->DeleteLocalRef(jBytes);
  return result;
}