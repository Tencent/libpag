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

#pragma once

#include <chrono>
#include "base/utils/Log.h"
#include "pag/pag.h"
#include "platform/android/JStringUtil.h"
#include "tgfx/core/ImageInfo.h"
#include "tgfx/core/Rect.h"
#include "tgfx/platform/android/Global.h"

namespace pag {
template <class T>
using Global = tgfx::Global<T>;

using JNIEnvironment = tgfx::JNIEnvironment;

jobject MakeRectFObject(JNIEnv* env, float x, float y, float width, float height);

jint MakeColorInt(JNIEnv* env, uint32_t red, uint32_t green, uint32_t blue);

jobject MakePAGFontObject(JNIEnv* env, const std::string& familyName, const std::string& fontStyle);

jobject MakeByteBufferObject(JNIEnv* env, const void* bytes, size_t length);

pag::Color ToColor(JNIEnv* env, jint color);

tgfx::Rect ToRect(JNIEnv* env, jobject rect);

std::unique_ptr<pag::ByteData> ReadBytesFromAssets(JNIEnv* env, jobject managerObj,
                                                   jstring pathObj);

jobjectArray ToPAGLayerJavaObjectList(JNIEnv* env,
                                      const std::vector<std::shared_ptr<pag::PAGLayer>>& layers);

jobject ToPAGLayerJavaObject(JNIEnv* env, std::shared_ptr<pag::PAGLayer> pagLayer);

std::shared_ptr<pag::PAGLayer> ToPAGLayerNativeObject(JNIEnv* env, jobject jLayer);

std::shared_ptr<pag::PAGComposition> ToPAGCompositionNativeObject(JNIEnv* env,
                                                                  jobject jComposition);

jobject ToPAGMarkerObject(JNIEnv* env, const pag::Marker* marker);

jobject ToPAGVideoRangeObject(JNIEnv* env, const pag::PAGVideoRange& range);
}  // namespace pag
