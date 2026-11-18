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

#include <cstdint>
#include <memory>
#include <string>
#include "pagx/PAGDisplayOptions.h"
#include "pagx/types/Color.h"

namespace pagx {

class VideoExportFrame;

/** A rational sampling rate in frames per second. Zero numerator with denominator 1 means auto. */
struct VideoFrameRate {
  /** Creates a frame rate from an exact numerator and denominator. */
  constexpr VideoFrameRate(int64_t frameRateNumerator = 0, int64_t frameRateDenominator = 1)
      : frameRateNumerator(frameRateNumerator), frameRateDenominator(frameRateDenominator) {
  }

  /** Creates an integer frame rate without ambiguity with the deleted floating-point overload. */
  constexpr VideoFrameRate(int frameRateNumerator)
      : VideoFrameRate(static_cast<int64_t>(frameRateNumerator)) {
  }

  /** Floating-point frame rates must be converted to an exact rational explicitly. */
  VideoFrameRate(double) = delete;

  /** Returns true for the automatic {0, 1} rate, which follows the document frame rate. */
  bool isAutomatic() const {
    return frameRateNumerator == 0 && frameRateDenominator == 1;
  }

  /** Returns the numeric frame rate, or zero when the denominator is zero. */
  double framesPerSecond() const {
    return frameRateDenominator == 0 ? 0.0
                                     : static_cast<double>(frameRateNumerator) /
                                           static_cast<double>(frameRateDenominator);
  }

  /**
   * Checks an exact positive rational range without floating-point rounding or multiplication
   * overflow.
   */
  bool isWithin(int minValue, int maxValue) const {
    if (frameRateNumerator <= 0 || frameRateDenominator <= 0) {
      return false;
    }
    auto quotient = frameRateNumerator / frameRateDenominator;
    auto remainder = frameRateNumerator % frameRateDenominator;
    return quotient >= minValue && quotient <= maxValue && (quotient != maxValue || remainder == 0);
  }

  /** Frame-rate numerator. Zero is the automatic sentinel only when the denominator is 1. */
  int64_t frameRateNumerator = 0;

  /** Frame-rate denominator. Explicit rates must use a positive denominator. */
  int64_t frameRateDenominator = 1;
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
   * Sequence length in seconds. Values <= 0 mean "use the longest mounted animation's effective
   * playback period". Once animations include any nested delay or trimming from their evaluation
   * offset; Loop and PingPong offsets only change phase and do not alter the period. A document
   * without active timelines produces one static frame by default; a positive duration
   * repeats that frame. Documents containing a state machine require an explicit positive duration
   * because state machines have no finite playback period. Must be finite. The frame count is
   * derived as `llround(duration * frameRate)`.
   */
  float duration = -1.0f;

  /**
   * Output scale factor applied to the document size. When the output size is derived, the exporter
   * renders at `ceil(documentSize * scale)` pixels. When outputWidth/outputHeight are set, this
   * value does not affect sizing: it only zooms the content into the explicit canvas, so content
   * that overflows is cropped instead of being scaled to fit. Must be finite and greater than zero;
   * Make() rejects invalid values instead of hiding host parameter errors.
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
   * `ceil(documentSize * scale)`. Cannot be combined with outputWidth/outputHeight: an explicit
   * size already decides the layout, so Make() rejects the combination instead of ignoring this
   * flag.
   */
  bool padToEvenSize = false;

  /**
   * Rendering mode used for every frame. Defaults to Partial to preserve the existing display-list
   * behavior. Hosts exporting animations can select Direct to bypass partial-frame caching and
   * match a full-frame readback pipeline.
   */
  PAGRenderMode renderMode = PAGRenderMode::Partial;

  /**
   * Optional explicit output dimensions. Both values must be positive to take effect; leaving both
   * at zero derives dimensions from documentSize * scale. A partially specified pair, a negative
   * value, or a combination with padToEvenSize is rejected by Make() before any scene or GPU
   * resource is created. Hosts with their own even rounding and texture/pixel-budget clamping can
   * use these fields without changing the scene zoom scale, which means content larger than the
   * explicit size is cropped instead of being scaled to fit.
   */
  int outputWidth = 0;
  int outputHeight = 0;
};

/**
 * Stable reason code for a failed export, independent of the human-readable message. Hosts map
 * these values onto their own user-facing errors instead of parsing text. Except for MixedMode and
 * Cancelled every code describes one failed attempt: the exporter keeps the uncommitted frame index
 * and can be retried, although the codes below marked as document problems only stop repeating once
 * the document or the options change.
 */
enum class VideoExportError {
  /** No failure happened. */
  None,
  /** An option, the document or the surface is invalid; rebuilding with valid input is required. */
  InvalidArgument,
  /** The document declares no playback period and the options supply no duration. */
  NoDuration,
  /** The document contains a state machine, which needs an explicit positive duration. */
  StateMachineRequiresDuration,
  /** Automatic frame rate was requested but the mounted animations declare different rates. */
  MixedFrameRates,
  /** The requested frame count or the last frame time does not fit in the supported range. */
  TooLong,
  /** The full-frame readback buffer could not be allocated; a lower resolution can be retried. */
  OutOfMemory,
  /** Drawing a frame failed; the same index is retried by the next start. */
  RenderFailed,
  /** The asynchronous surface readback failed; the same index is retried by the next start. */
  ReadbackFailed,
  /** The GPU device or its context is unavailable; retry once the device has recovered. */
  DeviceLost,
  /** nextFrame() and startNextFrame() were mixed; terminal, the exporter cannot continue. */
  MixedMode,
  /** The sequence was cancelled; terminal for every later attempt. */
  Cancelled,
};

/** Result of attempting to start an asynchronous frame. */
enum class VideoExportStartStatus {
  /** A new frame was started and carried in the result. */
  FrameStarted,
  /** The previously started frame is not consumed yet; the result carries that pending frame. */
  Busy,
  /** Every frame of the sequence was consumed; the result carries no frame. */
  End,
  /**
   * A failure stopped this start attempt; the result describes it with a message and a code.
   * Drawing, readback and device failures are retryable and keep the index uncommitted, while only
   * cancel() and MixedMode end the sequence.
   */
  Failed,
};

/** Current state of an asynchronous export frame. */
enum class VideoExportFrameStatus {
  /** The pixels are not available yet; poll again later. */
  Pending,
  /** The pixels are available, or the frame was already consumed. */
  Ready,
  /** The frame will never become readable. */
  Failed,
};

/** The explicit result of VideoExporter::startNextFrame(). */
struct VideoExportStartResult {
  /**
   * Outcome of the start attempt. FrameStarted and Busy carry a frame. Failed is the default, so a
   * result built without setting this field reports failure; read error and errorCode together with
   * it instead of relying on the default value.
   */
  VideoExportStartStatus status = VideoExportStartStatus::Failed;
  /** The new or pending frame, or null for End and Failed. */
  std::shared_ptr<VideoExportFrame> frame = nullptr;
  /** Human-readable failure description for Failed, empty otherwise. */
  std::string error = {};
  /** Stable failure code for Failed, None otherwise. */
  VideoExportError errorCode = VideoExportError::None;
};

}  // namespace pagx
