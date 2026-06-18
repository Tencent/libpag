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

#include <cstdint>
#include <deque>
#include <emscripten/val.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "tgfx/core/Color.h"
#include "tgfx/core/Image.h"
#include "tgfx/gpu/Recording.h"
#include "tgfx/layers/DisplayList.h"
#include "pagx/FontConfig.h"
#include "pagx/PAGXDocument.h"
#include "utils/StringParser.h"
#include "LayerBuilder.h"

namespace tgfx {
class Context;
}

namespace pagx {

/**
 * Quality tier for an externally supplied native image attached via
 * PAGXView::attachNativeImage(). The two tiers map to distinct slots on each Image node and obey
 * different lifecycle rules:
 *
 *   - Thumbnail: low-resolution preview (typically <= 256x256). Written to Image::thumbnailImage.
 *     Acts as a fallback when the full-resolution texture is missing or has been evicted, so the
 *     affected fill area never goes blank during progressive loading or memory pressure.
 *     Thumbnails are not subject to per-frame LRU eviction; they are only dropped when an
 *     incoming attachNativeImage() would push the thumbnail bucket over its byte budget, in
 *     which case the oldest thumbnails are silently dropped to make room.
 *
 *   - Full: full-resolution texture. Written to Image::decodedImage. Subject to per-frame LRU
 *     eviction once the full bucket exceeds its byte budget; evicted paths fire the
 *     onTextureEvict callback so the host can drop its own cached copy. Layers whose full
 *     texture has been evicted automatically fall back to the corresponding thumbnailImage on
 *     the next draw, keeping the fill visible until a new full texture arrives.
 */
enum class ImageQuality : uint8_t {
  Thumbnail = 0,
  Full = 1,
};

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
   * Releases every backend texture this view still owns. Run while the GL context the view
   * was bound to is still current — the JS-side destroy() handles makeCurrent/clearCurrent
   * around nativeView.delete(), so this destructor sees a valid GL state and can call
   * gl.deleteTexture directly.
   */
  ~PAGXView();

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
  bool loadFileData(const emscripten::val& filePathVal, const emscripten::val& fileData);

  /**
   * Attaches a host-decoded native image (typically an OffscreenCanvas) to Image nodes matching
   * the given file path under a specific quality tier. The image is queued for GPU texture
   * upload on the next draw() call (inside lockContext); the OffscreenCanvas can be released as
   * soon as this method returns. Affected layers are rebuilt in place after the upload so the
   * new asset shows up on the same frame.
   *
   * The quality argument selects which slot on the Image node receives the result:
   *   - ImageQuality::Thumbnail writes to Image::thumbnailImage (fallback rendering).
   *   - ImageQuality::Full writes to Image::decodedImage (primary rendering).
   *
   * Both quality tiers may be attached for the same filePath at different times; they live in
   * distinct caches and do not overwrite each other. Can be called both before and after
   * buildLayers(); the upload and rebuild happen at draw() time regardless.
   *
   * @param filePathVal The external file path to match against Image nodes.
   * @param nativeImage A JavaScript-side decoded image source (OffscreenCanvas, ImageBitmap, or
   *                    similar). Must be truthy.
   * @param qualityRaw The image quality as the integer value of ImageQuality, passed through as
   *                   a plain int because the JS binding cannot pass enum values directly.
   * @return True if the request was queued for upload.
   */
  bool attachNativeImage(const emscripten::val& filePathVal, const emscripten::val& nativeImage,
                         int qualityRaw);

