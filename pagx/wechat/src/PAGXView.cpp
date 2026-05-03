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
#include <cstdlib>
#include <tgfx/gpu/Context.h>
#include <tgfx/gpu/opengl/GLDevice.h>
#include "GridBackground.h"
#include "utils/StringParser.h"
#include "tgfx/core/Data.h"
#include "tgfx/core/Image.h"
#include "tgfx/core/ImageCodec.h"
#include "tgfx/core/Rect.h"
#include "tgfx/core/Stream.h"
#include "tgfx/core/Typeface.h"
#include "tgfx/platform/Print.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/PAGXImporter.h"
#include "pagx/types/Data.h"
#include "utils/ImagePatternMatrixCalculator.h"

using namespace emscripten;

namespace pagx {

// Maximum GPU resource cache limit. Raised from the earlier 256MB after profiling showed the
// previous value caused frequent LRU eviction of mipmapped textures: once a texture was evicted,
// scrolling back to it triggered a full codec re-decode even though the data had not changed.
// 512MB headroom keeps all typical PAGX documents (roughly 50 images, ~9MB mipmapped each)
// resident so rebuild counts stay near zero during pan/zoom. Lower this again if memory-limited
// devices start OOM-crashing.
constexpr size_t MAX_CACHE_LIMIT = 512U * 1024 * 1024;
// GPU resource expiration in frames. The previous value (10 * 60 = 600 frames) caused textures
// to be evicted purely by age even when the cache had plenty of free bytes -- for example, an
// image that moves out of view during panning and returns 600 frames later (only a few seconds
// of continuous gesture input at 60fps+) would be decoded again despite never running into the
// cache's byte ceiling. Setting this to tgfx's MAX_EXPIRATION_FRAMES effectively disables the
// age-based eviction path, leaving the byte-capacity path as the only way a resource is
// reclaimed; the intent is that 512MB of headroom now reliably translates into no re-decodes
// during typical pan/zoom sessions. Revisit if real-world memory pressure requires an upper
// bound on resource lifetime again.
constexpr size_t EXPIRATION_FRAMES = 1000000;
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

// Copies data from a JavaScript Uint8Array into a tgfx::Data object.
static std::shared_ptr<tgfx::Data> GetDataFromEmscripten(const val& emscriptenData) {
  if (emscriptenData.isUndefined()) {
    return nullptr;
  }
  unsigned int length = emscriptenData["length"].as<unsigned int>();
  if (length == 0) {
    return nullptr;
  }
  auto buffer = new (std::nothrow) uint8_t[length];
  if (buffer) {
    auto memory = val::module_property("HEAPU8")["buffer"];
    auto memoryView = emscriptenData["constructor"].new_(
        memory, static_cast<unsigned int>(reinterpret_cast<uintptr_t>(buffer)), length);
    memoryView.call<void>("set", emscriptenData);
    return tgfx::Data::MakeAdopted(buffer, length, tgfx::Data::DeleteProc);
  }
  return nullptr;
}

// Copies data from a JavaScript Uint8Array into a pagx::Data object.
static std::shared_ptr<Data> GetPagxDataFromEmscripten(const val& emscriptenData) {
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
  return Data::MakeAdopt(buffer, length);
}

PAGXView::PAGXView(std::shared_ptr<tgfx::Device> device, int width, int height)
: device(device), _width(width), _height(height) {
  displayList.setRenderMode(tgfx::RenderMode::Tiled);
  // Keep zoomScalePrecision at tgfx's default (1000, i.e. 0.001 step) so pinch gestures feel
  // smooth. A previous experiment bucketed zoom to 0.05 steps to improve TileCache reuse, but
  // that produced visible stair-stepping during continuous zoom gestures while failing to fix
  // the underlying shape-rasterize cost (which lives in ResourceCache, not tile bucketing).
  // setAllowZoomBlur(true) already covers the transition period by upsampling the nearest
  // ready TileCache, so removing the coarse quantization costs little in practice.
  displayList.setAllowZoomBlur(true);
  displayList.setMaxTileCount(256);
  displayList.setMaxTilesRefinedPerFrame(currentMaxTilesRefinedPerFrame);
  // Per-frame throttle for tiles that need to be rasterized from scratch. Left at 0 (disabled)
  // on entry and whenever the last user zoom action was zoom-in, because post-zoom-in the
  // current-scale cache is nearly empty and any texture-invalidation wiping fallback caches
  // will leave blank holes until deferred tiles catch up. Flipped to a positive value only
  // after the user zooms out (updateZoomScaleAndOffset below), where plenty of fallback exists
  // from the prior larger scale and throttling trades off a brief blur for smoother frames.
  displayList.setTileThrottlePerFrame(0);
}

void PAGXView::registerFonts(const val& fontVal, const val& emojiFontVal) {
  std::vector<std::shared_ptr<tgfx::Typeface>> fallbackTypefaces;
  auto fontData = GetDataFromEmscripten(fontVal);
  if (fontData) {
    auto typeface = tgfx::Typeface::MakeFromData(fontData, 0);
    if (typeface) {
      fallbackTypefaces.push_back(std::move(typeface));
    }
  }
  auto emojiFontData = GetDataFromEmscripten(emojiFontVal);
  if (emojiFontData) {
    auto typeface = tgfx::Typeface::MakeFromData(emojiFontData, 0);
    if (typeface) {
      fallbackTypefaces.push_back(std::move(typeface));
    }
  }
  fontConfig.addFallbackTypefaces(std::move(fallbackTypefaces));
}

void PAGXView::loadPAGX(const val& pagxData) {
  parsePAGX(pagxData);
  buildLayers();
}

void PAGXView::parsePAGX(const val& pagxData) {
  // Release old resources early to reduce peak memory usage when switching pages.
  displayList.root()->removeChildren();
  contentLayer = nullptr;
  // Drop the per-document layer builder state before releasing the document itself; its
  // internal maps hold pagx::Layer* pointers that would dangle once `document` resets.
  builderSession = nullptr;
  document = nullptr;

  auto data = GetPagxDataFromEmscripten(pagxData);
  if (!data) {
    return;
  }
  document = PAGXImporter::FromXML(data->bytes(), data->size());
}

std::vector<std::string> PAGXView::getExternalFilePaths() const {
  if (!document) {
    return {};
  }
  return document->getExternalFilePaths();
}

bool PAGXView::loadFileData(const std::string& filePath, const val& fileData) {
  if (!document) {
    return false;
  }
  auto data = GetPagxDataFromEmscripten(fileData);
  if (!data) {
    return false;
  }
  return document->loadFileData(filePath, std::move(data));
}

bool PAGXView::loadFileDataAsNativeImage(const std::string& filePath, const val& nativeImage) {
  if (!document || filePath.empty() || !nativeImage.as<bool>()) {
    return false;
  }
  // Wrap the host-decoded asset as a tgfx::Image backed by NativeCodec. This path bypasses
  // wasm-side libwebp/libpng decoding entirely: sampling the image later will go through
  // WebImageBuffer::onMakeTexture which uploads straight from the OffscreenCanvas.
  auto codec = tgfx::ImageCodec::MakeFrom(nativeImage);
  if (!codec) {
    return false;
  }
  auto tgfxImage = tgfx::Image::MakeFrom(codec);
  if (!tgfxImage) {
    return false;
  }
  tgfxImage = tgfxImage->makeMipmapped(true);

  auto* imageNode = document->loadDecodedImage(filePath, tgfxImage);
  return imageNode != nullptr;
}

bool PAGXView::upgradeImageFromNative(const std::string& filePath, const val& nativeImage) {
  if (!document || !builderSession || filePath.empty() || !nativeImage.as<bool>()) {
    return false;
  }
  auto codec = tgfx::ImageCodec::MakeFrom(nativeImage);
  if (!codec) {
    return false;
  }
  auto tgfxImage = tgfx::Image::MakeFrom(codec);
  if (!tgfxImage) {
    return false;
  }
  tgfxImage = tgfxImage->makeMipmapped(true);

  // Attach the upgraded decoded image first so the session's rebuild sees the new pixels when
  // it re-runs convertImagePattern on the affected layers. loadDecodedImage does not clear the
  // filePath, so subsequent calls can replace it again (useful for multi-stage upgrades).
  if (document->loadDecodedImage(filePath, tgfxImage) == nullptr) {
    return false;
  }
  // ImagePattern matrices are baked in parsePAGX() against the initial image's pixel
  // dimensions. Now that a higher-resolution image has replaced the initial one, those baked
  // matrices would leave the upgraded image visibly misaligned (correct shape placement but
  // wrong scale/offset inside the shape). resolveImagePatternMatricesByFilePath is idempotent
  // thanks to the paint-transform cache in customData, so each upgrade call refreshes against
  // the newly attached decodedImage without losing the original paint transform.
  resolveImagePatternMatricesByFilePath(document.get(), filePath);
  // Regenerate every layer whose fill/stroke currently points at this filePath. The return
  // value is the number of tgfx layers whose contents were refreshed; zero means nothing in
  // the document references the path (callers typically treat this as a no-op success, but
  // we surface it as a boolean so JS can log it).
  return builderSession->rebuildForFilePath(filePath) > 0;
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
  if (!document || !builderSession || !contentLayer) {
    return result;
  }
  auto* rootLayer = contentLayer->root();
  if (!rootLayer) {
    return result;
  }

  auto count = filePathList["length"].as<unsigned>();
  for (unsigned i = 0; i < count; ++i) {
    std::string filePath = filePathList[i].as<std::string>();
    auto affectedLayers = document->findLayersByImageFilePath(filePath);
    if (affectedLayers.empty()) {
      result.set(filePath, val::null());
      continue;
    }
    tgfx::Rect unionBounds = tgfx::Rect::MakeEmpty();
    tgfx::Rect largestBounds = tgfx::Rect::MakeEmpty();
    float largestArea = 0.0f;
    bool hasAny = false;
    for (const auto* pagxLayer : affectedLayers) {
      auto tgfxLayers = builderSession->getTgfxLayers(pagxLayer);
      for (const auto& tgfxLayer : tgfxLayers) {
        if (!tgfxLayer) {
          continue;
        }
        // First getBounds(rootLayer) for a given tgfx Layer triggers tgfx's lazy localBounds
        // evaluation (walks down to content, multiplies up ancestor matrices); later calls
        // hit the cached value.
        auto bounds = tgfxLayer->getBounds(rootLayer);
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

  auto parseFloatAttr = [](const std::unordered_map<std::string, std::string>& data,
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
  };

  auto parseIntAttr = [](const std::unordered_map<std::string, std::string>& data,
                         const char* key, int fallback) {
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
  };

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
    float origWidth = parseFloatAttr(data, "orig-image-width", 0.0f);
    float origHeight = parseFloatAttr(data, "orig-image-height", 0.0f);
    float nodeWidth = parseFloatAttr(data, "node-width", 0.0f);
    float nodeHeight = parseFloatAttr(data, "node-height", 0.0f);
    int scaleMode = parseIntAttr(data, "image-scale-mode", 0);
    // 0.5 matches the default used by ImagePatternMatrixCalculator so TILE patterns that omit
    // the attribute behave identically in C++ and JS.
    float scaleFactor = parseFloatAttr(data, "scale-factor", 0.5f);

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

  // PAGX files exported by CoCraft reference images by hash, so actual pixel dimensions are
  // unavailable at export time. The exporter stores a normalized transform and scale parameters in
  // customData instead. Now that images are downloaded, combine them with actual pixel dimensions
  // to compute the final ImagePattern matrices. PAGX files generated by libpag embed image data
  // directly and do not need this step.
  // TODO: Integrate this into LayerBuilder so all PAGX renderers get it automatically.
  resolveAllImagePatternMatrices(document.get());

  document->applyLayout(&fontConfig);

  // Route through LayerBuilderSession so the build state survives into the progressive image
  // upgrade path; see upgradeImageFromNative() below.
  builderSession = std::make_unique<LayerBuilderSession>();
  auto buildResult = builderSession->build(document.get());
  contentLayer = buildResult.root;

  if (!contentLayer) {
    return;
  }
  hasRenderedFirstFrame = false;
  pagxWidth = document->width;
  pagxHeight = document->height;
  applyDocumentCustomData();
  displayList.root()->addChild(contentLayer);
  applyCenteringTransform();
}

void PAGXView::applyDocumentCustomData() {
  backgroundVisible = false;
  backgroundTGFXColor = tgfx::Color::FromRGBA(245, 245, 245);
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
        _boundsOriginX = value;
      }
    }
    auto originYIt = customData.find("bounds-origin-y");
    if (originYIt != customData.end()) {
      char* end = nullptr;
      float value = std::strtof(originYIt->second.c_str(), &end);
      if (end != originYIt->second.c_str()) {
        _boundsOriginY = value;
      }
    }
  }
}

void PAGXView::updateSize(int width, int height) {
  if (width <= 0 || height <= 0) {
    return;
  }

  _width = width;
  _height = height;
  surface = nullptr;

  if (contentLayer) {
    applyCenteringTransform();
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
  if (_width <= 0 || _height <= 0 || pagxWidth <= 0 || pagxHeight <= 0) {
    return 0.0f;
  }
  float scaleX = static_cast<float>(_width) / pagxWidth;
  float scaleY = static_cast<float>(_height) / pagxHeight;
  return std::min(scaleX, scaleY);
}

void PAGXView::applyCenteringTransform() {
  if (!contentLayer) {
    return;
  }
  float scale = computeFitScale();
  if (scale <= 0.0f) {
    return;
  }
  float offsetX = (static_cast<float>(_width) - pagxWidth * scale) * 0.5f;
  float offsetY = (static_cast<float>(_height) - pagxHeight * scale) * 0.5f;

  auto matrix = tgfx::Matrix::MakeTrans(offsetX, offsetY);
  matrix.preScale(scale, scale);

  contentLayer->setMatrix(matrix);
}

void PAGXView::setBoundsOrigin(float x, float y) {
  _boundsOriginX = x;
  _boundsOriginY = y;
  boundsOriginOverridden = true;
}

val PAGXView::getContentTransform() const {
  float fitScale = computeFitScale();
  float centerOffsetX = 0.0f;
  float centerOffsetY = 0.0f;
  if (fitScale > 0.0f) {
    centerOffsetX = (static_cast<float>(_width) - pagxWidth * fitScale) * 0.5f;
    centerOffsetY = (static_cast<float>(_height) - pagxHeight * fitScale) * 0.5f;
  }
  val result = val::object();
  result.set("boundsOriginX", _boundsOriginX);
  result.set("boundsOriginY", _boundsOriginY);
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
  if (zoom <= 1.0f) {
    displayList.setSubtreeCacheMaxSize(1024);
  } else {
    displayList.setSubtreeCacheMaxSize(0);
  }

  if (zoomChanged) {
    if (!isZooming) {
      isZooming = true;
      zoomStartValue = lastZoom;
      accumulatedZoomChange = 0.0f;
      updateAdaptiveTileRefinement();
    }

    // Accumulate zoom change to determine overall direction
    float currentChange = zoom - lastZoom;
    accumulatedZoomChange += currentChange;

    // Determine direction based on accumulated change (ignoring noise < 0.01)
    if (std::abs(accumulatedZoomChange) > 0.01f) {
      bool newZoomingIn = (accumulatedZoomChange > 0.0f);
      if (newZoomingIn != isZoomingIn) {
        isZoomingIn = newZoomingIn;
        // Flip the per-frame tile throttle to match the latest confirmed direction. Zoom-in
        // (and the static period that follows) disables throttling so any tile missing at the
        // current scale can be rasterized this frame instead of leaving a blank hole when
        // texture-invalidation already wiped the fallback source. Zoom-out re-enables
        // throttling because the prior (larger) scale's tiles downsample cleanly into
        // fallback and the occasional deferred tile only shows as a brief blur.
        displayList.setTileThrottlePerFrame(isZoomingIn ? 0 : 3);
      }
    }

    lastZoomUpdateTimestampMs = emscripten_get_now();
  }

  displayList.setZoomScale(zoom);
  displayList.setContentOffset(offsetX, offsetY);
  lastZoom = zoom;
}

void PAGXView::onZoomEnd() {
  if (!isZooming) {
    return;
  }

  isZooming = false;

  currentMaxTilesRefinedPerFrame = 1;
  displayList.setMaxTilesRefinedPerFrame(currentMaxTilesRefinedPerFrame);

  tryUpgradeTimestampMs = emscripten_get_now() + 200.0;
}

bool PAGXView::draw() {
  if (device == nullptr) {
    return false;
  }

  double frameStartMs = emscripten_get_now();

  // Snapshot before any state update so the post-render logging block can tell whether this
  // particular draw() call was the one that produced the first frame. Cleared along with the
  // other temporary [PAGXView] first-frame telemetry once the 20p path is validated.
  bool wasFirstFrame = !hasRenderedFirstFrame;

  // Per-stage timings in milliseconds. Remain 0 for stages that are skipped in this frame.
  double surfaceMs = 0.0;
  double bgMs = 0.0;
  double renderMs = 0.0;
  double flushMs = 0.0;
  double submitMs = 0.0;
  double unlockMs = 0.0;
  bool dirty = displayList.hasContentChanged();

  if (dirty) {
    auto context = device->lockContext();
    if (context == nullptr) {
      return false;
    }

    double surfaceStartMs = emscripten_get_now();
    if (surface == nullptr || surface->width() != _width || surface->height() != _height) {
      context->setCacheLimit(MAX_CACHE_LIMIT);
      context->setResourceExpirationFrames(EXPIRATION_FRAMES);
      tgfx::GLFrameBufferInfo glInfo = {};
      glInfo.id = 0;
      glInfo.format = GL_RGBA8;
      tgfx::BackendRenderTarget renderTarget(glInfo, _width, _height);
      surface = tgfx::Surface::MakeFrom(context, renderTarget, tgfx::ImageOrigin::BottomLeft);
      if (surface == nullptr) {
        device->unlock();
        return false;
      }
    }
    surfaceMs = emscripten_get_now() - surfaceStartMs;

    double bgStartMs = emscripten_get_now();
    auto canvas = surface->getCanvas();
    canvas->clear();

    if (backgroundVisible) {
      DrawSolidBackground(canvas, _width, _height, backgroundTGFXColor);
    } else {
      DrawBackground(canvas, _width, _height, 1.0f);
    }
    bgMs = emscripten_get_now() - bgStartMs;

    double renderStartMs = emscripten_get_now();
    displayList.render(surface.get(), false);
    renderMs = emscripten_get_now() - renderStartMs;

    double flushStartMs = emscripten_get_now();
    auto recording = context->flush();
    flushMs = emscripten_get_now() - flushStartMs;

    if (recording) {
      double submitStartMs = emscripten_get_now();
      context->submit(std::move(recording));
      submitMs = emscripten_get_now() - submitStartMs;
      if (!hasRenderedFirstFrame) {
        hasRenderedFirstFrame = true;
      }
    }

    double unlockStartMs = emscripten_get_now();
    device->unlock();
    unlockMs = emscripten_get_now() - unlockStartMs;
  }

  double frameEndMs = emscripten_get_now();
  double frameDurationMs = frameEndMs - frameStartMs;

  // Log only frames that exceed the slow-frame threshold. The post-slow follow-up mechanism
  // below is kept as scaffolding for future debugging, but disabled here to keep the log clean.
  bool isSlowFrame = frameDurationMs > SLOW_FRAME_LOG_THRESHOLD_MS;
  bool shouldLogFrame = isSlowFrame;

  if (shouldLogFrame) {
    const auto& offset = displayList.contentOffset();
    float fitScale = computeFitScale();
    float effectiveScale = fitScale * displayList.zoomScale();
    // Expected drawn content size in canvas pixels (before viewport clipping). Anything much
    // larger than the canvas itself means most fragment work is wasted off-screen.
    float drawWidthPx = pagxWidth * effectiveScale;
    float drawHeightPx = pagxHeight * effectiveScale;
    tgfx::PrintLog(
        "[PAGXView] slow frame: total=%.2fms surface=%.2fms bg=%.2fms render=%.2fms "
        "flush=%.2fms submit=%.2fms unlock=%.2fms dirty=%d zoom=%.4f quantized=%.4f "
        "offset=(%.1f,%.1f) fitScale=%.4f effScale=%.4f canvas=(%dx%d) pagx=(%.0fx%.0f) "
        "drawPx=(%.0fx%.0f)",
        frameDurationMs, surfaceMs, bgMs, renderMs, flushMs, submitMs, unlockMs, dirty ? 1 : 0,
        lastZoom, displayList.zoomScale(), offset.x, offset.y, fitScale, effectiveScale, _width,
        _height, pagxWidth, pagxHeight, drawWidthPx, drawHeightPx);
  }

  // Temporary: log the first frame breakdown unconditionally so the 20p full-fit rollout can
  // be validated against the previous 1:1-crop baseline. Remove together with the [PAGX Perf]
  // debug logs once the numbers stabilize.
  if (wasFirstFrame && hasRenderedFirstFrame) {
    tgfx::PrintLog(
        "[PAGXView] first frame: total=%.2fms surface=%.2fms bg=%.2fms render=%.2fms "
        "flush=%.2fms submit=%.2fms unlock=%.2fms",
        frameDurationMs, surfaceMs, bgMs, renderMs, flushMs, submitMs, unlockMs);
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
          displayList.setMaxTilesRefinedPerFrame(targetCount);
          tryUpgradeTimestampMs = 0.0;
        } else {
          tryUpgradeTimestampMs = frameStartMs + UPGRADE_RETRY_DELAY_MS;
        }
      }
    } else {
      updateAdaptiveTileRefinement();
    }
  }

  return true;
}

bool PAGXView::firstFrameRendered() const {
  return hasRenderedFirstFrame;
}

int PAGXView::width() const {
  return _width;
}

int PAGXView::height() const {
  return _height;
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
    int count = static_cast<int>(zoom / 0.33f) + 1;
    return std::clamp(count, 1, 3);
  }

  return 3;
}

void PAGXView::updateAdaptiveTileRefinement() {
  int targetCount = calculateTargetTileRefinement(lastZoom);

  if (targetCount != currentMaxTilesRefinedPerFrame) {
    currentMaxTilesRefinedPerFrame = targetCount;
    displayList.setMaxTilesRefinedPerFrame(targetCount);
  }
}

}  // namespace pagx
