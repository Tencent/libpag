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

#include "pagx/VideoExporter.h"

namespace tgfx {
class Bitmap;
class Device;
class SurfaceReadback;
}  // namespace tgfx

namespace pagx {

enum class VideoExportMode {
  None,
  Sync,
  Async,
};

/** Shared by the exporter and its current frame without creating an ownership cycle. */
struct VideoExportState {
  VideoExportMode mode = VideoExportMode::None;
  int64_t currentFrame = 0;
  bool failed = false;
  bool cancelled = false;
  std::string error = {};
};

class VideoExportFrameImpl {
 public:
  static std::shared_ptr<VideoExportFrameImpl> MakeGPU(std::shared_ptr<VideoExportState> state,
                                                       int64_t index,
                                                       std::shared_ptr<PAGSurface> surface,
                                                       std::shared_ptr<tgfx::Bitmap> staticCache,
                                                       std::string* errorMsg);
  static std::shared_ptr<VideoExportFrameImpl> MakeCPU(std::shared_ptr<VideoExportState> state,
                                                       int64_t index,
                                                       std::shared_ptr<tgfx::Bitmap> pixels);

  VideoExportFrameImpl(std::shared_ptr<VideoExportState> state, int64_t index);

  int64_t index() const {
    return frameIndex;
  }
  const std::string& error() const {
    return errorMessage;
  }
  VideoExportFrameStatus status();
  bool readPixels(void* dstPixels, size_t dstRowBytes);
  void fail(const std::string& message);

  bool consumed = false;

 private:
  bool readGPUPixels(void* dstPixels, size_t dstRowBytes);

  std::shared_ptr<VideoExportState> state = nullptr;
  int64_t frameIndex = -1;
  std::shared_ptr<PAGSurface> surface = nullptr;
  std::shared_ptr<tgfx::Device> device = nullptr;
  std::shared_ptr<tgfx::SurfaceReadback> readback = nullptr;
  std::shared_ptr<tgfx::Bitmap> cpuPixels = nullptr;
  std::shared_ptr<tgfx::Bitmap> staticCache = nullptr;
  bool flipY = false;
  bool failed = false;
  std::string errorMessage = {};
};

}  // namespace pagx
