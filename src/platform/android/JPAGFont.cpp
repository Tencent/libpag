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
#include "pag/pag.h"
#include "platform/android/FontConfigAndroid.h"

using namespace pag;

extern "C" {
PAG_API void JNICALL Java_org_libpag_PAGFont_UnregisterFont(JNIEnv* env, jclass,
                                                            jstring font_family,
                                                            jstring font_style) {
  PAGFont::UnregisterFont(
      {SafeConvertToStdString(env, font_family), SafeConvertToStdString(env, font_style)});
}

PAG_API void Java_org_libpag_PAGFont_SetFallbackFontPaths(JNIEnv* env, jclass,
                                                          jobjectArray fontNameList,
                                                          jintArray ttcIndices) {
  std::vector<std::string> fallbackList;
  std::vector<int> ttcList;
  auto length = env->GetArrayLength(fontNameList);
  auto ttcLength = env->GetArrayLength(ttcIndices);
  length = std::min(length, ttcLength);
  auto ttcData = env->GetIntArrayElements(ttcIndices, nullptr);
  for (int index = 0; index < length; index++) {
    auto fontNameObject = (jstring)env->GetObjectArrayElement(fontNameList, index);
    auto fontFamily = SafeConvertToStdString(env, fontNameObject);
    fallbackList.push_back(fontFamily);
    ttcList.push_back(ttcData[index]);
  }
  env->ReleaseIntArrayElements(ttcIndices, ttcData, 0);
  PAGFont::SetFallbackFontPaths(fallbackList, ttcList);
}

PAG_API jobject JNICALL
Java_org_libpag_PAGFont_RegisterFont__Ljava_lang_String_2ILjava_lang_String_2Ljava_lang_String_2(
    JNIEnv* env, jclass, jstring font_path, jint ttc_index, jstring font_family,
    jstring font_style) {
  auto fontPath = SafeConvertToStdString(env, font_path);
  auto font = PAGFont::RegisterFont(fontPath, ttc_index, SafeConvertToStdString(env, font_family),
                                    SafeConvertToStdString(env, font_style));
  if (font.fontFamily.empty()) {
    return nullptr;
  }
  return MakePAGFontObject(env, font.fontFamily, font.fontStyle);
}

PAG_API jobject JNICALL
Java_org_libpag_PAGFont_RegisterFont__Landroid_content_res_AssetManager_2Ljava_lang_String_2ILjava_lang_String_2Ljava_lang_String_2(
    JNIEnv* env, jclass, jobject manager, jstring file_name, jint ttc_index, jstring font_family,
    jstring font_style) {
  auto path = SafeConvertToStdString(env, file_name);
  auto byteData = ReadBytesFromAssets(env, manager, file_name);
  if (byteData == nullptr) {
    return nullptr;
  }
  auto font = PAGFont::RegisterFont(byteData->data(), byteData->length(), ttc_index,
                                    SafeConvertToStdString(env, font_family),
                                    SafeConvertToStdString(env, font_style));
  if (font.fontFamily.empty()) {
    return nullptr;
  }
  return MakePAGFontObject(env, font.fontFamily, font.fontStyle);
}

PAG_API jobject Java_org_libpag_PAGFont_RegisterFontBytes(JNIEnv* env, jclass, jbyteArray bytes,
                                                          jint length, jint ttcIndex,
                                                          jstring font_family, jstring font_style) {
  auto data = env->GetByteArrayElements(bytes, nullptr);
  auto font = PAGFont::RegisterFont(data, static_cast<size_t>(length), ttcIndex,
                                    SafeConvertToStdString(env, font_family),
                                    SafeConvertToStdString(env, font_style));
  env->ReleaseByteArrayElements(bytes, data, 0);
  if (font.fontFamily.empty()) {
    return nullptr;
  }
  return MakePAGFontObject(env, font.fontFamily, font.fontStyle);
}
}
