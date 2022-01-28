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

namespace pag {
static Global<jclass> TraceImageClass;
static jmethodID TraceImage_Trace;

void JTraceImage::InitJNI(JNIEnv* env) {
  TraceImageClass.reset(env, env->FindClass("org/libpag/TraceImage"));
  TraceImage_Trace = env->GetStaticMethodID(TraceImageClass.get(), "Trace",
                                            "(Ljava/lang/String;Ljava/nio/ByteBuffer;II)V");
}

void JTraceImage::Trace(const PixelMap& pixelMap, const std::string& tag) {
  auto env = JNIEnvironment::Current();
  if (env == nullptr || pixelMap.isEmpty()) {
    return;
  }

  auto rowBytes = static_cast<size_t>(pixelMap.width() * 4);
  auto pixels = new(std::nothrow) uint8_t[pixelMap.height() * rowBytes];
  if (pixels == nullptr) {
    return;
  }
  auto info = ImageInfo::Make(pixelMap.width(), pixelMap.height(), ColorType::RGBA_8888,
                              AlphaType::Premultiplied, rowBytes);
  pixelMap.readPixels(info, pixels);
  Local<jobject> byteBuffer =
      {env, MakeByteBufferObject(env, pixels, static_cast<size_t>(pixelMap.height() * rowBytes))};
  Local<jstring> tagString = {env, SafeConvertToJString(env, tag.c_str())};
  env->CallStaticVoidMethod(TraceImageClass.get(), TraceImage_Trace, tagString.get(),
                            byteBuffer.get(),
                            pixelMap.width(), pixelMap.height());
  delete[] pixels;
}
}  // namespace pag
