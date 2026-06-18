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

#include "pagx/PAGScene.h"
#include "pagx/PAGViewModel.h"
#include "pagx/DataBindRuntime.h"
#include "pagx/DataContext.h"
#include "base/utils/Log.h"
#include "pagx/PAGLayer.h"
#include "pagx/PAGSurface.h"
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Animation.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/DataBind.h"
#include "pagx/nodes/ViewModel.h"
#include "pagx/nodes/ViewModelProperty.h"
#include "pagx/nodes/Layer.h"
#include "pagx/runtime/Drawable.h"
#include "pagx/types/Matrix.h"
#include "renderer/LayerBuilder.h"
#include "renderer/ToTGFX.h"
#include "tgfx/layers/DisplayList.h"

namespace pagx {

PAGScene::PAGScene() = default;

std::shared_ptr<PAGScene> PAGScene::Make(std::shared_ptr<PAGXDocument> document) {
  if (document == nullptr) {
    return nullptr;
  }
  if (!document->isLayoutApplied()) {
    document->applyLayout();
  }
  // A cyclic external composition reference leaves the document partially laid out: applyLayout()
  // aborts the offending subtree but the rest of the tree looks valid. Building a scene on top of
  // that inconsistent state would render a malformed tree, so refuse construction when a cycle was
  // reported. Other (non-fatal) parse errors such as duplicate ids do not block construction.
  for (const auto& error : document->errors) {
    if (error.rfind("Cyclic external composition reference detected", 0) == 0) {
      LOGE("PAGScene::Make() aborted: %s", error.c_str());
      return nullptr;
    }
  }
  auto scene = std::shared_ptr<PAGScene>(new PAGScene());
  scene->document = document;
  scene->displayList = std::make_unique<tgfx::DisplayList>();
  scene->displayOptions = std::unique_ptr<PAGDisplayOptions>(new PAGDisplayOptions(scene.get()));
  scene->buildRuntimeTree();
  document->registerLiveScene(scene);
  return scene;
}

void PAGScene::buildRuntimeTree() {
  // Detach any previously built tree (a rebuild after an external-document edit) so the display list
  // and timeline cache do not retain stale runtime layers.
  if (_rootComposition != nullptr && _rootComposition->runtimeLayer != nullptr) {
    _rootComposition->runtimeLayer->removeFromParent();
  }
  timelinesByAnimation.clear();
  auto buildResult = LayerBuilder::BuildForRuntime(document.get());
  auto rootComp = std::shared_ptr<PAGComposition>(
      new PAGComposition(nullptr, std::move(buildResult.root), shared_from_this()));
  *rootComp->binding = std::move(buildResult.binding);
  rootComp->document = document.get();
  _rootComposition = rootComp;
  std::unordered_set<const Composition*> visited = {};
  _rootComposition->buildChildren(document->layers, visited);
  displayList->root()->addChild(rootComp->runtimeLayer);
  buildViewModels();
}


std::shared_ptr<PAGViewModel> PAGScene::CreateViewModelFromSchema(
    ViewModel* schema, const std::shared_ptr<PAGScene>& scene) {
  if (schema == nullptr) return nullptr;
  auto vm = std::shared_ptr<PAGViewModel>(new PAGViewModel());
  vm->id = schema->id;
  for (auto* prop : schema->properties) {
    if (prop == nullptr) continue;
    std::shared_ptr<PAGViewModelValue> value = nullptr;
    switch (prop->propertyType) {
      case ViewModelPropertyType::Number: { auto v = std::make_shared<PAGViewModelValueNumber>(); v->propertyValue = prop->defaultNumber; v->type = ViewModelValueType::Number; value = std::move(v); break; }
      case ViewModelPropertyType::String: { auto v = std::make_shared<PAGViewModelValueString>(); v->propertyValue = prop->defaultString; v->type = ViewModelValueType::String; value = std::move(v); break; }
      case ViewModelPropertyType::Boolean: { auto v = std::make_shared<PAGViewModelValueBoolean>(); v->propertyValue = prop->defaultBoolean; v->type = ViewModelValueType::Boolean; value = std::move(v); break; }
      case ViewModelPropertyType::Color: { auto v = std::make_shared<PAGViewModelValueColor>(); v->propertyValue = prop->defaultColor; v->type = ViewModelValueType::Color; value = std::move(v); break; }
      case ViewModelPropertyType::Image: { auto v = std::make_shared<PAGViewModelValueImage>(); v->propertyValue = prop->defaultImage; v->type = ViewModelValueType::Image; value = std::move(v); break; }
      case ViewModelPropertyType::ViewModel: { auto v = std::make_shared<PAGViewModelValueViewModel>(); v->type = ViewModelValueType::ViewModel; value = std::move(v); break; }
    }
    if (value) { value->propertyName = prop->name; value->setScene(scene); vm->propertyMap[prop->name] = value; vm->propertyList.push_back(value); }
  }
  return vm;
}

void PAGScene::buildNestedViewModels(PAGComposition* rootComp) {
  if (rootComp == nullptr) return;
  for (auto& child : rootComp->children) {
    if (child == nullptr || child->layerType() != LayerType::Composition) continue;
    auto* childComp = static_cast<PAGComposition*>(child.get());
    const auto* sourceLayer = childComp->node;
    if (sourceLayer == nullptr || sourceLayer->composition == nullptr) continue;
    const auto* compSchema = sourceLayer->composition;
    if (compSchema->viewModel != nullptr) {
      auto childVM = PAGScene::CreateViewModelFromSchema(compSchema->viewModel, shared_from_this());
      childComp->compositionViewModel = childVM;
      childComp->dataBindRuntime = std::make_unique<DataBindRuntime>();
      auto dataCtx = std::make_shared<DataContext>(childVM);
      if (rootComp->dataContext != nullptr) dataCtx->parent(rootComp->dataContext);
      childComp->dataContext = dataCtx;
      if (rootComp->compositionViewModel != nullptr && !sourceLayer->vmContext.empty()) {
        auto propName = sourceLayer->vmContext;
        if (propName.find("$vm.") == 0) propName = propName.substr(4);
        auto prop = rootComp->compositionViewModel->propertyViewModel(propName);
        if (prop) prop->referenceInstance = childVM;
      }
      if (!compSchema->dataBinds.empty())
        childComp->dataBindRuntime->bind(compSchema->dataBinds, childComp->dataContext.get(),
                                         childComp->binding.get(), document.get());
    }
    buildNestedViewModels(childComp);
  }
}

void PAGScene::buildViewModels() {
  auto self = shared_from_this();
  rootViewModel = PAGScene::CreateViewModelFromSchema(document->viewModel, self);
  if (rootViewModel != nullptr && _rootComposition != nullptr) {
    _rootComposition->compositionViewModel = rootViewModel;
    _rootComposition->dataBindRuntime = std::make_unique<DataBindRuntime>();
    _rootComposition->dataContext = std::make_shared<DataContext>(rootViewModel);
    if (!document->dataBinds.empty())
      _rootComposition->dataBindRuntime->bind(document->dataBinds,
                                              _rootComposition->dataContext.get(),
                                              _rootComposition->binding.get(), document.get());
    buildNestedViewModels(_rootComposition.get());
  }
}


PAGScene::~PAGScene() {
  if (document != nullptr) {
    document->unregisterLiveScene(this);
  }
}

std::vector<std::string> PAGScene::getTimelineIds() const {
  std::vector<std::string> ids = {};
  if (document == nullptr) {
    return ids;
  }
  ids.reserve(document->animations.size());
  for (auto* animation : document->animations) {
    if (animation != nullptr) {
      ids.push_back(animation->id);
    }
  }
  return ids;
}

std::shared_ptr<PAGTimeline> PAGScene::getTimeline(const std::string& id) {
  if (document == nullptr) {
    return nullptr;
  }
  Animation* matched = nullptr;
  for (auto* animation : document->animations) {
    if (animation != nullptr && animation->id == id) {
      matched = animation;
      break;
    }
  }
  if (matched == nullptr) {
    return nullptr;
  }
  auto it = timelinesByAnimation.find(matched);
  if (it != timelinesByAnimation.end()) {
    return it->second;
  }
  // Construct with a null binding so the timeline resolves the scene's current root binding lazily
  // at apply time. A user-cached handle then survives a runtime-tree rebuild (foreign external-doc
  // edit) that replaces _rootComposition and frees the old binding, instead of dangling.
  auto timeline = std::shared_ptr<PAGTimeline>(
      new PAGTimeline(matched, nullptr, document.get(), weak_from_this()));
  timelinesByAnimation.emplace(matched, timeline);
  return timeline;
}

std::shared_ptr<PAGTimeline> PAGScene::getDefaultTimeline() {
  if (document == nullptr || document->animations.empty()) {
    return nullptr;
  }
  auto* first = document->animations.front();
  if (first == nullptr) {
    return nullptr;
  }
  return getTimeline(first->id);
}

std::shared_ptr<PAGComposition> PAGScene::rootComposition() const {
  return _rootComposition;
}

bool PAGScene::draw(const std::shared_ptr<PAGSurface>& surface, bool autoClear) {
  if (surface == nullptr || surface->drawable == nullptr) {
    return false;
  }
  if (_rootComposition == nullptr || _rootComposition->runtimeLayer == nullptr) {
    return false;
  }
  flushDataBinds();
  auto& drawable = surface->drawable;
  auto device = drawable->getDevice();
  if (device == nullptr) {
    return false;
  }
  auto* context = device->lockContext();
  if (context == nullptr) {
    return false;
  }
  auto tgfxSurface = drawable->getSurface(context);
  if (tgfxSurface == nullptr) {
    device->unlock();
    return false;
  }
  displayList->render(tgfxSurface.get(), autoClear);
  drawable->present(context);
  device->unlock();
  return true;
}

float PAGScene::width() const {
  return document != nullptr ? document->width : 0.0f;
}

float PAGScene::height() const {
  return document != nullptr ? document->height : 0.0f;
}

PAGDisplayOptions* PAGScene::getDisplayOptions() const {
  return displayOptions.get();
}

void PAGScene::advanceAndApply(int64_t deltaMicroseconds) {
  if (_rootComposition != nullptr) {
    _rootComposition->advance(deltaMicroseconds);
    _rootComposition->apply();
  }
}

std::shared_ptr<PAGViewModel> PAGScene::viewModel() {
  return rootViewModel;
}

void PAGScene::flushDataBinds() const {
  if (_rootComposition != nullptr) _rootComposition->updateDataBinds();
}

std::vector<std::shared_ptr<PAGLayer>> PAGScene::getLayersUnderPoint(float surfaceX,
                                                                     float surfaceY) {
  float rootX = 0;
  float rootY = 0;
  if (!surfaceToRoot(surfaceX, surfaceY, &rootX, &rootY)) {
    return {};
  }
  if (_rootComposition == nullptr || _rootComposition->runtimeLayer == nullptr) {
    return {};
  }
  std::vector<std::shared_ptr<PAGLayer>> result = {};
  auto hitLayers = _rootComposition->runtimeLayer->getLayersUnderPoint(rootX, rootY);
  for (const auto& hitLayer : hitLayers) {
    auto it = layerRegistry.find(hitLayer.get());
    if (it != layerRegistry.end() && it->second->layerType() == LayerType::Layer) {
      result.push_back(it->second->shared_from_this());
    }
  }
  return result;
}

Rect PAGScene::getGlobalBounds(const std::shared_ptr<PAGLayer>& pagLayer) const {
  if (pagLayer == nullptr || pagLayer->runtimeLayer == nullptr) {
    return {};
  }
  auto scene = pagLayer->rootScene.lock();
  if (scene.get() != this) {
    return {};
  }
  flushDataBinds();
  auto* rootLayer = _rootComposition != nullptr ? _rootComposition->runtimeLayer.get() : nullptr;
  auto rootBounds = pagLayer->runtimeLayer->getBounds(rootLayer);
  Matrix rootToSurface = {};
  rootToSurfaceMatrix(&rootToSurface);
  auto surfaceBounds = ToTGFX(rootToSurface).mapRect(rootBounds);
  return FromTGFX(surfaceBounds);
}

bool PAGScene::surfaceToRoot(float surfaceX, float surfaceY, float* rootX, float* rootY) const {
  if (rootX == nullptr || rootY == nullptr) {
    return false;
  }
  float zoomScale = displayList->zoomScale();
  if (zoomScale == 0.0f) {
    return false;
  }
  const auto& contentOffset = displayList->contentOffset();
  *rootX = (surfaceX - contentOffset.x) / zoomScale;
  *rootY = (surfaceY - contentOffset.y) / zoomScale;
  return true;
}

bool PAGScene::rootToSurfaceMatrix(Matrix* out) const {
  if (out == nullptr) {
    return false;
  }
  float zoomScale = displayList->zoomScale();
  const auto& contentOffset = displayList->contentOffset();
  *out = {zoomScale, 0, 0, zoomScale, contentOffset.x, contentOffset.y};
  return true;
}

void PAGScene::onNodesChanged(const std::vector<Node*>& dirtyNodes) {
  // The dirty nodes belong either to this scene's own document or to an external (child) document
  // this scene embeds. A child document dispatches to this scene through its own notifyChange; in
  // that case the nodes are not owned here, and an external composition is built into the runtime
  // tree once, so it cannot be patched in place — rebuild the whole runtime tree from the document.
  bool foreign = false;
  for (auto* node : dirtyNodes) {
    if (node != nullptr && (document == nullptr || !document->ownsNode(node))) {
      foreign = true;
      break;
    }
  }
  if (foreign) {
    buildRuntimeTree();
    return;
  }
  // The document node itself (NodeType::Document) triggers a full rebuild so changes to
  // PAGXDocument::width/height are reflected in the root runtime layer. Must be checked AFTER
  // the foreign-node path above so a Document node from an external document does not bypass
  // the foreign rebuild branch.
  for (auto* node : dirtyNodes) {
    if (node != nullptr && node->nodeType() == NodeType::Document) {
      buildRuntimeTree();
      return;
    }
  }
  if (_rootComposition != nullptr) {
    // The root composition has no source Composition node (it represents the document body), so the
    // ancestor path begins empty. refreshNodes pushes each child composition's source as it
    // descends.
    std::unordered_set<const Node*> dirtySet(dirtyNodes.begin(), dirtyNodes.end());
    std::unordered_set<const Composition*> visited = {};
    _rootComposition->refreshNodes(dirtyNodes, dirtySet, visited);
  }
  // Reset every timeline only when a timeline node (Animation / AnimationObject / Channel) changed.
  // Timelines can share targets and cross-reference, so the whole timeline tree is rebuilt rather
  // than patched in place; a removed animation node then simply produces no timeline. Edits that do
  // not touch a timeline node leave playback untouched.
  bool timelineDirty = false;
  for (auto* node : dirtyNodes) {
    if (node == nullptr) {
      continue;
    }
    auto type = node->nodeType();
    if (type == NodeType::Animation || type == NodeType::AnimationObject ||
        type == NodeType::Channel) {
      timelineDirty = true;
      break;
    }
  }
  if (timelineDirty) {
    if (_rootComposition != nullptr) {
      _rootComposition->resetTimelines();
    }
    timelinesByAnimation.clear();
  }
}

RuntimeBinding* PAGScene::mutableBinding() {
  return _rootComposition != nullptr ? _rootComposition->binding.get() : nullptr;
}

tgfx::DisplayList* PAGScene::getDisplayListForOptions() const {
  return displayList.get();
}

}  // namespace pagx