  /**
   * Registers a JavaScript handler that receives backend-texture lifecycle events from this
   * view. The handler object is expected to expose two methods (both optional; missing methods
   * are silently skipped):
   *
   *   - onTextureRequest(filePath: string): called for paths whose full-quality decodedImage
   *     was previously attached and then evicted by the LRU sweep, until a new
   *     attachNativeImage(Full) lands. Initial-attachment paths (decodedImage never present)
   *     are NOT surfaced here — those are owned by the host's own progressive image flow,
   *     bootstrapped from getExternalFilePaths(). The host should respond by downloading +
   *     decoding the asset and calling attachNativeImage(filePath, source, ImageQuality.Full);
   *     doing so atomically clears the in-flight marker for that path so further draws will
   *     re-request only if the asset is evicted again later. To prevent per-frame spam, each
   *     evicted path is only emitted once until the host responds via attachNativeImage(Full);
   *     hosts that drop a request without responding will not see it re-fired (and the renderer
   *     will keep falling back to thumbnailImage indefinitely).
   *
   *   - onTextureEvict(filePaths: string[]): called once per draw with every full-quality path
   *     that was just dropped by the LRU sweep. The host can use this hint to release the
   *     corresponding bytes from its own cache. Thumbnail evictions never fire this callback.
   *
   * Passing a falsy value (undefined / null) clears any previously registered handler. The
   * registration survives parsePAGX(), so callers only need to set it once after MakeFrom().
   */
  void setTextureEventHandler(const emscripten::val& handler);

  /**
   * Reports whether the full-quality cache has crossed its hard cap. Hosts driving a
   * progressive thumbnail-to-full upgrade flow should consult this at the top of every
   * upgrade pass and skip new uploads while it returns true. Reactive responses to
   * onTextureRequest are still safe because each one replaces an entry that has just been
   * evicted, leaving total bytes net-flat.
   *
   * The flag is purely a function of externalTexturesTotalBytes vs fullBudgetHardCap; it
   * clears automatically as soon as a viewport change lets the LRU drain enough off-viewport
   * entries to fall back below the cap. Hosts do not need to subscribe to a separate
   * "memory recovered" callback: the next gesture-triggered evaluate() naturally re-checks
   * this method and resumes upgrades.
   */
  bool isFullBudgetSaturated() const;



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
   * Records the original (authoring-time) pixel dimensions of an external image referenced by
   * filePath. New PAGX exports bake their ImagePattern.matrix in the original-image pixel
   * coordinate system, so when the host attaches a downscaled CDN variant via
   * attachNativeImage() the renderer needs to know the original dimensions to post-correct the
   * baked matrix against the actual attached pixels.
   *
   * Call this once per external image after parsePAGX() and before buildLayers() / the first
   * attachNativeImage() so ResolveImagePatternMatricesByFilePath sees the original size and
   * can apply the diag(origW/actualW, origH/actualH) post-scale to keep visual placement
   * stable across thumbnail/full quality swaps.
   *
   * Subsequent calls overwrite the previous record. parsePAGX() clears the entire table.
   *
   * @param filePath The external file path matching pagx::Image::filePath. Must be non-empty.
   * @param width    Original image width in pixels (> 0).
   * @param height   Original image height in pixels (> 0).
   */
  void setImageOriginalSize(const emscripten::val&  filePathVal, float width, float height);

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
   * Toggles the fitSnapshot fast path. When enabled (default), draw() blits a cached
   * fit-to-canvas snapshot during gestures and at zoom <= 1.02 to avoid the cost of a full
   * displayList.render(). When disabled, draw() always runs a full render and never captures
   * a fit snapshot, trading per-frame rendering cost for first-frame clarity and freshness
   * under progressive image loading.
   *
   * Disabling drops any existing fit snapshot and resets the zoom-out idle short-circuit so
   * the next draw() runs a full render. The setting persists across parsePAGX/buildLayers.
   */
  void setSnapshotEnabled(bool enabled);

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
  // composite fitSnapshot (whole document at zoom=1, blurry when zoomed in but always fills
  // the viewport) onto the surface. Cleared on parsePAGX/updateSize.
  bool gestureActive = false;
  std::shared_ptr<tgfx::Image> fitSnapshot = nullptr;
  // Supersampling factor of the fit snapshot: 1 = same resolution; >1 = N× pixel density, used to
  // keep ultra-wide / ultra-tall documents sharp.
  float fitSnapshotPixelScale = 1.0f;
  // Idle token for the zoom-out fast path: once draw() has painted the current view from
  // fitSnapshot alone, further idle frames short-circuit. Cleared by any view change.
  bool zoomedOutFrameSettled = false;
  // Master switch for the fitSnapshot fast path. When false, draw() always performs a full
  // displayList.render() and never captures a fit snapshot. Toggled via setSnapshotEnabled().
  bool snapshotEnabled = true;
  // Timestamp (emscripten_get_now ms) of the most recent gesture end. Used to defer the refresh of
  // fitSnapshot until the user truly stops: during dense gestures it reuses the last stable captured
  // snapshot, avoiding capturing a transitional frame that contains tile fallback and avoiding the
  // memory churn of consecutive full renders.
  // 0 means a gesture has never happened (before the first frame); the first-frame capture is not gated.
  double lastGestureEndMs = 0.0;
  // Session owns the LayerBuilder state so attachNativeImage() can regenerate a subset of the
  // layer tree when a higher-resolution asset replaces the thumbnail attached during the initial
  // buildLayers() pass. Destroyed on every parsePAGX() so old build state never leaks across
  // documents.
  std::unique_ptr<LayerBuilderSession> builderSession = nullptr;

