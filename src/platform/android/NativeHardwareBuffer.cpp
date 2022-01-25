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

#include "NativeHardwareBuffer.h"

#include <jni.h>
#include <string>

#include <android/hardware_buffer.h>
#include <dlfcn.h>
#include <unordered_set>

#include "GLHardwareTexture.h"
#include "NativeHardwareBufferInterface.h"

namespace pag {

std::shared_ptr<NativeHardwareBuffer> NativeHardwareBuffer::MakeAdopted(
    AHardwareBuffer* hardwareBuffer) {
    if (!hardwareBuffer || !NativeHardwareBufferInterface::Get()->AHardwareBuffer_allocate) {
        return nullptr;
    }
    return std::shared_ptr<NativeHardwareBuffer>(new NativeHardwareBuffer(hardwareBuffer));
}

std::shared_ptr<PixelBuffer> NativeHardwareBuffer::Make(int width, int height, bool alphaOnly) {
    auto interface = NativeHardwareBufferInterface::Get();
    if (alphaOnly || !interface->AHardwareBuffer_allocate) {
        return nullptr;
    }
    AHardwareBuffer* buffer = nullptr;
    AHardwareBuffer_Desc desc = {
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height),
        1,
        AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM,
        AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN | AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN |
        AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE | AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT,
        0,
        0,
        0
    };
    interface->AHardwareBuffer_allocate(&desc, &buffer);
    if (!buffer) {
        return nullptr;
    }
    auto hardwareBuffer = std::shared_ptr<NativeHardwareBuffer>(new NativeHardwareBuffer(buffer));
    interface->AHardwareBuffer_release(buffer);
    return hardwareBuffer;
}

static ImageInfo GetImageInfo(AHardwareBuffer* hardwareBuffer) {
    AHardwareBuffer_Desc desc;
    NativeHardwareBufferInterface::Get()->AHardwareBuffer_describe(hardwareBuffer, &desc);
    return ImageInfo::Make(desc.width, desc.height, ColorType::RGBA_8888, AlphaType::Premultiplied,
                           desc.stride * 4);
}

NativeHardwareBuffer::NativeHardwareBuffer(AHardwareBuffer* hardwareBuffer)
    : PixelBuffer(GetImageInfo(hardwareBuffer)), hardwareBuffer(hardwareBuffer) {
    NativeHardwareBufferInterface::Get()->AHardwareBuffer_acquire(hardwareBuffer);
}

std::shared_ptr<Texture> NativeHardwareBuffer::makeTexture(Context* context) const {
    return GLHardwareTexture::MakeFrom(context, hardwareBuffer);
}

void* NativeHardwareBuffer::lockPixels() {
    uint8_t* readPtr = nullptr;
    NativeHardwareBufferInterface::Get()->AHardwareBuffer_lock(
        hardwareBuffer, AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN | AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN,
        -1, nullptr, reinterpret_cast<void**>(&readPtr));
    return readPtr;
}

void NativeHardwareBuffer::unlockPixels() {
    NativeHardwareBufferInterface::Get()->AHardwareBuffer_unlock(hardwareBuffer, nullptr);
}

NativeHardwareBuffer::~NativeHardwareBuffer() {
    NativeHardwareBufferInterface::Get()->AHardwareBuffer_release(hardwareBuffer);
}
}  // namespace pag
