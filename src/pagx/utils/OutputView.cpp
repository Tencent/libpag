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
#include "tgfx/layers/Layer.h"

namespace pagx {

namespace {

bool IsFiniteRect(const Rect& rect) {
  if (!std::isfinite(rect.x) || !std::isfinite(rect.y) || !std::isfinite(rect.width) ||
      !std::isfinite(rect.height)) {
    return false;
  }
  if (rect.width <= 0 || rect.height <= 0) {
    return false;
  }
  return std::isfinite(rect.x + rect.width) && std::isfinite(rect.y + rect.height);
}

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
  bool ambiguousPath = false;
  std::unordered_set<const Layer*> onPath = {};
  std::unordered_set<const Layer*> visited = {};
};

void SearchLayers(const std::vector<Layer*>& layers, TargetSearch* search) {
  for (auto* layer : layers) {
    if (layer == nullptr || search->ambiguousPath) {
      continue;
    }
    if (search->onPath.find(layer) != search->onPath.end()) {
      search->ambiguousPath = true;
      return;
    }
    if (!search->visited.insert(layer).second) {
      search->ambiguousPath = true;
      return;
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

// Returns the layer that uses `mask` as its mask. TGFX stores the relation only on the mask side
// (Layer::maskOwner), so the owner has to be located from the tree. Owners inside the mask's own
// subtree are not reported: they reference the target as a mask without making the target itself a
// mask child, so the target still has to be drawn with that relationship intact.
std::shared_ptr<tgfx::Layer> FindMaskOwner(const std::shared_ptr<tgfx::Layer>& layer,
                                           const tgfx::Layer* mask) {
  if (layer == nullptr || mask == nullptr || layer.get() == mask) {
    return nullptr;
  }
  if (layer->mask().get() == mask) {
    return layer;
  }
  for (const auto& child : layer->children()) {
    auto owner = FindMaskOwner(child, mask);
    if (owner != nullptr) {
      return owner;
    }
  }
  return nullptr;
}

// Temporarily replaces the target with an identity parent so that the normal child compositing
// path applies the target's transform, alpha, visibility, mask and scrollRect exactly once. The
// original tree is restored on destruction.
class OutputTargetParent {
 public:
  OutputTargetParent(const std::shared_ptr<tgfx::Layer>& root,
                     const std::shared_ptr<tgfx::Layer>& target)
      : target(target), parent(target != nullptr ? target->parent() : nullptr) {
    if (target == nullptr || parent == nullptr) {
      return;
    }
    maskOwner = FindMaskOwner(root, target.get());
    wrapper = tgfx::Layer::Make();
    if (wrapper == nullptr || !parent->replaceChild(target, wrapper)) {
      wrapper = nullptr;
      return;
    }
    // TGFX skips child layers that act as another layer's mask, so a target referenced by a
    // `mask` attribute would never be drawn through the wrapper. The owner lives outside this
    // wrapper, so releasing the mask only affects that skip check for the duration of the draw;
    // the original relationship is restored in the destructor.
    if (maskOwner != nullptr) {
      maskOwner->setMask(nullptr);
    }
    wrapper->addChild(target);
  }

  ~OutputTargetParent() {
    if (wrapper == nullptr) {
      return;
    }
    parent->replaceChild(wrapper, target);
    if (maskOwner != nullptr) {
      maskOwner->setMask(target);
    }
  }

  OutputTargetParent(const OutputTargetParent&) = delete;
  OutputTargetParent& operator=(const OutputTargetParent&) = delete;

  bool isValid() const {
    return wrapper != nullptr;
  }

  const std::shared_ptr<tgfx::Layer>& layer() const {
    return wrapper;
  }

 private:
  std::shared_ptr<tgfx::Layer> target = nullptr;
  tgfx::Layer* parent = nullptr;
  std::shared_ptr<tgfx::Layer> maskOwner = nullptr;
  std::shared_ptr<tgfx::Layer> wrapper = nullptr;
};

OutputViewStatus ReportInvalid(std::string* error, const std::string& message) {
  if (error != nullptr) {
    *error = message;
  }
  return OutputViewStatus::Invalid;
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
  auto declaredTarget = OutputViewDeclarationAccess::DeclaredTargetLayer(*document);
  auto declaredBounds = OutputViewDeclarationAccess::DeclaredBounds(*document);
  // A field counts as declared when it either parsed or still carries raw authored text, so
  // clearing the parsed pair returns the document to the whole-canvas state instead of leaving a
  // sticky parse error behind.
  bool hasTarget = !document->targetLayerId.empty() || declaredTarget.has_value();
  bool hasBounds = document->outputBounds.has_value() || declaredBounds.has_value();
  if (!hasTarget && !hasBounds) {
    return OutputViewStatus::None;
  }
  if (hasTarget != hasBounds) {
    return ReportInvalid(
        error, "The 'targetLayer' and 'outputBounds' attributes must be specified together.");
  }
  // Report the authored text for an attribute that never parsed; a present parsed value is the
  // only evidence that the declaration survived import.
  if (document->targetLayerId.empty()) {
    return ReportInvalid(error, "Invalid value '" +
                                    (declaredTarget.has_value() ? *declaredTarget : std::string()) +
                                    "' for 'targetLayer' attribute. Expected a Layer reference "
                                    "'@id'.");
  }
  if (!document->outputBounds.has_value()) {
    return ReportInvalid(
        error, "Invalid value '" + (declaredBounds.has_value() ? *declaredBounds : std::string()) +
                   "' for 'outputBounds' attribute. Expected 'x,y,width,height' with finite "
                   "components and positive width/height.");
  }
  if (!IsValidTargetId(document->targetLayerId)) {
    return ReportInvalid(error, "Invalid 'targetLayer' value '" + document->targetLayerId +
                                    "': the id must be non-empty and contain no whitespace.");
  }
  if (!IsFiniteRect(*document->outputBounds)) {
    return ReportInvalid(
        error,
        "Invalid 'outputBounds': all components must be finite and width/height must "
        "be greater than zero.");
  }
  std::unordered_set<const PAGXDocument*> visitedDocuments;
  if (HasDynamicContent(document, visitedDocuments)) {
    return ReportInvalid(
        error,
        "Target view documents with animations, state machines or data binds are not "
        "supported.");
  }
  if (document->findNode(document->targetLayerId) == nullptr) {
    return ReportInvalid(error, "The 'targetLayer' reference '@" + document->targetLayerId +
                                    "' does not match any node in the document.");
  }
  int matchingNodes = 0;
  for (const auto& node : document->nodes) {
    if (node != nullptr && node->id == document->targetLayerId) {
      matchingNodes++;
    }
  }
  if (matchingNodes != 1) {
    return ReportInvalid(error, "The 'targetLayer' reference '@" + document->targetLayerId +
                                    "' is ambiguous: the id is used by multiple nodes.");
  }
  auto* node = document->findNode(document->targetLayerId);
  if (node->nodeType() != NodeType::Layer) {
    return ReportInvalid(error, "The 'targetLayer' reference '@" + document->targetLayerId +
                                    "' does not resolve to a Layer.");
  }
  TargetSearch search = {document->targetLayerId};
  SearchLayers(document->layers, &search);
  if (search.ambiguousPath) {
    return ReportInvalid(error, "The 'targetLayer' reference '@" + document->targetLayerId +
                                    "' is not uniquely reachable from the document layer tree.");
  }
  if (search.matches == 0) {
    return ReportInvalid(error, "The 'targetLayer' reference '@" + document->targetLayerId +
                                    "' is not reachable from the document layer tree.");
  }
  if (search.matches > 1) {
    return ReportInvalid(error, "The 'targetLayer' reference '@" + document->targetLayerId +
                                    "' is reachable through multiple paths.");
  }
  if (targetLayer != nullptr) {
    *targetLayer = search.found;
  }
  return OutputViewStatus::Valid;
}

bool HasDeclaredOutputView(const PAGXDocument& document) {
  return !document.targetLayerId.empty() || document.outputBounds.has_value() ||
         OutputViewDeclarationAccess::DeclaredTargetLayer(document).has_value() ||
         OutputViewDeclarationAccess::DeclaredBounds(document).has_value();
}

void DrawOutputTarget(tgfx::Canvas* canvas, const std::shared_ptr<tgfx::Layer>& root,
                      const std::shared_ptr<tgfx::Layer>& targetLayer) {
  if (canvas == nullptr || root == nullptr || targetLayer == nullptr) {
    return;
  }
  if (targetLayer == root) {
    root->draw(canvas);
    return;
  }
  OutputTargetParent parent(root, targetLayer);
  if (!parent.isValid()) {
    return;
  }
  auto wrapper = parent.layer();
  // Draw helpers must not leak the coordinate mapping into the caller's canvas state: surfaces
  // reuse one Canvas across frames.
  tgfx::AutoCanvasRestore canvasRestore(canvas);
  canvas->concat(wrapper->getRelativeMatrix(root.get()));
  wrapper->draw(canvas);
}

}  // namespace pagx
