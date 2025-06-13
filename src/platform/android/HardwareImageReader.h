/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "tgfx/platform/android/SurfaceTextureReader.h"
#include "rendering/video/VideoFormat.h"
#include "android/native_window.h"
#include "android/native_window_jni.h"
#include "media/NdkMediaCodec.h"
#include "media/NdkMediaFormat.h"
#include "media/NdkImage.h"
#include "media/NdkImageReader.h"
#include "tgfx/core/ImageReader.h"

namespace pag {
    class HardwareImageReader {
    public:
        ~HardwareImageReader();

//        HardwareImageReader(const VideoFormat& format);

        void notifyFrameAvailable();

        std::shared_ptr<tgfx::ImageBuffer> acquireNextBuffer();

        bool makeFrom(const VideoFormat& format);

        ANativeWindow* getANativeWindow() const{
            return aNativeWindow;
        }

        static void onImageAvailableStatic(void* context, AImageReader*);

    private:
        std::mutex locker = {};
        std::condition_variable condition = {};
        VideoFormat videoFormat{};
        ANativeWindow* aNativeWindow = nullptr;
        AImageReader* aImageReader = nullptr;
        AImage* aImage = nullptr;
        std::shared_ptr<tgfx::SurfaceTextureReader> imageReader = nullptr;
        bool frameAvailable = false;
    };
}  // namespace pag