  // Pending GPU upload requested by attachNativeImage(). Each entry parks the JS-side decoded
  // image (typically an OffscreenCanvas) until the next draw() so the actual texImage2D call
  // happens inside lockContext, where the WebGL context is bound to the renderer's GL state.
  // The JS reference held in nativeImage keeps the source alive across the gap between the
  // attach call and the draw that consumes it. width/height/sizeBytes are sampled once at
  // queue time (single wasm↔JS round-trip per attach) so the budget check and the eventual
  // flush can both reuse them without bouncing back through emscripten::val.
  struct PendingUpload {
    std::string filePath;
    emscripten::val nativeImage;
    ImageQuality quality = ImageQuality::Full;
    int width = 0;
    int height = 0;
    uint64_t sizeBytes = 0;
  };
  std::vector<PendingUpload> pendingUploads = {};
  // Running sum of sizeBytes for thumbnail-quality entries currently in pendingUploads. Lets
  // attachNativeImage(Thumbnail) compare against thumbnailBudget in O(1) without re-walking
  // the queue (and without bouncing into JS for width/height). Maintained alongside every
  // push_back / clear / swap that touches pendingUploads.
  uint64_t pendingThumbnailBytes = 0;
  // Mirror of pendingThumbnailBytes for full-quality entries. Used by attachNativeImage(Full)
  // to project the post-flush total before queuing so we can run a proactive
  // enforceFullBudget() pass and avoid the "attach 8 fulls → enforceFullBudget evicts in
  // bulk after flush" two-phase oscillation that wasted both upload bandwidth and triggered
  // visible thumbnail-fallback flicker on the freshly evicted paths.
  uint64_t pendingFullBytes = 0;

