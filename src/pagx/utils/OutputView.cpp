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
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "pagx/utils/OutputView.h"
#include <cmath>
#include <unordered_set>
#include <utility>
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Composition.h"
#include "tgfx/core/Canvas.h"
#include "tgfx/core/Point.h"
#include "tgfx/layers/Layer.h"

namespace pagx {

const char* OutputBoundsRequirement() {
  return "'x,y,width,height' with finite components and positive width/height";
}

bool IsValidOutputBounds(const Rect& bounds) {
  if (!std::isfinite(bounds.x) || !std::isfinite(bounds.y) || !std::isfinite(bounds.width) ||
      !std::isfinite(bounds.height)) {
    return false;
  }
  if (bounds.width <= 0 || bounds.height <= 0) {
    return false;
  }
  return std::isfinite(bounds.x + bounds.width) && std::isfinite(bounds.y + bounds.height);
}

namespace {

bool IsValidTargetId(const std::string& id) {
  if (id.empty()) {
    return false;
  }
  for (char character : id) {
    if (character == ' ' || character == '\t' || character == '\n' || character == '\r') {
      return false;
    }
  }
  return true;
}

struct TargetSearch {
  const std::string& targetId;
  const Layer* found = nullptr;
  int matches = 0;
  bool ambiguous = false;
  std::unordered_set<const Layer*> onPath = {};
  std::unordered_set<const Layer*> visited = {};
};

void SearchLayers(const std::vector<Layer*>& layers, TargetSearch* search) {
  for (auto* layer : layers) {
    if (layer == nullptr || search->ambiguous) {
      continue;
    }
    if (search->onPath.find(layer) != search->onPath.end()) {
      search->ambiguous = true;
      return;
    }
    if (!search->visited.insert(layer).second) {
      // A repeated pointer is not a second path: duplicate ids make the importer reuse one node,
      // so an unrelated repeated subtree must not invalidate an otherwise unique reference. Only a
      // repeated target means the reference itself is ambiguous.
      if (layer->id == search->targetId) {
        search->ambiguous = true;
        return;
      }
      continue;
    }
    if (layer->id == search->targetId) {
      search->found = layer;
      search->matches++;
    }
    search->onPath.insert(layer);
    SearchLayers(layer->children, search);
    search->onPath.erase(layer);
  }
}

bool HasDynamicContent(const PAGXDocument* document,
                       std::unordered_set<const PAGXDocument*>& visited) {
  if (!visited.insert(document).second) {
    return false;
  }
  if (!document->animations.empty() || !document->dataBinds.empty()) {
    return true;
  }
  for (const auto& node : document->nodes) {
    if (node == nullptr) {
      continue;
    }
    auto type = node->nodeType();
    if (type == NodeType::Animation || type == NodeType::StateMachine ||
        type == NodeType::DataBind) {
      return true;
    }
    if (type == NodeType::Composition &&
        !static_cast<const Composition*>(node.get())->animations.empty()) {
      return true;
    }
    if (type == NodeType::Layer) {
      const auto& externalDoc = static_cast<const Layer*>(node.get())->externalDoc;
      if (externalDoc != nullptr && HasDynamicContent(externalDoc.get(), visited)) {
        return true;
      }
    }
  }
  return false;
}

OutputViewStatus ReportInvalid(std::string* error, const std::string& message) {
  if (error != nullptr) {
    *error = message;
  }
  return OutputViewStatus::Invalid;
}

const Layer* ReportTargetError(std::string* error, const std::string& message) {
  if (error != nullptr) {
    *error = message;
  }
  return nullptr;
}

struct DeclarationFields {
  std::optional<std::string> targetText = std::nullopt;
  std::optional<std::string> boundsText = std::nullopt;
  bool hasTarget = false;
  bool hasBounds = false;
};

// A field counts as declared when it either parsed or still carries raw authored text, so clearing
// the parsed pair returns the document to the whole-canvas state instead of leaving a sticky parse
// error behind.
DeclarationFields CollectDeclaration(const PAGXDocument& document) {
  DeclarationFields fields = {};
  fields.targetText = OutputViewDeclarationAccess::DeclaredTargetLayer(document);
  fields.boundsText = OutputViewDeclarationAccess::DeclaredBounds(document);
  fields.hasTarget = !document.targetLayerId.empty() || fields.targetText.has_value();
  fields.hasBounds = document.outputBounds.has_value() || fields.boundsText.has_value();
  return fields;
}

// Checks the declaration fields, their text and the numeric rules, leaving target resolution to
// FindUniqueTargetLayer(). Returns None when the document declares no output view at all.
OutputViewStatus ValidateOutputViewDeclaration(const PAGXDocument& document, std::string* error) {
  auto fields = CollectDeclaration(document);
  if (!fields.hasTarget && !fields.hasBounds) {
    return OutputViewStatus::None;
  }
  if (fields.hasTarget != fields.hasBounds) {
    return ReportInvalid(
        error, "The 'targetLayer' and 'outputBounds' attributes must be specified together.");
  }
  // Report the authored text for an attribute that never parsed; a present parsed value is the
  // only evidence that the declaration survived import.
  if (document.targetLayerId.empty()) {
    return ReportInvalid(error,
                         "Invalid value '" +
                             (fields.targetText.has_value() ? *fields.targetText : std::string()) +
                             "' for 'targetLayer' attribute. Expected a Layer reference "
                             "'@id'.");
  }
  if (!document.outputBounds.has_value()) {
    return ReportInvalid(
        error, "Invalid value '" +
                   (fields.boundsText.has_value() ? *fields.boundsText : std::string()) +
                   "' for 'outputBounds' attribute. Expected 'x,y,width,height' with finite "
                   "components and positive width/height.");
  }
  if (!IsValidTargetId(document.targetLayerId)) {
    return ReportInvalid(error, "Invalid 'targetLayer' value '" + document.targetLayerId +
                                    "': the id must be non-empty and contain no whitespace.");
  }
  if (!IsValidOutputBounds(*document.outputBounds)) {
    return ReportInvalid(
        error, std::string("Invalid 'outputBounds': expected ") + OutputBoundsRequirement() + ".");
  }
  std::unordered_set<const PAGXDocument*> visitedDocuments;
  if (HasDynamicContent(&document, visitedDocuments)) {
    return ReportInvalid(
        error,
        "Target view documents with animations, state machines or data binds are not "
        "supported.");
  }
  return OutputViewStatus::Valid;
}

// Resolves the parsed id to one Layer. The caller has already validated the id syntax, so this only
// resolves the reference: one registry scan for existence, uniqueness and type, followed by one
// tree walk for reachability.
const Layer* FindUniqueTargetLayer(const PAGXDocument& document, std::string* error) {
  const Node* node = nullptr;
  int matchingNodes = 0;
  for (const auto& candidate : document.nodes) {
    if (candidate != nullptr && candidate->id == document.targetLayerId) {
      node = candidate.get();
      matchingNodes++;
    }
  }
  if (matchingNodes == 0) {
    return ReportTargetError(error, "The 'targetLayer' reference '@" + document.targetLayerId +
                                        "' does not match any node in the document.");
  }
  if (matchingNodes != 1) {
    return ReportTargetError(error, "The 'targetLayer' reference '@" + document.targetLayerId +
                                        "' is ambiguous: the id is used by multiple nodes.");
  }
  if (node->nodeType() != NodeType::Layer) {
    return ReportTargetError(error, "The 'targetLayer' reference '@" + document.targetLayerId +
                                        "' does not resolve to a Layer.");
  }
  TargetSearch search = {document.targetLayerId};
  SearchLayers(document.layers, &search);
  if (search.ambiguous) {
    return ReportTargetError(error,
                             "The 'targetLayer' reference '@" + document.targetLayerId +
                                 "' is not uniquely reachable from the document layer tree.");
  }
  if (search.matches == 0) {
    return ReportTargetError(error, "The 'targetLayer' reference '@" + document.targetLayerId +
                                        "' is not reachable from the document layer tree.");
  }
  if (search.matches > 1) {
    return ReportTargetError(error, "The 'targetLayer' reference '@" + document.targetLayerId +
                                        "' is reachable through multiple paths.");
  }
  return search.found;
}

}  // namespace

std::optional<std::string> OutputViewDeclarationAccess::DeclaredTargetLayer(
    const PAGXDocument& document) {
  return document.declaredTargetLayer;
}

std::optional<std::string> OutputViewDeclarationAccess::DeclaredBounds(
    const PAGXDocument& document) {
  return document.declaredOutputBounds;
}

void OutputViewDeclarationAccess::SetDeclared(PAGXDocument* document,
                                              std::optional<std::string> targetLayer,
                                              std::optional<std::string> bounds) {
  if (document == nullptr) {
    return;
  }
  document->declaredTargetLayer = std::move(targetLayer);
  document->declaredOutputBounds = std::move(bounds);
}

const Layer* ResolveOutputTargetLayer(const PAGXDocument* document, std::string* error) {
  if (error != nullptr) {
    error->clear();
  }
  if (document == nullptr || document->targetLayerId.empty()) {
    return nullptr;
  }
  if (!IsValidTargetId(document->targetLayerId)) {
    return ReportTargetError(error, "Invalid 'targetLayer' value '" + document->targetLayerId +
                                        "': the id must be non-empty and contain no whitespace.");
  }
  return FindUniqueTargetLayer(*document, error);
}

OutputViewStatus ResolveOutputView(const PAGXDocument* document, const Layer** targetLayer,
                                   std::string* error) {
  if (targetLayer != nullptr) {
    *targetLayer = nullptr;
  }
  if (error != nullptr) {
    error->clear();
  }
  if (document == nullptr) {
    return OutputViewStatus::None;
  }
  auto status = ValidateOutputViewDeclaration(*document, error);
  if (status != OutputViewStatus::Valid) {
    return status;
  }
  std::string targetError;
  auto* target = FindUniqueTargetLayer(*document, &targetError);
  if (target == nullptr) {
    return ReportInvalid(error, targetError);
  }
  if (targetLayer != nullptr) {
    *targetLayer = target;
  }
  return OutputViewStatus::Valid;
}

bool HasDeclaredOutputView(const PAGXDocument& document) {
  auto fields = CollectDeclaration(document);
  return fields.hasTarget || fields.hasBounds;
}

bool CheckOutputViewExportSupport(const PAGXDocument& document, const char* format,
                                  std::string* errorMsg) {
  if (!HasDeclaredOutputView(document)) {
    return true;
  }
  if (errorMsg != nullptr) {
    *errorMsg =
        std::string(format) + " export does not support documents with targetLayer/outputBounds.";
  }
  return false;
}

uint64_t OutputViewValidationAccess::RegistryVersion(const PAGXDocument& document) {
  return document.nodeRegistryVersion;
}

namespace {

bool IsRenderOnlyLeaf(NodeType type) {
  return type == NodeType::SolidColor || type == NodeType::Rectangle || type == NodeType::Image;
}

template <typename T>
bool SamePointerList(const std::vector<T*>& current, const std::vector<const T*>& snapshot) {
  if (current.size() != snapshot.size()) {
    return false;
  }
  for (size_t i = 0; i < current.size(); i++) {
    if (current[i] != snapshot[i]) {
      return false;
    }
  }
  return true;
}

}  // namespace

bool CaptureOutputViewValidation(const PAGXDocument& document, const Layer* targetLayer,
                                 OutputViewValidationSnapshot* snapshot) {
  if (snapshot == nullptr || targetLayer == nullptr) {
    return false;
  }
  snapshot->targetId = document.targetLayerId;
  snapshot->bounds = document.outputBounds;
  snapshot->declaredTargetLayer = OutputViewDeclarationAccess::DeclaredTargetLayer(document);
  snapshot->declaredBounds = OutputViewDeclarationAccess::DeclaredBounds(document);
  snapshot->layers.assign(document.layers.begin(), document.layers.end());
  snapshot->animations.assign(document.animations.begin(), document.animations.end());
  snapshot->dataBinds.assign(document.dataBinds.begin(), document.dataBinds.end());
  snapshot->targetLayer = targetLayer;
  snapshot->registryVersion = OutputViewValidationAccess::RegistryVersion(document);
  return true;
}

bool CanReuseOutputViewValidation(const PAGXDocument& document,
                                  const std::vector<Node*>& reportedDirty, bool layoutChanged,
                                  const OutputViewValidationSnapshot& snapshot) {
  if (layoutChanged || snapshot.targetLayer == nullptr) {
    return false;
  }
  for (auto* node : reportedDirty) {
    if (node == nullptr || !document.ownsNode(node) || !IsRenderOnlyLeaf(node->nodeType())) {
      return false;
    }
    // A render-only leaf that now carries the target id would make the reference ambiguous even
    // though the node id index was not re-registered.
    if (!node->id.empty() && node->id == snapshot.targetId) {
      return false;
    }
  }
  if (OutputViewValidationAccess::RegistryVersion(document) != snapshot.registryVersion) {
    return false;
  }
  if (document.targetLayerId != snapshot.targetId || document.outputBounds != snapshot.bounds) {
    return false;
  }
  if (OutputViewDeclarationAccess::DeclaredTargetLayer(document) != snapshot.declaredTargetLayer ||
      OutputViewDeclarationAccess::DeclaredBounds(document) != snapshot.declaredBounds) {
    return false;
  }
  if (!SamePointerList(document.layers, snapshot.layers) ||
      !SamePointerList(document.animations, snapshot.animations) ||
      !SamePointerList(document.dataBinds, snapshot.dataBinds)) {
    return false;
  }
  // Check ownership before reading the validated target: a removed node would otherwise be
  // dereferenced through a stale pointer.
  if (!document.ownsNode(snapshot.targetLayer)) {
    return false;
  }
  if (snapshot.targetLayer->id != snapshot.targetId) {
    return false;
  }
  return document.findNode(snapshot.targetId) == snapshot.targetLayer;
}

OutputTargetState::~OutputTargetState() {
  reset();
}

bool OutputTargetState::prepare(const std::shared_ptr<tgfx::Layer>& root,
                                const std::shared_ptr<tgfx::Layer>& target) {
  reset();
  if (root == nullptr || target == nullptr) {
    return false;
  }
  if (root != target && !root->contains(target)) {
    return false;
  }
  // Enable external mask release only for an actual wrapper: when the target is the root there is
  // no parent to insert into and Drawing handles that case directly.
  bool releaseExternalMask = root != target;
  if (!parent.prepare(root, target, releaseExternalMask)) {
    return false;
  }
  this->root = root;
  this->target = target;
  scopedParenting = parent.hasReleasedMask();
  return true;
}

bool OutputTargetState::draw(tgfx::Canvas* canvas) {
  if (canvas == nullptr || root == nullptr || target == nullptr) {
    if (scopedParenting) {
      parent.reset();
    }
    return false;
  }
  if (scopedParenting && parent.layer() == nullptr &&
      (!root->contains(target) || !parent.prepare(root, target, true))) {
    return false;
  }
  if (!parent.isValid()) {
    if (scopedParenting) {
      parent.reset();
    }
    return false;
  }
  const auto& wrapper = parent.layer();
  // Draw helpers must not leak the coordinate mapping into the caller's canvas state: surfaces
  // reuse one Canvas across frames.
  tgfx::AutoCanvasRestore canvasRestore(canvas);
  canvas->concat(wrapper->getRelativeMatrix(root.get()));
  wrapper->draw(canvas);
  if (scopedParenting) {
    parent.reset();
  }
  return true;
}

bool OutputTargetState::isPrepared() const {
  if (scopedParenting && parent.layer() == nullptr) {
    return root != nullptr && target != nullptr && root->contains(target);
  }
  return parent.isValid();
}

void OutputTargetState::reset() {
  parent.reset();
  root = nullptr;
  target = nullptr;
  scopedParenting = false;
}

void ApplyOutputViewTransform(tgfx::Canvas* canvas, const Rect& bounds, float zoom,
                              const tgfx::Point& contentOffset) {
  if (canvas == nullptr) {
    return;
  }
  canvas->translate(contentOffset.x, contentOffset.y);
  canvas->scale(zoom, zoom);
  canvas->translate(-bounds.x, -bounds.y);
}

bool DrawOutputTarget(tgfx::Canvas* canvas, const std::shared_ptr<tgfx::Layer>& root,
                      const std::shared_ptr<tgfx::Layer>& targetLayer) {
  if (canvas == nullptr || root == nullptr || targetLayer == nullptr) {
    return false;
  }
  if (targetLayer == root) {
    root->draw(canvas);
    return true;
  }
  OutputTargetState state;
  if (!state.prepare(root, targetLayer)) {
    return false;
  }
  return state.draw(canvas);
}

}  // namespace pagx
