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
#include "base/utils/Log.h"
#include "pagx/DataBindRuntime.h"
#include "pagx/DataContext.h"
#include "pagx/PAGAnimation.h"
#include "pagx/PAGImage.h"
#include "pagx/PAGLayer.h"
#include "pagx/PAGStateMachine.h"
#include "pagx/PAGSurface.h"
#include "pagx/PAGViewModel.h"
#include "pagx/PAGXDocument.h"
#include "pagx/PropertyData.h"
#include "pagx/nodes/Animation.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/DataBind.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/StateMachine.h"
#include "pagx/nodes/ViewModel.h"
#include "pagx/nodes/ViewModelProperty.h"
#include "pagx/runtime/Drawable.h"
#include "pagx/tgfx.h"
#include "pagx/types/Matrix.h"
#include "renderer/LayerBuilder.h"
#include "renderer/ToTGFX.h"
#include "tgfx/layers/DisplayList.h"

namespace pagx {

// Decodes an <Image> resource node into a PAGImage using the same precedence as the renderer's
// ImagePattern path: host-injected data first, then an inline data URI, then an external file path.
// Returns nullptr for a null node or one with no usable source.
static std::shared_ptr<PAGImage> DecodeImageNode(const Image* imageNode) {
  if (imageNode == nullptr) {
    return nullptr;
  }
  if (imageNode->data) {
    return PAGImage::MakeFromData(imageNode->data);
  }
  if (imageNode->filePath.find("data:") == 0) {
    return PAGImage::MakeFromDataURI(imageNode->filePath);
  }
  if (!imageNode->filePath.empty()) {
    return PAGImage::MakeFromPath(imageNode->filePath);
  }
  return nullptr;
}

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
  // A rebuild destroys the previous runtime tree and recreates all view models, so any pending
  // notifications held as raw PAGViewModelValue* would dangle. We therefore intentionally drop the
  // pending list and force-exit any active SuppressDelegation scope here. This means that if a
  // rebuild happens inside an active SuppressDelegation guard, its deferred observer notifications
  // are silently discarded (the guard's destructor later finds an empty list): the values they
  // referenced are being destroyed, so re-dispatching them is unsafe. Callers must not rely on
  // deferred notifications surviving a runtime-tree rebuild.
  pendingNotifications.clear();
  suppressNotify = false;
  timelinesByAnimation.clear();
  stateMachineTimelines.clear();
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
    ViewModel* schema, const std::shared_ptr<PAGScene>& scene,
    std::unordered_set<const ViewModel*>& visited) {
  if (schema == nullptr) return nullptr;
  // Guard against cyclic viewModelRef chains (A -> B -> A) which would recurse forever; the
  // importer does not reject such cycles, so stop at a schema already on the current path. The
  // schema is erased on return so two sibling properties referencing the same schema each get
  // their own instance rather than being mistaken for a cycle.
  if (!visited.insert(schema).second) return nullptr;
  auto vm = std::shared_ptr<PAGViewModel>(new PAGViewModel());
  vm->_id = schema->id;
  for (auto* prop : schema->properties) {
    if (prop == nullptr) continue;
    std::shared_ptr<PAGViewModelValue> value = nullptr;
    switch (prop->propertyType) {
      case ViewModelPropertyType::Number: {
        auto v = std::make_shared<PAGViewModelValueNumber>();
        v->propertyValue = prop->defaultNumber;
        v->type = ViewModelPropertyType::Number;
        value = std::move(v);
        break;
      }
      case ViewModelPropertyType::String: {
        auto v = std::make_shared<PAGViewModelValueString>();
        v->propertyValue = prop->defaultString;
        v->type = ViewModelPropertyType::String;
        value = std::move(v);
        break;
      }
      case ViewModelPropertyType::Boolean: {
        auto v = std::make_shared<PAGViewModelValueBoolean>();
        v->propertyValue = prop->defaultBoolean;
        v->type = ViewModelPropertyType::Boolean;
        value = std::move(v);
        break;
      }
      case ViewModelPropertyType::Color: {
        auto v = std::make_shared<PAGViewModelValueColor>();
        v->propertyValue = prop->defaultColor;
        v->type = ViewModelPropertyType::Color;
        value = std::move(v);
        break;
      }
      case ViewModelPropertyType::Image: {
        auto v = std::make_shared<PAGViewModelValueImage>();
        // Decode from the referenced <Image> resource. Remember the source node so a later resource
        // change (host loadFileData) can re-decode this value, unless the business side has assigned
        // its own value in the meantime (tracked by userAssigned).
        v->sourceImage = prop->defaultImage;
        v->propertyValue = DecodeImageNode(prop->defaultImage);
        v->type = ViewModelPropertyType::Image;
        value = std::move(v);
        break;
      }
      case ViewModelPropertyType::ViewModel: {
        auto v = std::make_shared<PAGViewModelValueViewModel>();
        v->type = ViewModelPropertyType::ViewModel;
        // Instantiate the nested VM from the referenced schema now, so a property declared as
        // ViewModel carries its child instance regardless of whether a Composition later binds
        // it. DataBind source paths ($vm.child.leaf) and referenceViewModel() both rely on this
        // link being present; without it, a pure data-nesting declaration (no Composition) could
        // neither be read through the VM API nor resolved by DataContext.
        if (prop->viewModelRef != nullptr) {
          v->referenceInstance = CreateViewModelFromSchema(prop->viewModelRef, scene, visited);
        }
        value = std::move(v);
        break;
      }
      case ViewModelPropertyType::Enum: {
        auto v = std::make_shared<PAGViewModelValueEnum>();
        v->propertyValue = prop->defaultEnum;
        v->type = ViewModelPropertyType::Enum;
        value = std::move(v);
        break;
      }
      case ViewModelPropertyType::Trigger: {
        auto v = std::make_shared<PAGViewModelValueTrigger>();
        v->type = ViewModelPropertyType::Trigger;
        value = std::move(v);
        break;
      }
    }
    if (value) {
      value->propertyName = prop->name;
      value->setScene(scene);
      value->converter = prop->dataConverter;
      std::shared_ptr<PropertyData> pd;
      switch (prop->propertyType) {
        case ViewModelPropertyType::Number: {
          auto d = std::make_shared<NumberPropertyData>();
          d->defaultValue = prop->defaultNumber;
          if (prop->minValue.has_value()) d->minValue = prop->minValue;
          if (prop->maxValue.has_value()) d->maxValue = prop->maxValue;
          pd = d;
          break;
        }
        case ViewModelPropertyType::String: {
          auto d = std::make_shared<StringPropertyData>();
          d->defaultValue = prop->defaultString;
          pd = d;
          break;
        }
        case ViewModelPropertyType::Boolean: {
          auto d = std::make_shared<BooleanPropertyData>();
          d->defaultValue = prop->defaultBoolean;
          pd = d;
          break;
        }
        case ViewModelPropertyType::Color: {
          auto d = std::make_shared<ColorPropertyData>();
          d->defaultValue = prop->defaultColor;
          pd = d;
          break;
        }
        case ViewModelPropertyType::Image: {
          auto d = std::make_shared<ImagePropertyData>();
          pd = d;
          break;
        }
        case ViewModelPropertyType::ViewModel: {
          auto d = std::make_shared<ViewModelPropertyData>();
          if (prop->viewModelRef != nullptr) d->viewModelId = prop->viewModelRef->id;
          pd = d;
          break;
        }
        case ViewModelPropertyType::Enum: {
          auto d = std::make_shared<EnumPropertyData>();
          d->options = prop->enumOptions;
          d->defaultValue = prop->defaultEnum;
          pd = d;
          break;
        }
        case ViewModelPropertyType::Trigger: {
          auto d = std::make_shared<TriggerPropertyData>();
          pd = d;
          break;
        }
      }
      pd->propertyName = prop->name;
      pd->customDataMap = prop->customData;
      value->pd = std::move(pd);
      // The importer rejects empty and duplicate property names, so a schema reaching this point is
      // expected to have unique non-empty names; keeping propertyMap and propertyList consistent
      // relies on that invariant.
      DEBUG_ASSERT(!prop->name.empty() &&
                   vm->propertyMap.find(prop->name) == vm->propertyMap.end());
      vm->propertyMap[prop->name] = value;
      vm->propertyList.push_back(value);
    }
  }
  visited.erase(schema);
  return vm;
}

