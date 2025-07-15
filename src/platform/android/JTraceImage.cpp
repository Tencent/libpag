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

#include "JTraceImage.h"
#include "JNIHelper.h"
#include "tgfx/core/Pixmap.h"

namespace pag {
static Global<jclass> TraceImageClass;
static jmethodID TraceImage_Trace;

void JTraceImage::InitJNI(JNIEnv* env) {
  TraceImageClass = env->FindClass("org/libpag/TraceImage");
  if (TraceImageClass.get() == nullptr) {
    LOGE("Could not run TraceImage.InitJNI(), TraceImageClass is not found!");
    return;
  }
  TraceImage_Trace = env->GetStaticMethodID(TraceImageClass.get(), "Trace",
                                            "(Ljava/lang/String;Ljava/nio/ByteBuffer;II)V");
}

void JTraceImage::Trace(const tgfx::ImageInfo& info, const void* pixels, const std::string& tag) {
  JNIEnvironment environment;
  auto env = environment.current();
  if (env == nullptr || info.isEmpty() || pixels == nullptr) {
    return;
  }
  if (TraceImageClass.get() == nullptr) {
    LOGE("Could not run TraceImage.Trace(), TraceImageClass is not found!");
    return;
  }
  auto rowBytes = static_cast<size_t>(info.width() * 4);
  auto dstPixels = new (std::nothrow) uint8_t[info.height() * rowBytes];
  if (dstPixels == nullptr) {
    return;
  }
  auto dstInfo = tgfx::ImageInfo::Make(info.width(), info.height(), tgfx::ColorType::RGBA_8888,
                                       tgfx::AlphaType::Premultiplied, rowBytes);
  tgfx::Pixmap pixmap(dstInfo, dstPixels);
  pixmap.writePixels(info, pixels);
  auto byteBuffer = MakeByteBufferObject(env, dstPixels, dstInfo.byteSize());
  auto tagString = SafeConvertToJString(env, tag);
  env->CallStaticVoidMethod(TraceImageClass.get(), TraceImage_Trace, tagString, byteBuffer,
                            info.width(), info.height());
  env->DeleteLocalRef(byteBuffer);
  env->DeleteLocalRef(tagString);
  delete[] dstPixels;
}
}  // namespace pag
