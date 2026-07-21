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

#include <emscripten/bind.h>
#include <deque>
#include "pag/gpu.h"
#include "pagx/FontConfig.h"
#include "pagx/PAGAnimation.h"
#include "pagx/PAGScene.h"
#include "pagx/PAGSurface.h"
#include "pagx/PAGTimeline.h"
#include "pagx/PAGXDocument.h"
#include "pagx/types/Color.h"
#include "tgfx/core/Surface.h"
#include "tgfx/gpu/Recording.h"
#include "tgfx/gpu/opengl/webgl/WebGLWindow.h"

namespace pagx {

class PAGXView {
 public:
  explicit PAGXView(const std::string& canvasID);

  void registerFonts(const emscripten::val& fontVal, const emscripten::val& emojiFontVal);

  void loadPAGX(const emscripten::val& pagxData);

  void parsePAGX(const emscripten::val& pagxData);

  std::vector<std::string> getExternalFilePaths() const;

  bool loadFileData(const std::string& filePath, const emscripten::val& fileData);

  void buildLayers();

  void updateSize();

  void updateZoomScaleAndOffset(float zoom, float offsetX, float offsetY);

  /**
   * Sets a solid background color. When set, the solid color will be used instead of the default
   * checkerboard pattern.
   * @param red Red component (0.0 - 1.0).
   * @param green Green component (0.0 - 1.0).
   * @param blue Blue component (0.0 - 1.0).
   * @param alpha Alpha component (0.0 - 1.0).
   */
  void setBackgroundColor(float red, float green, float blue, float alpha);

  /**
   * Clears the custom background color and reverts to the default checkerboard pattern.
   */
  void clearBackgroundColor();

  void draw();

  float contentWidth() const {
    return pagxWidth;
  }

  float contentHeight() const {
    return pagxHeight;
  }

  // Playback control methods
  void play();
  void pause();
  bool isPlaying() const;
  int64_t currentTimeMicros() const;
  int64_t durationMicros() const;
  float frameRate() const;
  void setCurrentTimeMicros(int64_t micros);
  void setLoop(bool loop);
  bool isLoop() const;

 private:
  void updateContentTransform();
  void applyDisplayTransform();
  void applySceneDisplayOptions();
  bool ensureWindow();
  void syncSurfaceSize(int canvasWidth, int canvasHeight);
  void advanceTimelines(double frameStartMs);
  void onZoomEnd();
  void updatePerformanceState(double frameDurationMs);
  void updateAdaptiveTileRefinement();
  int calculateTargetTileRefinement(float zoom) const;

  std::string canvasID = {};
  std::shared_ptr<tgfx::WebGLWindow> window = nullptr;
  std::shared_ptr<tgfx::Surface> tgfxSurface = nullptr;
  std::shared_ptr<PAGSurface> pagSurface = nullptr;
  std::shared_ptr<PAGScene> scene = nullptr;
  std::shared_ptr<PAGTimeline> defaultTimeline = nullptr;
  // The default timeline downcast to PAGAnimation when it is an animation. The playback value
  // methods (duration/frameRate/currentTime/setCurrentTime) live on PAGAnimation, not on the
  // PAGTimeline base, so they are routed through this pointer. Null when there is no default
  // timeline or it is not an animation.
  std::shared_ptr<PAGAnimation> defaultAnimation = nullptr;
  // Playback state maintained by the view itself. The timeline API no longer carries play/pause
  // state, and PAGComposition::pauseTimeline cannot gate the top-level timeline driven here, so the
  // view gates advancement on this flag.
  bool playing = true;
  // Player-level loop switch that overrides the file's loop mode at cycle boundaries: the engine
  // still drives in-cycle motion (including PingPong mirroring), but whether a completed cycle
  // repeats or stops is decided here instead of by the file's LoopMode. Defaults to looping.
  bool loopEnabled = true;
  std::unique_ptr<tgfx::Recording> lastRecording = nullptr;
  double lastAnimationTimeMs = -1.0;
  int lastSurfaceWidth = 0;
  int lastSurfaceHeight = 0;
  bool presentImmediately = true;
  float pagxWidth = 0.0f;
  float pagxHeight = 0.0f;
  float contentScale = 1.0f;
  float contentOffsetX = 0.0f;
  float contentOffsetY = 0.0f;
  float userZoom = 1.0f;
  float userOffsetX = 0.0f;
  float userOffsetY = 0.0f;
  FontConfig fontConfig = {};
  std::shared_ptr<PAGXDocument> document = nullptr;

  bool useCustomBackgroundColor = false;
  Color customBackgroundColor = {};

  // Performance monitoring
  struct FrameRecord {
    double timestampMs = 0.0;
    double durationMs = 0.0;
  };
  std::deque<FrameRecord> frameHistory = {};
  double frameHistoryTotalTime = 0.0;
  bool lastFrameSlow = false;

  // Zoom state tracking
  float lastZoom = 1.0f;
  float accumulatedZoomChange = 0.0f;
  bool isZooming = false;
  bool isZoomingIn = false;
  int currentMaxTilesRefinedPerFrame = 1;
  double tryUpgradeTimestampMs = 0.0;
  double lastZoomUpdateTimestampMs = 0.0;
};

}  // namespace pagx
