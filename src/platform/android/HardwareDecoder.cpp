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

#include "HardwareDecoder.h"
#include "base/utils/Log.h"
#include "platform/android/JStringUtil.h"
#include "tgfx/utils/Buffer.h"
#include "tgfx/utils/Task.h"

namespace pag {
static Global<jclass> HardwareDecoderClass;
static jmethodID HardwareDecoder_Create;
static jmethodID HardwareDecoder_onSendBytes;
static jmethodID HardwareDecoder_onEndOfStream;
static jmethodID HardwareDecoder_onDecodeFrame;
static jmethodID HardwareDecoder_onFlush;
static jmethodID HardwareDecoder_presentationTime;
static jmethodID HardwareDecoder_onRenderFrame;
static jmethodID HardwareDecoder_getVideoSurface;
static jmethodID HardwareDecoder_onRelease;

static Global<jclass> MediaFormatClass;
static jmethodID MediaFormat_createVideoFormat;
static jmethodID MediaFormat_setByteBuffer;
static jmethodID MediaFormat_setFloat;

void HardwareDecoder::InitJNI(JNIEnv* env) {
  HardwareDecoderClass = env->FindClass("org/libpag/HardwareDecoder");
  if (HardwareDecoderClass.get() == nullptr) {
    LOGE("Could not run HardwareDecoder.InitJNI(), HardwareDecoderClass is not found!");
    return;
  }
  HardwareDecoder_Create =
      env->GetStaticMethodID(HardwareDecoderClass.get(), "Create",
                             "(Landroid/media/MediaFormat;)Lorg/libpag/HardwareDecoder;");
  HardwareDecoder_onSendBytes =
      env->GetMethodID(HardwareDecoderClass.get(), "onSendBytes", "(Ljava/nio/ByteBuffer;J)I");
  HardwareDecoder_onEndOfStream =
      env->GetMethodID(HardwareDecoderClass.get(), "onEndOfStream", "()I");
  HardwareDecoder_onDecodeFrame =
      env->GetMethodID(HardwareDecoderClass.get(), "onDecodeFrame", "()I");
  HardwareDecoder_onFlush = env->GetMethodID(HardwareDecoderClass.get(), "onFlush", "()V");
  HardwareDecoder_presentationTime =
      env->GetMethodID(HardwareDecoderClass.get(), "presentationTime", "()J");
  HardwareDecoder_onRenderFrame =
      env->GetMethodID(HardwareDecoderClass.get(), "onRenderFrame", "()Z");
  HardwareDecoder_getVideoSurface = env->GetMethodID(HardwareDecoderClass.get(), "getVideoSurface",
                                                     "()Lorg/libpag/VideoSurface;");
  HardwareDecoder_onRelease = env->GetMethodID(HardwareDecoderClass.get(), "onRelease", "()V");

  MediaFormatClass = env->FindClass("android/media/MediaFormat");
  MediaFormat_createVideoFormat =
      env->GetStaticMethodID(MediaFormatClass.get(), "createVideoFormat",
                             "(Ljava/lang/String;II)Landroid/media/MediaFormat;");
  MediaFormat_setByteBuffer = env->GetMethodID(MediaFormatClass.get(), "setByteBuffer",
                                               "(Ljava/lang/String;Ljava/nio/ByteBuffer;)V");
  MediaFormat_setFloat =
      env->GetMethodID(MediaFormatClass.get(), "setFloat", "(Ljava/lang/String;F)V");
}

static void ReleaseDecoderAsync(jobject decoder) {
  tgfx::Task::Run([decoder]() {
    JNIEnvironment environment;
    auto env = environment.current();
    if (env == nullptr) {
      return;
    }
    // It may block the main thread, so we run it in an async task.
    env->CallVoidMethod(decoder, HardwareDecoder_onRelease);
    env->DeleteGlobalRef(decoder);
  });
}

HardwareDecoder::HardwareDecoder(const VideoFormat& format) {
  JNIEnvironment environment;
  auto env = environment.current();
  if (env == nullptr) {
    return;
  }
  isValid = initDecoder(env, format);
}

HardwareDecoder::~HardwareDecoder() {
  if (videoDecoder != nullptr) {
    ReleaseDecoderAsync(videoDecoder);
    videoDecoder = nullptr;
  }
}

bool HardwareDecoder::initDecoder(JNIEnv* env, const VideoFormat& format) {
  if (HardwareDecoderClass.get() == nullptr) {
    LOGE("Could not run HardwareDecoder.initDecoder(), HardwareDecoderClass is not found!");
    return false;
  }
  auto mimeType = SafeConvertToJString(env, format.mimeType.c_str());
  auto mediaFormat = env->CallStaticObjectMethod(
      MediaFormatClass.get(), MediaFormat_createVideoFormat, mimeType, format.width, format.height);
  if (format.mimeType == "video/hevc") {
    if (!format.headers.empty()) {
      char keyString[] = "csd-0";
      auto key = SafeConvertToJString(env, keyString);
      int dataLength = 0;
      for (auto& header : format.headers) {
        dataLength += header->size();
      }
      tgfx::Buffer buffer(dataLength);
      int pos = 0;
      for (auto& header : format.headers) {
        buffer.writeRange(pos, header->size(), header->data());
        pos += header->size();
      }
      auto bytes = env->NewDirectByteBuffer(buffer.data(), buffer.size());
      env->CallVoidMethod(mediaFormat, MediaFormat_setByteBuffer, key, bytes);
    }
  } else {
    int index = 0;
    for (auto& header : format.headers) {
      char keyString[6];
      snprintf(keyString, 6, "csd-%d", index);
      auto key = SafeConvertToJString(env, keyString);
      auto bytes = env->NewDirectByteBuffer(const_cast<uint8_t*>(header->bytes()), header->size());
      env->CallVoidMethod(mediaFormat, MediaFormat_setByteBuffer, key, bytes);
      index++;
    }
  }
  char frameRateKeyString[] = "frame-rate";
  auto frameRateKey = SafeConvertToJString(env, frameRateKeyString);
  env->CallVoidMethod(mediaFormat, MediaFormat_setFloat, frameRateKey, format.frameRate);
  auto decoder =
      env->CallStaticObjectMethod(HardwareDecoderClass.get(), HardwareDecoder_Create, mediaFormat);
  if (decoder == nullptr) {
    return false;
  }
  videoDecoder = env->NewGlobalRef(decoder);
  auto videoSurface = env->CallObjectMethod(decoder, HardwareDecoder_getVideoSurface);
  imageReader = JVideoSurface::GetImageReader(env, videoSurface);
  if (imageReader == nullptr) {
    ReleaseDecoderAsync(videoDecoder);
    videoDecoder = nullptr;
    return false;
  }
  return true;
}

DecodingResult HardwareDecoder::onSendBytes(void* bytes, size_t length, int64_t time) {
  JNIEnvironment environment;
  auto env = environment.current();
  if (env == nullptr) {
    LOGE("HardwareDecoder: Error on sending bytes for decoding.\n");
    return pag::DecodingResult::Error;
  }
  auto byteBuffer = env->NewDirectByteBuffer(bytes, length);
  auto result = env->CallIntMethod(videoDecoder, HardwareDecoder_onSendBytes, byteBuffer, time);
  return static_cast<pag::DecodingResult>(result);
}

DecodingResult HardwareDecoder::onDecodeFrame() {
  JNIEnvironment environment;
  auto env = environment.current();
  if (env == nullptr) {
    LOGE("HardwareDecoder: Error on decoding frame.\n");
    return pag::DecodingResult::Error;
  }
  return static_cast<pag::DecodingResult>(
      env->CallIntMethod(videoDecoder, HardwareDecoder_onDecodeFrame));
}

void HardwareDecoder::onFlush() {
  JNIEnvironment environment;
  auto env = environment.current();
  if (env == nullptr) {
    return;
  }
  env->CallVoidMethod(videoDecoder, HardwareDecoder_onFlush);
}

int64_t HardwareDecoder::presentationTime() {
  JNIEnvironment environment;
  auto env = environment.current();
  if (env == nullptr) {
    return -1;
  }
  return env->CallLongMethod(videoDecoder, HardwareDecoder_presentationTime);
}

DecodingResult HardwareDecoder::onEndOfStream() {
  JNIEnvironment environment;
  auto env = environment.current();
  if (env == nullptr) {
    LOGE("HardwareDecoder: Error on decoding frame.\n");
    return pag::DecodingResult::Error;
  }
  return static_cast<pag::DecodingResult>(
      env->CallIntMethod(videoDecoder, HardwareDecoder_onEndOfStream));
}

std::shared_ptr<tgfx::ImageBuffer> HardwareDecoder::onRenderFrame() {
  JNIEnvironment environment;
  auto env = environment.current();
  if (env == nullptr) {
    return nullptr;
  }
  auto result = env->CallBooleanMethod(videoDecoder, HardwareDecoder_onRenderFrame);
  if (!result) {
    return nullptr;
  }
  return imageReader->acquireNextBuffer();
}
}  // namespace pag