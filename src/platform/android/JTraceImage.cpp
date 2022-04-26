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

#include "JTraceImage.h"
#include "JNIHelper.h"
#include "tgfx/core/Bitmap.h"

namespace pag {
static Global<jclass> TraceImageClass;
static jmethodID TraceImage_Trace;

void JTraceImage::InitJNI(JNIEnv* env) {
  TraceImageClass.reset(env, env->FindClass("org/libpag/TraceImage"));
  TraceImage_Trace = env->GetStaticMethodID(TraceImageClass.get(), "Trace",
                                            "(Ljava/lang/String;Ljava/nio/ByteBuffer;II)V");
}

void JTraceImage::Trace(const tgfx::ImageInfo& info, const void* pixels, const std::string& tag) {
  auto env = JNIEnvironment::Current();
  if (env == nullptr || info.isEmpty() || pixels == nullptr) {
    return;
  }

  auto rowBytes = static_cast<size_t>(info.width() * 4);
  auto dstPixels = new (std::nothrow) uint8_t[info.height() * rowBytes];
  if (dstPixels == nullptr) {
    return;
  }
  auto dstInfo = tgfx::ImageInfo::Make(info.width(), info.height(), tgfx::ColorType::RGBA_8888,
                                       tgfx::AlphaType::Premultiplied, rowBytes);
  tgfx::Bitmap bitmap(dstInfo, dstPixels);
  bitmap.writePixels(info, pixels);
  Local<jobject> byteBuffer = {env, MakeByteBufferObject(env, dstPixels, dstInfo.byteSize())};
  Local<jstring> tagString = {env, SafeConvertToJString(env, tag.c_str())};
  env->CallStaticVoidMethod(TraceImageClass.get(), TraceImage_Trace, tagString.get(),
                            byteBuffer.get(), info.width(), info.height());
  delete[] dstPixels;
}
}  // namespace pag
