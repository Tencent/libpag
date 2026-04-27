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
#include <memory>
#include "tgfx/core/Color.h"
#include "tgfx/gpu/Recording.h"
#include "tgfx/layers/DisplayList.h"
#include "pagx/FontConfig.h"
#include "pagx/PAGXDocument.h"
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
   * This is a convenience method equivalent to calling parsePAGX() followed by buildLayers().
   * @param pagxData PAGX file data as a JavaScript Uint8Array.
   */
  void loadPAGX(const emscripten::val& pagxData);

  /**
   * Parses a PAGX file from the given data without building the layer tree. After parsing, call
   * getExternalFilePaths() to retrieve external resources, load them with loadFileData(), then
   * call buildLayers() to finalize rendering.
   * @param pagxData PAGX file data as a JavaScript Uint8Array.
   */
  void parsePAGX(const emscripten::val& pagxData);

  /**
   * Returns external file paths referenced by Image nodes that have no embedded data. Data URIs
   * are excluded. Call this after parsePAGX() to determine which files need to be fetched.
   */
  std::vector<std::string> getExternalFilePaths() const;

  /**
   * Loads external file data for an Image node matching the given file path. The data is embedded
   * into the matching node so the renderer uses it directly instead of file I/O.
   * @param filePath The external file path to match against Image nodes.
   * @param fileData The file content as a JavaScript Uint8Array.
   * @return True if a matching Image node was found and its data was loaded successfully.
   */
  bool loadFileData(const std::string& filePath, const emscripten::val& fileData);

  /**
   * Attaches an already-decoded native image to Image nodes matching the given file path. The
   * native image must be a JavaScript object tgfx's web NativeCodec understands (typically an
   * OffscreenCanvas produced by drawing a wx.createImage() result). This lets callers perform
   * asynchronous decoding on the host side (e.g. via the mini-program's native webp decoder) and
   * hand the renderer an already-decoded asset, bypassing the wasm libwebp path entirely.
   *
   * Unlike loadFileData(), this method can be called AFTER buildLayers(). When it is called
   * post-build, any VectorLayers that reference the image are rebuilt in place so the new asset
   * shows up on the next draw(); shapes whose fill was empty because the image had not yet
   * arrived become visible at that point. This is the primary mechanism for progressive image
   * loading on WeChat.
   *
   * @param filePath   The external file path to match against Image nodes.
   * @param nativeImage  A JavaScript-side decoded image object.
   * @return True if at least one matching Image node was found and attached.
   */
  bool loadFileDataAsNativeImage(const std::string& filePath,
                                 const emscripten::val& nativeImage);

  /**
   * Builds the layer tree from the previously parsed document. Call this after parsePAGX() and
   * any loadFileData() calls to finalize the rendering content.
   */
  void buildLayers();

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
   * Sets the bounds origin of the PAGX content relative to the cocraft canvas origin. This
   * overrides any values read from the document's customData. The values can be negative.
   * @param x The x coordinate of the content bounds origin in cocraft canvas coordinates.
   * @param y The y coordinate of the content bounds origin in cocraft canvas coordinates.
   */
  void setBoundsOrigin(float x, float y);

  /**
   * Returns the content transform parameters needed for mapping cocraft canvas coordinates to
   * canvas pixel positions. The returned JavaScript object contains:
   * - boundsOriginX/Y: PAGX content origin in cocraft canvas coordinates.
   * - fitScale: scale factor applied to fit PAGX content into the canvas (contain mode).
   * - centerOffsetX/Y: pixel offset for centering the scaled content in the canvas.
   *
   * Usage: baseX = (cocraftX - boundsOriginX) * fitScale + centerOffsetX
   */
  emscripten::val getContentTransform() const;

  /**
   * Looks up a node by ID and returns its position relative to the canvas.
   * The position is read from the node's "page-offset" custom data in "x,y" format.
   * @param nodeId The unique identifier of the node to look up.
   * @return A JavaScript object with { found: boolean, x: number, y: number }.
   *         If the node is not found or has no position data, found is false and x/y are 0.
   */
  emscripten::val getNodePosition(const std::string& nodeId) const;

  /**
   * Returns the width of the canvas in pixels.
   */
  int width() const;

  /**
   * Returns the height of the canvas in pixels.
   */
  int height() const;

 private:
  void applyCenteringTransform();

  void applyDocumentCustomData();

  void updatePerformanceState(double frameDurationMs);

  void updateAdaptiveTileRefinement();

  int calculateTargetTileRefinement(float zoom) const;

  std::shared_ptr<tgfx::Device> device = nullptr;
  std::shared_ptr<tgfx::Surface> surface = nullptr;
  tgfx::DisplayList displayList = {};
  std::shared_ptr<tgfx::Layer> contentLayer = nullptr;
  std::shared_ptr<PAGXDocument> document = nullptr;
  float pagxWidth = 0.0f;
  float pagxHeight = 0.0f;
  int _width = 0;
  int _height = 0;
  FontConfig fontConfig = {};

  // Background state from the PAGX document
  bool backgroundVisible = false;
  tgfx::Color backgroundTGFXColor = tgfx::Color::FromRGBA(245, 245, 245);

  // Performance monitoring
  struct FrameRecord {
    double timestampMs = 0.0;
    double durationMs = 0.0;
  };
  std::deque<FrameRecord> frameHistory = {};
  double frameHistoryTotalTime = 0.0;
  bool lastFrameSlow = false;

  // Bounds origin: PAGX content bounding box top-left relative to cocraft canvas origin
  float _boundsOriginX = 0.0f;
  float _boundsOriginY = 0.0f;
  bool boundsOriginOverridden = false;

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