  // Backing storage for backend-texture images uploaded by the host. Each entry pins one
  // tgfx::Image wrapping a GL texture that this view owns directly — tgfx is told the texture
  // is non-adopted (Image::MakeFrom, not MakeAdopted) so the underlying TextureView is
  // dropped from the ResourceCache the moment our shared_ptr releases, instead of being held
  // back in the scratch pool to wait for an LRU sweep. textureId stores the GL.textures index
  // returned by createBackendTexture so the eviction path can hand it to destroyBackendTexture
  // and free the GPU memory in the same draw that drops the entry. sizeBytes is the
  // approximate GPU footprint at RGBA8 plus a 1/3 mipmap multiplier; it is used for budget
  // accounting. LRU rank is no longer carried per-entry: enforceFullBudget() ranks candidates
  // on demand by their distance to the viewport center, and approximating "actively used" by
  // "still attached" turned out to mark every entry as equally fresh, which silently disabled
  // LRU pressure entirely.
  struct ExternalTextureEntry {
    std::shared_ptr<tgfx::Image> image = nullptr;
    uint64_t sizeBytes = 0;
    unsigned textureId = 0;
  };
  // Full-quality cache: subject to per-frame LRU eviction once externalTexturesTotalBytes
  // exceeds fullBudget. Eviction fires the onTextureEvict callback.
  std::unordered_map<std::string, ExternalTextureEntry> externalTextures = {};
  uint64_t externalTexturesTotalBytes = 0;
  // Thumbnail cache: not evicted on a per-frame basis. Only contracts when an incoming
  // attachNativeImage() would push thumbnailTexturesTotalBytes past thumbnailBudget, at which
  // point the oldest thumbnails are silently dropped. Thumbnails always survive when the full
  // bucket evicts, so the fill area can keep showing the blurry preview.
  std::unordered_map<std::string, ExternalTextureEntry> thumbnailTextures = {};
  uint64_t thumbnailTexturesTotalBytes = 0;
  // Tunable byte budgets. Sized against the 512MB tgfx cacheLimit on iOS WeChat (raising
  // tgfx beyond 512MB triggered OOM kills in production, so the limit is fixed). The full
  // bucket runs a two-tier policy:
  //   - fullBudget (256MB): soft cap. The LRU starts evicting off-viewport entries as soon as
  //     totalBytes crosses this line, but uploads are still allowed: the buffer between
  //     soft and hard caps lets visible-but-not-yet-evicted entries cohabit with newly
  //     attached fulls without immediately stalling the upgrade pipeline. Sized so the
  //     remaining cacheLimit headroom (512 - 256 - 56 = 200MB) absorbs tgfx's tile/surface
  //     cache growth during zoom-in: production logs showed the prior 320MB cap leaving only
  //     ~136MB for tile cache, which got exhausted (memoryUsage hit 509MB on a 512MB cap)
  //     when zooming into image-heavy PAGX documents.
  //   - fullBudgetHardCap (384MB): hard cap. Once totalBytes reaches this line the host's
  //     evaluate() loop is expected to stop issuing new full-quality upgrades (queried via
  //     isFullBudgetSaturated()). Reactive responses to onTextureRequest still proceed —
  //     those are 1:1 replacements for evicted entries that the renderer is already
  //     referencing, so they can't push totalBytes any higher than it was before the evict.
  //     Recovery is automatic: a viewport change causes the LRU to evict newly off-viewport
  //     entries, totalBytes drops back below the hard cap, and the saturation flag clears.
  //
  // 56MB for thumbnails covers ~168 256x256 previews — more than any realistic document
  // references. When thumbnailBudget is exhausted the oldest thumbnails are silently
  // dropped without a host callback; affected layers may render blank until a fresh
  // full-quality attach lands.
  size_t fullBudget = 256ULL * 1024 * 1024;
  size_t fullBudgetHardCap = 384ULL * 1024 * 1024;
  size_t thumbnailBudget = 56ULL * 1024 * 1024;
  // Monotonic counter incremented at the end of every draw(). Used by the eviction-overflow
  // warning to throttle log output to once per 60 frames.
  uint64_t currentFrameIndex = 0;
  // Frame index of the last "fullBudget exceeded with all paths viewport-protected" warning.
  // Used by enforceFullBudget to throttle the log to once per 60 frames so a sustained
  // overflow does not flood the console.
  uint64_t lastFullBudgetOverflowWarnFrame = 0;

  // JS-side event handler registered via setTextureEventHandler(). Default-constructed val is
  // undefined; we treat both undefined and null as "no handler". The handler object is held
  // by value here; emscripten::val keeps a reference into the JS heap that prevents garbage
  // collection until this PAGXView is destroyed (or the handler is replaced).
  emscripten::val textureEventHandler = emscripten::val::undefined();
  // Paths for which onTextureRequest has been emitted but no matching attachNativeImage(Full)
  // call has resolved yet. Prevents the per-draw scan from re-requesting the same asset on
  // every frame while a download is in flight. Cleared on parsePAGX() so a new document
  // starts with a fresh request budget.
  std::unordered_set<std::string> inFlightFullRequests = {};

  // Paths whose full-quality decodedImage was previously attached and then evicted by the LRU
  // sweep. The renderer uses this set to distinguish "never attached" from "attached and
  // evicted" — only the latter should drive new onTextureRequest events, because the former is
  // already covered by the host's initial getExternalFilePaths() / attachNativeImage flow and
  // surfacing it again would re-request the asset every frame during the perfectly normal
  // pre-attach window. Cleared on parsePAGX() and on each successful attachNativeImage(Full).
  std::unordered_set<std::string> evictedFullPaths = {};

