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
#include <GLES3/gl3.h>
#include <emscripten/html5.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "pagx/PAGComposition.h"
#include "pagx/PAGLayer.h"
#include "pagx/PAGXImporter.h"
#include "pagx/PAGXNodeChannel.h"
#include "pagx/nodes/Animation.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/Element.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/tgfx.h"
#include "pagx/types/Data.h"
#include "tgfx/core/Data.h"
#include "tgfx/core/Surface.h"
#include "tgfx/core/Typeface.h"
#include "tgfx/gpu/opengl/webgl/WebGLWindow.h"

using namespace emscripten;

namespace pagx {

// The frame duration threshold in milliseconds above which a frame is considered slow.
static constexpr double SlowFrameThresholdMs = 32.0;
// The time window in milliseconds for averaging frame durations to detect performance recovery.
static constexpr double RecoveryWindowMs = 2000.0;
// The timeout in milliseconds to detect the end of a zoom-in gesture.
static constexpr double ZoomInEndTimeoutMs = 300.0;
// The timeout in milliseconds to detect the end of a zoom-out gesture.
static constexpr double ZoomOutEndTimeoutMs = 800.0;
// The delay in milliseconds before retrying a tile refinement upgrade after zoom ends.
static constexpr double UpgradeRetryDelayMs = 300.0;
// The initial delay in milliseconds before upgrading tile refinement after zoom ends.
static constexpr double InitialUpgradeDelayMs = 200.0;
// The minimum number of normal frames required to recover from slow state in static mode.
static constexpr size_t MinRecoveryFramesStatic = 20;
// The minimum number of normal frames required to recover from slow state after zoom ends.
static constexpr size_t MinRecoveryFramesZoomEnd = 10;

static uint8_t* CopyFromEmscripten(const val& emscriptenData, unsigned int* outLength) {
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

static std::shared_ptr<tgfx::Data> GetTGFXDataFromEmscripten(const val& emscriptenData) {
  unsigned int length = 0;
  auto buffer = CopyFromEmscripten(emscriptenData, &length);
  if (!buffer) {
    return nullptr;
  }
  return tgfx::Data::MakeAdopted(buffer, length, tgfx::Data::DeleteProc);
}

static std::shared_ptr<Data> GetPagxDataFromEmscripten(const val& emscriptenData) {
  unsigned int length = 0;
  auto buffer = CopyFromEmscripten(emscriptenData, &length);
  if (!buffer) {
    return nullptr;
  }
  return Data::MakeAdopt(buffer, length);
}

PAGXView::PAGXView(const std::string& canvasID) : canvasID(canvasID) {
}

void PAGXView::registerFonts(const val& fontVal, const val& emojiFontVal) {
  auto fontData = GetTGFXDataFromEmscripten(fontVal);
  if (fontData) {
    fontConfig.addFallbackFont(fontData->data(), fontData->size(), 0);
  }
  auto emojiFontData = GetTGFXDataFromEmscripten(emojiFontVal);
  if (emojiFontData) {
    fontConfig.addFallbackFont(emojiFontData->data(), emojiFontData->size(), 0);
  }
}

void PAGXView::loadPAGX(const val& pagxData) {
  parsePAGX(pagxData);
  buildLayers();
}

void PAGXView::parsePAGX(const val& pagxData) {
  document = nullptr;
  scene = nullptr;
  defaultTimeline = nullptr;
  defaultAnimation = nullptr;
  lastRecording = nullptr;
  lastAnimationTimeMs = -1.0;
  fallbackClockUs = 0;
  auto data = GetPagxDataFromEmscripten(pagxData);
  if (!data) {
    return;
  }
  document = PAGXImporter::FromXML(data->bytes(), data->size());
}

static int ParseDiagnosticLine(const std::string& error) {
  constexpr char LinePrefix[] = "line ";
  if (error.rfind(LinePrefix, 0) != 0) {
    return 1;
  }
  auto cursor = error.data() + sizeof(LinePrefix) - 1;
  auto line = std::strtol(cursor, nullptr, 10);
  return line > 0 ? static_cast<int>(line) : 1;
}

static std::string ParseDiagnosticMessage(const std::string& error) {
  auto separator = error.find(": ");
  return separator == std::string::npos ? error : error.substr(separator + 2);
}

emscripten::val PAGXView::validatePAGX(const val& pagxData) const {
  auto diagnostics = val::array();
  auto data = GetPagxDataFromEmscripten(pagxData);
  if (!data) {
    return diagnostics;
  }
  auto validationDocument = PAGXImporter::FromXML(data->bytes(), data->size());
  if (!validationDocument) {
    // XML well-formedness is diagnosed by the browser-side DOMParser. Reaching here with valid
    // XML means the document root is not PAGX, which is a schema error at the root element.
    auto diagnostic = val::object();
    diagnostic.set("message", "Root element must be 'pagx'.");
    diagnostic.set("line", 1);
    diagnostic.set("column", 1);
    diagnostics.call<void>("push", diagnostic);
    return diagnostics;
  }
  // Collapse repeated messages: a single schema slip (e.g. one legacy element repeated on
  // hundreds of lines) otherwise floods the editor with identical diagnostics. The first
  // occurrence keeps its line and absorbs the rest into a trailing count.
  std::vector<std::pair<size_t, std::string>> keptDiagnostics = {};
  std::unordered_map<std::string, size_t> firstIndexByMessage = {};
  std::unordered_map<size_t, size_t> suppressedCount = {};
  for (const auto& error : validationDocument->errors) {
    auto message = ParseDiagnosticMessage(error);
    auto it = firstIndexByMessage.find(message);
    if (it == firstIndexByMessage.end()) {
      firstIndexByMessage[message] = keptDiagnostics.size();
      keptDiagnostics.emplace_back(ParseDiagnosticLine(error), std::move(message));
    } else {
      ++suppressedCount[it->second];
    }
  }
  for (size_t i = 0; i < keptDiagnostics.size(); ++i) {
    auto message = keptDiagnostics[i].second;
    auto it = suppressedCount.find(i);
    if (it != suppressedCount.end()) {
      message += " (and " + std::to_string(it->second) + " more)";
    }
    auto diagnostic = val::object();
    diagnostic.set("message", message);
    diagnostic.set("line", keptDiagnostics[i].first);
    diagnostic.set("column", 1);
    diagnostics.call<void>("push", diagnostic);
  }
  return diagnostics;
}

std::vector<std::string> PAGXView::getExternalFilePaths() const {
  if (!document) {
    return {};
  }
  return document->getExternalFilePaths();
}

static bool LayerHasOwnText(const Layer* layer) {
  for (const auto* element : layer->contents) {
    if (element != nullptr &&
        (element->nodeType() == NodeType::Text || element->nodeType() == NodeType::TextBox)) {
      return true;
    }
  }
  return false;
}

// Returns the laid-out box of the layer's own text element in the layer's local coordinates (the
// space the text positions its glyphs in), or an empty rect when the layer has no text of its own
// or has not been laid out yet.
static Rect TextBoxOf(const Layer* layer) {
  for (const auto* element : layer->contents) {
    if (element == nullptr) {
      continue;
    }
    if (element->nodeType() == NodeType::TextBox) {
      return static_cast<const TextBox*>(element)->layoutBounds();
    }
    if (element->nodeType() == NodeType::Text) {
      return static_cast<const Text*>(element)->layoutBounds();
    }
  }
  return {};
}

static bool SurfaceToLayerLocal(const PAGLayer& layer, float surfaceX, float surfaceY,
                                Point* localPoint) {
  auto matrix = layer.getGlobalMatrix();
  float determinant = matrix.a * matrix.d - matrix.b * matrix.c;
  if (determinant == 0.0f) {
    return false;
  }
  float dx = surfaceX - matrix.tx;
  float dy = surfaceY - matrix.ty;
  localPoint->x = (matrix.d * dx - matrix.c * dy) / determinant;
  localPoint->y = (matrix.a * dy - matrix.b * dx) / determinant;
  return true;
}

static bool RectContainsPoint(const Rect& rect, const Point& point) {
  return point.x >= rect.x && point.x <= rect.x + rect.width && point.y >= rect.y &&
         point.y <= rect.y + rect.height;
}

// Finds the runtime instance of node inside one composition instance, without descending into
// nested composition instances (a mask always refers to a layer of the same composition).
static std::shared_ptr<PAGLayer> FindLayerInComposition(const std::shared_ptr<PAGLayer>& layer,
                                                        const Layer* node) {
  for (const auto& child : layer->getChildren()) {
    if (child == nullptr) {
      continue;
    }
    if (child->getNode() == node) {
      return child;
    }
    if (child->layerType() == LayerType::Composition) {
      continue;
    }
    auto found = FindLayerInComposition(child, node);
    if (found != nullptr) {
      return found;
    }
  }
  return nullptr;
}

// True when the authored visibility, scroll clip and mask of the layer and all of its ancestors
// let the point through. The core already applies the runtime equivalents to its shape hits; this
// check only gates the text boxes the host adds on top of them.
static bool AuthoredChainAcceptsPoint(const std::shared_ptr<PAGLayer>& layer, float surfaceX,
                                      float surfaceY) {
  for (auto walker = layer; walker != nullptr; walker = walker->getParent()) {
    const auto* node = walker->getNode();
    if (node == nullptr) {
      continue;
    }
    if (!node->visible) {
      return false;
    }
    // Layout converts clipToBounds into scrollRect (Layer::updateScrollRect), so a single check
    // covers both authored forms.
    if (node->hasScrollRect) {
      Point localPoint = {};
      if (!SurfaceToLayerLocal(*walker, surfaceX, surfaceY, &localPoint) ||
          !RectContainsPoint(node->scrollRect, localPoint)) {
        return false;
      }
    }
    if (node->mask != nullptr) {
      // The masked layer's node lives in the composition that holds its parent, which is where
      // the mask layer is searched for.
      auto composition = walker->getParent();
      while (composition != nullptr && composition->layerType() != LayerType::Composition) {
        composition = composition->getParent();
      }
      auto maskLayer =
          composition != nullptr ? FindLayerInComposition(composition, node->mask) : nullptr;
      if (maskLayer != nullptr && !maskLayer->hitTestPoint(surfaceX, surfaceY, true)) {
        return false;
      }
    }
  }
  return true;
}

static bool TextBoxContainsPoint(const PAGLayer& layer, float surfaceX, float surfaceY) {
  const auto* node = layer.getNode();
  if (node == nullptr) {
    return false;
  }
  auto box = TextBoxOf(node);
  if (box.isEmpty()) {
    return false;
  }
  Point localPoint = {};
  return SurfaceToLayerLocal(layer, surfaceX, surfaceY, &localPoint) &&
         RectContainsPoint(box, localPoint);
}

// Walks the runtime tree in reverse paint order (later children first, a layer's children before
// the layer itself) and returns the first layer that is either the core's top-most shape hit or a
// text layer whose laid-out box contains the point. Selecting text by its box is an editor policy:
// the core reports exact glyph outlines, which reject the gaps between glyphs and make a text
// block feel unclickable, so the host widens text to its box here without changing what the core
// reports to other callers.
static std::shared_ptr<PAGLayer> FindTopMostSelectable(const std::shared_ptr<PAGLayer>& layer,
                                                       const PAGLayer* shapeHit, float surfaceX,
                                                       float surfaceY) {
  const auto& children = layer->getChildren();
  for (auto item = children.rbegin(); item != children.rend(); ++item) {
    if (*item == nullptr) {
      continue;
    }
    auto found = FindTopMostSelectable(*item, shapeHit, surfaceX, surfaceY);
    if (found != nullptr) {
      return found;
    }
  }
  if (layer.get() == shapeHit) {
    return layer;
  }
  if (TextBoxContainsPoint(*layer, surfaceX, surfaceY) &&
      AuthoredChainAcceptsPoint(layer, surfaceX, surfaceY)) {
    return layer;
  }
  return nullptr;
}

// Resolves a surface point to the runtime layer a host should select: the top-most layer under the
// point (a text layer counts over its whole laid-out box), walked up to the first Composition whose
// source node belongs to the given document, so a click inside a <Layer composition="@X"> instance
// selects the reference layer instead of the internal definition. Compositions built from an
// embedded external document are skipped (their nodes are numbered against a different document),
// and a hit that lands entirely inside one selects nothing rather than reporting a node the host
// cannot map. The core runtime keeps no source-node -> runtime-layer map, so this resolution
// belongs to the host, which owns the source document. Returns nullptr when nothing should be
// selected.
static std::shared_ptr<PAGLayer> ResolveSelectionTarget(const std::shared_ptr<PAGScene>& scene,
                                                        PAGXDocument* document, float surfaceX,
                                                        float surfaceY) {
  if (scene == nullptr || document == nullptr) {
    return nullptr;
  }
  auto root = scene->rootComposition();
  if (root == nullptr) {
    return nullptr;
  }
  auto layers = scene->getLayersUnderPoint(surfaceX, surfaceY);
  const PAGLayer* shapeHit = layers.empty() ? nullptr : layers.front().get();
  auto target = FindTopMostSelectable(root, shapeHit, surfaceX, surfaceY);
  if (target == nullptr) {
    return nullptr;
  }
  for (auto walker = target; walker != nullptr; walker = walker->getParent()) {
    const auto* node = walker->getNode();
    if (walker->layerType() == LayerType::Composition && node != nullptr &&
        document->ownsNode(node)) {
      target = walker;
      break;
    }
  }
  const auto* node = target->getNode();
  if (node == nullptr || !document->ownsNode(node)) {
    return nullptr;
  }
  return target;
}

emscripten::val PAGXView::hitTest(float surfaceX, float surfaceY) {
  auto target = ResolveSelectionTarget(scene, document.get(), surfaceX, surfaceY);
  if (target == nullptr) {
    return emscripten::val::null();
  }
  // The host owns the source document, so the source-side fields come from the node the runtime
  // layer points at; the runtime itself only supplies the handle and its on-screen rect.
  const auto* node = target->getNode();
  auto bounds = scene->getTightGlobalBounds(target);
  auto obj = emscripten::val::object();
  obj.set("index", node->index);
  obj.set("startLine", node->sourceLine);
  obj.set("endLine", node->endLine);
  if (bounds.isEmpty()) {
    obj.set("bounds", emscripten::val::null());
  } else {
    auto boundsObj = emscripten::val::object();
    boundsObj.set("x", bounds.x);
    boundsObj.set("y", bounds.y);
    boundsObj.set("w", bounds.width);
    boundsObj.set("h", bounds.height);
    obj.set("bounds", boundsObj);
  }
  return obj;
}

emscripten::val PAGXView::getNodeSourceMap() const {
  auto array = emscripten::val::array();
  if (document == nullptr) {
    return array;
  }
  for (const auto& source : document->getNodeSourceMap()) {
    auto entry = emscripten::val::object();
    entry.set("index", source.index);
    entry.set("startLine", source.startLine);
    entry.set("endLine", source.endLine);
    entry.set("nodeType", static_cast<int>(source.nodeType));
    auto channels = emscripten::val::array();
    for (const auto& c : source.channels) {
      channels.call<void>("push", c);
    }
    entry.set("channels", channels);
    array.call<void>("push", entry);
  }
  return array;
}

// Collects every runtime layer instance built from the given source node by walking the runtime
// tree in pre-order. The core runtime intentionally does not maintain a source-node -> instance
// map, so the host (which holds the source document) owns the node-to-runtime correspondence.
// A pre-order walk matches the tree's construction order, so instances come back in the same
// order the runtime tree declares them.
static void CollectLayersForNode(const std::shared_ptr<PAGLayer>& layer, const Layer* node,
                                 std::vector<std::shared_ptr<PAGLayer>>& out) {
  if (layer == nullptr) {
    return;
  }
  if (layer->getNode() == node) {
    out.push_back(layer);
  }
  for (const auto& child : layer->getChildren()) {
    CollectLayersForNode(child, node, out);
  }
}

emscripten::val PAGXView::getNodeBounds(int index) const {
  if (document == nullptr || index < 0 || index >= static_cast<int>(document->nodes.size())) {
    return emscripten::val::null();
  }
  Node* n = document->nodes[index].get();
  // hitTest granularity is Layer-level, so bounds are only meaningful for Layer nodes.
  if (n->nodeType() != NodeType::Layer) {
    return emscripten::val::null();
  }
  if (scene == nullptr) {
    return emscripten::val::null();
  }
  auto root = scene->rootComposition();
  if (root == nullptr) {
    return emscripten::val::null();
  }
  auto* layerNode = static_cast<Layer*>(n);
  std::vector<std::shared_ptr<PAGLayer>> instances = {};
  CollectLayersForNode(root, layerNode, instances);
  // Selection-outline slack for FreeType's anti-aliasing overshoot: per-glyph tight bounds stop
  // at the geometric baseline while the rasterizer paints slightly below it, visually clipping
  // the last row of ink. Extending the bottom by 5% of the layer height covers the overshoot,
  // with a 0.5 root-space floor scaled by the current zoom. This is presentation-only slack,
  // kept here so the bounds query itself stays tight. Only glyphs overshoot, so layers without
  // their own text (shapes, images, containers) keep their exact bounds.
  bool applyTextSlack = LayerHasOwnText(layerNode);
  float zoom = contentScale * userZoom;
  auto array = emscripten::val::array();
  size_t visibleCount = 0;
  for (auto& layer : instances) {
    auto rect = scene->getTightGlobalBounds(layer);
    if (rect.isEmpty()) {
      continue;
    }
    if (applyTextSlack) {
      rect.height += std::max(0.5f * zoom, rect.height * 0.05f);
    }
    auto obj = emscripten::val::object();
    obj.set("x", rect.x);
    obj.set("y", rect.y);
    obj.set("w", rect.width);
    obj.set("h", rect.height);
    array.call<void>("push", obj);
    ++visibleCount;
  }
  if (visibleCount == 0) {
    return emscripten::val::null();
  }
  return array;
}

bool PAGXView::setNodeChannel(int index, const std::string& channel, const std::string& value) {
  if (document == nullptr || index < 0 || index >= static_cast<int>(document->nodes.size())) {
    return false;
  }
  Node* node = document->nodes[index].get();
  if (node == nullptr || !ChannelExists(node->nodeType(), channel)) {
    return false;
  }
  if (!SetNodeChannelFromString(node, channel, value)) {
    return false;
  }
  // RequiresLayout tells whether the edit only shows up after a layout pass; a render-only refresh
  // (layoutChanged=false) is the cheap path for edits that cannot move geometry (alpha/color/...).
  bool layoutChanged = RequiresLayout(node->nodeType(), channel);
  document->notifyChange({node}, layoutChanged);
  presentImmediately = true;
  return true;
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

void PAGXView::buildLayers() {
  if (!document) {
    return;
  }
  document->applyLayout(&fontConfig);
  scene = PAGScene::Make(document);
  if (scene == nullptr) {
    return;
  }
  defaultTimeline = scene->getDefaultTimeline();
  defaultAnimation = nullptr;
  if (defaultTimeline != nullptr && defaultTimeline->type() == TimelineType::Animation) {
    defaultAnimation = std::static_pointer_cast<PAGAnimation>(defaultTimeline);
  }
  playing = true;
  lastAnimationTimeMs = -1.0;
  fallbackClockUs = 0;
  pagxWidth = scene->width();
  pagxHeight = scene->height();
  applySceneDisplayOptions();
  updateContentTransform();
  presentImmediately = true;
}

void PAGXView::advanceTimelines(double frameStartMs) {
  int64_t deltaUs = 0;
  if (lastAnimationTimeMs >= 0.0) {
    deltaUs = static_cast<int64_t>(std::max(0.0, frameStartMs - lastAnimationTimeMs) * 1000.0);
  }
  lastAnimationTimeMs = frameStartMs;
  if (deltaUs <= 0) {
    return;
  }
  if (playing) {
    if (defaultAnimation != nullptr) {
      advanceAnimationUnit(defaultAnimation, deltaUs);
    } else if (defaultTimeline != nullptr) {
      // Non-animation timelines (state machines) have no queryable duration to gate; drive
      // as-is and accumulate the fallback clock so the playback bar stays functional.
      advanceFallbackTimeline(deltaUs);
    }
    // Drive the scene inside the playing gate so pausing freezes the whole picture: this advances
    // the auto-playing nested compositions, which would otherwise keep animating (and keep
    // hasContentChanged() true) even while the top-level animation is paused.
    if (scene != nullptr) {
      scene->advanceAndApply(deltaUs);
      // Track the fallback clock for documents whose only animations are nested (no top-level
      // timeline at all), so frame stepping has a position reference.
      if (defaultTimeline == nullptr) {
        fallbackClockUs += deltaUs;
      }
    }
  }
}

// Drives one animation by delta with the player-level loop policy: the engine keeps the in-cycle
// motion (including PingPong mirroring) and this method decides what happens at a cycle boundary
// based on loopEnabled. The boundary is tracked on the linear playbackPosition rather than
// currentTime. currentTime is the folded in-cycle phase, which for PingPong is a triangle wave
// that turns around at the half point; that turn made a single pass stop after only the forward
// half. playbackPosition rises across one full loop period (duration for Once/Loop, 2 * duration
// for PingPong) and only wraps back down when a complete pass finishes, so it marks the true end
// for every mode.
void PAGXView::advanceAnimationUnit(const std::shared_ptr<PAGAnimation>& animation,
                                    int64_t deltaUs) {
  int64_t duration = animation->duration();
  int64_t before = animation->playbackPosition();
  bool changed = animation->advanceAndApply(deltaUs);
  int64_t after = animation->playbackPosition();
  if (!changed) {
    // A Once file clamps at the last frame and stops changing. Rewind to the head either way;
    // when looping keep playing for the next cycle, otherwise park on the first frame so a
    // finished single pass resets to the start instead of freezing on the last frame.
    if (duration > 0) {
      animation->setCurrentTime(0);
      animation->apply();
    }
    if (!loopEnabled) {
      playing = false;
    }
  } else if (!loopEnabled && duration > 0 && after < before) {
    // A Loop/PingPong file crossed its period boundary while the user wants a single pass. The
    // linear position climbs monotonically to the period and then wraps back down, so the first
    // backward step marks one completed pass: rewind to the first frame and stop there. For
    // PingPong the period is 2 * duration, so this fires only after the full forward-and-back
    // trip, not at the half-way turning point. Here duration > 0 is only a validity guard; the
    // actual boundary is the playbackPosition period, which already accounts for PingPong's
    // doubled span, so this condition does not need the period value itself.
    animation->setCurrentTime(0);
    animation->apply();
    playing = false;
  }
}

void PAGXView::updateSize() {
  if (!ensureWindow()) {
    return;
  }
  int canvasWidth = 0;
  int canvasHeight = 0;
  emscripten_get_canvas_element_size(canvasID.c_str(), &canvasWidth, &canvasHeight);
  syncSurfaceSize(canvasWidth, canvasHeight);
}

bool PAGXView::ensureWindow() {
  if (window == nullptr) {
    window = tgfx::WebGLWindow::MakeFrom(canvasID);
  }
  return window != nullptr && window->getDevice() != nullptr;
}

void PAGXView::syncSurfaceSize(int canvasWidth, int canvasHeight) {
  if (!ensureWindow() || canvasWidth <= 0 || canvasHeight <= 0) {
    return;
  }
  if (tgfxSurface != nullptr && lastSurfaceWidth == canvasWidth &&
      lastSurfaceHeight == canvasHeight) {
    return;
  }
  auto device = window->getDevice();
  auto context = device->lockContext();
  if (context == nullptr) {
    return;
  }
  tgfxSurface = tgfx::Surface::MakeFrom(context, window);
  device->unlock();
  if (tgfxSurface == nullptr) {
    return;
  }
  pagSurface = pagx::MakeFrom(tgfxSurface);
  lastSurfaceWidth = canvasWidth;
  lastSurfaceHeight = canvasHeight;
  updateContentTransform();
  presentImmediately = true;
}

void PAGXView::updateContentTransform() {
  if (lastSurfaceWidth <= 0 || lastSurfaceHeight <= 0) {
    return;
  }
  if (pagxWidth <= 0 || pagxHeight <= 0) {
    return;
  }
  float scaleX = static_cast<float>(lastSurfaceWidth) / pagxWidth;
  float scaleY = static_cast<float>(lastSurfaceHeight) / pagxHeight;
  contentScale = std::min(scaleX, scaleY);
  contentOffsetX = (static_cast<float>(lastSurfaceWidth) - pagxWidth * contentScale) * 0.5f;
  contentOffsetY = (static_cast<float>(lastSurfaceHeight) - pagxHeight * contentScale) * 0.5f;
  applyDisplayTransform();
}

void PAGXView::applyDisplayTransform() {
  if (scene == nullptr) {
    return;
  }
  scene->getDisplayOptions()->setZoomScale(contentScale * userZoom);
  scene->getDisplayOptions()->setContentOffset(contentOffsetX * userZoom + userOffsetX,
                                               contentOffsetY * userZoom + userOffsetY);
}

void PAGXView::applySceneDisplayOptions() {
  if (scene == nullptr) {
    return;
  }
  auto options = scene->getDisplayOptions();
  if (defaultTimeline != nullptr) {
    // Tiled caches are invalidated on every animated frame, so tiling only adds overhead during
    // playback. Partial mode redraws just the dirty regions and keeps animated frames smooth.
    options->setRenderMode(PAGRenderMode::Partial);
    return;
  }
  options->setRenderMode(PAGRenderMode::Tiled);
  options->setTileUpdateMode(PAGTileUpdateMode::Smooth);
  options->setMaxTileCount(512);
  options->setMaxTilesRefinedPerFrame(currentMaxTilesRefinedPerFrame);
}

void PAGXView::updateZoomScaleAndOffset(float zoom, float offsetX, float offsetY) {
  if (scene != nullptr) {
    if (zoom <= 1.0f) {
      scene->getDisplayOptions()->setSubtreeCacheMaxSize(1024);
    } else {
      scene->getDisplayOptions()->setSubtreeCacheMaxSize(0);
    }
  }

  bool zoomChanged = (std::abs(zoom - lastZoom) > 0.001f);
  if (zoomChanged) {
    if (!isZooming) {
      isZooming = true;
      accumulatedZoomChange = 0.0f;
      updateAdaptiveTileRefinement();
    }
    float currentChange = zoom - lastZoom;
    accumulatedZoomChange += currentChange;
    if (std::abs(accumulatedZoomChange) > 0.01f) {
      isZoomingIn = (accumulatedZoomChange > 0.0f);
    }
    lastZoomUpdateTimestampMs = emscripten_get_now();
  }

  userZoom = zoom;
  userOffsetX = offsetX;
  userOffsetY = offsetY;
  applyDisplayTransform();
  presentImmediately = true;
  lastZoom = zoom;
}

void PAGXView::setBackgroundColor(float red, float green, float blue, float alpha) {
  useCustomBackgroundColor = true;
  customBackgroundColor = {std::clamp(red, 0.0f, 1.0f), std::clamp(green, 0.0f, 1.0f),
                           std::clamp(blue, 0.0f, 1.0f), std::clamp(alpha, 0.0f, 1.0f)};
  if (scene != nullptr) {
    scene->getDisplayOptions()->setBackgroundColor(customBackgroundColor);
  }
  presentImmediately = true;
}

void PAGXView::clearBackgroundColor() {
  useCustomBackgroundColor = false;
  customBackgroundColor = {};
  if (scene != nullptr) {
    scene->getDisplayOptions()->setBackgroundColor(customBackgroundColor);
  }
  presentImmediately = true;
}

void PAGXView::draw() {
  if (!ensureWindow() || scene == nullptr) {
    return;
  }
  double frameStartMs = emscripten_get_now();
  advanceTimelines(frameStartMs);
  int currentCanvasWidth = 0;
  int currentCanvasHeight = 0;
  emscripten_get_canvas_element_size(canvasID.c_str(), &currentCanvasWidth, &currentCanvasHeight);
  syncSurfaceSize(currentCanvasWidth, currentCanvasHeight);
  if (tgfxSurface == nullptr) {
    return;
  }
  // Dirty gate: skip the Record()/submit pass on idle frames. advanceTimelines() above already
  // refreshed the scene's content-changed flag, so hasContentChanged() reflects the latest state
  // (a running animation or in-progress tile refinement keeps it true). presentImmediately forces a
  // render after loads/resizes/zoom/background changes; lastRecording must still be flushed when
  // present, otherwise the double-buffered frame it holds would be dropped.
  if (!presentImmediately && lastRecording == nullptr && !scene->hasContentChanged()) {
    return;
  }
  if (useCustomBackgroundColor) {
    scene->getDisplayOptions()->setBackgroundColor(customBackgroundColor);
  } else {
    scene->getDisplayOptions()->setBackgroundColor({});
  }
  auto device = window->getDevice();
  auto context = device->lockContext();
  if (context != nullptr) {
    auto recording = pagx::Record(context, scene, pagSurface, true);
    if (presentImmediately) {
      // Force the freshest frame on screen right now. Drop whatever frame is still parked in
      // lastRecording so the deferred (older) content cannot resurface one frame later.
      presentImmediately = false;
      lastRecording = nullptr;
      if (recording) {
        context->submit(std::move(recording));
      }
    } else {
      // Double buffer: park this frame's recording and submit the one deferred from the previous
      // frame, giving the GPU an extra frame to finish. When the scene content is unchanged,
      // Record() returns null; swapping that null into lastRecording lets the dirty gate resume
      // skipping idle frames once the last deferred frame has been flushed out.
      std::swap(lastRecording, recording);
      if (recording) {
        context->submit(std::move(recording));
      }
    }
    device->unlock();
  }

  double frameEndMs = emscripten_get_now();
  double frameDurationMs = frameEndMs - frameStartMs;
  updatePerformanceState(frameDurationMs);

  if (isZooming && lastZoomUpdateTimestampMs > 0.0) {
    double currentTimeoutMs = isZoomingIn ? ZoomInEndTimeoutMs : ZoomOutEndTimeoutMs;
    double timeSinceLastUpdate = frameStartMs - lastZoomUpdateTimestampMs;
    if (timeSinceLastUpdate >= currentTimeoutMs) {
      onZoomEnd();
    }
  }

  if (!isZooming && tryUpgradeTimestampMs > 0.0) {
    if (frameStartMs >= tryUpgradeTimestampMs) {
      if (!lastFrameSlow) {
        int targetCount = calculateTargetTileRefinement(lastZoom);
        currentMaxTilesRefinedPerFrame = targetCount;
        if (scene != nullptr) {
          scene->getDisplayOptions()->setMaxTilesRefinedPerFrame(targetCount);
        }
        tryUpgradeTimestampMs = 0.0;
      } else {
        tryUpgradeTimestampMs = frameStartMs + UpgradeRetryDelayMs;
      }
    }
  } else if (!isZooming) {
    updateAdaptiveTileRefinement();
  }
}

void PAGXView::onZoomEnd() {
  if (!isZooming) {
    return;
  }
  isZooming = false;
  currentMaxTilesRefinedPerFrame = 1;
  if (scene != nullptr) {
    scene->getDisplayOptions()->setMaxTilesRefinedPerFrame(currentMaxTilesRefinedPerFrame);
  }
  tryUpgradeTimestampMs = emscripten_get_now() + InitialUpgradeDelayMs;
}

void PAGXView::updatePerformanceState(double frameDurationMs) {
  double now = emscripten_get_now();
  if (frameDurationMs > SlowFrameThresholdMs) {
    if (!lastFrameSlow) {
      frameHistory.clear();
      frameHistoryTotalTime = 0.0;
    }
    lastFrameSlow = true;
  }
  frameHistory.push_back({now, frameDurationMs});
  frameHistoryTotalTime += frameDurationMs;
  double windowStart = now - RecoveryWindowMs;
  while (!frameHistory.empty() && frameHistory.front().timestampMs < windowStart) {
    frameHistoryTotalTime -= frameHistory.front().durationMs;
    frameHistory.pop_front();
  }
  if (lastFrameSlow && !frameHistory.empty()) {
    double avgTime = frameHistoryTotalTime / static_cast<double>(frameHistory.size());
    size_t minFrames = isZooming ? MinRecoveryFramesZoomEnd : MinRecoveryFramesStatic;
    if (avgTime <= SlowFrameThresholdMs && frameHistory.size() >= minFrames) {
      lastFrameSlow = false;
    }
  }
}

int PAGXView::calculateTargetTileRefinement(float zoom) const {
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
    if (scene != nullptr) {
      scene->getDisplayOptions()->setMaxTilesRefinedPerFrame(targetCount);
    }
  }
}

void PAGXView::advanceFallbackTimeline(int64_t deltaUs) {
  defaultTimeline->advanceAndApply(deltaUs);
  fallbackClockUs += deltaUs;
}

void PAGXView::play() {
  playing = true;
}

void PAGXView::pause() {
  playing = false;
}

bool PAGXView::isPlaying() const {
  return playing;
}

int64_t PAGXView::currentTimeMicros() const {
  if (defaultAnimation != nullptr) {
    // Report the linear timeline position so the UI progress bar advances monotonically across one
    // full loop period. For PingPong this treats a complete forward-and-back pass as one timeline
    // (0 -> 2 * duration) instead of currentTime()'s folded triangle-wave phase, which would make
    // the progress bar run backward on the return half.
    return defaultAnimation->playbackPosition();
  }
  if (defaultTimeline != nullptr) {
    // Duration-less timeline (e.g. a state machine): the fallback clock is the only position.
    return fallbackClockUs;
  }
  return 0;
}

int64_t PAGXView::durationMicros() const {
  if (defaultAnimation != nullptr) {
    // Match currentTimeMicros(): expose the full loop period so PingPong reports 2 * duration (one
    // complete round trip) and the progress bar / time / frame readouts stay consistent.
    return defaultAnimation->playbackPeriod();
  }
  return 0;
}

float PAGXView::frameRate() const {
  if (defaultAnimation != nullptr) {
    return defaultAnimation->frameRate();
  }
  if (hasTimeline()) {
    // Duration-less timelines (state machines, nested-only animations) have no single frame
    // rate. Return the PAGX default so frame stepping still has a sane step unit.
    return 60.0f;
  }
  return 0.0f;
}

// Returns true when any composition in the runtime subtree has spawned timelines, i.e. there is
// animated content even without a top-level default timeline (all animations are
// composition-scoped and driven purely through <Timelines> references).
static bool HasAnySpawnedTimeline(const std::shared_ptr<PAGLayer>& layer) {
  if (layer == nullptr) {
    return false;
  }
  if (layer->layerType() == LayerType::Composition &&
      static_cast<PAGComposition*>(layer.get())->hasTimelines()) {
    return true;
  }
  for (const auto& child : layer->getChildren()) {
    if (HasAnySpawnedTimeline(child)) {
      return true;
    }
  }
  return false;
}

bool PAGXView::hasTimeline() const {
  return defaultTimeline != nullptr ||
         (scene != nullptr && HasAnySpawnedTimeline(scene->rootComposition()));
}

// Known limitation: seek only repositions the default (top-level) animation. Nested
// auto-playing compositions rendered by `scene` are delta-driven via advanceAndApply() and have
// no absolute-time entry point, so their frame lags behind the main timeline after a scrub:
// the main animation jumps to `micros` while the nested compositions stay wherever their
// accumulated delta left them. Acceptable for the MVP viewer; a future fix would need per-scene
// seek support (or a full scene rebuild) rather than a workaround here.
void PAGXView::setCurrentTimeMicros(int64_t micros) {
  if (defaultAnimation != nullptr) {
    defaultAnimation->setCurrentTime(micros);
    // setCurrentTime only moves the playhead; apply() is required to reflect it in the content.
    // Force a present so a manual seek (e.g. dragging the progress bar while paused) updates the
    // frame immediately instead of being skipped by the idle dirty gate in draw().
    defaultAnimation->apply();
    lastAnimationTimeMs = -1.0;
    presentImmediately = true;
  } else if (defaultTimeline != nullptr) {
    // Fallback mode (duration-less timeline): the caller always sends current + signed delta
    // because the slider is disabled in this mode and only relative frame steps come through.
    int64_t delta = micros - fallbackClockUs;
    if (delta != 0) {
      advanceFallbackTimeline(delta);
      lastAnimationTimeMs = -1.0;
      presentImmediately = true;
    }
  } else if (scene != nullptr) {
    // Nested-only animations: no top-level timeline to drive, so frame stepping advances the
    // scene directly (same relative-delta convention as the fallback branch above).
    int64_t delta = micros - fallbackClockUs;
    if (delta != 0) {
      scene->advanceAndApply(delta);
      fallbackClockUs += delta;
      lastAnimationTimeMs = -1.0;
      presentImmediately = true;
    }
  }
}

void PAGXView::setLoop(bool loop) {
  loopEnabled = loop;
}

bool PAGXView::isLoop() const {
  return loopEnabled;
}

}  // namespace pagx
