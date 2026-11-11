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
#include <numeric>
#include <string>
#include <vector>
#include "pagx/types/Color.h"

namespace tgfx {
class Bitmap;
class Pixmap;
}  // namespace tgfx

namespace pagx {

class PAGAnimation;
class PAGComposition;
class PAGScene;
class PAGSurface;
class PAGTimeline;
class PAGXDocument;

/** A rational sampling rate in frames per second. Zero numerator with denominator 1 means auto. */
struct VideoFrameRate {
  /** Creates a frame rate from a numerator and denominator. */
  constexpr VideoFrameRate(int64_t numeratorValue = 0, int64_t denominatorValue = 1)
      : numerator(numeratorValue), denominator(denominatorValue) {
  }

  /** Returns the numeric frame rate, or zero when the denominator is zero. */
  double framesPerSecond() const {
    return denominator == 0 ? 0.0
                            : static_cast<double>(numerator) / static_cast<double>(denominator);
  }

  /**
   * Checks an exact positive rational range without floating-point rounding or multiplication
   * overflow.
   */
  bool isWithin(int minValue, int maxValue) const {
    if (numerator <= 0 || denominator <= 0) {
      return false;
    }
    auto quotient = numerator / denominator;
    auto remainder = numerator % denominator;
    return quotient >= minValue && quotient <= maxValue &&
           (quotient != maxValue || remainder == 0);
  }

  /** Returns the equivalent ratio reduced by its greatest common divisor. */
  VideoFrameRate normalized() const {
    auto divisor = std::gcd(numerator, denominator);
    if (divisor == 0) {
      return *this;
    }
    return {numerator / divisor, denominator / divisor};
  }

  /** Returns an integer or numerator/denominator representation suitable for ffmpeg. */
  std::string toString() const {
    if (denominator == 1) {
      return std::to_string(numerator);
    }
    return std::to_string(numerator) + "/" + std::to_string(denominator);
  }

  /** Frame-rate numerator. Zero is the automatic sentinel only when denominator is 1. */
  int64_t numerator = 0;

  /** Frame-rate denominator. Explicit rates must use a positive denominator. */
  int64_t denominator = 1;
};

/**
 * Export options for VideoExporter.
 *
 * This struct is part of the public API but does not promise binary ABI stability. Callers and
 * the library must use compatible headers. Changes to the field layout require recompilation.
 */
struct VideoExportOptions {
  /**
   * Sampling frame rate in frames per second. The default {0, 1} uses the document animation frame
   * rate, falling back to 60 when the document does not declare one. A non-integer document frame
   * rate is rejected in this mode instead of being guessed or rounded; use an explicit ratio such
   * as {30000, 1001} for NTSC content. All declared top-level animation frame rates must match in
   * automatic mode. Explicit rates must be in [1, 120] with a positive denominator.
   */
  VideoFrameRate frameRate = {0, 1};

  /**
   * Sequence length in seconds. Values <= 0 mean "use the longest mounted animation's playback
   * period", including both passes of a PingPong animation and any nested evaluation offset. A
   * document without active timelines produces one static frame by default; a positive duration
   * repeats that frame. Documents containing a state machine require an explicit positive duration
   * because state machines have no finite playback period. Must be finite. The frame count is
   * derived as `llround(duration * frameRate)`.
   */
  float duration = -1.0f;

  /**
   * Output scale factor applied to the document size. The exporter renders at
   * `ceil(documentSize * scale)` pixels. Must be finite and greater than zero; Make() rejects
   * invalid values instead of hiding host parameter errors.
   */
  float scale = 1.0f;

  /**
   * Background color drawn under the content when alpha is greater than zero. Defaults to fully
   * transparent. Whether an opaque background is required is decided by the output container, not
   * here. Frames are always premultiplied RGBA, so an opaque background makes premultiplication a
   * no-op and lets the host pass the pixels through unchanged.
   */
  Color background = {0.0f, 0.0f, 0.0f, 0.0f, ColorSpace::SRGB};