  // Original (authoring-time) pixel dimensions for external images, populated by the host
  // through setImageOriginalSize(). New PAGX exports bake ImagePattern.matrix in the
  // original-image pixel coordinate system; when the host attaches a downscaled CDN variant
  // (thumbnail or scaled full) the renderer reads this map to post-correct the baked matrix
  // against the attached image's actual pixel dimensions, keeping visual placement stable
  // regardless of which CDN variant landed.
  //
  // Cleared on parsePAGX(); the host is responsible for repopulating it before buildLayers()
  // and any subsequent attachNativeImage() so resolve passes have the data they need.
  std::unordered_map<std::string, std::pair<float, float>> imageOriginalSizes = {};

  // GL.textures indices retired during the current draw (eviction, replacement upload, or
  // document reset) but not yet handed to gl.deleteTexture. drainPendingTextureDeletes()
  // empties this list at the very end of draw(), after flush + submit have run, so the
  // GL command buffer that used these textures has already been built and submitted to the
  // driver. WebGL guarantees in-flight commands keep working with a deleted texture id; only
  // future bind calls fail (which is exactly what we want — the entry is already gone from
  // externalTextures / thumbnailTextures and no further sampling can reference it).
  std::vector<unsigned> pendingTextureDeletes = {};

  // Records textureId for retirement at the next drainPendingTextureDeletes(). 0 is treated
  // as "no id" and silently dropped so callers do not need to special-case empty entries.
  void retireTextureId(unsigned textureId);

  // Hands every queued texture id to JS gl.deleteTexture and clears pendingTextureDeletes.
  // Must run on the GL thread inside an active lockContext window so the call sequence races
  // with no other GL work; called twice in draw() (once per lockContext branch) so the GPU
  // memory backing an evicted texture is freed in the same frame it was retired.
  void drainPendingTextureDeletes();

  // Drains pendingUploads inside lockContext. For each request: registers a new GL texture id
  // via the JS helper, wraps it in a non-adopted tgfx::Image (Image::MakeFrom) so tgfx never
  // claims ownership of the GL texture, writes it to the matching Image node, and bookkeeps
  // the entry into externalTextures or thumbnailTextures by quality. The wasm side keeps the
  // textureId on the entry so retireTextureId() can reclaim the GPU memory directly when the
  // entry is later evicted or replaced. Called from draw() after lockContext() succeeds and
  // before any rendering happens.
  void flushPendingUploads(tgfx::Context* context);

  // Evicts full-quality entries until externalTexturesTotalBytes drops at or below fullBudget.
  // Candidates are restricted to paths whose union bounds fall outside the (1.2x-expanded)
  // viewport — visible paths are hard-protected so the user never sees a texture they're
  // looking at disappear. Off-viewport candidates are sorted by distance from the viewport
  // center descending (farther first), modeled on tgfx's tiled rasterization which uses the
  // same focal-distance ordering. For each evicted path: removes the cache entry (releasing
  // the adopted backend texture), clears the matching Image node's decodedImage, and rebuilds
  // the affected layers so the next frame falls back to thumbnailImage.
  //
  // When every cache entry is inside the protected viewport the budget is allowed to overflow
  // temporarily — evicting a visible texture would only cause it to be re-fetched and re-
  // attached the same frame, oscillating against itself. A 60-frame-throttled warning is
  // logged in this case so sustained overflow stays visible without flooding the console.
  void enforceFullBudget();

  // Computes the current viewport rectangle in displayList.root() coordinates. The result is
  // suitable for direct intersection with the bounds returned by Layer::getBounds(rootLayer).
  // Returns an empty rect when the canvas is unavailable or the live zoom is non-positive
  // (degenerate transform); callers must treat empty as "no viewport protection possible".
  tgfx::Rect computeViewportInRootCoords() const;

  // Collects the union of layer bounds (in displayList.root() coordinates) for every filePath
  // currently sitting in externalTextures. Used by enforceFullBudget for two consecutive
  // decisions: (1) whether each path intersects the viewport (visibility protection) and (2)
  // how far the off-viewport paths sit from the viewport center (eviction priority). Sharing
  // a single pass keeps the cost of one full bounds-collection low — every getBounds() call
  // is cached by tgfx after the first hit, so repeated invocations across frames are O(1).
  // Paths whose layers all return empty bounds are absent from the result.
  std::unordered_map<std::string, tgfx::Rect> computeFullPathBounds() const;

