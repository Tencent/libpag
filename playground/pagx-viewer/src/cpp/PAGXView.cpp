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
#include "pagx/PAGStateMachine.h"
#include "pagx/PAGXImporter.h"
#include "pagx/PAGXNodeChannel.h"
#include "pagx/nodes/Animation.h"
#include "pagx/nodes/AnimationObject.h"
#include "pagx/nodes/AnimationTimeline.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/State.h"
#include "pagx/nodes/StateMachine.h"
#include "pagx/nodes/StateMachineInput.h"
#include "pagx/nodes/StateMachineTimeline.h"
#include "pagx/nodes/StateRegion.h"
#include "pagx/nodes/StateTransition.h"
#include "pagx/nodes/TransitionCondition.h"
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

emscripten::val PAGXView::hitTest(float surfaceX, float surfaceY) {
  if (scene == nullptr) {
    return emscripten::val::null();
  }
  auto hit = scene->hitTest(surfaceX, surfaceY);
  if (hit.index < 0) {
    return emscripten::val::null();
  }
  auto obj = emscripten::val::object();
  obj.set("index", hit.index);
  obj.set("startLine", hit.startLine);
  obj.set("endLine", hit.endLine);
  if (hit.bounds.isEmpty()) {
    obj.set("bounds", emscripten::val::null());
  } else {
    auto boundsObj = emscripten::val::object();
    boundsObj.set("x", hit.bounds.x);
    boundsObj.set("y", hit.bounds.y);
    boundsObj.set("w", hit.bounds.width);
    boundsObj.set("h", hit.bounds.height);
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
  auto rects = scene->getGlobalBoundsForNode(static_cast<Layer*>(n));
  // Selection-outline slack for FreeType's anti-aliasing overshoot: per-glyph tight bounds stop
  // at the geometric baseline while the rasterizer paints slightly below it, visually clipping
  // the last row of ink. Extending the bottom by 5% of the layer height covers the overshoot,
  // with a 0.5 root-space floor scaled by the current zoom. This is presentation-only slack,
  // kept here so PAGScene::getGlobalBounds keeps returning tight bounds.
  float zoom = contentScale * userZoom;
  auto array = emscripten::val::array();
  size_t visibleCount = 0;
  for (auto& rect : rects) {
    if (rect.isEmpty()) {
      continue;
    }
    rect.height += std::max(0.5f * zoom, rect.height * 0.05f);
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
  // The scene was rebuilt, so any solo-preview instance belongs to the old scene and must be
  // dropped; the host re-selects a unit of the new document if it wants to.
  previewAnimation = nullptr;
  selectedUnitKind.clear();
  selectedUnitId.clear();
  playing = true;
  lastAnimationTimeMs = -1.0;
  pagxWidth = scene->width();
  pagxHeight = scene->height();
  applySceneDisplayOptions();
  updateContentTransform();
  presentImmediately = true;
}

static const char* LoopModeName(LoopMode mode) {
  switch (mode) {
    case LoopMode::Once:
      return "once";
    case LoopMode::Loop:
      return "loop";
    case LoopMode::PingPong:
      return "pingPong";
  }
  return "once";
}

static const char* InputTypeName(StateMachineInputType type) {
  switch (type) {
    case StateMachineInputType::Bool:
      return "bool";
    case StateMachineInputType::Number:
      return "number";
    case StateMachineInputType::Trigger:
      return "trigger";
  }
  return "?";
}

static const char* ConditionOpName(TransitionConditionOp op) {
  switch (op) {
    case TransitionConditionOp::Equal:
      return "==";
    case TransitionConditionOp::NotEqual:
      return "!=";
    case TransitionConditionOp::LessThan:
      return "<";
    case TransitionConditionOp::LessThanOrEqual:
      return "<=";
    case TransitionConditionOp::GreaterThan:
      return ">";
    case TransitionConditionOp::GreaterThanOrEqual:
      return ">=";
    case TransitionConditionOp::Trigger:
      return "trigger";
  }
  return "?";
}

static std::string DescribeConditions(
    const StateTransition* transition,
    const std::unordered_map<std::string, StateMachineInputType>& inputTypes) {
  if (transition == nullptr || transition->conditions.empty()) {
    return "always";
  }
  std::string text = {};
  for (const auto* condition : transition->conditions) {
    if (condition == nullptr) {
      continue;
    }
    if (!text.empty()) {
      text += " && ";
    }
    text += condition->inputName + " " + ConditionOpName(condition->op);
    if (condition->op == TransitionConditionOp::Trigger) {
      continue;
    }
    auto it = inputTypes.find(condition->inputName);
    if (it != inputTypes.end() && it->second == StateMachineInputType::Bool) {
      text += condition->valueBool ? " true" : " false";
    } else {
      char buffer[32] = {};
      std::snprintf(buffer, sizeof(buffer), " %.3g", condition->valueNumber);
      text += buffer;
    }
  }
  return text;
}

// Walks the source layer tree and prints every <Timelines> mount point (the driver declarations,
// not runtime instances). Composition references are followed with a path-scoped visited set so a
// cyclic composition reference terminates while sibling instances of the same composition are all
// listed (each instance may mount different drivers).
static double AnimationDurationUs(const Animation* anim) {
  if (anim == nullptr || anim->frameRate <= 0.0f) {
    return -1;
  }
  return static_cast<double>(anim->duration) * 1000000.0 / static_cast<double>(anim->frameRate);
}

static double StateAnimationDurationUs(const AnimationState* state, PAGXDocument* doc) {
  if (state->animationId.empty()) {
    return 0;
  }
  auto* node = doc != nullptr ? doc->findNode(state->animationId) : nullptr;
  if (node == nullptr || node->nodeType() != NodeType::Animation) {
    return -1;
  }
  return AnimationDurationUs(static_cast<const Animation*>(node));
}

// Returns whether every target referenced by the animation can be resolved through the root
// binding scope: false means the animation is designed to run under a mount (e.g. a top-level
// definition consumed only through <Timelines> inside a composition), so solo preview via
// scene->getAnimation() would tick internally but not update the stage. Used by the UI to grey
// out unpreviewable rows before M2 wires up mount-scoped preview instances.
static bool AnimationTargetsInRoot(const Animation* anim,
                                   const std::unordered_set<std::string>& rootLayerIds) {
  if (anim == nullptr) {
    return false;
  }
  for (const auto* obj : anim->objects) {
    if (obj == nullptr || obj->target.empty()) {
      continue;
    }
    if (rootLayerIds.find(obj->target) == rootLayerIds.end()) {
      return false;
    }
  }
  return true;
}

static emscripten::val MakeAnimationNode(const Animation* anim, const std::string& path,
                                         bool isDefault, bool previewSupported) {
  auto node = emscripten::val::object();
  node.set("path", path);
  node.set("kind", "animation");
  node.set("id", anim->id);
  node.set("name", anim->id);
  node.set("durationUs", AnimationDurationUs(anim));
  node.set("frameRate", anim->frameRate);
  node.set("loop", LoopModeName(anim->loop));
  node.set("isDefault", isDefault);
  node.set("previewSupported", previewSupported);
  node.set("children", emscripten::val::array());
  return node;
}

static emscripten::val MakeStateMachineNode(const StateMachine* sm, PAGXDocument* doc,
                                            const std::shared_ptr<PAGScene>& scene,
                                            const std::string& path, bool isDefault,
                                            const std::unordered_set<std::string>& rootLayerIds) {
  auto node = emscripten::val::object();
  node.set("path", path);
  node.set("kind", "stateMachine");
  node.set("id", sm->id);
  node.set("name", sm->id);
  node.set("durationUs", -1.0);
  node.set("isDefault", isDefault);
  std::unordered_map<std::string, StateMachineInputType> inputTypes = {};
  auto inputs = emscripten::val::array();
  for (const auto* input : sm->inputs) {
    if (input == nullptr) {
      continue;
    }
    inputTypes[input->name] = input->type;
    auto inputVal = emscripten::val::object();
    inputVal.set("name", input->name);
    inputVal.set("type", InputTypeName(input->type));
    inputs.call<void>("push", inputVal);
  }
  node.set("inputs", inputs);
  auto runtime = scene != nullptr ? scene->getStateMachineTimeline(sm->id) : nullptr;
  auto regions = emscripten::val::array();
  for (const auto* region : sm->regions) {
    if (region == nullptr) {
      continue;
    }
    auto regionVal = emscripten::val::object();
    regionVal.set("name", region->name);
    regionVal.set("initial", region->initialState);
    regionVal.set("current",
                  runtime != nullptr ? runtime->getCurrentState(region->name) : std::string());
    auto states = emscripten::val::array();
    for (const auto* state : region->states) {
      if (state == nullptr) {
        continue;
      }
      auto stateVal = emscripten::val::object();
      stateVal.set("name", state->name);
      if (state->stateType() == StateType::Animation) {
        auto* animationState = static_cast<const AnimationState*>(state);
        stateVal.set("animationId", animationState->animationId);
        stateVal.set("durationUs", StateAnimationDurationUs(animationState, doc));
        auto* animDef =
            animationState->animationId.empty()
                ? nullptr
                : (doc != nullptr ? doc->findNode(animationState->animationId) : nullptr);
        stateVal.set(
            "previewSupported",
            animDef != nullptr && animDef->nodeType() == NodeType::Animation &&
                AnimationTargetsInRoot(static_cast<const Animation*>(animDef), rootLayerIds));
      } else {
        stateVal.set("animationId", std::string());
        stateVal.set("durationUs", -1.0);
        stateVal.set("previewSupported", false);
      }
      states.call<void>("push", stateVal);
    }
    regionVal.set("states", states);
    auto transitions = emscripten::val::array();
    for (const auto* transition : region->transitions) {
      if (transition == nullptr) {
        continue;
      }
      auto transitionVal = emscripten::val::object();
      transitionVal.set("from", transition->from);
      transitionVal.set("to", transition->to);
      transitionVal.set("fromAny", transition->from == AnyStateName);
      transitionVal.set("conditions", DescribeConditions(transition, inputTypes));
      transitions.call<void>("push", transitionVal);
    }
    regionVal.set("transitions", transitions);
    regions.call<void>("push", regionVal);
  }
  node.set("regions", regions);
  node.set("children", emscripten::val::array());
  return node;
}

static emscripten::val MakeMountNode(const Layer* layer, const Timeline* driver, PAGXDocument* doc,
                                     const std::string& path) {
  auto node = emscripten::val::object();
  node.set("path", path);
  node.set("kind", "mount");
  node.set("name", layer->id.empty() ? "(no id)" : layer->id);
  node.set("layerId", layer->id);
  node.set("children", emscripten::val::array());
  if (driver->timelineType() == TimelineType::Animation) {
    auto* animationDriver = static_cast<const AnimationTimeline*>(driver);
    node.set("id", animationDriver->animationId);
    node.set("name", animationDriver->animationId);
    node.set("refKind", "animation");
    node.set("playing", animationDriver->playing);
    node.set("offsetFrames", static_cast<double>(animationDriver->evaluationOffset));
    auto* definition = doc != nullptr ? doc->findNode(animationDriver->animationId) : nullptr;
    if (definition != nullptr && definition->nodeType() == NodeType::Animation) {
      auto* anim = static_cast<const Animation*>(definition);
      node.set("durationUs", AnimationDurationUs(anim));
      node.set("frameRate", anim->frameRate);
      node.set("loop", LoopModeName(anim->loop));
    } else {
      node.set("durationUs", -1.0);
    }
  } else {
    auto* smDriver = static_cast<const StateMachineTimeline*>(driver);
    node.set("id", smDriver->stateMachineId);
    node.set("refKind", "stateMachine");
    node.set("durationUs", -1.0);
  }
  return node;
}

// Collects the ids of every layer in the root binding scope (the layer tree excluding any layers
// reachable only through a `composition` reference), used to check whether an animation's targets
// stay inside the root binding.
static void CollectRootLayerIds(const std::vector<Layer*>& layers,
                                std::unordered_set<std::string>* out) {
  for (const auto* layer : layers) {
    if (layer == nullptr) {
      continue;
    }
    if (!layer->id.empty()) {
      out->insert(layer->id);
    }
    CollectRootLayerIds(layer->children, out);
    // Do NOT recurse into layer->composition: those layers live in the composition scope, not the
    // root binding, and the root-binding apply cannot reach them.
  }
}

// Collects mount nodes from the layer list into `out`. Layout follows the layer physical
// hierarchy: any layer that references a composition (with or without its own drivers) becomes a
// synthetic group wrapper whose children are (a) the layer's own drivers and (b) the mounts found
// deeper inside the referenced composition. A layer that does not reference a composition pushes
// its drivers as flat mount nodes at the current level. `visited` guards against cyclic
// composition references (sibling instances of one composition are all listed; a cycle
// terminates).
static void CollectMountNodes(const std::vector<Layer*>& layers, PAGXDocument* doc,
                              const std::string& parentPath, int* topIndex, int* localIndex,
                              std::unordered_set<const Composition*>& visited,
                              emscripten::val& out) {
  auto nextPath = [&]() {
    if (parentPath.empty()) {
      return std::to_string((*topIndex)++);
    }
    return parentPath + "/" + std::to_string((*localIndex)++);
  };
  for (const auto* layer : layers) {
    if (layer == nullptr) {
      continue;
    }
    if (layer->composition != nullptr) {
      // Layer references a composition: even a layer with drivers of its own becomes a group so
      // the "layer + its mount drivers + the mounts inside the composition" nesting is visible.
      const bool descend = visited.insert(layer->composition).second;
      const std::string wrapperPath = nextPath();
      auto* subDoc = layer->externalDoc != nullptr ? layer->externalDoc.get() : doc;
      auto wrapper = emscripten::val::object();
      wrapper.set("path", wrapperPath);
      wrapper.set("kind", "compositionGroup");
      wrapper.set("name", layer->id.empty() ? "(composition)" : layer->id);
      wrapper.set("id", layer->id);
      wrapper.set("refKind", emscripten::val::null());
      wrapper.set("durationUs", 0);
      wrapper.set("layerId", layer->id);
      wrapper.set("loop", emscripten::val::null());
      wrapper.set("frameRate", 0);
      wrapper.set("playing", false);
      wrapper.set("offsetFrames", 0);
      auto children = emscripten::val::array();
      int childIndex = 0;
      for (const auto& driver : layer->timelines) {
        if (driver == nullptr) {
          continue;
        }
        const std::string driverPath = wrapperPath + "/" + std::to_string(childIndex++);
        children.call<void>("push", MakeMountNode(layer, driver.get(), doc, driverPath));
      }
      if (descend) {
        CollectMountNodes(layer->composition->layers, subDoc, wrapperPath, topIndex, &childIndex,
                          visited, children);
        visited.erase(layer->composition);
      }
      wrapper.set("children", children);
      out.call<void>("push", wrapper);
    } else {
      // No composition reference: drivers (if any) are flat mount nodes at the current level. Also
      // recurse into structural children so deeply nested layer trees still get walked.
      for (const auto& driver : layer->timelines) {
        if (driver == nullptr) {
          continue;
        }
        const std::string path = nextPath();
        out.call<void>("push", MakeMountNode(layer, driver.get(), doc, path));
      }
      CollectMountNodes(layer->children, doc, parentPath, topIndex, localIndex, visited, out);
    }
  }
}

emscripten::val PAGXView::getTimelineTree() {
  auto result = emscripten::val::array();
  if (document == nullptr) {
    return result;
  }
  const std::string defaultId =
      defaultTimeline != nullptr ? defaultTimeline->getId() : std::string();
  std::unordered_set<std::string> rootLayerIds = {};
  CollectRootLayerIds(document->layers, &rootLayerIds);
  int topIndex = 1;
  for (const auto* node : document->animations) {
    if (node == nullptr) {
      continue;
    }
    const std::string path = std::to_string(topIndex++);
    if (node->nodeType() == NodeType::Animation) {
      auto* anim = static_cast<const Animation*>(node);
      result.call<void>("push", MakeAnimationNode(anim, path, anim->id == defaultId,
                                                  AnimationTargetsInRoot(anim, rootLayerIds)));
    } else if (node->nodeType() == NodeType::StateMachine) {
      auto* sm = static_cast<const StateMachine*>(node);
      result.call<void>("push", MakeStateMachineNode(sm, document.get(), scene, path,
                                                     sm->id == defaultId, rootLayerIds));
    }
  }
  std::unordered_set<const Composition*> visited = {};
  CollectMountNodes(document->layers, document.get(), "", &topIndex, nullptr, visited, result);
  return result;
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
    if (!selectedUnitId.empty()) {
      // Solo mode: a selected animation is the only clock that runs; a selected state machine has
      // no clock at all. Either way the default timeline and the scene stay frozen at their
      // current phase until the selection is cleared.
      if (previewAnimation != nullptr) {
        advanceAnimationUnit(previewAnimation, deltaUs);
      }
      return;
    }
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
  if (defaultAnimation != nullptr) {
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
  if (previewAnimation != nullptr) {
    return previewAnimation->playbackPosition();
  }
  if (!selectedUnitId.empty()) {
    // A selected state machine has no time axis; report a frozen position.
    return 0;
  }
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
  if (previewAnimation != nullptr) {
    return previewAnimation->playbackPeriod();
  }
  if (!selectedUnitId.empty()) {
    // A selected state machine has no time axis; untimed mode.
    return 0;
  }
  if (defaultAnimation != nullptr) {
    // Match currentTimeMicros(): expose the full loop period so PingPong reports 2 * duration (one
    // complete round trip) and the progress bar / time / frame readouts stay consistent.
    return defaultAnimation->playbackPeriod();
  }
  return 0;
}

float PAGXView::frameRate() const {
  if (previewAnimation != nullptr) {
    return previewAnimation->frameRate();
  }
  if (!selectedUnitId.empty()) {
    // A selected state machine has no single frame rate; the PAGX default keeps frame stepping
    // sane in untimed mode.
    return 60.0f;
  }
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
  if (previewAnimation != nullptr) {
    previewAnimation->setCurrentTime(micros);
    // setCurrentTime only moves the playhead; apply() is required to reflect it in the content.
    // Force a present so a manual seek (e.g. dragging the progress bar while paused) updates the
    // frame immediately instead of being skipped by the idle dirty gate in draw().
    previewAnimation->apply();
    lastAnimationTimeMs = -1.0;
    presentImmediately = true;
  } else if (!selectedUnitId.empty()) {
    // A selected state machine has no time axis; scrubbing and frame stepping are no-ops.
  } else if (defaultAnimation != nullptr) {
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

bool PAGXView::setSMInputBool(const std::string& name, bool value) {
  if (defaultTimeline == nullptr || defaultTimeline->type() != TimelineType::StateMachine) {
    return false;
  }
  return std::static_pointer_cast<PAGStateMachine>(defaultTimeline)->setBool(name, value);
}

bool PAGXView::setSMInputNumber(const std::string& name, float value) {
  if (defaultTimeline == nullptr || defaultTimeline->type() != TimelineType::StateMachine) {
    return false;
  }
  return std::static_pointer_cast<PAGStateMachine>(defaultTimeline)->setNumber(name, value);
}

bool PAGXView::fireSMInputTrigger(const std::string& name) {
  if (defaultTimeline == nullptr || defaultTimeline->type() != TimelineType::StateMachine) {
    return false;
  }
  return std::static_pointer_cast<PAGStateMachine>(defaultTimeline)->fireTrigger(name);
}

bool PAGXView::selectTimelineUnit(const std::string& kind, const std::string& id) {
  if (scene == nullptr) {
    return false;
  }
  if (id.empty() || kind.empty()) {
    const bool hadSelection = !selectedUnitId.empty();
    previewAnimation = nullptr;
    selectedUnitKind.clear();
    selectedUnitId.clear();
    if (hadSelection) {
      // Returning to the main animation: resume playback (solo playback may have parked the
      // playing flag at a finished once unit). A state machine default restarts from the head:
      // once regions that already finished cannot be resumed by advancing, and a state machine has
      // no archived phase worth restoring, so it plays again from its initial states. An animation
      // default keeps its frozen phase and simply continues (archive semantics).
      playing = true;
      if (defaultTimeline != nullptr && defaultTimeline->type() == TimelineType::StateMachine) {
        std::static_pointer_cast<PAGStateMachine>(defaultTimeline)->reset();
        defaultTimeline->apply();
        fallbackClockUs = 0;
      }
    }
    lastAnimationTimeMs = -1.0;
    presentImmediately = true;
    return true;
  }
  const bool isDefaultUnit =
      defaultTimeline != nullptr && defaultTimeline->getId() == id &&
      ((kind == "animation" && defaultTimeline->type() == TimelineType::Animation) ||
       (kind == "stateMachine" && defaultTimeline->type() == TimelineType::StateMachine));
  if (kind == "animation") {
    // Lazily instantiate the requested top-level animation. This instance is independent of any
    // state-machine-owned runtime of the same definition (verified by the diagnostics dump), so
    // driving it never disturbs the SM runtime. Mounted (nested) units reuse the same lazy
    // instance: the mount's own runtime (offset / per-instance phase) is private to PAGComposition,
    // so the preview plays the animation definition itself.
    auto animation = scene->getAnimation(id);
    if (animation == nullptr) {
      // Definition not in the document's top-level <Animations> (e.g. declared inside a
      // Composition's local <Animations> for a mount, or a dangling id): the mount-preview path
      // (M2) will instantiate nested runtimes; until then this call is refused.
      return false;
    }
    // Refuse when the animation targets a layer that only exists inside a nested composition:
    // the lazy instance's root binding will silently drop those targets on apply, so the preview
    // would advance internally without ever updating the stage. Fixing this requires the M2
    // mount-preview path that instantiates the animation with the mount's own binding scope.
    if (auto* def = document->findNode(id);
        def != nullptr && def->nodeType() == NodeType::Animation) {
      auto* animDef = static_cast<Animation*>(def);
      std::unordered_set<std::string> rootLayerIds = {};
      CollectRootLayerIds(document->layers, &rootLayerIds);
      for (const auto* obj : animDef->objects) {
        if (obj == nullptr || obj->target.empty()) {
          continue;
        }
        if (rootLayerIds.find(obj->target) == rootLayerIds.end()) {
          // Target defined inside a nested composition; preview would not update the stage. M2
          // will handle mount-scoped previews.
          return false;
        }
      }
    }
    // Switching from one preview unit to another parks the previous preview at its first frame:
    // the lazy instance stays cached inside the scene, so without rewinding it here the next
    // visit would resume from wherever this solo pass left off instead of starting over.
    if (previewAnimation != nullptr) {
      previewAnimation->setCurrentTime(0);
      previewAnimation->apply();
    }
    previewAnimation = std::move(animation);
    if (!isDefaultUnit && defaultTimeline != nullptr &&
        defaultTimeline->type() == TimelineType::StateMachine) {
      // Entering a solo preview freezes the default state machine; rewinding it to the initial
      // states first makes the frozen frame deterministic (blueprint semantics: preview always
      // parks the SM at its head) and clears the fallback clock the playback bar showed.
      std::static_pointer_cast<PAGStateMachine>(defaultTimeline)->reset();
      defaultTimeline->apply();
      fallbackClockUs = 0;
    }
    if (isDefaultUnit) {
      // Selecting the default unit means "prepare a replay of the main animation": rewind it to
      // frame 0 and park paused — the user starts playback with the play button (rive's
      // default-timeline semantics; its play button on the main timeline is a reset button).
      previewAnimation->setCurrentTime(0);
      previewAnimation->apply();
      playing = false;
    } else {
      // Sub-unit previews start immediately: a previous solo unit may have finished (a once
      // animation parks playing=false at its boundary), and without re-arming the flag the new
      // selection would sit frozen behind the playing gate in advanceTimelines().
      playing = true;
    }
  } else if (kind == "stateMachine") {
    // Selecting a state machine shows its structure only (regions/states/inputs); it has no time
    // axis and all clocks stay frozen while selected. Nested (mounted) state machines cannot be
    // instantiated through the scene's top-level registry, so the lookup fails for them.
    if (scene->getStateMachineTimeline(id) == nullptr) {
      return false;
    }
    previewAnimation = nullptr;
    if (isDefaultUnit) {
      // Same default-replay semantics as animations: reset to the initial states at frame 0 and
      // wait for the user to press play (once regions that already finished cannot be resumed by
      // advancing, reset() is the only way back).
      std::static_pointer_cast<PAGStateMachine>(defaultTimeline)->reset();
      defaultTimeline->apply();
      fallbackClockUs = 0;
      playing = false;
    } else {
      playing = true;
    }
  } else {
    return false;
  }
  selectedUnitKind = kind;
  selectedUnitId = id;
  // Drop the pending frame delta so switching units does not apply one stale jump first.
  lastAnimationTimeMs = -1.0;
  presentImmediately = true;
  return true;
}

emscripten::val PAGXView::getSelectedTimelineUnit() const {
  if (selectedUnitId.empty()) {
    return emscripten::val::null();
  }
  auto result = emscripten::val::object();
  result.set("kind", selectedUnitKind);
  result.set("id", selectedUnitId);
  return result;
}

emscripten::val PAGXView::getSMCurrentStates() const {
  auto result = emscripten::val::object();
  if (defaultTimeline == nullptr || defaultTimeline->type() != TimelineType::StateMachine ||
      document == nullptr) {
    return result;
  }
  auto* node = document->findNode(defaultTimeline->getId());
  if (node == nullptr || node->nodeType() != NodeType::StateMachine) {
    return result;
  }
  auto* sm = static_cast<const StateMachine*>(node);
  auto smTimeline = std::static_pointer_cast<PAGStateMachine>(defaultTimeline);
  for (const auto* region : sm->regions) {
    if (region == nullptr || region->name.empty()) {
      continue;
    }
    result.set(region->name, smTimeline->getCurrentState(region->name));
  }
  return result;
}

}  // namespace pagx
