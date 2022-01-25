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

#pragma once
#include "NativeGraphicBufferInterface.h"
#include <android/hardware_buffer.h>
#include "gpu/opengl/GLTexture.h"
#include "EGL/egl.h"
#include "EGL/eglext.h"
#include "GLES/gl.h"
#include "GLES/glext.h"
#include "NativeHardwareBufferInterface.h"

namespace pag {
class GLHardwareTexture : public GLTexture {
public:
    static std::shared_ptr<GLHardwareTexture> MakeFrom(
        Context* context, AHardwareBuffer* hardwareBuffer);
    static std::shared_ptr<GLHardwareTexture> MakeFrom(
        Context* context, android::GraphicBuffer* graphicBuffer);

    static std::shared_ptr<GLHardwareTexture> MakeFrom(
        Context* context, AHardwareBuffer* hardwareBuffer,
        android::GraphicBuffer* graphicBuffer, EGLClientBuffer client_buffer, int width,
        int height);

    size_t memoryUsage() const override {
        return 0;
    }

private:
    explicit GLHardwareTexture(AHardwareBuffer* hardwareBuffer,
                               android::GraphicBuffer* graphicBuffer, EGLImageKHR eglImage,
                               int width, int height);
    EGLImageKHR _eglImage = EGL_NO_IMAGE_KHR;
    AHardwareBuffer* hardwareBuffer = nullptr;
    android::GraphicBuffer* graphicBuffer = nullptr;
    void onRelease(Context* context) override;
    static void ComputeRecycleKey(BytesKey* recycleKey, void* hardware_buffer);
};
}  // namespace pag
