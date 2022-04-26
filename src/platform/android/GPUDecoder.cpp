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

#include "GPUDecoder.h"
#include "platform/android/JStringUtil.h"
#include "tgfx/core/Buffer.h"

namespace pag {
static Global<jclass> GPUDecoderClass;
static jmethodID GPUDecoder_ForceSoftwareDecoder;
static jmethodID GPUDecoder_Create;
static jmethodID GPUDecoder_onSendBytes;
static jmethodID GPUDecoder_onEndOfStream;
static jmethodID GPUDecoder_onDecodeFrame;
static jmethodID GPUDecoder_onFlush;
static jmethodID GPUDecoder_presentationTime;
static jmethodID GPUDecoder_onRenderFrame;
static jmethodID GPUDecoder_onRelease;

static Global<jclass> MediaFormatClass;
static jmethodID MediaFormat_createVideoFormat;
static jmethodID MediaFormat_setByteBuffer;
static jmethodID MediaFormat_setFloat;

void GPUDecoder::InitJNI(JNIEnv* env, const std::string& className) {
  GPUDecoderClass.reset(env, env->FindClass(className.c_str()));
  GPUDecoder_ForceSoftwareDecoder =
      env->GetStaticMethodID(GPUDecoderClass.get(), "ForceSoftwareDecoder", "()Z");
  std::string createSig =
      std::string("(Lorg/libpag/VideoSurface;Landroid/media/MediaFormat;)L") + className + ";";
  GPUDecoder_Create = env->GetStaticMethodID(GPUDecoderClass.get(), "Create", createSig.c_str());
  GPUDecoder_onSendBytes =
      env->GetMethodID(GPUDecoderClass.get(), "onSendBytes", "(Ljava/nio/ByteBuffer;J)I");
  GPUDecoder_onEndOfStream = env->GetMethodID(GPUDecoderClass.get(), "onEndOfStream", "()I");
  GPUDecoder_onDecodeFrame = env->GetMethodID(GPUDecoderClass.get(), "onDecodeFrame", "()I");
  GPUDecoder_onFlush = env->GetMethodID(GPUDecoderClass.get(), "onFlush", "()V");
  GPUDecoder_presentationTime = env->GetMethodID(GPUDecoderClass.get(), "presentationTime", "()J");
  GPUDecoder_onRenderFrame = env->GetMethodID(GPUDecoderClass.get(), "onRenderFrame", "()Z");
  GPUDecoder_onRelease = env->GetMethodID(GPUDecoderClass.get(), "onRelease", "()V");

  MediaFormatClass.reset(env, env->FindClass("android/media/MediaFormat"));
  MediaFormat_createVideoFormat =
      env->GetStaticMethodID(MediaFormatClass.get(), "createVideoFormat",
                             "(Ljava/lang/String;II)Landroid/media/MediaFormat;");
  MediaFormat_setByteBuffer = env->GetMethodID(MediaFormatClass.get(), "setByteBuffer",
                                               "(Ljava/lang/String;Ljava/nio/ByteBuffer;)V");
  MediaFormat_setFloat =
      env->GetMethodID(MediaFormatClass.get(), "setFloat", "(Ljava/lang/String;F)V");
}

GPUDecoder::GPUDecoder(const VideoFormat& format) {
  auto env = JNIEnvironment::Current();
  if (env == nullptr ||
      env->CallStaticBooleanMethod(GPUDecoderClass.get(), GPUDecoder_ForceSoftwareDecoder)) {
    return;
  }
  _isValid = initDecoder(env, format);
}

GPUDecoder::~GPUDecoder() {
  if (videoDecoder.get() != nullptr) {
    auto env = JNIEnvironment::Current();
    if (env == nullptr) {
      return;
    }
    env->CallVoidMethod(videoDecoder.get(), GPUDecoder_onRelease);
  }
}

bool GPUDecoder::initDecoder(JNIEnv* env, const VideoFormat& format) {
  videoSurface = VideoSurface::Make(format.width, format.height);
  if (videoSurface == nullptr) {
    return false;
  }
  videoWidth = format.width;
  videoHeight = format.height;
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
      env, env->CallStaticObjectMethod(GPUDecoderClass.get(), GPUDecoder_Create,
                                       videoSurface->getVideoSurface(), mediaFormat.get())};
  if (decoder.empty()) {
    return false;
  }
  videoDecoder.reset(env, decoder.get());
  return true;
}

DecodingResult GPUDecoder::onSendBytes(void* bytes, size_t length, int64_t time) {
  auto env = JNIEnvironment::Current();
  if (env == nullptr) {
    LOGE("GPUDecoder: Error on sending bytes for decoding.\n");
    return pag::DecodingResult::Error;
  }
  Local<jobject> byteBuffer = {env, env->NewDirectByteBuffer(bytes, length)};
  auto result =
      env->CallIntMethod(videoDecoder.get(), GPUDecoder_onSendBytes, byteBuffer.get(), time);
  return static_cast<pag::DecodingResult>(result);
}

DecodingResult GPUDecoder::onDecodeFrame() {
  auto env = JNIEnvironment::Current();
  if (env == nullptr) {
    LOGE("GPUDecoder: Error on decoding frame.\n");
    return pag::DecodingResult::Error;
  }
  return static_cast<pag::DecodingResult>(
      env->CallIntMethod(videoDecoder.get(), GPUDecoder_onDecodeFrame));
}

void GPUDecoder::onFlush() {
  auto env = JNIEnvironment::Current();
  if (env == nullptr) {
    return;
  }
  env->CallVoidMethod(videoDecoder.get(), GPUDecoder_onFlush);
}

int64_t GPUDecoder::presentationTime() {
  auto env = JNIEnvironment::Current();
  if (env == nullptr) {
    return -1;
  }
  return env->CallLongMethod(videoDecoder.get(), GPUDecoder_presentationTime);
}

DecodingResult GPUDecoder::onEndOfStream() {
  auto env = JNIEnvironment::Current();
  if (env == nullptr) {
    LOGE("GPUDecoder: Error on decoding frame.\n");
    return pag::DecodingResult::Error;
  }
  return static_cast<pag::DecodingResult>(
      env->CallIntMethod(videoDecoder.get(), GPUDecoder_onEndOfStream));
}

std::shared_ptr<VideoBuffer> GPUDecoder::onRenderFrame() {
  auto env = JNIEnvironment::Current();
  if (env == nullptr) {
    return nullptr;
  }
  videoSurface->clearPendingTexImage();
  auto result = env->CallBooleanMethod(videoDecoder.get(), GPUDecoder_onRenderFrame);
  if (!result) {
    return nullptr;
  }
  videoSurface->markPendingTexImage();
  return VideoImage::MakeFrom(videoSurface, videoWidth, videoHeight);
}
}  // namespace pag