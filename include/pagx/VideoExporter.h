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
#include "pagx/PAGDisplayOptions.h"
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
struct VideoExportState;

/**
 * Export options for VideoExporter.
 *
 * This struct is part of the public API but does not promise binary ABI stability. Callers and
 * the library must use compatible headers. Changes to the field layout require recompilation.
 */
struct VideoExportOptions {
  /**
   * Sampling frame rate in frames per second. Zero means "use the document animation frame rate",
   * falling back to 60 when the document does not declare one. A non-integer document frame rate is
   * rejected in this mode instead of being silently rounded. All declared top-level animation
   * frame rates must also match in this mode. Otherwise the value must be in [1, 120]; Make()
   * rejects anything outside that range and negative values.
   */
  int frameRate = 0;

  /**
   * Sequence length in seconds. Values <= 0 mean "use the longest top-level animation's playback
   * period", including both passes of a PingPong animation. A document without animations produces
   * one static frame by default; a positive duration repeats that frame. Must be finite. The frame
   * count is derived as `llround(duration * frameRate)`.
   */
  float duration = -1.0f;

  /**
   * Output scale factor applied to the document size. The exporter renders at
   * `ceil(documentSize * scale)` pixels. Must be finite and greater than zero; Make() rejects
   * invalid values instead of hiding host parameter errors.
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

  /**
   * Whether to round the output width and height up to even numbers. Codecs that subsample chroma
   * 4:2:0 (H.264 yuv420p and friends) cannot encode an odd dimension, so hosts writing into such a
   * container must request it. Defaults to false, which keeps `ceil(documentSize * scale)`.
   */
  bool alignToEvenSize = false;

  /**
   * Rendering mode used for every frame. Defaults to Partial to preserve the existing display-list
   * behavior. Hosts exporting animations can select Direct to bypass partial-frame caching and
   * match a full-frame readback pipeline.
   */
  PAGRenderMode renderMode = PAGRenderMode::Partial;

  /**
   * Optional explicit output dimensions. Both values must be positive to take effect; leaving both
   * at zero derives dimensions from documentSize * scale. Hosts with their own even rounding and
   * texture/pixel-budget clamping can use these fields without changing the scene zoom scale.
   */
  int outputWidth = 0;
  int outputHeight = 0;
};

/** Result of attempting to start an asynchronous frame. */
enum class VideoExportStartStatus {
  FrameStarted,
  Busy,
  End,
  Failed,
};

/** Current state of an asynchronous export frame. */
enum class VideoExportFrameStatus {
  Pending,
  Ready,
  Failed,
};

class VideoExportFrameImpl;

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

  /** Returns the current state without waiting for the GPU. Consumed frames remain Ready. */
  VideoExportFrameStatus status();

  /** Returns the failure reason, or an empty string when the frame has not failed. */
  std::string error() const;

  /**
   * Copies premultiplied RGBA_8888 pixels to the caller's buffer. dstRowBytes must be at least
   * width * 4. The first successful call consumes the frame and commits its index to the exporter.
   * Returns false while Pending, after consumption, or on failure. No index is committed twice.
   */
  bool readPixels(void* dstPixels, size_t dstRowBytes);

 private:
  explicit VideoExportFrame(std::shared_ptr<VideoExportFrameImpl> impl);

  std::shared_ptr<VideoExportFrameImpl> impl = nullptr;

  friend class VideoExporter;
};

/** The explicit result of VideoExporter::startNextFrame(). */
struct VideoExportStartResult {
  VideoExportStartStatus status = VideoExportStartStatus::Failed;
  std::shared_ptr<VideoExportFrame> frame = nullptr;
  std::string error = {};
};

/**
 * VideoExporter produces an RGBA frame sequence from a document's animations or static scene. The
 * scene and the offscreen surface are created once and reused for every frame, so the per-frame
 * cost is limited to timeline evaluation, drawing and the pixel readback. Documents with no
 * animation or state-machine definitions draw and read back only the first frame; later frames
 * reuse the same RGBA buffer.
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
   * sampled; their ids must be non-empty and unique. A document without animations produces one
   * static frame by default, or repeats its static scene when duration is positive.
   *
   * @param document the source document, which is retained for the lifetime of the returned
   *                 exporter. Must be non-null.
   * @param options sampling frame rate, duration, scale and background.
   * @param errorMsg optional pointer that receives a human-readable description on failure.
   * @return a new exporter, or nullptr when the document or options are invalid, an animation has
   *         no duration and none was supplied, the computed frame range or dimensions overflow, or
   *         the offscreen surface cannot be created.
   */
  static std::shared_ptr<VideoExporter> Make(std::shared_ptr<PAGXDocument> document,
                                             const Options& options = {},
                                             std::string* errorMsg = nullptr);

  /**
   * Creates an exporter that renders into a host-provided surface. The surface dimensions must
   * exactly match the output dimensions derived from document and options. The host must ensure
   * that the surface is suitable for offscreen rendering, remains exclusively used by the exporter,
   * and shares a compatible device with the document's GPU resources. Readback capability is
   * validated when a frame is started, not by a speculative readback during construction.
   */
  static std::shared_ptr<VideoExporter> MakeWithSurface(std::shared_ptr<PAGXDocument> document,
                                                        std::shared_ptr<PAGSurface> surface,
                                                        const Options& options = {},
                                                        std::string* errorMsg = nullptr);

  ~VideoExporter();

  /**
   * Returns the rendering mode applied to the scene's display options.
   */
  PAGRenderMode renderMode() const;

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
   * Advances, draws and starts reading the next frame without waiting for GPU readback. The first
   * successful use of nextFrame() or startNextFrame() fixes the exporter's mode; mixing modes is
   * a terminal usage error. Only one uncommitted frame may exist at a time.
   */
  VideoExportStartResult startNextFrame();

  /**
   * Cancels the sequence and abandons any pending frame. This is terminal: later asynchronous
   * starts return Failed with "cancelled". Must run on the device-owning thread.
   */
  void cancel();

  /** Returns the terminal error or the last synchronous read error, if any. */
  std::string error() const;

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

  static std::shared_ptr<VideoExporter> MakeInternal(std::shared_ptr<PAGXDocument> document,
                                                     std::shared_ptr<PAGSurface> surface,
                                                     const Options& options, std::string* errorMsg);

  bool drawNextFrame(std::string* errorMsg);

  std::shared_ptr<VideoExportState> state = {};
  std::shared_ptr<VideoExportFrame> pendingFrame = {};
  std::shared_ptr<PAGScene> scene = {};
  std::shared_ptr<PAGSurface> surface = {};
  std::vector<std::shared_ptr<PAGAnimation>> animations = {};
  std::shared_ptr<tgfx::Bitmap> bitmap = {};
  std::shared_ptr<tgfx::Pixmap> pixmap = {};
  int width_ = 0;
  int height_ = 0;
  int frameRate_ = 60;
  int64_t frameCount_ = 0;
  int64_t previousTimeUs_ = 0;
  bool reuseStaticFrame_ = false;
};

}  // namespace pagx
