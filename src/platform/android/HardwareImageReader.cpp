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

#include "HardwareImageReader.h"
#include "JNIHelper.h"
#include "JVideoSurface.h"
#include "base/utils/Log.h"

namespace pag {

#if __ANDROID_API__ >= 26
static constexpr int MAX_IMAGES = 3;
#endif

HardwareImageReader::~HardwareImageReader() {
#if __ANDROID_API__ >= 26
  if (aImageReader != nullptr) {
    AImageReader_delete(aImageReader);
    aImageReader = nullptr;
  }
  if (aImage != nullptr) {
    AImage_delete(aImage);
    aImage = nullptr;
  }
#else
  if (aNativeWindow != nullptr) {
    ANativeWindow_release(aNativeWindow);
    aNativeWindow = nullptr;
  }
#endif
}

bool HardwareImageReader::makeFrom(const VideoFormat& format) {
#if __ANDROID_API__ >= 26
  media_status_t status;
  status =
      AImageReader_newWithUsage(format.width, format.height, AIMAGE_FORMAT_YUV_420_888,
                                AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN, MAX_IMAGES, &aImageReader);
  if (status != AMEDIA_OK) {
    LOGE("HardwareImageReader: Error on creating AImageReader.\n");
    AImageReader_delete(aImageReader);
    aImageReader = nullptr;
    return false;
  }
  status = AImageReader_getWindow(aImageReader, &aNativeWindow);
  if (status != AMEDIA_OK) {
    LOGE("HardwareImageReader: Error on getting ANativeWindow.\n");
    AImageReader_delete(aImageReader);
    aImageReader = nullptr;
    return false;
  }

  AImageReader_ImageListener listener = {};
  listener.context = this;
  listener.onImageAvailable = &HardwareImageReader::OnImageAvailableStatic;
  status = AImageReader_setImageListener(aImageReader, &listener);
  if (status != AMEDIA_OK) {
    LOGE("HardwareImageReader: Error setting image listener.\n");
    AImageReader_delete(aImageReader);
    aImageReader = nullptr;
    return false;
  }
#else
  JNIEnvironment environment;
  auto env = environment.current();
  if (env == nullptr) {
    return false;
  }
  auto videoSurface = JVideoSurface::Make(env, format.width, format.height);
  if (videoSurface == nullptr) {
    return false;
  }
  imageReader = JVideoSurface::GetImageReader(env, videoSurface);
  if (imageReader == nullptr) {
    return false;
  }
  auto surface = imageReader->getInputSurface();
  if (surface == nullptr) {
    return false;
  }
  aNativeWindow = ANativeWindow_fromSurface(env, surface);
#endif
  return true;
}

void HardwareImageReader::notifyFrameAvailable() {
  std::lock_guard<std::mutex> autoLock(locker);
  frameAvailable = true;
  condition.notify_all();
}

void HardwareImageReader::OnImageAvailableStatic(void* context, AImageReader*) {
  static_cast<HardwareImageReader*>(context)->notifyFrameAvailable();
}

std::shared_ptr<tgfx::ImageBuffer> HardwareImageReader::acquireNextBuffer() {
  std::shared_ptr<tgfx::ImageBuffer> image_buffer;
#if __ANDROID_API__ >= 26
  std::unique_lock<std::mutex> autoLock(locker);
  if (!frameAvailable) {
    static const auto TIMEOUT = std::chrono::seconds(1);
    auto condition_status = condition.wait_for(autoLock, TIMEOUT);
    if (condition_status == std::cv_status::timeout) {
      LOGE(
          "HardwareImageReader::acquireNextBuffer(): timeout when waiting for the frame "
          "available!");
      return nullptr;
    }
  }
  frameAvailable = false;
  if (aImage != nullptr) {
    AImage_delete(aImage);
    aImage = nullptr;
  }
  media_status_t status;
  status = AImageReader_acquireNextImage(aImageReader, &aImage);
  if (status != AMEDIA_OK || aImage == nullptr) {
    LOGE("HardwareImageReader: Error on acquiring next image.");
    return nullptr;
  }
  AHardwareBuffer* hardwareBuffer = nullptr;
  status = AImage_getHardwareBuffer(aImage, &hardwareBuffer);
  if (status != AMEDIA_OK || hardwareBuffer == nullptr) {
    LOGE("HardwareImageReader: Error getting AHardwareBuffer in acquireNextBuffer.");
    AImage_delete(aImage);
    return nullptr;
  }
  image_buffer = tgfx::ImageBuffer::MakeFrom(hardwareBuffer, videoFormat.colorSpace);
#else
  image_buffer = imageReader->acquireNextBuffer();
#endif
  return image_buffer;
}

}  // namespace pag