  // Attempts to free at least neededBytes from the thumbnail cache by evicting entries in
  // hash-map iteration order until the requested amount is freed (or the cache is empty).
  // Returns the number of bytes actually freed; the caller is responsible for re-checking
  // that the upload can now fit. Eviction is silent (no host callback).
  //
  // Thumbnails do not get the per-entry recency / viewport-distance ranking that the full
  // bucket gets: the cache is sized to comfortably hold every thumbnail in any realistic
  // document (56MB / ~340KB per entry ≈ 168 thumbnails), so the eviction path is a true
  // last-resort fallback. Spending complexity on a smarter ordering here would have no
  // observable benefit.
  uint64_t evictOldestThumbnailsUntilFits(uint64_t neededBytes);

  // Walks every Image node and emits onTextureRequest(filePath) for paths that are referenced
  // by the document but currently lack a full-quality decodedImage and have not already been
  // requested in a previous draw. Triggered once per draw after the LRU sweep so freshly
  // evicted paths immediately re-enter the in-flight set on the same frame.
  void scanAndRequestMissingTextures();

  // Invokes the JS handler's onTextureRequest hook with the given filePath. No-op when the
  // handler has not been registered or does not expose this method. Called from
  // scanAndRequestMissingTextures(); never from inside lockContext.
  void emitTextureRequest(const std::string& filePath);

  // Invokes the JS handler's onTextureEvict hook with the given filePaths. No-op when the
  // list is empty, when the handler has not been registered, or when the handler does not
  // expose this method. Called from enforceFullBudget(); never from inside lockContext.
  void emitTextureEvict(const std::vector<std::string>& filePaths);
  float pagxWidth = 0.0f;
  float pagxHeight = 0.0f;
  int canvasWidth = 0;
  int canvasHeight = 0;
  FontConfig fontConfig = {};

  // Background state from the PAGX document
  bool backgroundVisible = false;
  tgfx::Color backgroundTGFXColor = DefaultBackgroundColor();

  // Performance monitoring
  struct FrameRecord {
    double timestampMs = 0.0;
    double durationMs = 0.0;
  };
  std::deque<FrameRecord> frameHistory = {};
  double frameHistoryTotalTime = 0.0;
  bool lastFrameSlow = false;

  // Bounds origin: PAGX content bounding box top-left relative to cocraft canvas origin
  float boundsOriginX = 0.0f;
  float boundsOriginY = 0.0f;
  bool boundsOriginOverridden = false;

  // State tracking
  bool hasRenderedFirstFrame = false;

  // Accumulated decoded image accounting (debug only). Reset in parsePAGX(). Pixel total
  // approximates the GPU texture footprint at RGBA8 (×4 bytes), useful for correlating wasm
  // heap with image load. Currently only incremented by flushPendingUploads.
  uint64_t imageDecodedPixelTotal = 0;
  uint32_t imageDecodedCount = 0;
  // Histogram of decoded image sizes by pixel area, indexed by bucket: 0:<10K, 1:<100K,
  // 2:<500K, 3:<1M, 4:<2M, 5:>=2M. Lets us tell at a glance whether the document is dominated
  // by tiny icons or by large photos without scrolling through per-image log lines.
  static constexpr size_t IMAGE_SIZE_BUCKET_COUNT = 6;
  uint32_t imageSizeBuckets[IMAGE_SIZE_BUCKET_COUNT] = {};

  float lastZoom = 1.0f;
  bool isZooming = false;
  // Frame counter driving the throttled GPU cache footprint probe in draw(). Per-instance so
  // each view's probe cadence is independent rather than shared across all PAGXView instances.
  int gpuProbeCounter = 0;
  // isZoomingIn is derived per-frame from a single (zoom > lastZoom) comparison and is only
  // consumed by the in/out timeout split in draw() (ZOOM_IN/OUT_END_TIMEOUT_MS). The throttle
  // itself is no longer driven from here -- tgfx infers direction internally.
  bool isZoomingIn = false;
  int currentMaxTilesRefinedPerFrame = 1;
  double tryUpgradeTimestampMs = 0.0;
  double lastZoomUpdateTimestampMs = 0.0;
};

}  // namespace pagx
