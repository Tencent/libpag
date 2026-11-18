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
#include "pagx/VideoExportFrame.h"
#include "pagx/VideoExportTypes.h"

namespace pagx {

class PAGSurface;
class PAGXDocument;
class VideoExporterImpl;

/**
 * VideoExporter produces an RGBA frame sequence from a document's animations or static scene. The
 * scene and the offscreen surface are created once and reused for every frame, so the per-frame
 * cost is limited to timeline evaluation, drawing and the pixel readback. Documents with no active
 * top-level or mounted nested timelines draw and read back only the first frame; later frames reuse
 * the same RGBA buffer.
 *
 * The exporter only renders. Encoding and muxing are the host's responsibility, and no video codec
 * ships inside pagx.
 *
 * Frames are pulled one at a time so that hosts can interleave other work (a UI thread, an encoder
 * queue) between frames. Rendering the whole sequence in a single call is not supported because a
 * 1080p frame can be expensive to draw and read back.
 *
 * Frames are premultiplied RGBA8888. Containers that carry alpha need the host to unpremultiply;
 * opaque output can pass the pixels through unchanged.
 *
 * Thread safety: an instance is not thread-safe. Every entry point must run on the thread that
 * owns the exporter's GPU device. Make(), MakeWithSurface(), nextFrame(), startNextFrame(),
 * cancel(), destruction and every VideoExportFrame call acquire the device context lock internally,
 * so callers must not already hold that lock: lockContext() is not recursive, and a nested call
 * blocks forever instead of reporting an error.
 *
 * The caller drives asynchronous readback. The exporter starts no thread and no timer, so a frame
 * only becomes readable while the host keeps polling VideoExportFrame::status() on the device
 * thread.
 *
 * Layout: PAGScene::Make() applies layout automatically when the document has not applied it yet,
 * so callers only need to register fonts on PAGXDocument::fontConfig() beforehand when the
 * document relies on external fonts.
 */
class VideoExporter {
 public:
  using Options = VideoExportOptions;

  static constexpr int MIN_FRAME_RATE = 1;
  static constexpr int MAX_FRAME_RATE = 120;

  /**
   * Creates an exporter bound to the given document. Top-level animations and state machines are
   * sampled in declaration order, and timelines mounted on nested compositions advance with the
   * scene. Top-level animation and state machine ids must be non-empty and unique. A document
   * without active timelines produces one static frame by default, or repeats its static scene
   * when duration is positive. A document containing a state machine requires an explicit positive
   * duration.
   *
   * After Make() succeeds, the source document and all reachable external composition documents
   * must remain unmodified until the exporter is destroyed. Create a new exporter after changing
   * their contents, dimensions, frame rates, or timeline structure.
   *
   * @param document the source document, which is retained for the lifetime of the returned
   *                 exporter. Must be non-null.
   * @param options sampling frame rate, duration, scale and background.
   * @param errorMsg optional pointer that receives a human-readable description on failure.
   * @param errorCode optional pointer that receives the stable failure code on failure.
   * @return a new exporter, or nullptr when the document or options are invalid, an animation has
   *         no duration and none was supplied, the computed frame range or dimensions overflow, or
   *         the offscreen surface cannot be created.
   */
  static std::shared_ptr<VideoExporter> Make(std::shared_ptr<PAGXDocument> document,
                                             const Options& options = {},
                                             std::string* errorMsg = nullptr,
                                             VideoExportError* errorCode = nullptr);

  /**
   * Creates an exporter that renders into a host-provided surface. The surface dimensions must
   * exactly match the output dimensions derived from document and options; a mismatch is reported
   * instead of being resized silently. The host must ensure that the surface is suitable for
   * offscreen rendering, remains exclusively used by the exporter, and shares a compatible device
   * with the document's GPU resources. Readback capability is validated when a frame is started,
   * not by a speculative readback during construction.
   *
   * @param document the source document, which is retained for the lifetime of the returned
   *                 exporter. Must be non-null.
   * @param surface the host surface to render into; retained for the lifetime of the exporter and
   *                required to match the resolved output dimensions exactly. Must be non-null.
   * @param options sampling frame rate, duration, scale, background and explicit output size.
   * @param errorMsg optional pointer that receives a human-readable description on failure.
   * @param errorCode optional pointer that receives the stable failure code on failure.
   * @return a new exporter, or nullptr when the document, surface or options are invalid, the
   *         surface dimensions do not match the resolved output size, the surface has no valid GPU
   *         device or context, or the document playback plan cannot be resolved.
   */
  static std::shared_ptr<VideoExporter> MakeWithSurface(std::shared_ptr<PAGXDocument> document,
                                                        std::shared_ptr<PAGSurface> surface,
                                                        const Options& options = {},
                                                        std::string* errorMsg = nullptr,
                                                        VideoExportError* errorCode = nullptr);

