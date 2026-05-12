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
#include <string>
#include "tgfx/core/Color.h"
#include "tgfx/core/Image.h"
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
   * Replaces the decoded image attached to Image nodes that share the given filePath with a new
   * version and immediately regenerates the vector contents of every affected VectorLayer. Use
   * this to swap a low-resolution thumbnail for a full-resolution version (progressive image
   * loading); the next draw() call uses the new asset without any explicit rebuildLayers().
   *
   * The call is a no-op when buildLayers() has not been invoked (there is no layer tree to
   * update yet). For the first-time load of an image during initial buildLayers() use
   * loadFileDataAsNativeImage() instead -- that entry point only attaches the decoded image
   * without touching any layers, which is what the pre-build path needs.
   *
   * Must be called on the main thread and not from inside draw(); the layer mutation below
   * relies on there being no concurrent render in progress.
   *
   * @param filePath The external file path to match against Image nodes.
   * @param nativeImage A JavaScript-side decoded image object (same contract as
   *                    loadFileDataAsNativeImage).
   * @return True if at least one layer's contents were regenerated as a result of the swap.
   */
  bool upgradeImageFromNative(const std::string& filePath,
                              const emscripten::val& nativeImage);

  /**
   * Returns root-space bounds (canvas pixel coordinates, already accounting for fitScale and
   * the centering offset applied to contentLayer) for every filePath in the input list. Each
   * returned entry has both a unionBounds (axis-aligned union of every referencing layer, used
   * for viewport-intersection tests) and a largestBounds (the single referencing layer with the
   * biggest area, used for focus-distance scoring). filePaths with no matching layer map to
   * null so callers can tell apart "off-canvas" from "no such filePath".
   *
   * Must be called after buildLayers() so the contentLayer has been attached to the
   * displayList; otherwise the result is an empty object. The first call triggers the tgfx
   * layer tree to lazily compute and cache each layer's localBounds (estimated at 50-250ms
   * total for a ~50-image document), so it is cheaper to defer the call to a moment when the
   * user is already waiting (e.g. the first idle window after the initial frame has rendered)
   * rather than running it synchronously alongside buildLayers().
   *
   * @param filePathList A JavaScript Array of file path strings.
   * @return A JavaScript object keyed by filePath. Each value is either
   *         { unionBounds: {x,y,w,h}, largestBounds: {x,y,w,h} } or null.
   */
  emscripten::val getImageBounds(const emscripten::val& filePathList) const;

  /**
   * Returns the ImagePattern usage metadata for every externally referenced image. The return
   * value lets the JS layer size thumbnail downloads and compute display scale without having
   * to re-parse the PAGX XML on its own; everything needed (original image dimensions, target
   * node dimensions, scale mode, scaleFactor for tiled patterns) is forwarded from the
   * ImagePattern::customData entries already populated by PAGXImporter.
   *
   * Must be called after parsePAGX() so customData has been filled in; calling earlier
   * returns an empty array. Data URIs and inline data-backed Image nodes are excluded -- only
   * Images with a non-empty filePath participate.
   *
   * @return A JavaScript Array of entries:
   *         [ { filePath, origWidth, origHeight,
   *             usages: [ { nodeWidth, nodeHeight, scaleMode, scaleFactor }, ... ] }, ... ]
   */
  emscripten::val getImageMetadata() const;

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
   * Resets internal "first frame rendered" / fit snapshot state so callers can wait for a
   * fresh first frame after applying gesture state on a newly loaded document. Use after
   * parsePAGX + buildLayers + applyGestureState to ensure the upcoming first frame is
   * captured against the final view state, not against the transient post-buildLayers
   * default zoom/offset.
   */
  void resetForFreshCapture();

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
   * Enables/disables gesture-freeze rendering. When enabled, draw() stops calling
   * displayList.render() and instead blits the last fully-rendered frame to the surface,
   * applying the delta between the snapshot's zoom/offset and the current ones. This
   * eliminates per-frame shape retriangulation and slow-op execution for the duration of a
   * user pan/zoom, at the cost of visual blur when zooming beyond the snapshot scale.
   *
   * The caller (JavaScript gesture layer) is responsible for calling setGestureActive(true)
   * on zoomStart/panStart and setGestureActive(false) on zoomEnd/panEnd. The first draw()
   * after disabling freezes performs a normal render, producing a "refocus" frame that is
   * expected to be slow; subsequent idle frames are unaffected.
   *
   * No-op if a snapshot is unavailable (e.g. before the first frame has rendered). Freeze
   * never latches across documents: parsePAGX/buildLayers clear the cached snapshot.
   */
  void setGestureActive(bool active);

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

  // Shared contain-mode fit scale used by both the content matrix and the JS-facing content
  // transform. Keeping a single source of truth prevents comment pins from drifting relative to
  // the rendered content.
  float computeFitScale() const;

  void updatePerformanceState(double frameDurationMs);

  void updateAdaptiveTileRefinement();

  int calculateTargetTileRefinement(float zoom) const;

  std::shared_ptr<tgfx::Device> device = nullptr;
  std::shared_ptr<tgfx::Surface> surface = nullptr;
  tgfx::DisplayList displayList = {};
  std::shared_ptr<tgfx::Layer> contentLayer = nullptr;
  std::shared_ptr<PAGXDocument> document = nullptr;

  // Gesture-freeze pipeline. During active pan/zoom we bypass displayList.render() and
  // composite two snapshots onto the surface: fitSnapshot (whole document at zoom=1,
  // blurry when zoomed in but always fills the viewport) + cachedSnapshot (last rendered
  // viewport, sharp at its capture zoom). Both cleared on parsePAGX/updateSize.
  bool gestureActive = false;
  std::shared_ptr<tgfx::Image> cachedSnapshot = nullptr;
  float snapshotZoom = 1.0f;
  tgfx::Point snapshotOffset = {};
  // 单调递增计数器：每捕获一次 cached 自增 1，parsePAGX 时也自增。用于诊断
  // composite 路径实际 blit 的是不是最新的快照（防止某处持有旧的 shared_ptr）。
  uint32_t cachedVersion = 0;
  std::shared_ptr<tgfx::Image> fitSnapshot = nullptr;
  float fitSnapshotZoom = 1.0f;
  tgfx::Point fitSnapshotOffset = {};
  uint32_t fitVersion = 0;
  // fit 的超采样倍数：1 = 同分辨率；>1 = N 倍像素密度，用于超宽/超长文档清晰度。
  float fitSnapshotPixelScale = 1.0f;
  // Idle token for the zoom-out fast path: once draw() has painted the current view from
  // fitSnapshot alone, further idle frames short-circuit. Cleared by any view change.
  bool zoomedOutFrameSettled = false;
  // 距离最近一次手势结束的时间戳（emscripten_get_now ms）。用于让 cachedSnapshot 的
  // 刷新延后到用户真正停下来再做：手势密集时复用上一次稳定 capture 的 cached，避免
  // capture 抓到含 tile fallback 的过渡画面、避免连续 full render 的内存抖动。
  // 0 表示从未发生过手势（首帧前），首帧 capture 不受门控限制。
  double lastGestureEndMs = 0.0;
  // Session owns the LayerBuilder state so upgradeImageFromNative() can regenerate a subset of
  // the layer tree when a higher-resolution asset replaces the thumbnail attached during the
  // initial buildLayers() pass. Destroyed on every parsePAGX() so old build state never leaks
  // across documents.
  std::unique_ptr<LayerBuilderSession> builderSession = nullptr;
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

  // Accumulated decoded image accounting (debug only). Reset in parsePAGX(). Counts each call
  // to loadFileDataAsNativeImage / upgradeImageFromNative; pixel total approximates the GPU
  // texture footprint at RGBA8 (×4 bytes), useful for correlating wasm heap with image load.
  uint64_t imageDecodedPixelTotal = 0;
  uint32_t imageDecodedCount = 0;
  // Histogram of decoded image sizes by pixel area, indexed by bucket: 0:<10K, 1:<100K,
  // 2:<500K, 3:<1M, 4:<2M, 5:>=2M. Lets us tell at a glance whether the document is dominated
  // by tiny icons or by large photos without scrolling through per-image log lines.
  uint32_t imageSizeBuckets[6] = {0, 0, 0, 0, 0, 0};

  float lastZoom = 1.0f;
  bool isZooming = false;
  // isZoomingIn is derived per-frame from a single (zoom > lastZoom) comparison and is only
  // consumed by the in/out timeout split in draw() (ZOOM_IN/OUT_END_TIMEOUT_MS). The throttle
  // itself is no longer driven from here -- tgfx infers direction internally.
  bool isZoomingIn = false;
  int currentMaxTilesRefinedPerFrame = 1;
  double tryUpgradeTimestampMs = 0.0;
  double lastZoomUpdateTimestampMs = 0.0;
};

}  // namespace pagx
