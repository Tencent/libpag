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

namespace pag {
static Global<jclass> GPUDecoderClass;
static jmethodID GPUDecoder_Create;
static jmethodID GPUDecoder_onConfigure;
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
    std::string createSig = std::string("(Landroid/view/Surface;)L") + className + ";";
    GPUDecoder_Create = env->GetStaticMethodID(GPUDecoderClass.get(), "Create", createSig.c_str());
    GPUDecoder_onConfigure =
        env->GetMethodID(GPUDecoderClass.get(), "onConfigure", "(Landroid/media/MediaFormat;)Z");
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
    MediaFormat_setFloat = env->GetMethodID(MediaFormatClass.get(), "setFloat",
                                            "(Ljava/lang/String;F)V");
}

GPUDecoder::GPUDecoder(const VideoConfig& config) {
    auto env = JNIEnvironment::Current();
    if (env == nullptr) {
        return;
    }
    videoSurface = VideoSurface::Make(config.width, config.height, config.hasAlpha);
    if (videoSurface == nullptr) {
        return;
    }
    jobject outputSurface = videoSurface->getOutputSurface(env);
    jobject decoder =
        env->CallStaticObjectMethod(GPUDecoderClass.get(), GPUDecoder_Create, outputSurface);
    env->DeleteLocalRef(outputSurface);
    if (decoder == nullptr) {
        _isValid = false;
        return;
    }
    videoDecoder.reset(env, decoder);
    _isValid = onConfigure(decoder, config);
    env->DeleteLocalRef(decoder);
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

bool GPUDecoder::onConfigure(jobject decoder, const VideoConfig& config) {
    videoWidth = config.width;
    videoHeight = config.height;
    auto env = JNIEnvironment::Current();
    if (env == nullptr) {
        return false;
    }
    jstring mimeType = SafeConvertToJString(env, config.mimeType.c_str());
    auto mediaFormat = env->CallStaticObjectMethod(
                           MediaFormatClass.get(), MediaFormat_createVideoFormat, mimeType, config.width, config.height);
    env->DeleteLocalRef(mimeType);
    if (config.mimeType == "video/hevc") {
        if (!config.headers.empty()) {
            char keyString[] = "csd-0";
            auto key = SafeConvertToJString(env, keyString);
            int dataLength = 0;
            for (auto& header : config.headers) {
                dataLength += header->length();
            }
            auto data = new uint8_t[dataLength];
            int index = 0;
            for (auto& header : config.headers) {
                memcpy(data + index, header->data(), header->length());
                index += header->length();
            }
            auto bytes = env->NewDirectByteBuffer(data, dataLength);
            env->CallVoidMethod(mediaFormat, MediaFormat_setByteBuffer, key, bytes);
            env->DeleteLocalRef(key);
            env->DeleteLocalRef(bytes);
            delete[] data;
        }
    } else {
        int index = 0;
        for (auto& header : config.headers) {
            char keyString[6];
            snprintf(keyString, 6, "csd-%d", index);
            auto key = SafeConvertToJString(env, keyString);
            auto bytes = env->NewDirectByteBuffer(header->data(), header->length());
            env->CallVoidMethod(mediaFormat, MediaFormat_setByteBuffer, key, bytes);
            env->DeleteLocalRef(key);
            env->DeleteLocalRef(bytes);
            index++;
        }
    }
    char frameRateKeyString[] = "frame-rate";
    auto frameRateKey = SafeConvertToJString(env, frameRateKeyString);
    env->CallVoidMethod(mediaFormat, MediaFormat_setFloat, frameRateKey, config.frameRate);
    env->DeleteLocalRef(frameRateKey);
    auto result = env->CallBooleanMethod(decoder, GPUDecoder_onConfigure, mediaFormat);
    env->DeleteLocalRef(mediaFormat);

    return result;
}

DecodingResult GPUDecoder::onSendBytes(void* bytes, size_t length, int64_t time) {
    auto env = JNIEnvironment::Current();
    if (env == nullptr) {
        LOGE("GPUDecoder: Error on sending bytes for decoding.\n");
        return pag::DecodingResult::Error;
    }
    auto byteBuffer = env->NewDirectByteBuffer(bytes, length);
    auto result = env->CallIntMethod(videoDecoder.get(), GPUDecoder_onSendBytes, byteBuffer, time);
    env->DeleteLocalRef(byteBuffer);
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
    videoSurface->updateTexImage();
    auto result = env->CallBooleanMethod(videoDecoder.get(), GPUDecoder_onRenderFrame);
    if (!result) {
        return nullptr;
    }
    return VideoImage::MakeFrom(videoSurface, videoWidth, videoHeight);
}
}  // namespace pag