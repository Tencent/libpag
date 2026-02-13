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

#include <deque>
#include <emscripten/bind.h>
#include "tgfx/gpu/Recording.h"
#include "tgfx/layers/DisplayList.h"
#include "LayerBuilder.h"

namespace pagx {

class PAGXView {
 public:
  /**
   * Creates a PAGXView instance for WeChat Mini Program rendering.
   * @param width The width of the canvas in pixels.
   * @param height The height of the canvas in pixels.
   * @return A shared pointer to the created PAGXView, or nullptr if creation fails.
   *
   * Note: Before calling this method, the JavaScript code must:
   * 1. Get the Canvas object from WeChat API
   * 2. Call canvas.getContext('webgl2') to get WebGL2RenderingContext
   * 3. Register the context via GL.registerContext(gl)
   */
  static std::shared_ptr<PAGXView> MakeFrom(int width, int height);

  /**
   * Constructs a PAGXView with the given device and canvas dimensions.
   */
  PAGXView(std::shared_ptr<tgfx::Device> device, int width, int height);

  /**
   * Registers fallback fonts for text rendering.
   * @param fontVal Font file data as a JavaScript Uint8Array (e.g. NotoSansSC-Regular.otf).
   * @param emojiFontVal Emoji font file data as a JavaScript Uint8Array (e.g. NotoColorEmoji.ttf).
   */
  void registerFonts(const emscripten::val& fontVal, const emscripten::val& emojiFontVal);

  /**
   * Loads a PAGX file from the given data and builds the layer tree for rendering.
   * @param pagxData PAGX file data as a JavaScript Uint8Array.
   */
  void loadPAGX(const emscripten::val& pagxData);

  /**
   * Updates the canvas size and recreates the surface.
   * @param width New width in pixels.
   * @param height New height in pixels.
   */
  void updateSize(int width, int height);

  /**
   * Updates the zoom scale and content offset for the display list.
   * @param zoom The current zoom scale factor.
   * @param offsetX The horizontal content offset in pixels.
   * @param offsetY The vertical content offset in pixels.
   */
  void updateZoomScaleAndOffset(float zoom, float offsetX, float offsetY);

  /**
   * Notifies that zoom gesture has ended. This will restore tile refinement.
   * Note: This is called automatically by internal detection, no need to call manually.
   */
  void onZoomEnd();

  /**
   * Renders the current frame to the canvas. Returns true if the rendering succeeds.
   */
  bool draw();

  /**
   * Returns true if the first frame has been rendered successfully.
   */
  bool firstFrameRendered() const;

  /**
   * Returns the width of the PAGX content in content pixels.
   */
  float contentWidth() const {
    return pagxWidth;
  }

  /**
   * Returns the height of the PAGX content in content pixels.
   */
  float contentHeight() const {
    return pagxHeight;
  }

  /**
   * Returns the width of the canvas in pixels.
   */
  int width() const;

  /**
   * Returns the height of the canvas in pixels.
   */
  int height() const;

  /**
   * Enable or disable performance-based adaptation.
   * Default: true
   */
  void setPerformanceAdaptationEnabled(bool enabled);

  /**
   * Set slow frame threshold in milliseconds.
   * Default: 50.0ms (more lenient than desktop 32ms due to WeChat environment)
   */
  void setSlowFrameThreshold(double thresholdMs);

  /**
   * Set recovery time window in milliseconds.
   * Default: 3000ms (3 seconds, longer than desktop 2s to reduce jitter)
   */
  void setRecoveryWindow(double windowMs);

 private:
  void applyCenteringTransform();

  void updatePerformanceState(double frameDurationMs);

  void updateAdaptiveTileRefinement();

  int calculateTargetTileRefinement(float zoom) const;

  std::shared_ptr<tgfx::Device> device = nullptr;
  std::shared_ptr<tgfx::Surface> surface = nullptr;
  tgfx::DisplayList displayList = {};
  std::shared_ptr<tgfx::Layer> contentLayer = nullptr;
  float pagxWidth = 0.0f;
  float pagxHeight = 0.0f;
  int _width = 0;
  int _height = 0;
  Typesetter typesetter = {};

  // Performance monitoring
  struct FrameRecord {
    double timestampMs = 0.0;
    double durationMs = 0.0;
  };
  std::deque<FrameRecord> frameHistory = {};
  double frameHistoryTotalTime = 0.0;
  bool lastFrameSlow = false;

  // Configuration
  bool enablePerformanceAdaptation = true;
  double slowFrameThresholdMs = 50.0;
  double recoveryWindowMs = 3000.0;
  double zoomInEndTimeoutMs = 300.0;    // Timeout for zoom in (faster refinement)
  double zoomOutEndTimeoutMs = 800.0;   // Timeout for zoom out (slower refinement)
  double upgradeRetryDelayMs = 300.0;   // Delay before retrying upgrade when performance is still slow
  size_t minRecoveryFramesStatic = 20;  // Minimum frames to confirm recovery in static state
  size_t minRecoveryFramesZoomEnd = 10; // Minimum frames to confirm recovery after zoom ends

  // State tracking
  bool hasRenderedFirstFrame = false;

  float lastZoom = 1.0f;
  float zoomStartValue = 1.0f;          // Zoom value at gesture start
  float accumulatedZoomChange = 0.0f;   // Accumulated zoom change during gesture
  bool isZooming = false;
  bool isZoomingIn = false;
  int currentMaxTilesRefinedPerFrame = 1;
  double tryUpgradeTimestampMs = 0.0;
  double lastZoomUpdateTimestampMs = 0.0;
};

}  // namespace pagx
