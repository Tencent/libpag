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
#include "LayerBuilder.h"
#include "TextLayout.h"
#include "pagx/PAGXDocument.h"
#include "tgfx/gpu/Recording.h"
#include "tgfx/gpu/opengl/webgl/WebGLWindow.h"
#include "GridBackground.h"
#include "tgfx/layers/DisplayList.h"

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

  void draw();

  float contentWidth() const {
    return pagxWidth;
  }

  float contentHeight() const {
    return pagxHeight;
  }

 private:
  void applyCenteringTransform();
  void onZoomEnd();
  void updatePerformanceState(double frameDurationMs);
  void updateAdaptiveTileRefinement();
  int calculateTargetTileRefinement(float zoom) const;

  std::string canvasID = {};
  std::shared_ptr<tgfx::Window> window = nullptr;
  tgfx::DisplayList displayList = {};
  std::shared_ptr<tgfx::Layer> contentLayer = nullptr;
  std::unique_ptr<tgfx::Recording> lastRecording = nullptr;
  int lastSurfaceWidth = 0;
  int lastSurfaceHeight = 0;
  bool presentImmediately = true;
  float pagxWidth = 0.0f;
  float pagxHeight = 0.0f;
  TextLayout textLayout = {};
  std::shared_ptr<PAGXDocument> document = nullptr;

  // Background layer cache
  std::shared_ptr<GridBackgroundLayer> backgroundLayer = nullptr;
  int lastBackgroundWidth = 0;
  int lastBackgroundHeight = 0;
  float lastBackgroundDensity = 0.0f;

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
