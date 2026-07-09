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

#include "PAGXView.h"
#include <emscripten/html5.h>
#include <GLES3/gl31.h>
#include <algorithm>
#include <cstdlib>
#include <limits>
#include <tgfx/gpu/Context.h>
#include <tgfx/gpu/Backend.h>
#include <tgfx/gpu/opengl/GLDevice.h>
#include <tgfx/gpu/opengl/GLTypes.h>
#include "GridBackground.h"
#include "utils/StringParser.h"
#include "tgfx/core/Data.h"
#include "tgfx/core/Image.h"
#include "tgfx/core/ImageCodec.h"
#include "tgfx/core/Rect.h"
#include "tgfx/core/Stream.h"
#include "tgfx/core/Typeface.h"
#include "tgfx/platform/Print.h"
#include "pagx/PAGImage.h"
#include "pagx/tgfx.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/PAGXImporter.h"
#include "pagx/types/Data.h"
#include "utils/ImagePatternMatrixCalculator.h"
#include "base/utils/Log.h"

using namespace emscripten;

namespace pagx {

// RGBA bytes per pixel.
constexpr uint32_t RGBA_BYTES_PER_PIXEL = 4;
// Byte unit conversions.
constexpr size_t BYTES_PER_KB = 1024;
constexpr size_t BYTES_PER_MB = BYTES_PER_KB * BYTES_PER_KB;
// Mipmap overhead factor for GPU footprint estimation (4/3 ≈ 1.33x).
constexpr uint64_t MIPMAP_FACTOR_NUM = 4;
constexpr uint64_t MIPMAP_FACTOR_DEN = 3;
// GPU resource cache limit. Capped at 512MB on iOS WeChat: 1GB and 768MB both led to the
// mini-program being terminated under memory pressure (observed in production logs).
// 512MB causes more eviction churn during pan/zoom on image-heavy PAGX, but a brief
// retexture is preferable to losing the entire session.
constexpr size_t MAX_CACHE_LIMIT = 512U * BYTES_PER_MB;
// GPU resource expiration in frames. Matches the tgfx default; the byte-capacity path above
// remains the primary cap.
constexpr size_t EXPIRATION_FRAMES = 120;
// Slow frame threshold in milliseconds (more lenient than desktop 32ms due to WeChat environment).
constexpr double SLOW_FRAME_THRESHOLD_MS = 50.0;
// Recovery time window in milliseconds (longer than desktop 2s to reduce jitter).
constexpr double RECOVERY_WINDOW_MS = 3000.0;
// Timeout for detecting zoom-in gesture end (faster refinement).
constexpr double ZOOM_IN_END_TIMEOUT_MS = 300.0;
// Timeout for detecting zoom-out gesture end (slower refinement).
constexpr double ZOOM_OUT_END_TIMEOUT_MS = 800.0;
// Delay before retrying tile refinement upgrade when performance is still slow.
constexpr double UPGRADE_RETRY_DELAY_MS = 300.0;
// Minimum frames to confirm recovery in static state.
constexpr size_t MIN_RECOVERY_FRAMES_STATIC = 20;
// Minimum frames to confirm recovery after zoom ends.
constexpr size_t MIN_RECOVERY_FRAMES_ZOOM_END = 10;
// Log a breakdown whenever a single draw() call takes longer than this threshold. Adjust this
// constant to tune log verbosity. submit() is asynchronous in the WebGL backend, so its reported
// time only reflects CPU-side command submission, not actual GPU execution.
constexpr double SLOW_FRAME_LOG_THRESHOLD_MS = 200.0;
// Master switch for all draw()-path telemetry logs (slow-frame breakdown, first-frame breakdown).
// Hardcoded off in shipped builds to keep the WeChat log channel quiet; flip to true and rebuild
// when investigating a frame-pacing regression. `if constexpr` lets the compiler fully strip the
// log branches when disabled, so there is zero runtime cost in the default configuration.
constexpr bool DRAW_LOG_ENABLED = false;
// Wasm page size in bytes (64 KiB per page).
constexpr size_t WASM_PAGE_SIZE = 65536;
// Image size bucket thresholds in pixels.
constexpr uint64_t TINY_BUCKET_MAX = 10000ULL;
constexpr uint64_t SMALL_BUCKET_MAX = 100000ULL;
constexpr uint64_t MID_BUCKET_MAX = 500000ULL;
constexpr uint64_t LARGE_BUCKET_MAX = 1000000ULL;
constexpr uint64_t XL_BUCKET_MAX = 2000000ULL;
// Pixel threshold for [BigImg] log marker (1 MPx).
constexpr uint64_t BIG_IMG_THRESHOLD = 1000000ULL;
// Log throttling interval for image load/upgrade logs.
constexpr uint32_t IMAGE_LOG_THROTTLE = 32;
// Display list tile and subtree cache configuration.
constexpr size_t DEFAULT_MAX_TILE_COUNT = 256;
constexpr size_t DEFAULT_SUBTREE_CACHE_SIZE = 2048;
// Subtree cache size when zoomed out.
constexpr size_t ZOOMED_OUT_SUBTREE_CACHE_SIZE = 1024;
// Zoom drift margin to absorb float imprecision.
constexpr float ZOOM_DRIFT_MARGIN = 1.02f;
// Fit content scale threshold for high-resolution offscreen capture.
constexpr float HIGH_RES_FIT_SCALE_THRESHOLD = 0.15f;
// High-resolution pixel scale for offscreen capture.
constexpr float HIGH_RES_PIXEL_SCALE = 2.0f;
// Default pixel scale.
constexpr float DEFAULT_PIXEL_SCALE = 1.0f;
// Post-zoom-end delay before retrying tile refinement upgrade (ms).
constexpr double POST_ZOOM_UPGRADE_DELAY_MS = 200.0;
// Default ImagePattern scale factor (matches ImagePatternMatrixCalculator default).
constexpr float DEFAULT_IMAGE_SCALE_FACTOR = 0.5f;
// Tile refinement zoom divisor for zoomed-out content.
constexpr float TILE_REFINEMENT_ZOOM_DIVISOR = 0.33f;
// Max tile refinement count per frame.
constexpr int MAX_TILE_REFINEMENT = 3;
// CoCraft business-enum scale mode values.
constexpr int SCALE_MODE_FILL = 0;
constexpr int SCALE_MODE_FIT = 1;
constexpr int SCALE_MODE_STRETCH = 2;
constexpr int SCALE_MODE_TILE = 3;
// GPU probe log interval in frames.
constexpr int GPU_PROBE_INTERVAL = 60;



// Wasm linear memory size in bytes (HEAP8.byteLength equivalent). Each wasm page is 64 KiB.
// Returns -1 on non-wasm platforms so the same probe can stay in cross-platform code.
static int64_t SampleWasmHeap() {
#if defined(__EMSCRIPTEN__) || defined(__wasm__)
  return static_cast<int64_t>(__builtin_wasm_memory_size(0)) * WASM_PAGE_SIZE;
#else
  return -1;
#endif
}

// One-shot wasm heap log. Format kept simple so it can be grepped from the miniprogram console.
static void LogMemProbe(const char* tag) {
  LOGI("[MemProbe] tag=%s wasmHeap=%lld", tag,
                 static_cast<long long>(SampleWasmHeap()));
}

// Bucket index for an image's pixel area. Boundaries align with the [Img] log analysis: tiny
// icons cluster in bucket 0, thumbnail-grade photos in 1-2, full-resolution stuff in 3-5.
static int ImageSizeBucket(uint64_t pixels) {
  if (pixels < TINY_BUCKET_MAX) return 0;
  if (pixels < SMALL_BUCKET_MAX) return 1;
  if (pixels < MID_BUCKET_MAX) return 2;
  if (pixels < LARGE_BUCKET_MAX) return 3;
  if (pixels < XL_BUCKET_MAX) return 4;
  return 5;
}

std::shared_ptr<PAGXView> PAGXView::MakeFrom(int width, int height) {
  if (width <= 0 || height <= 0) {
    return nullptr;
  }

  auto device = tgfx::GLDevice::Current();
  if (device == nullptr) {
    return nullptr;
  }

  return std::shared_ptr<PAGXView>(new PAGXView(device, width, height));
}

// Reads the length of a JavaScript Uint8Array, allocates a matching uint8_t buffer with
// new(std::nothrow), and copies the array contents into it through a HEAPU8-backed view.
// Returns the freshly allocated buffer (caller owns it) and writes its length to outLength, or
// returns nullptr (with outLength = 0) when the input is undefined, empty, or the allocation
// fails. Shared by the tgfx::Data and pagx::Data converters below, which differ only in the
// final factory step.
static uint8_t* AllocAndCopyEmscriptenData(const val& emscriptenData, unsigned int* outLength) {
  *outLength = 0;
  if (emscriptenData.isUndefined()) {
    return nullptr;
  }
  unsigned int length = emscriptenData["length"].as<unsigned int>();
  if (length == 0) {
    return nullptr;
  }
  auto buffer = new (std::nothrow) uint8_t[length];
  if (!buffer) {
    return nullptr;
  }
  auto memory = val::module_property("HEAPU8")["buffer"];
  auto memoryView = emscriptenData["constructor"].new_(
      memory, static_cast<unsigned int>(reinterpret_cast<uintptr_t>(buffer)), length);
  memoryView.call<void>("set", emscriptenData);
  *outLength = length;
  return buffer;
}

// Copies data from a JavaScript Uint8Array into a tgfx::Data object.
static std::shared_ptr<tgfx::Data> GetDataFromEmscripten(const val& emscriptenData) {
  unsigned int length = 0;
  auto buffer = AllocAndCopyEmscriptenData(emscriptenData, &length);
  if (!buffer) {
    return nullptr;
  }
  return tgfx::Data::MakeAdopted(buffer, length, tgfx::Data::DeleteProc);
}

// Copies data from a JavaScript Uint8Array into a pagx::Data object.
static std::shared_ptr<Data> GetPagxDataFromEmscripten(const val& emscriptenData) {
  unsigned int length = 0;
  auto buffer = AllocAndCopyEmscriptenData(emscriptenData, &length);
  if (!buffer) {
    return nullptr;
  }
  return Data::MakeAdopt(buffer, length);
}

PAGXView::PAGXView(std::shared_ptr<tgfx::Device> device, int width, int height)
: device(device), canvasWidth(width), canvasHeight(height) {
  // Display-list configuration (render mode, tile budgets, subtree cache) now lives on the
  // scene's PAGDisplayOptions and is applied in buildLayers() once the scene exists. The
  // constructor only records the device and canvas dimensions.
}

PAGXView::~PAGXView() {
  // Reclaim every backend texture this view still owns. The JS-side destroy() makes the GL
  // context current before invoking nativeView.delete(), so calling destroyBackendTextures
  // here goes to the right context. Walk both caches before clearing so the local
  // pendingTextureDeletes list captures every id; clearing the maps afterwards drops the
  // tgfx::Image shared_ptrs which in turn release the (non-adopted) TextureView from tgfx's
  // ResourceCache. Anything already retired earlier this draw but not yet drained piggy-backs
  // on the same destroyBackendTextures call.
  for (auto& kv : externalTextures) {
    retireTextureId(kv.second.textureId);
  }
  for (auto& kv : thumbnailTextures) {
    retireTextureId(kv.second.textureId);
  }
  externalTextures.clear();
  thumbnailTextures.clear();
  drainPendingTextureDeletes();
}

void PAGXView::registerFonts(const val& fontVal, const val& emojiFontVal) {
  auto fontData = GetDataFromEmscripten(fontVal);
  if (fontData) {
    fontConfig.registerFont(fontData->bytes(), fontData->size(), 0);
  }
  auto emojiFontData = GetDataFromEmscripten(emojiFontVal);
  if (emojiFontData) {
    fontConfig.addFallbackFont(emojiFontData->bytes(), emojiFontData->size(), 0);
  }
}

void PAGXView::loadPAGX(const val& pagxData) {
  parsePAGX(pagxData);
  buildLayers();
}

void PAGXView::parsePAGX(const val& pagxData) {
  LogMemProbe("parsePAGX.enter");
  // Release old resources early to reduce peak memory usage when switching pages. Dropping the
  // scene detaches the entire runtime layer tree and ViewModel binding; the timeline and the
  // surface wrapper go with it. document resets last so the scene's teardown still sees it.
  scene = nullptr;
  defaultTimeline = nullptr;
  pagSurface = nullptr;
  document = nullptr;
  // Drop snapshots so the new document doesn't blit pixels from the old one.
  fitSnapshot = nullptr;
  fitSnapshotPixelScale = DEFAULT_PIXEL_SCALE;
  gestureActive = false;
  zoomedOutFrameSettled = false;
  lastGestureEndMs = 0.0;
  // Reset the user pan/zoom. Without this, a render that happens between parsePAGX and
  // buildLayers (the loader is async; the render loop keeps running) would compose the new
  // layout against the previous document's view state, giving the user the look-and-feel of the
  // previous page.
  userZoom = 1.0f;
  userOffsetX = 0.0f;
  userOffsetY = 0.0f;
  lastZoom = 1.0f;
  // Reset first-frame flag so JS-side loading flow can wait for the new document's first
  // render via isFirstFrameRendered() instead of seeing a stale true from the previous one.
  hasRenderedFirstFrame = false;
  // Reset image-decode accounting for the new document.
  imageDecodedPixelTotal = 0;
  imageDecodedCount = 0;
  for (auto& b : imageSizeBuckets) {
    b = 0;
  }
  // Reset backend-texture caches: the previous document's entries hold both a tgfx::Image
  // (released by the ExternalTextureEntry destructor below) and an externally-owned GL
  // texture. We retire the GL texture ids here so the next draw's drainPendingTextureDeletes
  // can free the GPU memory; the tgfx side has no lingering scratch-pool reference because
  // these entries were created with Image::MakeFrom (non-adopted). Pending uploads from the
  // previous document carry JS-side OffscreenCanvas references that should be released too —
  // the corresponding pagx::Image nodes no longer exist, so any successful upload would have
  // nowhere to land.
  pendingUploads.clear();
  pendingThumbnailBytes = 0;
  pendingFullBytes = 0;
  for (auto& kv : externalTextures) {
    retireTextureId(kv.second.textureId);
  }
  externalTextures.clear();
  for (auto& kv : thumbnailTextures) {
    retireTextureId(kv.second.textureId);
  }
  thumbnailTextures.clear();
  externalTexturesTotalBytes = 0;
  thumbnailTexturesTotalBytes = 0;
  currentFrameIndex = 0;
  // Drop any in-flight full requests carried over from the previous document; the new document
  // has its own set of paths and the host will receive fresh onTextureRequest events on the
  // first draw after buildLayers(). The handler registration is intentionally preserved so
  // callers do not need to re-register on every parsePAGX().
  inFlightFullRequests.clear();
  evictedFullPaths.clear();
  imageOriginalSizes.clear();
  // Reset document-derived state up front so a failed parse below leaves clean defaults instead
  // of retaining the previous document's dimensions / background. On success these are set again
  // from the freshly parsed document right after FromXML.
  pagxWidth = 0.0f;
  pagxHeight = 0.0f;
  backgroundVisible = false;
  backgroundTGFXColor = DefaultBackgroundColor();

  auto data = GetPagxDataFromEmscripten(pagxData);
  if (!data) {
    return;
  }
  size_t xmlSize = data->size();
  document = PAGXImporter::FromXML(data->bytes(), data->size());
  if (document) {
    // Surface document dimensions and document-level custom data (background, bounds origin)
    // immediately after parse so contentWidth()/contentHeight()/getContentTransform() are usable
    // before buildLayers(). The progressive loader reads getContentTransform().fitScale right
    // after parsePAGX() to decide first-frame image quality. buildLayers() no longer repeats it.
    pagxWidth = document->width;
    pagxHeight = document->height;
    applyDocumentCustomData();
    auto paths = document->getExternalFilePaths();
    LOGI(
        "[MemProbe] tag=parsePAGX.exit wasmHeap=%lld xmlBytes=%zu externalImageRefs=%zu",
        static_cast<long long>(SampleWasmHeap()), xmlSize, paths.size());
  } else {
    LogMemProbe("parsePAGX.exit.failed");
  }
}

std::vector<std::string> PAGXView::getExternalFilePaths() const {
  if (!document) {
    return {};
  }
  return document->getExternalFilePaths();
}

bool PAGXView::loadFileData(const val& filePathVal, const val& fileData) {
  auto filePath = filePathVal.as<std::string>();
  if (!document) {
    return false;
  }
  auto data = GetPagxDataFromEmscripten(fileData);
  if (!data) {
    return false;
  }
  return document->loadFileData(filePath, std::move(data));
}



bool PAGXView::attachNativeImage(const val& filePathVal, const val& nativeImage,
                                 int qualityRaw) {
  if (!document || !nativeImage.as<bool>()) {
    return false;
  }

  // filePath is taken as val and converted locally instead of using `const std::string&`:
  // eviction below may grow wasm memory, which detaches JS-side HEAP views; an embind-managed
  // string parameter would then be freed on a stale view in onDone -> runDestructors and trap.
  // Do not add new memory.grow-prone work here while keeping any embind-managed heap parameter.
  std::string filePath = filePathVal.as<std::string>();
  if (filePath.empty()) {
    return false;
  }

  if (qualityRaw != static_cast<int>(ImageQuality::Thumbnail) &&
      qualityRaw != static_cast<int>(ImageQuality::Full)) {
    return false;
  }
  auto quality = static_cast<ImageQuality>(qualityRaw);

  // Sample width/height once and stash them on the PendingUpload so neither the budget check
  // below nor flushPendingUploads needs to bounce back through emscripten::val (each property
  // access is a wasm↔JS round-trip, ~100x slower than a struct field read).
  int width = nativeImage["width"].as<int>();
  int height = nativeImage["height"].as<int>();
  if (width < 1 || height < 1) {
    return false;
  }
  uint64_t bytes =
      static_cast<uint64_t>(width) * static_cast<uint64_t>(height) * static_cast<uint64_t>(RGBA_BYTES_PER_PIXEL)
      * MIPMAP_FACTOR_NUM / MIPMAP_FACTOR_DEN;

  // Thumbnail budget enforcement: estimate the upload's GPU footprint up front so we can run a
  // silent LRU eviction before queuing. The estimate must match flushPendingUploads' bytes
  // formula (RGBA8 + 1/3 mipmap multiplier) so the post-upload total stays consistent. If even
  // after eviction the budget cannot fit, we drop the request rather than letting the cache
  // grow unbounded; the caller will see false and can decide whether to skip or retry later.
  if (quality == ImageQuality::Thumbnail) {
    // Account for thumbnails that are already in pendingUploads but have not yet uploaded:
    // flushPendingUploads commits all of them sequentially, so without this pendingUploads
    // could bloat the realised total above thumbnailBudget. The running sum is kept in
    // pendingThumbnailBytes so this stays O(1) regardless of queue length.
    //
    // Replacement subtraction mirrors the full branch below: when the same path already has a
    // committed thumbnail entry, flushPendingUploads swaps it in place (-= old + += new), so
    // the net delta is `bytes - oldEntry`, not `bytes`. Subtract the existing entry's sizeBytes
    // here to avoid over-projecting and triggering an unnecessary eviction or drop.
    uint64_t replacingBytes = 0;
    auto existing = thumbnailTextures.find(filePath);
    if (existing != thumbnailTextures.end()) {
      replacingBytes = existing->second.sizeBytes;
    }
    uint64_t projected =
        thumbnailTexturesTotalBytes + pendingThumbnailBytes + bytes - replacingBytes;
    if (projected > thumbnailBudget) {
      uint64_t over = projected - thumbnailBudget;
      evictOldestThumbnailsUntilFits(over);
      // Re-fetch after eviction: evictOldestThumbnailsUntilFits iterates in hash order and may
      // have dropped this very path, in which case its bytes are already gone from
      // thumbnailTexturesTotalBytes and must no longer be subtracted as a replacement.
      replacingBytes = 0;
      existing = thumbnailTextures.find(filePath);
      if (existing != thumbnailTextures.end()) {
        replacingBytes = existing->second.sizeBytes;
      }
      projected =
          thumbnailTexturesTotalBytes + pendingThumbnailBytes + bytes - replacingBytes;
    }
    if (projected > thumbnailBudget) {
      LOGI("[PAGX] thumbnailBudget exhausted, drop path=%s size=%dx%d",
                     filePath.c_str(), width, height);
      return false;
    }
  } else {
    // Full budget enforcement: same rationale as the thumbnail branch above, but eviction
    // routes through enforceFullBudget() which honours the viewport-distance ranking and
    // visible-path protection. Running this proactively here — instead of relying on the
    // post-flush sweep inside flushPendingUploads — collapses the previous two-phase pattern
    // (queue 8 fulls → flush all 8 → bulk-evict 17 in one shot) into a steady "evict 1, attach
    // 1" rhythm that keeps externalTexturesTotalBytes hugging the budget line instead of
    // overshooting and snapping back. The running sum pendingFullBytes mirrors
    // pendingThumbnailBytes so the projection is O(1) regardless of queue length.
    //
    // Replacement subtraction: when the same path is already attached, flushPendingUploads
    // performs an in-place swap (-= old + += new), so the net delta is `bytes - oldEntry`,
    // not `bytes`. We subtract the existing entry's sizeBytes here to avoid over-projecting.
    // SDK contract guarantees no Full→Full racing in practice (host only re-fetches after
    // onTextureEvict), but the subtraction stays correct for the rare edge case.
    uint64_t replacingBytes = 0;
    auto existing = externalTextures.find(filePath);
    if (existing != externalTextures.end()) {
      replacingBytes = existing->second.sizeBytes;
    }
    uint64_t projected =
        externalTexturesTotalBytes + pendingFullBytes + bytes - replacingBytes;
    if (projected > fullBudget) {
      // enforceFullBudget evicts off-viewport paths until totalBytes <= fullBudget. Running
      // it before push_back means the upcoming upload sees an already-trimmed cache, so the
      // post-flush enforceFullBudget call inside draw() typically becomes a no-op. We do not
      // re-check after the eviction — even if the projected total still exceeds budget (every
      // remaining entry sits inside the protected viewport), letting the queue accept the new
      // upload is strictly better than silently dropping a host-driven attach: the post-flush
      // sweep will log the overflow once per 60 frames and the host can react via
      // isFullBudgetSaturated() on the next evaluate.
      enforceFullBudget();
    }
  }

  pendingUploads.push_back({filePath, nativeImage, quality, width, height, bytes});
  if (quality == ImageQuality::Thumbnail) {
    pendingThumbnailBytes += bytes;
  } else {
    pendingFullBytes += bytes;
  }
  // Mark the request closed at queue time so a scan running before the next flush does not
  // re-emit onTextureRequest for an asset the host has already handed back. If the eventual
  // flush fails (system-level GL failure), the path will render from thumbnail indefinitely
  // — accepted as a trade-off against an unbounded request → fail → re-request loop.
  if (quality == ImageQuality::Full) {
    inFlightFullRequests.erase(filePath);
    evictedFullPaths.erase(filePath);
  }
  return true;
}

uint64_t PAGXView::evictOldestThumbnailsUntilFits(uint64_t neededBytes) {
  if (neededBytes == 0 || thumbnailTextures.empty() || !document) {
    return 0;
  }
  // Thumbnails are intentionally evicted in hash-map iteration order rather than by recency
  // or viewport distance: the cache is sized so this path is essentially unreachable in
  // practice (~168 thumbnails fit, far more than any real document references), so any
  // cycles spent on smarter ranking would be wasted. If hitting this path becomes routine
  // the right fix is to grow thumbnailBudget, not to refine the policy.
  //
  // Collect the paths to evict first so we don't mutate the map while iterating it. Stop as
  // soon as we have freed enough bytes, even if more entries remain.
  std::vector<std::string> toEvict;
  uint64_t freed = 0;
  for (const auto& [path, entry] : thumbnailTextures) {
    if (freed >= neededBytes) {
      break;
    }
    toEvict.push_back(path);
    freed += entry.sizeBytes;
  }
  for (const auto& path : toEvict) {
    auto it = thumbnailTextures.find(path);
    if (it == thumbnailTextures.end()) {
      continue;
    }
    thumbnailTexturesTotalBytes -= it->second.sizeBytes;
    retireTextureId(it->second.textureId);
    thumbnailTextures.erase(it);
    // Clear the runtime image on matching Image nodes so a subsequent draw does not keep
    // referencing a tgfx::Image that no longer has a cache entry pinning its lifetime.
    // loadFileData with an empty shared_ptr clears Image::runtimeImage and refreshes the
    // affected layers in place.
    document->loadFileData(path, std::shared_ptr<PAGImage>{});
  }
  return freed;
}

void PAGXView::flushPendingUploads(tgfx::Context* context) {
  if (pendingUploads.empty() || context == nullptr || !document) {
    return;
  }
  // Snapshot then clear so the JS-side OffscreenCanvas references held inside each PendingUpload
  // are released as soon as the upload completes; if anything throws or we early-return the
  // queue is still emptied for the next frame.
  std::vector<PendingUpload> uploads;
  uploads.swap(pendingUploads);
  // pendingThumbnailBytes mirrored pendingUploads — once we swap the queue out, the running
  // sum belongs to the local `uploads` vector and the in-place sum resets to zero. Subsequent
  // attachNativeImage(Thumbnail) calls during the upload (defensive: shouldn't happen since
  // we're on the wasm thread) will start from a clean tally.
  pendingThumbnailBytes = 0;
  pendingFullBytes = 0;

  for (auto& p : uploads) {
    // Hand the JS-side native image to the TS helper which creates a GL texture, registers it
    // with emscripten's GL.textures table, uploads pixels via texImage2D, and runs
    // generateMipmap. Returns the GL.textures index (>0 on success) so the wasm side can pass
    // the same id to tgfx through GLTextureInfo.
    int textureId = val::module_property("tgfx").call<int>(
        "createBackendTexture", val::module_property("GL"), p.nativeImage);
    if (textureId <= 0) {
      LOGI("[PAGX] attachNativeImage: createBackendTexture failed path=%s",
                     p.filePath.c_str());
      continue;
    }

    tgfx::GLTextureInfo glInfo = {};
    glInfo.id = static_cast<unsigned>(textureId);
    tgfx::BackendTexture backendTex(glInfo, p.width, p.height);
    // Use MakeFrom (non-adopted) rather than MakeAdopted: tgfx wraps the GL texture without
    // claiming ownership, so when our shared_ptr<Image> drops the underlying TextureView is
    // removed from the ResourceCache immediately instead of being parked in the scratch pool
    // to wait for an LRU sweep. We retain the textureId on the entry and call gl.deleteTexture
    // ourselves at the end of draw() (drainPendingTextureDeletes), guaranteeing the GPU memory
    // is released in the same frame the entry is dropped.
    auto tgfxImage = tgfx::Image::MakeFrom(context, backendTex, tgfx::ImageOrigin::TopLeft);
    if (!tgfxImage) {
      LOGI("[PAGX] attachNativeImage: MakeFrom failed path=%s", p.filePath.c_str());
      retireTextureId(static_cast<unsigned>(textureId));
      continue;
    }
    // Wrap the uploaded tgfx::Image as a PAGImage without re-locking the context.
    auto pagImage = pagx::MakeFrom(tgfxImage);

    if (p.quality == ImageQuality::Full) {
      // Replace the previous full entry. The old entry's PAGImage drops here; retire the previous
      // textureId so its GPU memory is reclaimed at the end of this same draw().
      auto& entry = externalTextures[p.filePath];
      if (entry.image) {
        externalTexturesTotalBytes -= entry.sizeBytes;
        retireTextureId(entry.textureId);
      }
      entry.image = pagImage;
      entry.sizeBytes = p.sizeBytes;
      entry.textureId = static_cast<unsigned>(textureId);
      externalTexturesTotalBytes += p.sizeBytes;
      // Refresh ImagePattern matrices against the newly attached image dimensions before
      // installing the runtime image, so the in-place refresh picks up the updated matrix.
      ResolveImagePatternMatricesByFilePath(document.get(), p.filePath, &imageOriginalSizes,
                                            static_cast<float>(tgfxImage->width()),
                                            static_cast<float>(tgfxImage->height()));
      // Push the PAGImage onto matching Image nodes. loadFileData sets Image::runtimeImage and
      // refreshes affected layers in place.
      document->loadFileData(p.filePath, pagImage);
    } else {
      // Thumbnail: budget enforcement and silent LRU eviction happen up front in
      // attachNativeImage() so flushPendingUploads only sees uploads that already fit.
      auto& entry = thumbnailTextures[p.filePath];
      if (entry.image) {
        thumbnailTexturesTotalBytes -= entry.sizeBytes;
        retireTextureId(entry.textureId);
      }
      entry.image = pagImage;
      entry.sizeBytes = p.sizeBytes;
      entry.textureId = static_cast<unsigned>(textureId);
      thumbnailTexturesTotalBytes += p.sizeBytes;
      // Refresh ImagePattern matrices against the freshly attached thumbnail's pixel dimensions.
      ResolveImagePatternMatricesByFilePath(document.get(), p.filePath, &imageOriginalSizes,
                                            static_cast<float>(tgfxImage->width()),
                                            static_cast<float>(tgfxImage->height()));
      // Install the thumbnail as the runtime image so the renderer picks it up.
      document->loadFileData(p.filePath, pagImage);
    }
  }
}

void PAGXView::enforceFullBudget() {
  if (!document || externalTexturesTotalBytes <= fullBudget || externalTextures.empty()) {
    return;
  }
  // Bounds collection runs only on the over-budget path. We pay this cost once per draw that
  // actually exceeds budget, never on the steady-state happy path.
  tgfx::Rect viewport = computeViewportInRootCoords();
  auto pathBounds = computeFullPathBounds();
  // Visibility protection uses the (1.2x-expanded) viewport returned by
  // computeViewportInRootCoords; the same rect's center is the focal point used to rank
  // off-viewport candidates. Both reads of `viewport` below operate on the same struct so the
  // two notions of "near" stay consistent.
  float focusX = viewport.centerX();
  float focusY = viewport.centerY();
  size_t visibleCount = 0;

  // Candidate := off-viewport entry that may be evicted. Visible paths are dropped on the
  // floor so they never compete for eviction. Distance-squared (no sqrt) is enough for
  // sorting since sqrt is monotonic.
  struct Candidate {
    float distSq;
    uint64_t sizeBytes;
    std::string path;
    // Orders candidates farthest-first so eviction starts with the entries furthest from focus.
    static bool FartherFirst(const Candidate& a, const Candidate& b) {
      return a.distSq > b.distSq;
    }
  };
  std::vector<Candidate> candidates;
  candidates.reserve(externalTextures.size());
  for (const auto& [path, entry] : externalTextures) {
    auto it = pathBounds.find(path);
    if (it == pathBounds.end()) {
      // Path has no resolvable layer bounds (degenerate / not yet attached to layer tree).
      // Treat as "infinitely far" so it always loses to bounded candidates, matching the
      // intuition that an entry no layer references is the cheapest to drop.
      candidates.push_back({std::numeric_limits<float>::infinity(), entry.sizeBytes, path});
      continue;
    }
    const auto& bounds = it->second;
    if (tgfx::Rect::Intersects(bounds, viewport)) {
      ++visibleCount;
      continue;
    }
    float dx = bounds.centerX() - focusX;
    float dy = bounds.centerY() - focusY;
    candidates.push_back({dx * dx + dy * dy, entry.sizeBytes, path});
  }
  // Farthest first. Modeled on tgfx's tiled rasterization which prioritizes tiles closest to
  // the user's focal point — we keep the same focal-distance gradient but invert the sweep
  // direction (closest stays, farthest goes).
  std::sort(candidates.begin(), candidates.end(), &Candidate::FartherFirst);

  std::vector<std::string> evictedPaths;
  for (const auto& c : candidates) {
    if (externalTexturesTotalBytes <= fullBudget) {
      break;
    }
    auto it = externalTextures.find(c.path);
    if (it == externalTextures.end()) {
      continue;
    }
    externalTexturesTotalBytes -= it->second.sizeBytes;
    // Reclaim the GL texture this draw, before flush+submit publishes the new RenderTask
    // graph. drainPendingTextureDeletes() runs after submit so any in-flight render that
    // sampled the texture has already been recorded into a command buffer; WebGL guarantees
    // those draws keep working until the buffer drains.
    retireTextureId(it->second.textureId);
    externalTextures.erase(it);
    // Clear the runtime image on matching Image nodes so the renderer falls back to thumbnail
    // (or blank) on the next draw. loadFileData with an empty shared_ptr clears
    // Image::runtimeImage and refreshes the affected layers in place.
    // scanAndRequestMissingTextures() will pick the dropped paths up on the same frame and
    // emit onTextureRequest, so the host can start fetching a replacement immediately.
    document->loadFileData(c.path, std::shared_ptr<PAGImage>{});
    evictedPaths.push_back(c.path);
    // Mark the path as a candidate for the next scanAndRequestMissingTextures sweep so the
    // host receives onTextureRequest the same draw and can start re-fetching immediately.
    // attachNativeImage(Full) will erase the entry once a replacement upload is queued.
    evictedFullPaths.insert(c.path);
  }
  // Still over budget after the sweep means every remaining entry sits inside the protected
  // viewport. Letting the budget overflow temporarily is strictly better than evicting a
  // visible texture (which would fall back to thumbnail and immediately trigger a re-download
  // under handleTextureRequest, oscillating between full and thumbnail at the user's focal
  // point). Throttle the warning so a sustained overflow does not flood the log.
  if (externalTexturesTotalBytes > fullBudget &&
      currentFrameIndex - lastFullBudgetOverflowWarnFrame >= GPU_PROBE_INTERVAL) {
    lastFullBudgetOverflowWarnFrame = currentFrameIndex;
    LOGI(
        "[PAGX] fullBudget exceeded by %lluMB; %zu visible paths protected from eviction",
        static_cast<unsigned long long>((externalTexturesTotalBytes - fullBudget) / BYTES_PER_MB),
        visibleCount);
  }
  // Notify the host once per draw with the full batch so JS only takes a single trip across
  // the boundary regardless of how many paths were dropped.
  emitTextureEvict(evictedPaths);
}

tgfx::Rect PAGXView::computeViewportInRootCoords() const {
  if (canvasWidth <= 0 || canvasHeight <= 0 || !scene) {
    return tgfx::Rect::MakeEmpty();
  }
  // PAGScene::getImageBounds() returns bounds in the root composition's local (document)
  // coordinate space, before the display zoomScale/contentOffset are applied. The effective
  // zoom/offset written to PAGDisplayOptions by applyMergedTransform() map that space onto the
  // canvas:
  //   canvas_pixel = doc_coord * effectiveZoom + effectiveOffset
  //   doc_coord    = (canvas_pixel - effectiveOffset) / effectiveZoom
  // so inverting the display transform maps the canvas viewport back into the bounds' space.
  auto* options = scene->getDisplayOptions();
  float zoomScale = options->getZoomScale();
  if (zoomScale <= 0.0f) {
    return tgfx::Rect::MakeEmpty();
  }
  auto offset = options->getContentOffset();
  float left = (-offset.x) / zoomScale;
  float top = (-offset.y) / zoomScale;
  float width = static_cast<float>(canvasWidth) / zoomScale;
  float height = static_cast<float>(canvasHeight) / zoomScale;
  // 1.2x expansion around the viewport center absorbs fling-deceleration overshoot and small
  // user adjustments that would otherwise momentarily push an in-focus texture outside the
  // strict viewport, triggering an avoidable evict→re-upload cycle. Match the same factor
  // anywhere else that intersects this rect so the protection envelope stays consistent.
  constexpr float kViewportExpansion = 1.2f;
  float expandX = width * (kViewportExpansion - 1.0f) * 0.5f;
  float expandY = height * (kViewportExpansion - 1.0f) * 0.5f;
  return tgfx::Rect::MakeLTRB(left, top, left + width, top + height)
      .makeOutset(expandX, expandY);
}

std::unordered_map<std::string, tgfx::Rect> PAGXView::computeFullPathBounds() const {
  std::unordered_map<std::string, tgfx::Rect> result;
  if (!scene) {
    return result;
  }
  result.reserve(externalTextures.size());
  // Only inspect paths actually present in the cache — they are the only candidates the
  // eviction sweep needs to reason about. Iterating every image node in the document would
  // waste cycles on paths that have no full-quality entry to begin with.
  for (const auto& kv : externalTextures) {
    const auto& path = kv.first;
    // getImageBounds() returns document-space bounds (pagx::Rect) for every runtime layer that
    // references this path; tgfx caches each layer's bounds after the first evaluation.
    auto boundsList = scene->getImageBounds(path);
    if (boundsList.empty()) {
      continue;
    }
    tgfx::Rect unionBounds = tgfx::Rect::MakeEmpty();
    bool hasAny = false;
    for (const auto& r : boundsList) {
      if (r.isEmpty()) {
        continue;
      }
      tgfx::Rect bounds = tgfx::Rect::MakeXYWH(r.x, r.y, r.width, r.height);
      if (!hasAny) {
        unionBounds = bounds;
        hasAny = true;
      } else {
        unionBounds.join(bounds);
      }
    }
    if (hasAny) {
      result.emplace(path, unionBounds);
    }
  }
  return result;
}

void PAGXView::setTextureEventHandler(const val& handler) {
  // Treat both undefined and null as "clear handler"; everything else is recorded as-is. The
  // val is held by value so the JS-side object stays reachable for the lifetime of this view.
  if (!handler.as<bool>()) {
    textureEventHandler = val::undefined();
    return;
  }
  textureEventHandler = handler;
}

bool PAGXView::isFullBudgetSaturated() const {
  return externalTexturesTotalBytes >= fullBudgetHardCap;
}

void PAGXView::retireTextureId(unsigned textureId) {
  // 0 means "no GL id was associated", which legitimately happens for default-constructed
  // entries that never completed an upload. Skipping zero keeps callers from having to guard
  // every single retire site, and prevents handing 0 to gl.deleteTexture (a no-op in spec
  // but spammy in driver logs).
  if (textureId == 0) {
    return;
  }
  pendingTextureDeletes.push_back(textureId);
}

void PAGXView::drainPendingTextureDeletes() {
  if (pendingTextureDeletes.empty()) {
    return;
  }
  // Hand the whole batch to JS in one call so the wasm↔JS boundary is crossed once instead of
  // once per id. The JS helper iterates the array internally and removes each id from
  // GL.textures alongside the gl.deleteTexture invocation. Any retire that snuck in after the
  // helper started running stays in the queue until the next drain — we swap a fresh empty
  // vector in to make that explicit.
  std::vector<unsigned> toDelete;
  toDelete.swap(pendingTextureDeletes);
  val arr = val::array();
  for (size_t i = 0; i < toDelete.size(); ++i) {
    arr.set(i, toDelete[i]);
  }
  val::module_property("tgfx").call<void>("destroyBackendTextures", val::module_property("GL"),
                                          arr);
}

void PAGXView::emitTextureRequest(const std::string& filePath) {
  if (!textureEventHandler.as<bool>() || filePath.empty()) {
    return;
  }
  // hasOwnProperty would not see methods on the prototype chain; "in" mirrors what JS code
  // would do with `if ('onTextureRequest' in handler)`. Missing methods are silently ignored
  // so callers can register a handler with only the events they care about.
  if (!textureEventHandler["onTextureRequest"].as<bool>()) {
    return;
  }
  textureEventHandler.call<void>("onTextureRequest", filePath);
}

void PAGXView::emitTextureEvict(const std::vector<std::string>& filePaths) {
  if (filePaths.empty() || !textureEventHandler.as<bool>()) {
    return;
  }
  if (!textureEventHandler["onTextureEvict"].as<bool>()) {
    return;
  }
  // Build a JS Array out of the C++ vector so the host receives a native array rather than a
  // wasm-side opaque handle.
  val arr = val::array();
  for (size_t i = 0; i < filePaths.size(); ++i) {
    arr.set(i, filePaths[i]);
  }
  textureEventHandler.call<void>("onTextureEvict", arr);
}

void PAGXView::scanAndRequestMissingTextures() {
  if (!document || !textureEventHandler.as<bool>() || evictedFullPaths.empty()) {
    return;
  }
  // We deliberately scan only paths that have been evicted by a previous LRU sweep. The
  // initial-attach window — where externalTextures has no full image because the host has not
  // yet downloaded and called attachNativeImage(Full) for the first time — is owned by the
  // host's own progressive image flow, not by us. Emitting onTextureRequest there would force
  // the host into an "always fetch full" mode and defeat thumbnail-first loading.
  //
  // Snapshot into a vector so the inFlight bookkeeping below can mutate evictedFullPaths
  // without invalidating an in-progress iterator.
  std::vector<std::string> candidates(evictedFullPaths.begin(), evictedFullPaths.end());
  for (const auto& filePath : candidates) {
    // Defensive: skip paths whose full image somehow re-arrived between eviction and this
    // scan (concurrent attach in another emit path, or future code paths). evictedFullPaths
    // gets erased on attach, so this is mostly belt-and-braces.
    if (filePath.empty()) {
      continue;
    }
    if (inFlightFullRequests.count(filePath) > 0) {
      continue;
    }
    inFlightFullRequests.insert(filePath);
    emitTextureRequest(filePath);
  }
}

namespace {

val RectToJSObject(const tgfx::Rect& rect) {
  val obj = val::object();
  obj.set("x", rect.left);
  obj.set("y", rect.top);
  obj.set("w", rect.width());
  obj.set("h", rect.height());
  return obj;
}

}  // namespace

val PAGXView::getImageBounds(const val& filePathList) const {
  val result = val::object();
  if (!document || !scene) {
    return result;
  }
  float fitScale = computeFitScale();
  if (fitScale <= 0.0f) {
    return result;
  }
  // Map document-space bounds into canvas pixel space using only the contain-mode fit transform
  // (fitScale + centering offset), WITHOUT the user pan/zoom. This matches the JS contract
  // (types.ts): callers apply their own zoom/offset on top of these base positions.
  float centerOffsetX = (static_cast<float>(canvasWidth) - pagxWidth * fitScale) * 0.5f;
  float centerOffsetY = (static_cast<float>(canvasHeight) - pagxHeight * fitScale) * 0.5f;
  tgfx::Matrix fitMatrix = tgfx::Matrix::MakeTrans(centerOffsetX, centerOffsetY);
  fitMatrix.preScale(fitScale, fitScale);

  auto count = filePathList["length"].as<unsigned>();
  for (unsigned i = 0; i < count; ++i) {
    std::string filePath = filePathList[i].as<std::string>();
    auto boundsList = scene->getImageBounds(filePath);
    if (boundsList.empty()) {
      result.set(filePath, val::null());
      continue;
    }
    tgfx::Rect unionBounds = tgfx::Rect::MakeEmpty();
    tgfx::Rect largestBounds = tgfx::Rect::MakeEmpty();
    float largestArea = 0.0f;
    bool hasAny = false;
    for (const auto& r : boundsList) {
      if (r.isEmpty()) {
        continue;
      }
      tgfx::Rect docRect = tgfx::Rect::MakeXYWH(r.x, r.y, r.width, r.height);
      tgfx::Rect bounds = fitMatrix.mapRect(docRect);
      if (bounds.isEmpty()) {
        continue;
      }
      if (!hasAny) {
        unionBounds = bounds;
        largestBounds = bounds;
        largestArea = bounds.width() * bounds.height();
        hasAny = true;
      } else {
        unionBounds.join(bounds);
        float area = bounds.width() * bounds.height();
        if (area > largestArea) {
          largestArea = area;
          largestBounds = bounds;
        }
      }
    }
    if (hasAny) {
      val entry = val::object();
      entry.set("unionBounds", RectToJSObject(unionBounds));
      entry.set("largestBounds", RectToJSObject(largestBounds));
      result.set(filePath, entry);
    } else {
      result.set(filePath, val::null());
    }
  }
  return result;
}

void PAGXView::setImageOriginalSize(const val& filePathVal, float width, float height) {
  auto filePath = filePathVal.as<std::string>();
  if (filePath.empty() || width <= 0.0f || height <= 0.0f) {
    return;
  }
  imageOriginalSizes[filePath] = {width, height};
}

// Parses a float-valued customData attribute, returning `fallback` when the key is absent or
// the value is not numeric.
static float ParseFloatAttr(const std::unordered_map<std::string, std::string>& data,
                            const char* key, float fallback) {
  auto it = data.find(key);
  if (it == data.end()) {
    return fallback;
  }
  char* end = nullptr;
  float value = std::strtof(it->second.c_str(), &end);
  if (end == it->second.c_str()) {
    return fallback;
  }
  return value;
}

// Parses an int-valued customData attribute, returning `fallback` when the key is absent or
// the value is not numeric.
static int ParseIntAttr(const std::unordered_map<std::string, std::string>& data, const char* key,
                        int fallback) {
  auto it = data.find(key);
  if (it == data.end()) {
    return fallback;
  }
  char* end = nullptr;
  long value = std::strtol(it->second.c_str(), &end, 10);
  if (end == it->second.c_str()) {
    return fallback;
  }
  return static_cast<int>(value);
}

val PAGXView::getImageMetadata() const {
  val result = val::array();
  if (!document) {
    return result;
  }

  // Aggregate ImagePattern usages by the filePath of their referenced Image node so a single
  // file shared across multiple fills only produces one entry. We keep the insertion order
  // stable so later JS-side logs stay readable; switching to unordered_map would shuffle the
  // entries across runs. Data URIs and inline data-backed images never surface to the JS
  // downloader, so we skip them here as well.
  std::vector<std::string> orderedPaths;
  std::unordered_map<std::string, int> pathIndex;
  std::unordered_map<std::string, std::pair<float, float>> originalDimensions;
  std::unordered_map<std::string, std::vector<val>> usagesByPath;

  for (const auto& node : document->nodes) {
    if (!node || node->nodeType() != NodeType::ImagePattern) {
      continue;
    }
    const auto* pattern = static_cast<const ImagePattern*>(node.get());
    if (!pattern->image || pattern->image->filePath.empty()) {
      continue;
    }
    const std::string& filePath = pattern->image->filePath;
    // Skip data URIs: the JS downloader never touches them so surfacing them would just
    // clutter the output.
    if (filePath.compare(0, 5, "data:") == 0) {
      continue;
    }

    const auto& data = pattern->customData;
    float origWidth = ParseFloatAttr(data, "orig-image-width", 0.0f);
    float origHeight = ParseFloatAttr(data, "orig-image-height", 0.0f);
    float nodeWidth = ParseFloatAttr(data, "node-width", 0.0f);
    float nodeHeight = ParseFloatAttr(data, "node-height", 0.0f);
    // scaleMode mapping: the JS side expects the CoCraft business enum
    // (0=FILL, 1=FIT, 2=STRETCH, 3=TILE), not the pagx::ScaleMode enum. Two PAGX exporter
    // generations need to be handled here:
    //   - Legacy (CoCraft pre-rework): writes "image-scale-mode" into customData with the
    //     business-enum integer directly. The XML side does not carry a "scaleMode" attribute,
    //     so PAGXImporter falls back to the default LetterBox and pattern->scaleMode is not a
    //     reliable source. We honour the customData value as-is.
    //   - New (CoCraft post-rework): omits "image-scale-mode" and instead writes the canonical
    //     "scaleMode" XML attribute plus tileModeX="repeat" for tiled fills. PAGXImporter
    //     deserialises both onto the pattern, so we recover the business-enum integer by
    //     inspecting the structured fields. The mapping is unambiguous because TILE is the
    //     only mode that uses tileModeX=Repeat.
    int scaleMode = 0;
    if (data.find("image-scale-mode") != data.end()) {
      scaleMode = ParseIntAttr(data, "image-scale-mode", 0);
    } else if (pattern->tileModeX == TileMode::Repeat) {
      scaleMode = SCALE_MODE_TILE;
    } else if (pattern->scaleMode == ScaleMode::Zoom) {
      scaleMode = SCALE_MODE_FILL;
    } else if (pattern->scaleMode == ScaleMode::LetterBox) {
      scaleMode = SCALE_MODE_FIT;
    } else {
      // Stretch / None both fall through here. None is only emitted by SVGImporter for
      // absolute-coordinate fills which the WeChat host never feeds through this metadata
      // path, so treating it as STRETCH is a safe fallback for the edge case.
      scaleMode = SCALE_MODE_STRETCH;
    }
    // DEFAULT_IMAGE_SCALE_FACTOR matches the default used by ImagePatternMatrixCalculator so TILE
    // patterns that omit the attribute behave identically in C++ and JS.
    float scaleFactor = ParseFloatAttr(data, "scale-factor", DEFAULT_IMAGE_SCALE_FACTOR);

    if (pathIndex.find(filePath) == pathIndex.end()) {
      pathIndex[filePath] = static_cast<int>(orderedPaths.size());
      orderedPaths.push_back(filePath);
      originalDimensions[filePath] = {origWidth, origHeight};
      usagesByPath[filePath] = {};
    } else {
      // Keep the first positive dimension we saw: different ImagePatterns pointing at the
      // same Image node may store zero for one or both dimensions depending on export path.
      auto& existing = originalDimensions[filePath];
      if (existing.first <= 0.0f && origWidth > 0.0f) existing.first = origWidth;
      if (existing.second <= 0.0f && origHeight > 0.0f) existing.second = origHeight;
    }

    val usage = val::object();
    usage.set("nodeWidth", nodeWidth);
    usage.set("nodeHeight", nodeHeight);
    usage.set("scaleMode", scaleMode);
    usage.set("scaleFactor", scaleFactor);
    usagesByPath[filePath].push_back(std::move(usage));
  }

  for (size_t i = 0; i < orderedPaths.size(); ++i) {
    const auto& filePath = orderedPaths[i];
    val entry = val::object();
    entry.set("filePath", filePath);
    entry.set("origWidth", originalDimensions[filePath].first);
    entry.set("origHeight", originalDimensions[filePath].second);
    val usageArr = val::array();
    const auto& usages = usagesByPath[filePath];
    for (size_t j = 0; j < usages.size(); ++j) {
      usageArr.set(static_cast<unsigned>(j), usages[j]);
    }
    entry.set("usages", usageArr);
    result.set(static_cast<unsigned>(i), entry);
  }
  return result;
}

void PAGXView::buildLayers() {
  if (!document) {
    return;
  }
  LOGI(
      "[MemProbe] tag=buildLayers.enter wasmHeap=%lld imageCount=%u pixels=%llu (~%lluMB RGBA)",
      static_cast<long long>(SampleWasmHeap()), imageDecodedCount,
      static_cast<unsigned long long>(imageDecodedPixelTotal),
      static_cast<unsigned long long>(imageDecodedPixelTotal * RGBA_BYTES_PER_PIXEL / BYTES_PER_MB));
  LOGI(
      "[ImgHist] tiny<10K=%u small<100K=%u mid<500K=%u large<1M=%u xl<2M=%u huge>=2M=%u",
      imageSizeBuckets[0], imageSizeBuckets[1], imageSizeBuckets[2],
      imageSizeBuckets[3], imageSizeBuckets[4], imageSizeBuckets[5]);

  // PAGX files exported by CoCraft reference images by hash, so actual pixel dimensions are
  // unavailable at export time. The exporter stores a normalized transform and scale parameters in
  // customData instead. Now that images are downloaded, combine them with actual pixel dimensions
  // to compute the final ImagePattern matrices. PAGX files generated by libpag embed image data
  // directly and do not need this step.
  // TODO: Integrate this into LayerBuilder so all PAGX renderers get it automatically.
  ResolveAllImagePatternMatrices(document.get(), &imageOriginalSizes);

  document->applyLayout(&fontConfig);
  LogMemProbe("buildLayers.afterApplyLayout");

  // Build the runtime scene from the laid-out document. PAGScene::Make() would apply layout
  // itself if it had not been applied, but it does so without our fontConfig, so we call
  // applyLayout(&fontConfig) above first to keep text measurement correct.
  scene = PAGScene::Make(document);
  if (!scene) {
    LogMemProbe("buildLayers.exit.noScene");
    return;
  }
  defaultTimeline = scene->getDefaultTimeline();

  // Mirror the display-list tuning that previously lived in the constructor onto the scene's
  // options. Subtree cache: caches static layers as textures to avoid per-layer shape
  // retriangulation, the dominant hotspot for PAGX content (Shape=200-400ms/frame on singleton
  // shapes). Background is set to explicit transparent so the display list never paints over the
  // manual checkerboard / solid background drawn on the surface before Record().
  auto* options = scene->getDisplayOptions();
  options->setRenderMode(PAGRenderMode::Tiled);
  options->setTileUpdateMode(PAGTileUpdateMode::Fast);
  options->setMaxTileCount(static_cast<int>(DEFAULT_MAX_TILE_COUNT));
  options->setMaxTilesRefinedPerFrame(currentMaxTilesRefinedPerFrame);
  options->setSubtreeCacheMaxSize(static_cast<int>(DEFAULT_SUBTREE_CACHE_SIZE));
  options->setBackgroundColor({0.0f, 0.0f, 0.0f, 0.0f});

  hasRenderedFirstFrame = false;
  applyMergedTransform();
  LogMemProbe("buildLayers.exit");
}

void PAGXView::applyDocumentCustomData() {
  backgroundVisible = false;
  backgroundTGFXColor = DefaultBackgroundColor();
  auto& customData = document->customData;
  auto visibleIt = customData.find("bg-visible");
  if (visibleIt != customData.end()) {
    backgroundVisible = (visibleIt->second == "true");
  }
  if (backgroundVisible) {
    auto colorIt = customData.find("bg-color");
    if (colorIt != customData.end()) {
      backgroundTGFXColor = ParseHexColor(colorIt->second);
    }
    auto alphaIt = customData.find("bg-alpha");
    if (alphaIt != customData.end()) {
      char* end = nullptr;
      float alpha = std::strtof(alphaIt->second.c_str(), &end);
      if (end != alphaIt->second.c_str()) {
        backgroundTGFXColor.alpha *= alpha;
      }
    }
  }
  if (!boundsOriginOverridden) {
    auto originXIt = customData.find("bounds-origin-x");
    if (originXIt != customData.end()) {
      char* end = nullptr;
      float value = std::strtof(originXIt->second.c_str(), &end);
      if (end != originXIt->second.c_str()) {
        boundsOriginX = value;
      }
    }
    auto originYIt = customData.find("bounds-origin-y");
    if (originYIt != customData.end()) {
      char* end = nullptr;
      float value = std::strtof(originYIt->second.c_str(), &end);
      if (end != originYIt->second.c_str()) {
        boundsOriginY = value;
      }
    }
  }
}

void PAGXView::updateSize(int width, int height) {
  if (width <= 0 || height <= 0) {
    return;
  }

  canvasWidth = width;
  canvasHeight = height;
  surface = nullptr;
  // The PAGSurface wraps the old tgfx::Surface; drop it so draw() rewraps the freshly created
  // one against the new dimensions.
  pagSurface = nullptr;
  // Snapshots are bound to the old surface size; drop them to avoid dimension mismatch.
  fitSnapshot = nullptr;
  fitSnapshotPixelScale = DEFAULT_PIXEL_SCALE;
  gestureActive = false;
  zoomedOutFrameSettled = false;
  lastGestureEndMs = 0.0;

  if (scene) {
    applyMergedTransform();
  }
}

// Computes the contain-mode fit scale for the PAGX content against the canvas. Used by both
// applyCenteringTransform() (content matrix) and getContentTransform() (comment-pin
// coordinates) so the two stay in sync; any divergence here produces pins that drift relative
// to the content.
//
// No lower bound is applied: when the design is larger than the canvas we scale it down to fit
// so the user sees the whole document on first frame. This is mandatory for the progressive
// image loading flow because a 1:1 clamp would skip rasterization of any tile outside the
// canvas and therefore any image outside the top-left region, which in turn starves the
// viewport-based upgrade scoring in ProgressiveImageUpgrader. The increased fill cost is
// offset by downloading 20p thumbnails first, so the first-frame texture payload stays low.
// Returns 0 when dimensions are invalid; callers must check before using the result.
float PAGXView::computeFitScale() const {
  if (canvasWidth <= 0 || canvasHeight <= 0 || pagxWidth <= 0 || pagxHeight <= 0) {
    return 0.0f;
  }
  float scaleX = static_cast<float>(canvasWidth) / pagxWidth;
  float scaleY = static_cast<float>(canvasHeight) / pagxHeight;
  return std::min(scaleX, scaleY);
}

void PAGXView::applyMergedTransform() {
  if (!scene) {
    return;
  }
  float fitScale = computeFitScale();
  if (fitScale <= 0.0f) {
    return;
  }
  // Contain-mode centering offset in canvas pixels.
  float centerOffsetX = (static_cast<float>(canvasWidth) - pagxWidth * fitScale) * 0.5f;
  float centerOffsetY = (static_cast<float>(canvasHeight) - pagxHeight * fitScale) * 0.5f;
  // Fold the two former transform levels into one. Old pipeline: contentLayer carried
  //   root = doc * fitScale + centerOffset
  // and the display list carried
  //   screen = root * userZoom + userOffset.
  // Substituting gives screen = doc * (fitScale*userZoom) + (centerOffset*userZoom + userOffset),
  // which is exactly the single zoom/offset written to PAGDisplayOptions below.
  float effectiveZoom = fitScale * userZoom;
  float effectiveOffsetX = centerOffsetX * userZoom + userOffsetX;
  float effectiveOffsetY = centerOffsetY * userZoom + userOffsetY;

  auto* options = scene->getDisplayOptions();
  options->setZoomScale(effectiveZoom);
  options->setContentOffset(effectiveOffsetX, effectiveOffsetY);
}

void PAGXView::setBoundsOrigin(float x, float y) {
  boundsOriginX = x;
  boundsOriginY = y;
  boundsOriginOverridden = true;
}

void PAGXView::setGestureActive(bool active) {
  if (active) {
    // At this stage only fitSnapshot is used for compositing; cached is no longer captured.
    if (fitSnapshot == nullptr) {
      gestureActive = false;
      return;
    }
  } else {
    lastGestureEndMs = emscripten_get_now();
  }
  zoomedOutFrameSettled = false;
  gestureActive = active;
}

void PAGXView::setSnapshotEnabled(bool enabled) {
  if (snapshotEnabled == enabled) {
    return;
  }
  snapshotEnabled = enabled;
  if (!enabled) {
    // Drop the captured snapshot so a stale image cannot resurface if the switch is later
    // re-enabled mid-session. Also clear the zoom-out idle token so the next draw() runs a full
    // render instead of short-circuiting on a now-disabled path. gestureActive stays as-is
    // (the caller's gesture lifecycle is independent), but with no snapshot the fast path
    // in draw() naturally falls through to the full-render branch.
    fitSnapshot = nullptr;
    fitSnapshotPixelScale = DEFAULT_PIXEL_SCALE;
    zoomedOutFrameSettled = false;
  }
}

val PAGXView::getContentTransform() const {
  float fitScale = computeFitScale();
  float centerOffsetX = 0.0f;
  float centerOffsetY = 0.0f;
  if (fitScale > 0.0f) {
    centerOffsetX = (static_cast<float>(canvasWidth) - pagxWidth * fitScale) * 0.5f;
    centerOffsetY = (static_cast<float>(canvasHeight) - pagxHeight * fitScale) * 0.5f;
  }
  val result = val::object();
  result.set("boundsOriginX", boundsOriginX);
  result.set("boundsOriginY", boundsOriginY);
  result.set("fitScale", fitScale);
  result.set("centerOffsetX", centerOffsetX);
  result.set("centerOffsetY", centerOffsetY);
  return result;
}

val PAGXView::getNodePosition(const std::string& nodeId) const {
  val result = val::object();
  result.set("found", false);
  result.set("x", 0.0f);
  result.set("y", 0.0f);

  if (!document || nodeId.empty()) {
    return result;
  }

  auto* node = document->findNode(nodeId);
  if (!node) {
    return result;
  }

  auto it = node->customData.find("page-offset");
  if (it == node->customData.end()) {
    return result;
  }

  const std::string& value = it->second;
  auto commaPos = value.find(',');
  if (commaPos == std::string::npos) {
    return result;
  }

  char* endX = nullptr;
  char* endY = nullptr;
  float x = std::strtof(value.c_str(), &endX);
  float y = std::strtof(value.c_str() + commaPos + 1, &endY);

  if (endX != value.c_str() && endY != value.c_str() + commaPos + 1) {
    result.set("found", true);
    result.set("x", x);
    result.set("y", y);
  }

  return result;
}

void PAGXView::updateZoomScaleAndOffset(float zoom, float offsetX, float offsetY) {
  bool zoomChanged = (std::abs(zoom - lastZoom) > 0.001f);
  // View changed, invalidate the zoom-out idle token so the next draw() re-evaluates.
  zoomedOutFrameSettled = false;
  if (scene) {
    auto* options = scene->getDisplayOptions();
    if (zoom <= 1.0f) {
      options->setSubtreeCacheMaxSize(static_cast<int>(ZOOMED_OUT_SUBTREE_CACHE_SIZE));
    } else {
      options->setSubtreeCacheMaxSize(0);
    }
  }

  if (zoomChanged) {
    if (!isZooming) {
      isZooming = true;
      updateAdaptiveTileRefinement();
    }

    // Direction tracking is owned by tgfx (the display list's zoom deadband accumulator). pagx
    // still needs a coarse direction signal for the in/out timeout split below
    // (ZOOM_IN_END_TIMEOUT_MS vs ZOOM_OUT_END_TIMEOUT_MS), so we derive it from the current
    // single-frame delta. Timeouts only fire after several hundred milliseconds of no updates, by
    // which point any single jittery frame's direction has long stopped mattering.
    isZoomingIn = (zoom > lastZoom);

    lastZoomUpdateTimestampMs = emscripten_get_now();
  }

  // zoom is the user zoom relative to the fit transform; applyMergedTransform() combines it with
  // fitScale and the centering offset before writing to PAGDisplayOptions.
  userZoom = zoom;
  userOffsetX = offsetX;
  userOffsetY = offsetY;
  applyMergedTransform();
  lastZoom = zoom;
}

void PAGXView::onZoomEnd() {
  if (!isZooming) {
    return;
  }

  isZooming = false;

  currentMaxTilesRefinedPerFrame = 1;
  if (scene) {
    scene->getDisplayOptions()->setMaxTilesRefinedPerFrame(currentMaxTilesRefinedPerFrame);
  }

  tryUpgradeTimestampMs = emscripten_get_now() + POST_ZOOM_UPGRADE_DELAY_MS;
}

bool PAGXView::draw() {
  if (device == nullptr) {
    return false;
  }

  double frameStartMs = emscripten_get_now();

  // Capture before rendering so the post-render logging block can identify the first frame.
  bool wasFirstFrame = !hasRenderedFirstFrame;

  // Per-stage timings in milliseconds. All capture sites are gated on DRAW_LOG_ENABLED so the
  // emscripten_get_now() probes are fully stripped from shipped builds, where the only consumers
  // (the slow-frame / first-frame breakdowns below) are compiled out.
  [[maybe_unused]] double surfaceMs = 0.0;
  [[maybe_unused]] double bgMs = 0.0;
  [[maybe_unused]] double renderMs = 0.0;
  [[maybe_unused]] double submitMs = 0.0;
  [[maybe_unused]] double unlockMs = 0.0;
  [[maybe_unused]] double surfaceStartMs = 0.0;
  [[maybe_unused]] double bgStartMs = 0.0;
  [[maybe_unused]] double renderStartMs = 0.0;
  [[maybe_unused]] double submitStartMs = 0.0;
  [[maybe_unused]] double unlockStartMs = 0.0;

  // Full render every frame: the fitSnapshot blit and idle short-circuit fast paths were dropped
  // in the PAGScene migration (to be reintroduced as a later performance pass). The locked context
  // is still needed each frame so flushPendingUploads can commit queued attachNativeImage()
  // textures.
  auto context = device->lockContext();
  if (context == nullptr) {
    return false;
  }

  // Drain queued attachNativeImage() requests inside the locked context so the texImage2D calls
  // go to PAGXView's own GL context. Run before any rendering work; the resulting backend-texture
  // images are picked up by the scene render through the Image-node rebuilds triggered inside
  // flushPendingUploads.
  flushPendingUploads(context);

  if constexpr (DRAW_LOG_ENABLED) {
    surfaceStartMs = emscripten_get_now();
  }
  if (surface == nullptr || surface->width() != canvasWidth || surface->height() != canvasHeight) {
    context->setCacheLimit(MAX_CACHE_LIMIT);
    context->setResourceExpirationFrames(EXPIRATION_FRAMES);
    tgfx::GLFrameBufferInfo glInfo = {};
    glInfo.id = 0;
    glInfo.format = GL_RGBA8;
    tgfx::BackendRenderTarget renderTarget(glInfo, canvasWidth, canvasHeight);
    surface = tgfx::Surface::MakeFrom(context, renderTarget, tgfx::ImageOrigin::BottomLeft);
    if (surface == nullptr) {
      device->unlock();
      return false;
    }
    // Wrap the surface so pagx::Record() renders into the same GL framebuffer. Recreated in
    // lockstep with `surface` so the wrapper never points at a stale render target.
    pagSurface = pagx::MakeFrom(surface);
  }
  if constexpr (DRAW_LOG_ENABLED) {
    surfaceMs = emscripten_get_now() - surfaceStartMs;
  }

  if constexpr (DRAW_LOG_ENABLED) {
    bgStartMs = emscripten_get_now();
  }
  auto canvas = surface->getCanvas();
  canvas->clear();
  if (backgroundVisible) {
    DrawSolidBackground(canvas, canvasWidth, canvasHeight, backgroundTGFXColor);
  } else {
    DrawBackground(canvas, canvasWidth, canvasHeight, 1.0f);
  }
  if constexpr (DRAW_LOG_ENABLED) {
    bgMs = emscripten_get_now() - bgStartMs;
  }

  // Record the scene on top of the manual background. autoClear=false preserves the background;
  // Record() flushes internally (the background draws queued on the same surface are captured in
  // that same flush) and returns a Recording we submit here. Record covers the flush cost, so a
  // separate flush timing is no longer tracked.
  if constexpr (DRAW_LOG_ENABLED) {
    renderStartMs = emscripten_get_now();
  }
  std::unique_ptr<tgfx::Recording> recording;
  if (scene != nullptr && pagSurface != nullptr) {
    recording = Record(context, scene, pagSurface, false);
  } else {
    // No scene yet (parsePAGX succeeded but buildLayers has not run): still flush the background
    // so the canvas shows the checkerboard instead of stale pixels.
    recording = context->flush();
  }
  if constexpr (DRAW_LOG_ENABLED) {
    renderMs = emscripten_get_now() - renderStartMs;
  }

  // Throttled GPU cache footprint probe. context->memoryUsage() reports total bytes held by the
  // tgfx ResourceCache (textures + buffers); purgeableBytes is the LRU-evictable subset. The
  // SDK-side accounting (external/thumb totals + entry counts + pending uploads) makes it possible
  // to tell apart "tgfx is full of backend textures we asked for" from "tgfx is full of tile cache
  // or other internal resources" by diffing against memoryUsage. imageCount is legacy: it counts
  // only pre-build synchronous loads via attachNativeImage, so the post-build backend-texture path
  // does not increment it; expect imageCount=0 in scenarios that exclusively use the post-build
  // flow.
  if ((gpuProbeCounter++ % GPU_PROBE_INTERVAL) == 0) {
    LOGI(
        "[MemProbe] tag=gpu.cache memoryUsage=%lluMB purgeable=%lluMB cacheLimit=%lluMB "
        "imageCount=%u externalFull=%zu/%lluMB(budget=%lluMB) "
        "thumb=%zu/%lluMB(budget=%lluMB) pendingUploads=%zu",
        static_cast<unsigned long long>(context->memoryUsage() / BYTES_PER_MB),
        static_cast<unsigned long long>(context->purgeableBytes() / BYTES_PER_MB),
        static_cast<unsigned long long>(context->cacheLimit() / BYTES_PER_MB),
        imageDecodedCount,
        externalTextures.size(),
        static_cast<unsigned long long>(externalTexturesTotalBytes / BYTES_PER_MB),
        static_cast<unsigned long long>(fullBudget / BYTES_PER_MB),
        thumbnailTextures.size(),
        static_cast<unsigned long long>(thumbnailTexturesTotalBytes / BYTES_PER_MB),
        static_cast<unsigned long long>(thumbnailBudget / BYTES_PER_MB),
        pendingUploads.size());
  }

  if (recording) {
    if constexpr (DRAW_LOG_ENABLED) {
      submitStartMs = emscripten_get_now();
    }
    context->submit(std::move(recording));
    if constexpr (DRAW_LOG_ENABLED) {
      submitMs = emscripten_get_now() - submitStartMs;
    }
    // Only count a rendered scene as the first frame. A background-only flush before buildLayers
    // must not satisfy the JS loading flow's firstFrameRendered() wait.
    if (scene != nullptr && !hasRenderedFirstFrame) {
      hasRenderedFirstFrame = true;
    }
  }

  // Free GL textures retired during this draw (eviction in enforceFullBudget, replacement upload
  // in flushPendingUploads, or document swap in parsePAGX). Must happen after flush+submit so the
  // command buffers that referenced the texture have all been built and queued; WebGL keeps
  // in-flight commands valid even after deleteTexture.
  drainPendingTextureDeletes();

  if constexpr (DRAW_LOG_ENABLED) {
    unlockStartMs = emscripten_get_now();
  }
  device->unlock();
  if constexpr (DRAW_LOG_ENABLED) {
    unlockMs = emscripten_get_now() - unlockStartMs;
  }

  double frameEndMs = emscripten_get_now();
  double frameDurationMs = frameEndMs - frameStartMs;

  // Log only frames that exceed the slow-frame threshold. The post-slow follow-up mechanism
  // below is kept as scaffolding for future debugging, but disabled here to keep the log clean.
  bool isSlowFrame = frameDurationMs > SLOW_FRAME_LOG_THRESHOLD_MS;
  bool shouldLogFrame = isSlowFrame;

  if constexpr (DRAW_LOG_ENABLED) {
    if (shouldLogFrame) {
      float fitScale = computeFitScale();
      // With the merged transform, PAGDisplayOptions carries the effective zoom (fitScale*userZoom)
      // and effective content offset directly, so read them back for the drawn-size estimate.
      float effectiveScale = fitScale;
      float offsetX = 0.0f;
      float offsetY = 0.0f;
      if (scene != nullptr) {
        auto* options = scene->getDisplayOptions();
        effectiveScale = options->getZoomScale();
        auto offset = options->getContentOffset();
        offsetX = offset.x;
        offsetY = offset.y;
      }
      // Expected drawn content size in canvas pixels (before viewport clipping). Anything much
      // larger than the canvas itself means most fragment work is wasted off-screen.
      float drawWidthPx = pagxWidth * effectiveScale;
      float drawHeightPx = pagxHeight * effectiveScale;
      LOGI(
          "[PAGXView] slow frame: total=%.2fms surface=%.2fms bg=%.2fms render=%.2fms "
          "submit=%.2fms unlock=%.2fms zoom=%.4f "
          "offset=(%.1f,%.1f) fitScale=%.4f effScale=%.4f canvas=(%dx%d) pagx=(%.0fx%.0f) "
          "drawPx=(%.0fx%.0f)",
          frameDurationMs, surfaceMs, bgMs, renderMs, submitMs, unlockMs,
          userZoom, offsetX, offsetY, fitScale, effectiveScale, canvasWidth,
          canvasHeight, pagxWidth, pagxHeight, drawWidthPx, drawHeightPx);
    }
  }

  // First-frame breakdown for telemetry.
  if constexpr (DRAW_LOG_ENABLED) {
    if (wasFirstFrame && hasRenderedFirstFrame) {
      LOGI(
          "[PAGXView] first frame: total=%.2fms surface=%.2fms bg=%.2fms render=%.2fms "
          "submit=%.2fms unlock=%.2fms",
          frameDurationMs, surfaceMs, bgMs, renderMs, submitMs, unlockMs);
    }
  }

  updatePerformanceState(frameDurationMs);

  if (isZooming && lastZoomUpdateTimestampMs > 0.0) {
    double currentTimeoutMs = isZoomingIn ? ZOOM_IN_END_TIMEOUT_MS : ZOOM_OUT_END_TIMEOUT_MS;
    double timeSinceLastUpdate = frameStartMs - lastZoomUpdateTimestampMs;

    if (timeSinceLastUpdate >= currentTimeoutMs) {
      onZoomEnd();
    }
  }

  if (!isZooming) {
    if (tryUpgradeTimestampMs > 0.0) {
      if (frameStartMs >= tryUpgradeTimestampMs) {
        if (!lastFrameSlow) {
          int targetCount = calculateTargetTileRefinement(lastZoom);
          currentMaxTilesRefinedPerFrame = targetCount;
          if (scene) {
            scene->getDisplayOptions()->setMaxTilesRefinedPerFrame(targetCount);
          }
          tryUpgradeTimestampMs = 0.0;
        } else {
          tryUpgradeTimestampMs = frameStartMs + UPGRADE_RETRY_DELAY_MS;
        }
      }
    } else {
      updateAdaptiveTileRefinement();
    }
  }

  // Backend-texture cache bookkeeping. The call runs after lockContext has been released
  // because it only mutates externalTextures and runtimeImage state, never GL state.
  // enforceFullBudget sweeps the full bucket against fullBudget; entries pushed past the
  // budget are removed and the next draw falls back to thumbnail.
  enforceFullBudget();
  // After eviction, any path that just lost its full texture is now a candidate for the
  // request scan. Emitting onTextureRequest on the same draw shortens the time-to-recovery
  // for the host: the download can start one frame earlier than if we waited until the next
  // frame to scan.
  scanAndRequestMissingTextures();
  // Bump the frame counter last so all bookkeeping above shares the same currentFrameIndex
  // and the next attachNativeImage() / draw() sees a strictly larger value.
  currentFrameIndex += 1;

  return true;
}

bool PAGXView::firstFrameRendered() const {
  return hasRenderedFirstFrame;
}

void PAGXView::resetForFreshCapture() {
  // On a page switch the TS side first calls parsePAGX/buildLayers, then gestureManager.init +
  // applyGestureState. Between these two steps RAF may run a full render and capture cached/fit in
  // the intermediate "not yet init" view state. This method lets the TS side discard those temporary
  // snapshots after init completes, reset the first-frame flag, and wait for a new first frame — the
  // snapshot captured then is the initial picture the user actually sees.
  fitSnapshot = nullptr;
  fitSnapshotPixelScale = DEFAULT_PIXEL_SCALE;
  hasRenderedFirstFrame = false;
  zoomedOutFrameSettled = false;
}

int PAGXView::width() const {
  return canvasWidth;
}

int PAGXView::height() const {
  return canvasHeight;
}

void PAGXView::updatePerformanceState(double frameDurationMs) {
  double now = emscripten_get_now();

  if (frameDurationMs > SLOW_FRAME_THRESHOLD_MS) {
    if (!lastFrameSlow) {
      frameHistory.clear();
      frameHistoryTotalTime = 0.0;
    }
    lastFrameSlow = true;
  }

  frameHistory.push_back({now, frameDurationMs});
  frameHistoryTotalTime += frameDurationMs;

  double windowStart = now - RECOVERY_WINDOW_MS;
  while (!frameHistory.empty() && frameHistory.front().timestampMs < windowStart) {
    frameHistoryTotalTime -= frameHistory.front().durationMs;
    frameHistory.pop_front();
  }

  if (lastFrameSlow && !frameHistory.empty()) {
    double avgTime = frameHistoryTotalTime / static_cast<double>(frameHistory.size());
    size_t minFrames = isZooming ? MIN_RECOVERY_FRAMES_ZOOM_END : MIN_RECOVERY_FRAMES_STATIC;
    if (avgTime <= SLOW_FRAME_THRESHOLD_MS && frameHistory.size() >= minFrames) {
      lastFrameSlow = false;
    }
  }
}

int PAGXView::calculateTargetTileRefinement(float zoom) const {
  // Performance-first strategy: disable tile refinement during zooming to reduce stutter
  if (isZooming) {
    return 0;
  }

  if (lastFrameSlow) {
    return 1;
  }

  if (zoom < 1.0f) {
    int count = static_cast<int>(zoom / TILE_REFINEMENT_ZOOM_DIVISOR) + 1;
    return std::clamp(count, 1, MAX_TILE_REFINEMENT);
  }

  return MAX_TILE_REFINEMENT;
}

void PAGXView::updateAdaptiveTileRefinement() {
  int targetCount = calculateTargetTileRefinement(lastZoom);

  if (targetCount != currentMaxTilesRefinedPerFrame) {
    currentMaxTilesRefinedPerFrame = targetCount;
    if (scene) {
      scene->getDisplayOptions()->setMaxTilesRefinedPerFrame(targetCount);
    }
  }
}

}  // namespace pagx
