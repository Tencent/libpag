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
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include "pagx/VideoExportTypes.h"

namespace pagx {

class VideoExportFrameImpl;
class VideoExporterImpl;

/**
 * A single frame started by VideoExporter::startNextFrame(). Frames may be backed by an
 * asynchronous GPU readback or a cached static CPU image. All operations must run on the thread
 * that owns the exporter's GPU device.
 */
class VideoExportFrame {
 public:
  ~VideoExportFrame();

  VideoExportFrame(const VideoExportFrame&) = delete;
  VideoExportFrame& operator=(const VideoExportFrame&) = delete;

  /** The sequential frame index, fixed when the frame is started. */
  int64_t index() const;

  /**
   * Returns the current state without waiting for the GPU. Consumed frames remain Ready. Polling
   * advances the pending readback, so this method is not const and must run on the device thread.
   */
  VideoExportFrameStatus status();

  /** Returns the failure reason, or an empty string when the frame has not failed. */
  std::string error() const;

  /** Returns the stable failure code of this frame, or None when it has not failed. */
  VideoExportError errorCode() const;

  /**
   * Copies premultiplied RGBA_8888 pixels into the caller's buffer.
   *
   * @param dstPixels the destination buffer, which must hold at least height * dstRowBytes bytes.
   * @param dstRowBytes the byte distance between consecutive destination rows; must be at least
   *                    width * 4 and may be larger, so containers with a fixed stride need no
   *                    repacking before a copy.
   * @return true when the pixels were copied. The first successful call consumes the frame and
   *         commits its index to the exporter. Returns false while Pending, after consumption,
   *         when a parameter is invalid, or when the frame has failed; no index is committed
   *         twice and a failed read does not skip the frame. An invalid destination is a caller
   *         error only: the frame stays Ready and no exporter state changes, so the caller can
   *         retry with a correct buffer.
   */
  bool readPixels(void* dstPixels, size_t dstRowBytes);

 private:
  explicit VideoExportFrame(std::shared_ptr<VideoExportFrameImpl> impl);

  std::shared_ptr<VideoExportFrameImpl> impl = nullptr;

  friend class VideoExporterImpl;
};

}  // namespace pagx