  /**
   * Whether to pad the output width and height up to even numbers. This is a layout capability;
   * the host decides whether its selected pixel format requires it. Defaults to false, which keeps
   * `ceil(documentSize * scale)`.
   */
  bool padToEvenSize = false;
};

/**
 * VideoExporter produces an RGBA frame sequence from a document's animations or static scene. The
 * scene and the offscreen surface are created once and reused for every frame, so the per-frame
 * cost is limited to timeline evaluation, drawing and the pixel readback. Documents with no active
 * top-level or mounted nested timelines draw and read back only the first frame; later frames reuse
 * the same RGBA buffer.
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

  static constexpr int MIN_FRAME_RATE = 1;
  static constexpr int MAX_FRAME_RATE = 120;

  /**
   * Creates an exporter bound to the given document. Top-level animations and state machines are
   * sampled in declaration order, and timelines mounted on nested compositions advance with the
   * scene. Top-level animation ids must be non-empty and unique. A document without active
   * timelines produces one static frame by default, or repeats its static scene when duration is
   * positive. A document containing a state machine requires an explicit positive duration.
   *
   * After Make() succeeds, the source document and all reachable external composition documents
   * must remain unmodified until the exporter is destroyed. Create a new exporter after changing
   * their contents, dimensions, frame rates, or timeline structure.
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

  ~VideoExporter();

  VideoExporter(const VideoExporter&) = delete;
  VideoExporter& operator=(const VideoExporter&) = delete;
  VideoExporter(VideoExporter&&) = delete;
  VideoExporter& operator=(VideoExporter&&) = delete;

  /**
   * Computes output dimensions from document dimensions, scale and padding. Returns false for
   * non-finite or non-positive inputs and dimensions that do not fit in an int.
   *
   * @param documentWidth source width in document coordinates.
   * @param documentHeight source height in document coordinates.
   * @param scale positive finite output scale.
   * @param padToEvenSize whether to pad each computed dimension up to an even number.
   * @param width receives the computed width on success.
   * @param height receives the computed height on success.
   */
  static bool ComputeOutputDimensions(float documentWidth, float documentHeight, float scale,
                                      bool padToEvenSize, int* width, int* height);

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
  struct PlaybackPlan {
    std::vector<std::shared_ptr<PAGTimeline>> timelines = {};
    int64_t longestDurationUs = 0;
    float documentFrameRate = 0.0f;
    bool hasMixedFrameRates = false;
    bool hasDynamicContent = false;
    bool hasStateMachine = false;
  };

  VideoExporter();

  static bool ResolvePlaybackPlan(const std::shared_ptr<PAGScene>& scene,
                                  const std::shared_ptr<PAGXDocument>& document, PlaybackPlan* plan,
                                  std::string* errorMsg);
  static bool AddAnimation(const std::shared_ptr<PAGAnimation>& animation, int64_t offsetUs,
                           bool includeInFrameRate, PlaybackPlan* plan, std::string* errorMsg);
  static bool CollectNestedPlayback(PAGComposition* composition, PlaybackPlan* plan,
                                    std::string* errorMsg);
  static bool ResolveSampling(const Options& options, const PlaybackPlan& plan,
                              VideoFrameRate* frameRate, int64_t* frameCount,
                              std::string* errorMsg);

  std::shared_ptr<PAGScene> scene = {};
  std::shared_ptr<PAGSurface> surface = {};
  std::vector<std::shared_ptr<PAGTimeline>> timelines = {};
  std::shared_ptr<tgfx::Bitmap> bitmap = {};
  std::shared_ptr<tgfx::Pixmap> pixmap = {};
  int outputWidth = 0;
  int outputHeight = 0;
  VideoFrameRate samplingFrameRate = {60, 1};
  int64_t totalFrames = 0;
  int64_t nextFrameIndex = 0;
  int64_t previousFrameTimeUs = 0;
  bool reuseStaticFrame = false;
};

}  // namespace pagx
