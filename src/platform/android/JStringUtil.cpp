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
  Local<jbyteArray> array = {env, env->NewByteArray(length)};
  env->SetByteArrayRegion(array.get(), 0, length, reinterpret_cast<const jbyte*>(text));
  Local<jstring> stringUTF = {env, env->NewStringUTF("UTF-8")};
  return (jstring)env->NewObject(StringClass.get(), StringConstructID, array.get(),
                                 stringUTF.get());
}

std::string SafeConvertToStdString(JNIEnv* env, jstring jText) {
  if (jText == NULL) {
    return "";
  }
  std::string result;
  static Global<jclass> StringClass(env, env->FindClass("java/lang/String"));
  static jmethodID GetBytesID =
      env->GetMethodID(StringClass.get(), "getBytes", "(Ljava/lang/String;)[B");
  Local<jstring> encoding = {env, env->NewStringUTF("utf-8")};
  Local<jbyteArray> jBytes = {env,
                              (jbyteArray)env->CallObjectMethod(jText, GetBytesID, encoding.get())};
  jsize textLength = env->GetArrayLength(jBytes.get());
  if (textLength > 0) {
    char* bytes = new char[textLength];
    env->GetByteArrayRegion(jBytes.get(), 0, textLength, (jbyte*)bytes);
    result = std::string(bytes, (unsigned)textLength);
    delete[] bytes;
  }
  return result;
}