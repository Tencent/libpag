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
#include "platform/android/JStringUtil.h"
#include "tgfx/core/Buffer.h"

namespace pag {
static Global<jclass> HardwareDecoderClass;
static jmethodID HardwareDecoder_ForceSoftwareDecoder;
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

void HardwareDecoder::InitJNI(JNIEnv* env, const std::string& className) {
  HardwareDecoderClass.reset(env, env->FindClass(className.c_str()));
  HardwareDecoder_ForceSoftwareDecoder =
      env->GetStaticMethodID(HardwareDecoderClass.get(), "ForceSoftwareDecoder", "()Z");
  std::string createSig = std::string("(Landroid/media/MediaFormat;)L") + className + ";";
  HardwareDecoder_Create =
      env->GetStaticMethodID(HardwareDecoderClass.get(), "Create", createSig.c_str());
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

  MediaFormatClass.reset(env, env->FindClass("android/media/MediaFormat"));
  MediaFormat_createVideoFormat =
      env->GetStaticMethodID(MediaFormatClass.get(), "createVideoFormat",
                             "(Ljava/lang/String;II)Landroid/media/MediaFormat;");
  MediaFormat_setByteBuffer = env->GetMethodID(MediaFormatClass.get(), "setByteBuffer",
                                               "(Ljava/lang/String;Ljava/nio/ByteBuffer;)V");
  MediaFormat_setFloat =
      env->GetMethodID(MediaFormatClass.get(), "setFloat", "(Ljava/lang/String;F)V");
}

HardwareDecoder::HardwareDecoder(const VideoFormat& format) {
  auto env = JNIEnvironment::Current();
  if (env == nullptr || env->CallStaticBooleanMethod(HardwareDecoderClass.get(),
                                                     HardwareDecoder_ForceSoftwareDecoder)) {
    return;
  }
  isValid = initDecoder(env, format);
}

HardwareDecoder::~HardwareDecoder() {
  if (videoDecoder.get() != nullptr) {
    auto env = JNIEnvironment::Current();
    if (env == nullptr) {
      return;
    }
    env->CallVoidMethod(videoDecoder.get(), HardwareDecoder_onRelease);
  }
}

bool HardwareDecoder::initDecoder(JNIEnv* env, const VideoFormat& format) {
  Local<jstring> mimeType = {env, SafeConvertToJString(env, format.mimeType.c_str())};
  Local<jobject> mediaFormat = {
      env, env->CallStaticObjectMethod(MediaFormatClass.get(), MediaFormat_createVideoFormat,
                                       mimeType.get(), format.width, format.height)};
  if (format.mimeType == "video/hevc") {
    if (!format.headers.empty()) {
      char keyString[] = "csd-0";
      Local<jstring> key = {env, SafeConvertToJString(env, keyString)};
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
      Local<jobject> bytes = {env, env->NewDirectByteBuffer(buffer.data(), buffer.size())};
      env->CallVoidMethod(mediaFormat.get(), MediaFormat_setByteBuffer, key.get(), bytes.get());
    }
  } else {
    int index = 0;
    for (auto& header : format.headers) {
      char keyString[6];
      snprintf(keyString, 6, "csd-%d", index);
      Local<jstring> key = {env, SafeConvertToJString(env, keyString)};
      Local<jobject> bytes = {
          env, env->NewDirectByteBuffer(const_cast<uint8_t*>(header->bytes()), header->size())};
      env->CallVoidMethod(mediaFormat.get(), MediaFormat_setByteBuffer, key.get(), bytes.get());
      index++;
    }
  }
  char frameRateKeyString[] = "frame-rate";
  Local<jstring> frameRateKey = {env, SafeConvertToJString(env, frameRateKeyString)};
  env->CallVoidMethod(mediaFormat.get(), MediaFormat_setFloat, frameRateKey.get(),
                      format.frameRate);
  Local<jobject> decoder = {
      env, env->CallStaticObjectMethod(HardwareDecoderClass.get(), HardwareDecoder_Create,
                                       mediaFormat.get())};
  if (decoder.empty()) {
    return false;
  }
  auto videoSurface = env->CallObjectMethod(decoder.get(), HardwareDecoder_getVideoSurface);
  imageReader = JVideoSurface::GetImageReader(env, videoSurface);
  if (imageReader == nullptr) {
    env->CallVoidMethod(decoder.get(), HardwareDecoder_onRelease);
    return false;
  }
  videoDecoder.reset(env, decoder.get());
  return true;
}

DecodingResult HardwareDecoder::onSendBytes(void* bytes, size_t length, int64_t time) {
  auto env = JNIEnvironment::Current();
  if (env == nullptr) {
    LOGE("HardwareDecoder: Error on sending bytes for decoding.\n");
    return pag::DecodingResult::Error;
  }
  Local<jobject> byteBuffer = {env, env->NewDirectByteBuffer(bytes, length)};
  auto result =
      env->CallIntMethod(videoDecoder.get(), HardwareDecoder_onSendBytes, byteBuffer.get(), time);
  return static_cast<pag::DecodingResult>(result);
}

DecodingResult HardwareDecoder::onDecodeFrame() {
  auto env = JNIEnvironment::Current();
  if (env == nullptr) {
    LOGE("HardwareDecoder: Error on decoding frame.\n");
    return pag::DecodingResult::Error;
  }
  return static_cast<pag::DecodingResult>(
      env->CallIntMethod(videoDecoder.get(), HardwareDecoder_onDecodeFrame));
}

void HardwareDecoder::onFlush() {
  auto env = JNIEnvironment::Current();
  if (env == nullptr) {
    return;
  }
  env->CallVoidMethod(videoDecoder.get(), HardwareDecoder_onFlush);
}

int64_t HardwareDecoder::presentationTime() {
  auto env = JNIEnvironment::Current();
  if (env == nullptr) {
    return -1;
  }
  return env->CallLongMethod(videoDecoder.get(), HardwareDecoder_presentationTime);
}

DecodingResult HardwareDecoder::onEndOfStream() {
  auto env = JNIEnvironment::Current();
  if (env == nullptr) {
    LOGE("HardwareDecoder: Error on decoding frame.\n");
    return pag::DecodingResult::Error;
  }
  return static_cast<pag::DecodingResult>(
      env->CallIntMethod(videoDecoder.get(), HardwareDecoder_onEndOfStream));
}

std::shared_ptr<tgfx::ImageBuffer> HardwareDecoder::onRenderFrame() {
  auto env = JNIEnvironment::Current();
  if (env == nullptr) {
    return nullptr;
  }
  auto result = env->CallBooleanMethod(videoDecoder.get(), HardwareDecoder_onRenderFrame);
  if (!result) {
    return nullptr;
  }
  return imageReader->acquireNextBuffer();
}
}  // namespace pag