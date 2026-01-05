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

#include <android/bitmap.h>
#include <jni.h>
#include "JNIHelper.h"
#include "rendering/layers/ContentVersion.h"

extern "C" {

PAG_API jint Java_org_libpag_PAGImageView_ContentVersion(JNIEnv* env, jclass,
                                                         jobject jPagComposition) {
  auto composition = pag::ToPAGCompositionNativeObject(env, jPagComposition);
  if (composition == nullptr) {
    return 0;
  }
  return pag::ContentVersion::Get(composition);
}
}