void PAGScene::buildNestedViewModels(PAGComposition* parentComp) {
  if (parentComp == nullptr) return;
  std::vector<PAGComposition*> childComps = {};
  PAGComposition::CollectChildCompositions(parentComp, childComps);
  for (auto* childComp : childComps) {
    const auto* sourceLayer = childComp->node;
    if (sourceLayer == nullptr || sourceLayer->composition == nullptr) continue;
    const auto* compSchema = sourceLayer->composition;
    std::shared_ptr<PAGViewModel> childVM = nullptr;
    if (compSchema->viewModel != nullptr) {
      // Reuse the nested VM the parent already declared for this slot (instantiated from the
      // property's viewModelRef during the parent VM creation) when present, so the Composition
      // shares the parent's instance rather than building a divergent one. An open slot (property
      // declared without viewModelRef) is back-filled here from the Composition's own schema.
      std::shared_ptr<PAGViewModelValueViewModel> parentSlot;
      if (parentComp->compositionViewModel != nullptr && !sourceLayer->vmContext.empty()) {
        auto propName = sourceLayer->vmContext;
        if (propName.find("$vm.") == 0) propName = propName.substr(4);
        parentSlot = parentComp->compositionViewModel->propertyViewModel(propName);
        if (parentSlot != nullptr) {
          childVM = parentSlot->referenceInstance;
        }
      }
      if (childVM == nullptr) {
        std::unordered_set<const ViewModel*> nestedVisited = {};
        childVM = PAGScene::CreateViewModelFromSchema(compSchema->viewModel, shared_from_this(),
                                                      nestedVisited);
        if (parentSlot != nullptr) {
          parentSlot->referenceInstance = childVM;
        }
      }
      childComp->compositionViewModel = childVM;
    }
    // A nested Composition may carry DataBinds that reference a parent ViewModel property even when
    // it has no ViewModel of its own. Build a DataContext (with null VM when absent) chained to the
    // parent so resolve() can walk up to the parent VM. Binding is deferred to buildViewModels'
    // final pass: a parent dataBind may descend through a ViewModel reference into a nested
    // instance, and DataBindRuntime::bind() resolves (and caches) source paths once, so every
    // referenceInstance must be linked before any bind() runs.
    if (compSchema->viewModel != nullptr || !compSchema->dataBinds.empty()) {
      childComp->dataBindRuntime = std::make_unique<DataBindRuntime>();
      auto dataCtx = std::make_shared<DataContext>(childVM);
      if (parentComp->dataContext != nullptr) dataCtx->parent(parentComp->dataContext);
      childComp->dataContext = dataCtx;
    }
    buildNestedViewModels(childComp);
  }
}

