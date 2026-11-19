/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "pagx/VideoExportInternal.h"

namespace tgfx {
class Context;
class Device;
class Pixmap;
class SurfaceReadback;
}  // namespace tgfx

namespace pagx {

class VideoExportFrameImpl {
 public:
  static std::shared_ptr<VideoExportFrameImpl> MakeGPU(std::shared_ptr<VideoExportState> state,
                                                       int64_t index,
                                                       std::shared_ptr<PAGSurface> surface,
                                                       std::shared_ptr<tgfx::Pixmap> staticCache,
                                                       VideoExportFailure* failure);
  static std::shared_ptr<VideoExportFrameImpl> MakeCPU(std::shared_ptr<VideoExportState> state,
                                                       int64_t index,
                                                       std::shared_ptr<tgfx::Pixmap> pixels);

  VideoExportFrameImpl(std::shared_ptr<VideoExportState> state, int64_t index);

  int64_t index() const {
    return frameIndex;
  }
  const std::string& error() const {
    return errorMessage;
  }
  VideoExportError errorCode() const {
    return failureCode;
  }
  VideoExportFrameStatus status();
  bool readPixels(void* dstPixels, size_t dstRowBytes);
  void fail(VideoExportError code, const std::string& message);

  bool hasEnded() const {
    return consumed || failed;
  }

  bool consumed = false;

 private:
  tgfx::Context* lockReadbackSession();
  static bool copyPixels(const tgfx::Pixmap& source, void* dstPixels, size_t dstRowBytes);
  VideoExportFrameStatus pollLocked(tgfx::Context* context);
  bool copyGPUPixelsLocked(tgfx::Context* context, void* dstPixels, size_t dstRowBytes);
  void absorbSharedFailure();
  void commit();

  std::shared_ptr<VideoExportState> state = nullptr;
  int64_t frameIndex = -1;
  std::shared_ptr<PAGSurface> surface = nullptr;
  std::shared_ptr<tgfx::Device> device = nullptr;
  std::shared_ptr<tgfx::SurfaceReadback> readback = nullptr;
  std::shared_ptr<tgfx::Pixmap> cpuPixels = nullptr;
  std::shared_ptr<tgfx::Pixmap> staticCache = nullptr;
  bool flipY = false;
  bool failed = false;
  VideoExportError failureCode = VideoExportError::None;
  std::string errorMessage = {};
};

}  // namespace pagx
