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

#include "platform/ReportData.h"
#include <jni.h>
#include <platform/android/Global.h>
#include <platform/android/JNIEnvironment.h>
#include "JNIHelper.h"

namespace pag {
jobject ToHashMapObject(JNIEnv* env, std::unordered_map<std::string, std::string> map) {
  if (env == nullptr) {
    return nullptr;
  }
  static Global<jclass> HashMap_Class(env, env->FindClass("java/util/HashMap"));
  static auto HashMap_Construct = env->GetMethodID(HashMap_Class.get(), "<init>", "()V");
  static jmethodID HashMap_put = env->GetMethodID(
      HashMap_Class.get(), "put", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
  auto jMap = env->NewObject(HashMap_Class.get(), HashMap_Construct, "");
  if (map.size() > 0) {
    for (auto& item : map) {
      env->CallObjectMethod(jMap, HashMap_put, SafeConvertToJString(env, item.first.c_str()),
                            SafeConvertToJString(env, item.second.c_str()));
    }
  }
  return jMap;
}

void OnReportData(std::unordered_map<std::string, std::string>& reportMap) {
  auto env = JNIEnvironment::Current();
  if (env == nullptr) {
    return;
  }
  // PAGReporter-OnReportData
  static Global<jclass> PAGReporter_Class(env, env->FindClass("org/libpag/reporter/PAGReporter"));
  if (PAGReporter_Class.get() == nullptr) {
    return;
  }
  static jmethodID PAGReporter_onReportData =
      env->GetStaticMethodID(PAGReporter_Class.get(), "OnReportData", "(Ljava/util/HashMap;)V");

  env->CallStaticVoidMethod(PAGReporter_Class.get(), PAGReporter_onReportData,
                            ToHashMapObject(env, reportMap));
}

}  // namespace pag