void PAGScene::buildViewModels() {
  auto self = shared_from_this();
  std::unordered_set<const ViewModel*> visited = {};
  rootViewModel = PAGScene::CreateViewModelFromSchema(document->viewModel, self, visited);
  if (rootViewModel != nullptr && _rootComposition != nullptr) {
    _rootComposition->compositionViewModel = rootViewModel;
    _rootComposition->dataBindRuntime = std::make_unique<DataBindRuntime>();
    _rootComposition->dataContext = std::make_shared<DataContext>(rootViewModel);
  }
  if (_rootComposition == nullptr) {
    return;
  }
  // Build the entire nested ViewModel tree (VMs, contexts, referenceInstance links) before binding
  // any dataBinds. DataBindRuntime::bind() resolves each source path once and caches the result, so
  // a cross-level source such as "$vm.nestedRef.childProp" only resolves correctly after every
  // nested referenceInstance is linked. Binding eagerly during tree construction would resolve such
  // a path to null and permanently drop the bind.
  buildNestedViewModels(_rootComposition.get());
  // Deferred binding pass over the whole composition tree. Root binds document->dataBinds against
  // the document; each nested composition binds its own schema's dataBinds against its document.
  std::vector<PAGComposition*> pending = {_rootComposition.get()};
  while (!pending.empty()) {
    auto* comp = pending.back();
    pending.pop_back();
    if (comp->dataBindRuntime != nullptr) {
      const std::vector<DataBind*>* binds = nullptr;
      PAGXDocument* bindDoc = nullptr;
      if (comp == _rootComposition.get()) {
        binds = &document->dataBinds;
        bindDoc = document.get();
      } else if (comp->node != nullptr && comp->node->composition != nullptr) {
        binds = &comp->node->composition->dataBinds;
        bindDoc = comp->document;
      }
      if (binds != nullptr && !binds->empty()) {
        comp->dataBindRuntime->bind(*binds, comp->dataContext.get(), bindDoc, comp->binding.get());
      }
    }
    std::vector<PAGComposition*> childComps = {};
    PAGComposition::CollectChildCompositions(comp, childComps);
    for (auto* childComp : childComps) {
      pending.push_back(childComp);
    }
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
  for (auto* node : document->animations) {
    if (node != nullptr && node->nodeType() == NodeType::Animation) {
      ids.push_back(static_cast<Animation*>(node)->id);
    }
  }
  return ids;
}

std::shared_ptr<PAGAnimation> PAGScene::getTimeline(const std::string& id) {
  if (document == nullptr) {
    return nullptr;
  }
  Animation* matched = nullptr;
  for (auto* node : document->animations) {
    if (node != nullptr && node->nodeType() == NodeType::Animation) {
      auto* animation = static_cast<Animation*>(node);
      if (animation->id == id) {
        matched = animation;
        break;
      }
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
  auto timeline = std::shared_ptr<PAGAnimation>(
      new PAGAnimation(matched, nullptr, document.get(), weak_from_this()));
  timelinesByAnimation.emplace(matched, timeline);
  return timeline;
}

std::shared_ptr<PAGTimeline> PAGScene::getDefaultTimeline() {
  if (document == nullptr || document->animations.empty()) {
    return nullptr;
  }
  for (auto* node : document->animations) {
    if (node == nullptr) {
      continue;
    }
    if (node->nodeType() == NodeType::Animation) {
      return getTimeline(static_cast<Animation*>(node)->id);
    }
    if (node->nodeType() == NodeType::StateMachine) {
      return getStateMachineTimeline(static_cast<StateMachine*>(node)->id);
    }
  }
  return nullptr;
}

// Collects StateMachine nodes that are owned by a Composition (nested), so top-level lookups can
// skip them. Composition-owned StateMachines live in document->nodes alongside top-level ones, but
// are referenced from a Composition's animations list.
static std::unordered_set<const Node*> CollectCompositionOwnedStateMachines(
    const PAGXDocument* document) {
  std::unordered_set<const Node*> compOwned;
  for (const auto& node : document->nodes) {
    if (node == nullptr || node->nodeType() != NodeType::Composition) {
      continue;
    }
    for (const auto* animChild : static_cast<const Composition*>(node.get())->animations) {
      if (animChild != nullptr && animChild->nodeType() == NodeType::StateMachine) {
        compOwned.insert(animChild);
      }
    }
  }
  return compOwned;
}

std::vector<std::string> PAGScene::getStateMachineIds() const {
  std::vector<std::string> ids;
  if (document == nullptr) {
    return ids;
  }
  // Composition-owned StateMachines live in document->nodes but belong to a nested composition, not
  // the document root; exclude them so only top-level state machines are reported.
  std::unordered_set<const Node*> compOwned = CollectCompositionOwnedStateMachines(document.get());
  for (const auto& node : document->nodes) {
    if (node != nullptr && node->nodeType() == NodeType::StateMachine &&
        compOwned.find(node.get()) == compOwned.end()) {
      ids.push_back(node->id);
    }
  }
  return ids;
}

std::shared_ptr<PAGStateMachine> PAGScene::getStateMachineTimeline(const std::string& id) {
  if (document == nullptr) {
    return nullptr;
  }
  auto it = stateMachineTimelines.find(id);
  if (it != stateMachineTimelines.end()) {
    return it->second;
  }
  // Composition-owned StateMachines live in document->nodes but belong to a nested composition;
  // exclude them so only top-level state machines are vended, matching getStateMachineIds().
  std::unordered_set<const Node*> compOwned = CollectCompositionOwnedStateMachines(document.get());
  StateMachine* matched = nullptr;
  for (const auto& node : document->nodes) {
    if (node != nullptr && node->nodeType() == NodeType::StateMachine && node->id == id &&
        compOwned.find(node.get()) == compOwned.end()) {
      matched = static_cast<StateMachine*>(node.get());
      break;
    }
  }
  if (matched == nullptr) {
    return nullptr;
  }
  // Construct with a null binding so the timeline resolves the scene's current root binding lazily
  // at apply time, same as PAGAnimation.
  auto timeline = std::shared_ptr<PAGStateMachine>(
      new PAGStateMachine(matched, nullptr, document.get(), weak_from_this()));
  stateMachineTimelines.emplace(id, timeline);
  return timeline;
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
  if (!renderTo(surface, context, autoClear)) {
    device->unlock();
    return false;
  }
  drawable->present(context);
  device->unlock();
  return true;
}

bool PAGScene::renderTo(const std::shared_ptr<PAGSurface>& surface, tgfx::Context* context,
                        bool autoClear) {
  auto tgfxSurface = surface->drawable->getSurface(context);
  if (tgfxSurface == nullptr) {
    return false;
  }
  displayList->render(tgfxSurface.get(), autoClear);
  clearAllViewModelsDirty();
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

std::shared_ptr<PAGViewModel> PAGScene::viewModel() const {
  return rootViewModel;
}

void PAGScene::flushDataBinds() {
  if (_rootComposition != nullptr) _rootComposition->updateDataBinds();
}

void PAGScene::clearAllViewModelsDirty() {
  if (_rootComposition != nullptr) {
    ClearCompositionTreeDirty(_rootComposition.get());
  }
}

void PAGScene::ClearCompositionTreeDirty(PAGComposition* comp) {
  if (comp == nullptr) {
    return;
  }
  if (comp->compositionViewModel != nullptr) {
    comp->compositionViewModel->clearDirty();
  }
  std::vector<PAGComposition*> childComps = {};
  PAGComposition::CollectChildCompositions(comp, childComps);
  for (auto* childComp : childComps) {
    ClearCompositionTreeDirty(childComp);
  }
}

void PAGScene::RefreshViewModelImages(PAGComposition* comp,
                                      const std::unordered_set<const Image*>& changed) {
  if (comp == nullptr) {
    return;
  }
  if (comp->compositionViewModel != nullptr) {
    for (const auto& value : comp->compositionViewModel->properties()) {
      if (value == nullptr || value->valueType() != ViewModelPropertyType::Image) {
        continue;
      }
      auto* imageValue = static_cast<PAGViewModelValueImage*>(value.get());
      if (imageValue->sourceImage == nullptr ||
          changed.find(imageValue->sourceImage) == changed.end()) {
        continue;
      }
      // Preserve a business-side assignment: only re-decode the schema default while the value has
      // not been explicitly assigned through the public setter. Route through
      // setValueInternal(fromVM=true) so observers fire and dependent DataBinds are marked dirty
      // (the new image reaches bound targets on the next draw).
      if (imageValue->userAssigned) {
        continue;
      }
      imageValue->setValueInternal(DecodeImageNode(imageValue->sourceImage), true);
    }
  }
  std::vector<PAGComposition*> childComps = {};
  PAGComposition::CollectChildCompositions(comp, childComps);
  for (auto* childComp : childComps) {
    RefreshViewModelImages(childComp, changed);
  }
}

void PAGScene::onImageResourcesChanged(const std::vector<Image*>& changedImages) {
  if (changedImages.empty() || _rootComposition == nullptr) {
    return;
  }
  std::unordered_set<const Image*> changed(changedImages.begin(), changedImages.end());
  RefreshViewModelImages(_rootComposition.get(), changed);
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
    if (it != layerRegistry.end()) {
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
  // Reset every timeline only when a timeline node changed. Timelines (Animation drivers and the
  // state machines that play them) can share targets and cross-reference, so the whole timeline
  // tree is rebuilt rather than patched in place; a removed animation or state machine then simply
  // produces no timeline, and structural edits to a state machine (added/removed states,
  // transitions, inputs) drop the stale instance instead of running off dangling pointers. Edits
  // that do not touch a timeline node leave playback untouched.
  bool timelineDirty = false;
  for (auto* node : dirtyNodes) {
    if (node == nullptr) {
      continue;
    }
    auto type = node->nodeType();
    if (type == NodeType::Animation || type == NodeType::AnimationObject ||
        type == NodeType::Channel || type == NodeType::StateMachine ||
        type == NodeType::StateRegion || type == NodeType::State ||
        type == NodeType::StateTransition || type == NodeType::TransitionCondition ||
        type == NodeType::StateMachineInput) {
      timelineDirty = true;
      break;
    }
  }
  if (timelineDirty) {
    if (_rootComposition != nullptr) {
      _rootComposition->resetTimelines();
    }
    timelinesByAnimation.clear();
    stateMachineTimelines.clear();
  }
}

RuntimeBinding* PAGScene::mutableBinding() {
  return _rootComposition != nullptr ? _rootComposition->binding.get() : nullptr;
}

tgfx::DisplayList* PAGScene::getDisplayListForOptions() const {
  return displayList.get();
}

std::unique_ptr<tgfx::Recording> Record(tgfx::Context* context,
                                        const std::shared_ptr<PAGScene>& scene,
                                        const std::shared_ptr<PAGSurface>& surface,
                                        bool autoClear) {
  if (scene == nullptr || context == nullptr || surface == nullptr) {
    return nullptr;
  }
  scene->flushDataBinds();
  if (!scene->renderTo(surface, context, autoClear)) {
    return nullptr;
  }
  auto recording = context->flush();
  return recording;
}

}  // namespace pagx
