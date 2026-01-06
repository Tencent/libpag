/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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
#include "rendering/caches/DiskCache.h"

namespace pag {
static Global<jclass> PAGClass;
static jmethodID PAG_GetDefaultCacheDir;

void JPAGDiskCache::InitJNI(JNIEnv* env) {
  PAGClass = env->FindClass("org/libpag/PAGDiskCache");
  if (PAGClass.get() == nullptr) {
    LOGE("Could not run PAGDiskCache.InitJNI(), PAGClass is not found!");
    return;
  }
  PAG_GetDefaultCacheDir =
      env->GetStaticMethodID(PAGClass.get(), "GetDefaultCacheDir", "()Ljava/lang/String;");
}

std::string JPAGDiskCache::GetCacheDir() {
  JNIEnvironment environment;
  auto env = environment.current();
  if (env == nullptr) {
    return "";
  }
  if (PAGClass.get() == nullptr) {
    LOGE("Could not run PAGDiskCache.GetCacheDir(), PAGClass is not found!");
    return "";
  }
  jobject cacheDirPath = env->CallStaticObjectMethod(PAGClass.get(), PAG_GetDefaultCacheDir);
  return SafeConvertToStdString(env, reinterpret_cast<jstring>(cacheDirPath));
}
}  // namespace pag

extern "C" {
PAG_API void JNICALL Java_org_libpag_PAGDiskCache_SetCacheDir(JNIEnv* env, jclass, jstring dir) {
  auto cacheDir = pag::SafeConvertToStdString(env, dir);
  pag::PAGDiskCache::SetCacheDir(cacheDir);
}

PAG_API jlong JNICALL Java_org_libpag_PAGDiskCache_MaxDiskSize(JNIEnv*, jclass) {
  return static_cast<jlong>(pag::PAGDiskCache::MaxDiskSize());
}

PAG_API void JNICALL Java_org_libpag_PAGDiskCache_SetMaxDiskSize(JNIEnv*, jclass, jlong size) {
  pag::PAGDiskCache::SetMaxDiskSize(size);
}

PAG_API void JNICALL Java_org_libpag_PAGDiskCache_RemoveAll(JNIEnv*, jclass) {
  pag::PAGDiskCache::RemoveAll();
}

PAG_API jbyteArray JNICALL Java_org_libpag_PAGDiskCache_ReadFile(JNIEnv* env, jclass, jstring key) {
  auto data = pag::DiskCache::ReadFile(pag::SafeConvertToStdString(env, key));
  if (data == nullptr) {
    return nullptr;
  }
  auto bytes = env->NewByteArray(data->size());
  env->SetByteArrayRegion(bytes, 0, data->size(), reinterpret_cast<const jbyte*>(data->data()));
  return bytes;
}

PAG_API jboolean JNICALL Java_org_libpag_PAGDiskCache_WriteFile(JNIEnv* env, jclass, jstring jkey,
                                                                jbyteArray bytes) {
  auto key = pag::SafeConvertToStdString(env, jkey);
  if (bytes == nullptr || key.empty()) {
    LOGE("PAGDiskCache.WriteFile() Invalid file bytes specified.");
    return JNI_FALSE;
  }
  auto data = env->GetByteArrayElements(bytes, nullptr);
  auto length = env->GetArrayLength(bytes);
  auto byteData = tgfx::Data::MakeWithoutCopy(data, length);
  auto result = pag::DiskCache::WriteFile(key, byteData);
  env->ReleaseByteArrayElements(bytes, data, JNI_ABORT);
  return result;
}
}
