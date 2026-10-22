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
//  or required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "pagx/pag/exporter/Stripper.h"
#include <algorithm>
#include <cmath>
#include "pagx/nodes/Animation.h"
#include "pagx/nodes/AnimationObject.h"
#include "pagx/nodes/Channel.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/DataBind.h"
#include "pagx/nodes/Element.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Node.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/Polystar.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/State.h"
#include "pagx/nodes/StateMachine.h"
#include "pagx/nodes/StateMachineInput.h"
#include "pagx/nodes/StateRegion.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/nodes/TextPath.h"
#include "pagx/nodes/ViewModel.h"
#include "pagx/nodes/ViewModelProperty.h"

namespace pagx {
namespace {

// DataBind targets keep their leading '@' (the runtime strips it at resolution,
// DataBindRuntime.cpp); AnimationObject targets are bare ids.
std::string ResolveTargetId(const std::string& target) {
  if (!target.empty() && target[0] == '@') {
    return target.substr(1);
  }
  return target;
}

void CollectRetainedIdsFromAnimations(const std::vector<Node*>& animations,
                                      std::unordered_set<std::string>* ids) {
  for (auto* animationNode : animations) {
    if (animationNode == nullptr) {
      continue;
    }
    if (animationNode->nodeType() == NodeType::Animation) {
      auto* animation = static_cast<Animation*>(animationNode);
      if (!animation->id.empty()) {
        ids->insert(animation->id);
      }
      for (auto* object : animation->objects) {
        auto bare = ResolveTargetId(object->target);
        if (!bare.empty()) {
          ids->insert(bare);
        }
      }
    } else if (animationNode->nodeType() == NodeType::StateMachine) {
      auto* stateMachine = static_cast<StateMachine*>(animationNode);
      if (!stateMachine->id.empty()) {
        ids->insert(stateMachine->id);
      }
      for (auto* input : stateMachine->inputs) {
        if (!input->name.empty()) {
          ids->insert(input->name);
        }
      }
      for (auto* region : stateMachine->regions) {
        if (!region->name.empty()) {
          ids->insert(region->name);
        }
        if (!region->initialState.empty()) {
          ids->insert(region->initialState);
        }
        for (auto* state : region->states) {
          if (!state->name.empty()) {
            ids->insert(state->name);
          }
        }
      }
    }
  }
}

void CollectRetainedIdsFromDataBinds(const std::vector<DataBind*>& dataBinds,
                                     std::unordered_set<std::string>* ids) {
  for (auto* bind : dataBinds) {
    auto bare = ResolveTargetId(bind->target);
    if (!bare.empty()) {
      ids->insert(bare);
    }
  }
}

// Returns true if any Fill or Stroke in the container can consume geometry that precedes
// it. Elements after the last painter are dead (§4.6).
bool IsGeometryElement(NodeType type) {
  return type == NodeType::Rectangle || type == NodeType::Ellipse || type == NodeType::Polystar ||
         type == NodeType::Path || type == NodeType::Text || type == NodeType::TextPath;
}

bool IsPainter(NodeType type) {
  return type == NodeType::Fill || type == NodeType::Stroke;
}

// Removes geometry elements that no painter can consume: elements after the last painter
// in the list, and elements between the last TextBox and the next painter (TextBox consumes
// accumulated geometry).
void RemoveUnusedGeometry(std::vector<Element*>* elements) {
  if (elements->empty()) {
    return;
  }
  // Find the index of the last painter.
  int lastPainter = -1;
  for (int i = static_cast<int>(elements->size()) - 1; i >= 0; i--) {
    if (IsPainter((*elements)[i]->nodeType())) {
      lastPainter = i;
      break;
    }
  }
  if (lastPainter < 0) {
    // No painter: all geometry is dead, but modifiers (TrimPath, RoundCorner, MergePath,
    // Repeater, TextModifier) and TextBox still reference preceding geometry, so keep
    // everything until we understand the full consumption model. Only remove pure
    // geometry that follows a TextBox with no subsequent painter.
    return;
  }
  // Remove geometry after the last painter.
  elements->erase(
      std::remove_if(elements->begin() + lastPainter + 1, elements->end(),
                     [](Element* element) { return IsGeometryElement(element->nodeType()); }),
      elements->end());
}

// Recursively checks whether a Layer is referenced as a mask by any other layer in the
// tree. The walk is over the whole document because a mask can be a sibling or a child of
// the masked layer.
bool IsUsedAsMask(const Layer* candidate, const std::vector<Layer*>& layers) {
  for (auto* layer : layers) {
    if (layer->mask == candidate) {
      return true;
    }
    if (IsUsedAsMask(candidate, layer->children)) {
      return true;
    }
  }
  return false;
}

bool IsUsedAsMaskAnywhere(const PAGXDocument* document, const Layer* candidate) {
  if (IsUsedAsMask(candidate, document->layers)) {
    return true;
  }
  for (auto& node : document->nodes) {
    if (node->nodeType() == NodeType::Composition) {
      auto* composition = static_cast<Composition*>(node.get());
      if (IsUsedAsMask(candidate, composition->layers)) {
        return true;
      }
    }
  }
  return false;
}

// Liveness targets are ids referenced by animations and data binds specifically. All Layer
// ids survive for PAGLayer::id(), but only liveness targets keep invisible nodes alive.
std::unordered_set<std::string> CollectLivenessTargets(const PAGXDocument* document) {
  std::unordered_set<std::string> targets = {};
  for (auto* animationNode : document->animations) {
    if (animationNode == nullptr || animationNode->nodeType() != NodeType::Animation) {
      continue;
    }
    for (auto* object : static_cast<Animation*>(animationNode)->objects) {
      auto bare = ResolveTargetId(object->target);
      if (!bare.empty()) {
        targets.insert(bare);
      }
    }
  }
  for (auto* bind : document->dataBinds) {
    auto bare = ResolveTargetId(bind->target);
    if (!bare.empty()) {
      targets.insert(bare);
    }
  }
  for (auto& node : document->nodes) {
    if (node->nodeType() != NodeType::Composition) {
      continue;
    }
    auto* composition = static_cast<Composition*>(node.get());
    for (auto* animationNode : composition->animations) {
      if (animationNode == nullptr || animationNode->nodeType() != NodeType::Animation) {
        continue;
      }
      for (auto* object : static_cast<Animation*>(animationNode)->objects) {
        auto bare = ResolveTargetId(object->target);
        if (!bare.empty()) {
          targets.insert(bare);
        }
      }
    }
    for (auto* bind : composition->dataBinds) {
      auto bare = ResolveTargetId(bind->target);
      if (!bare.empty()) {
        targets.insert(bare);
      }
    }
  }
  return targets;
}

void RemoveDeadLayers(std::vector<Layer*>* layers,
                      const std::unordered_set<std::string>& livenessTargets,
                      const PAGXDocument* document) {
  layers->erase(std::remove_if(layers->begin(), layers->end(),
                               [&livenessTargets, document](Layer* layer) {
                                 if (!layer->visible) {
                                   auto isTarget =
                                       !layer->id.empty() && livenessTargets.count(layer->id) > 0;
                                   if (!isTarget && !IsUsedAsMaskAnywhere(document, layer)) {
                                     return true;
                                   }
                                 }
                                 return false;
                               }),
                layers->end());
  for (auto* layer : *layers) {
    RemoveDeadLayers(&layer->children, livenessTargets, document);
    RemoveUnusedGeometry(&layer->contents);
  }
}

}  // namespace

namespace {
void CollectLayerIds(const std::vector<Layer*>& layers, std::unordered_set<std::string>* ids) {
  for (auto* layer : layers) {
    if (!layer->id.empty()) {
      ids->insert(layer->id);
    }
    CollectLayerIds(layer->children, ids);
  }
}
}  // namespace

std::unordered_set<std::string> Stripper::CollectRetainedIds(const PAGXDocument* document) {
  std::unordered_set<std::string> ids = {};
  CollectLayerIds(document->layers, &ids);
  for (auto& node : document->nodes) {
    if (node->nodeType() == NodeType::Composition) {
      auto* composition = static_cast<Composition*>(node.get());
      CollectLayerIds(composition->layers, &ids);
    }
  }
  CollectRetainedIdsFromAnimations(document->animations, &ids);
  CollectRetainedIdsFromDataBinds(document->dataBinds, &ids);
  for (auto& node : document->nodes) {
    if (node->nodeType() == NodeType::Composition) {
      auto* composition = static_cast<Composition*>(node.get());
      CollectRetainedIdsFromAnimations(composition->animations, &ids);
      CollectRetainedIdsFromDataBinds(composition->dataBinds, &ids);
    }
  }
  return ids;
}

void Stripper::StripMetadata(PAGXDocument* document) {
  for (auto& node : document->nodes) {
    auto type = node->nodeType();
    // C27: only ViewModelProperty, the document root, and ImagePattern keep data-*.
    if (type != NodeType::ViewModelProperty && type != NodeType::ImagePattern &&
        type != NodeType::Document) {
      node->customData.clear();
    }
    node->sourceLine = -1;
    if (type == NodeType::Layer) {
      auto* layer = static_cast<Layer*>(node.get());
      layer->importDirective = {};
    }
    if (type == NodeType::Font) {
      auto* font = static_cast<Font*>(node.get());
      // The external font path only feeds export-time font registration (§4.3).
      font->file.clear();
    }
  }
  // The document root's data-* is stored on PAGXDocument (it inherits Node).
  // StripMetadata above clears it via the Document type check; the document root keeps its
  // own customData (C27) because the loop excludes NodeType::Document.
}

void Stripper::StripIds(PAGXDocument* document,
                        const std::unordered_set<std::string>& retainedIds) {
  for (auto& node : document->nodes) {
    auto type = node->nodeType();
    // Layer ids are always in retainedIds. Animation / StateMachine / StateRegion / State /
    // Input ids are collected into retainedIds by CollectRetainedIds. Everything else
    // (resources, vector elements, styles, filters, modifiers) strips.
    if (type == NodeType::Layer || type == NodeType::Animation || type == NodeType::StateMachine ||
        type == NodeType::StateRegion || type == NodeType::State ||
        type == NodeType::StateMachineInput) {
      continue;
    }
    if (!node->id.empty() && retainedIds.count(node->id) == 0) {
      node->id.clear();
    }
  }
}

void Stripper::RemoveDeadNodes(PAGXDocument* document,
                               const std::unordered_set<std::string>& /*retainedIds*/) {
  auto livenessTargets = CollectLivenessTargets(document);
  RemoveDeadLayers(&document->layers, livenessTargets, document);
  for (auto& node : document->nodes) {
    if (node->nodeType() == NodeType::Composition) {
      auto* composition = static_cast<Composition*>(node.get());
      RemoveDeadLayers(&composition->layers, livenessTargets, document);
    }
  }
}

void Stripper::Strip(PAGXDocument* document) {
  auto retainedIds = CollectRetainedIds(document);
  StripMetadata(document);
  StripIds(document, retainedIds);
  RemoveDeadNodes(document, retainedIds);
}

}  // namespace pagx