  ~VideoExporter();

  VideoExporter(const VideoExporter&) = delete;
  VideoExporter& operator=(const VideoExporter&) = delete;
  VideoExporter(VideoExporter&&) = delete;
  VideoExporter& operator=(VideoExporter&&) = delete;

  /**
   * Returns the rendering mode applied to the scene's display options.
   */
  PAGRenderMode renderMode() const;

  /**
   * Returns the output width in pixels, which is `ceil(documentWidth * scale)` plus optional padding.
   */
  int width() const;

  /**
   * Returns the output height in pixels, including optional even-size padding.
   */
  int height() const;

  /**
   * Returns the resolved sampling frame rate as a rational value.
   */
  VideoFrameRate frameRate() const;

  /**
   * Returns the total number of frames in the sequence.
   */
  int64_t frameCount() const;

  /**
   * Returns the number of committed frames, which is also the index of the next frame to be
   * rendered. A synchronous frame is committed by a successful nextFrame(); an asynchronous frame
   * is committed by the first successful VideoExportFrame::readPixels(), so a frame that was
   * drawn or read but not consumed does not advance this value.
   */
  int64_t currentFrame() const;

  /**
   * Renders the next frame of the sequence and makes it readable through pixels().
   *
   * @param errorMsg optional pointer cleared on entry and set to the drawing or pixel-readback
   *                 failure reason. Normal end of sequence leaves it empty.
   * @return true when a frame was rendered; false at the end of the sequence or on failure. Use
   *         `currentFrame() < frameCount()` to tell the two apart: a failure leaves the index
   *         unadvanced and sets errorCode() to the stable failure reason.
   */
  bool nextFrame(std::string* errorMsg = nullptr);

  /**
   * Advances, draws and starts reading the next frame without waiting for GPU readback. The first
   * successful use of nextFrame() or startNextFrame() fixes the exporter's mode; mixing modes is
   * a terminal usage error reported as Failed with VideoExportError::MixedMode.
   *
   * Only one uncommitted frame may exist at a time. While the previously started frame is
   * unconsumed the result is Busy, and the result carries that same pending frame so the caller can
   * still read it even after dropping its own reference; a dropped frame therefore never leaves a
   * later start stuck in Busy. The caller reads the frame or calls cancel() to end the sequence.
   *
   * Drawing or readback failures are reported as Failed without ending the sequence: the index is
   * not advanced and the next start retries the same frame, which matches the recoverable behavior
   * of nextFrame(). Only cancel() and MixedMode are terminal.
   *
   * @return the start result. FrameStarted carries the new frame, whose status() must be polled on
   *         this thread until Ready before the next start is allowed. Busy carries the previously
   *         started frame that is still pending.
   */
  VideoExportStartResult startNextFrame();

  /**
   * Cancels the sequence and abandons any pending frame. This is terminal: later asynchronous
   * starts return Failed with VideoExportError::Cancelled. A pending frame that was not consumed
   * is marked Failed with the same code. Cancelling an exporter that already ended on a usage
   * error keeps that original code and message. Must run on the device-owning thread.
   */
  void cancel();

  /**
   * Returns the last failure reason, or an empty string. The value is cleared by the next
   * successful frame. Draw and readback failures are retryable, so this describes the most recent
   * problem rather than a terminal state.
   */
  std::string error() const;

  /** Returns the stable code matching error(), or None when the last frame succeeded. */
  VideoExportError errorCode() const;

  /**
   * Returns the pixels of the most recently rendered frame as RGBA8888, premultiplied. The buffer
   * is owned by the exporter and stays valid only until the next nextFrame() call, so the host
   * must consume it (write to a pipe, wrap in a VideoFrame) before advancing.
   *
   * Only synchronous frames are exposed here, and the buffer is allocated on the first such frame,
   * so this returns nullptr before it and after startNextFrame() has been used: asynchronous frames
   * are read through VideoExportFrame::readPixels() into the host buffer instead.
   */
  const uint8_t* pixels() const;

  /**
   * Returns the number of bytes between the start of two consecutive pixel rows. May be larger than
   * `width() * 4`, in which case raw frame sinks must repack the rows. Returns 0 before the first
   * synchronous frame and after startNextFrame() has been used, since asynchronous frames do not use
   * the internal buffer.
   */
  size_t rowBytes() const;

 private:
  explicit VideoExporter(std::shared_ptr<VideoExporterImpl> exporterImpl);

  std::shared_ptr<VideoExporterImpl> impl = nullptr;
};

}  // namespace pagx
