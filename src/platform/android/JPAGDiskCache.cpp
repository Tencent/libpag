/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "JPAGDiskCache.h"

namespace pag {
static Global<jclass> PAGClass;
static jmethodID PAG_GetCacheDir;

void JPAGDiskCache::InitJNI(JNIEnv* env) {
  PAGClass = env->FindClass("org/libpag/PAGDiskCache");
  if (PAGClass.get() == nullptr) {
    LOGE("Could not run PAGDiskCache.InitJNI(), PAGClass is not found!");
    return;
  }
  PAG_GetCacheDir = env->GetStaticMethodID(PAGClass.get(), "GetCacheDir", "()Ljava/lang/String;");
}

std::string JPAGDiskCache::GetCacheDir() {
  JNIEnvironment environment;
  auto env = environment.current();
  if (env == nullptr) {
    return "";
  }
  if (PAGClass.get() == nullptr) {
    env->ExceptionClear();
    LOGE("Could not run PAGDiskCache.GetCacheDir(), PAGClass is not found!");
    return "";
  }
  jobject cacheDirPath = env->CallStaticObjectMethod(PAGClass.get(), PAG_GetCacheDir);
  return SafeConvertToStdString(env, reinterpret_cast<jstring>(cacheDirPath));
}
}  // namespace pag

extern "C" {
PAG_API jlong JNICALL Java_org_libpag_PAGDiskCache_MaxDiskSize(JNIEnv*, jclass) {
  return static_cast<jlong>(pag::PAGDiskCache::MaxDiskSize());
}

PAG_API void JNICALL Java_org_libpag_PAGDiskCache_SetMaxDiskSize(JNIEnv*, jclass, jlong size) {
  pag::PAGDiskCache::SetMaxDiskSize(size);
}
}
