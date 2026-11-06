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
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "pagx/types/Color.h"

namespace tgfx {
class Bitmap;
class Pixmap;
}  // namespace tgfx

namespace pagx {

class PAGAnimation;
class PAGScene;
class PAGSurface;
class PAGXDocument;

/**
 * Export options for VideoExporter.
 *
 * ABI stability: this struct is part of the public API. New optional fields must be appended at
 * the end with sensible defaults so that callers compiled against an older header continue to
 * link and behave correctly. Reordering, removing, or changing the type of an existing field is
 * a breaking change.
 */
struct VideoExportOptions {
  /**
   * Sampling frame rate in frames per second. Zero means "use the document animation frame rate",
   * falling back to 60 when the document does not declare one. Valid range: (0, 120].
   */
  int frameRate = 0;

  /**
   * Sequence length in seconds. Values <= 0 mean "use the longest top-level animation's duration".
   * The frame count is derived as `llround(duration * frameRate)`.
   */
  float duration = -1.0f;

  /**
   * Output scale factor applied to the document size. The exporter renders at
   * `ceil(documentSize * scale)` pixels. Must be greater than zero; invalid values fall back to 1.
   */
  float scale = 1.0f;

  /**
   * Whether to draw an opaque background under the content. Defaults to false, which keeps the
   * frames transparent.
   *
   * Whether an opaque background is required is decided by the output container, not here: H.264
   * carries no alpha, while VP9 does. Frames are always premultiplied RGBA, so an opaque
   * background (alpha == 255) makes premultiplication a no-op and lets the host pass the pixels
   * through unchanged.
   */
  bool hasBackground = false;

  /**
   * Background color drawn under the content when hasBackground is true. Ignored otherwise.
   */
  Color background = {};
};

/**
 * VideoExporter produces an RGBA frame sequence from a document's animations. The scene and the
 * offscreen surface are created once and reused for every frame, so the per-frame cost is limited
 * to timeline evaluation, drawing and the pixel readback.
 *
 * The exporter only renders. Encoding and muxing are the host's responsibility: an external ffmpeg
 * process on native platforms, WebCodecs on the web. No video codec ships inside pagx.
 *
 * Frames are pulled one at a time so that hosts can interleave other work (a UI thread, an encoder
 * queue) between frames. Rendering the whole sequence in a single call is not supported because a
 * 1080p frame costs roughly 50ms and would block a browser main thread for tens of seconds.
 *
 * Frames are premultiplied RGBA8888. Containers that carry alpha need the host to unpremultiply;
 * opaque output (the mp4 path) can pass the pixels through unchanged.
 *
 * Thread safety: an instance is not thread-safe. Make() must run on a thread that owns a GPU
 * context; on wasm that means the main thread, because Devices::MakeDefault() only returns a
 * device already bound to the calling thread. Callers must not hold the same device's context lock
 * while calling nextFrame(), since draw() and readPixels() acquire it internally.
 *
 * Layout: PAGScene::Make() applies layout automatically when the document has not applied it yet,
 * so callers only need to register fonts on PAGXDocument::fontConfig() beforehand when the
 * document relies on external fonts.
 */
class VideoExporter {
 public:
  using Options = VideoExportOptions;

  /**
   * Creates an exporter bound to the given document. The document's top-level animations are
   * sampled; a document without any top-level animation cannot be exported.
   *
   * @param document the source document, which is retained for the lifetime of the returned
   *                 exporter. Must be non-null.
   * @param options sampling frame rate, duration, scale and background.
   * @param errorMsg optional pointer that receives a human-readable description on failure.
   * @return a new exporter, or nullptr when the document is null, has no top-level animation, has
   *         no duration and none was supplied, or the offscreen surface cannot be created.
   */
  static std::shared_ptr<VideoExporter> Make(std::shared_ptr<PAGXDocument> document,
                                             const Options& options = {},
                                             std::string* errorMsg = nullptr);

  ~VideoExporter();

  /**
   * Returns the output width in pixels, which is `ceil(documentWidth * scale)`.
   */
  int width() const;

  /**
   * Returns the output height in pixels, which is `ceil(documentHeight * scale)`.
   */
  int height() const;

  /**
   * Returns the sampling frame rate in frames per second.
   */
  int frameRate() const;

  /**
   * Returns the total number of frames in the sequence.
   */
  int64_t frameCount() const;

  /**
   * Returns the index of the next frame to be rendered, i.e. the number of frames rendered so far.
   */
  int64_t currentFrame() const;

  /**
   * Renders the next frame of the sequence and makes it readable through pixels().
   *
   * @return true when a frame was rendered; false at the end of the sequence or on failure. Use
   *         `currentFrame() < frameCount()` to tell the two apart: a failure leaves the index
   *         unadvanced.
   */
  bool nextFrame();

  /**
   * Returns the pixels of the most recently rendered frame as RGBA8888, premultiplied. The buffer
   * is owned by the exporter and stays valid only until the next nextFrame() call, so the host
   * must consume it (write to a pipe, wrap in a VideoFrame) before advancing.
   */
  const uint8_t* pixels() const;

  /**
   * Returns the number of bytes between the start of consecutive pixel rows. May be larger than
   * `width() * 4`, in which case raw frame sinks must repack the rows.
   */
  size_t rowBytes() const;

 private:
  VideoExporter();

  std::shared_ptr<PAGScene> scene = {};
  std::shared_ptr<PAGSurface> surface = {};
  std::vector<std::shared_ptr<PAGAnimation>> animations = {};
  std::shared_ptr<tgfx::Bitmap> bitmap = {};
  std::shared_ptr<tgfx::Pixmap> pixmap = {};
  int width_ = 0;
  int height_ = 0;
  int frameRate_ = 60;
  int64_t frameCount_ = 0;
  int64_t currentFrame_ = 0;
  int64_t previousTimeUs_ = 0;
};

}  // namespace pagx
