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
//  license is distributed on an "AS IS" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "pagx/svg/SVGAnimationPlan.h"
#include <unordered_set>
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Animation.h"
#include "pagx/nodes/AnimationObject.h"
#include "pagx/nodes/AnimationTimeline.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Timeline.h"
#include "pagx/utils/StringParser.h"

namespace pagx {

namespace {

void AddWarning(std::vector<std::string>* warnings, std::string message) {
  if (warnings != nullptr) {
    warnings->push_back(std::move(message));
  }
}

bool IsLayerTransformChannel(const std::string& name) {
  // This exporter's transform model projects matrix3D / matrix / renderPosition only, so `matrix`
  // is the single transform channel an SMIL animation can drive. The rotation / scale / skew /
  // anchor components are not part of it and fall through to the unsupported-channel warning.
  return name == "matrix";
}

bool IsGroupTransformChannel(const std::string& name) {
  return name == "position.x" || name == "position.y" || name == "anchor.x" || name == "anchor.y" ||
         name == "rotation" || name == "scale.x" || name == "scale.y" || name == "skew" ||
         name == "skewAxis";
}

}  // namespace

bool SVGAnimationPlan::IsTransformChannel(NodeType nodeType, const std::string& channelName) {
  if (nodeType == NodeType::Layer) {
    return IsLayerTransformChannel(channelName);
  }
  if (nodeType == NodeType::Group || nodeType == NodeType::TextBox) {
    return IsGroupTransformChannel(channelName);
  }
  return false;
}

namespace {

bool IsSupportedChannel(NodeType nodeType, const std::string& name) {
  switch (nodeType) {
    case NodeType::Layer:
      return name == "alpha" || name == "visible" || IsLayerTransformChannel(name);
    case NodeType::Group:
    case NodeType::TextBox:
      return name == "alpha" || IsGroupTransformChannel(name);
    case NodeType::Rectangle:
      return name == "position.x" || name == "position.y" || name == "size.width" ||
             name == "size.height" || name == "roundness";
    case NodeType::Ellipse:
      return name == "position.x" || name == "position.y" || name == "size.width" ||
             name == "size.height";
    case NodeType::Fill:
      return name == "color" || name == "alpha";
    case NodeType::Stroke:
      return name == "color" || name == "alpha" || name == "width" || name == "dashOffset" ||
             name == "miterLimit";
    case NodeType::BlurFilter:
      return name == "blurX" || name == "blurY";
    case NodeType::DropShadowFilter:
    case NodeType::InnerShadowFilter:
    case NodeType::DropShadowStyle:
    case NodeType::InnerShadowStyle:
      // `color` is not listed: the shadow colour is baked into feColorMatrix constants, so it has
      // no single animatable attribute yet.
      return name == "offsetX" || name == "offsetY" || name == "blurX" || name == "blurY";
    case NodeType::BlendFilter:
      return name == "color";
    default:
      return false;
  }
}

void CollectTimelines(const std::vector<Layer*>& layers,
                      std::vector<const AnimationTimeline*>* out) {
  for (const auto* layer : layers) {
    if (layer == nullptr) {
      continue;
    }
    for (const auto& timeline : layer->timelines) {
      if (timeline != nullptr && timeline->timelineType() == TimelineType::Animation) {
        out->push_back(static_cast<const AnimationTimeline*>(timeline.get()));
      }
    }
    CollectTimelines(layer->children, out);
    if (layer->composition != nullptr) {
      CollectTimelines(layer->composition->layers, out);
    }
  }
}

}  // namespace

std::unique_ptr<SVGAnimationPlan> SVGAnimationPlan::Build(PAGXDocument* document,
                                                          std::vector<std::string>* warnings) {
  auto plan = std::unique_ptr<SVGAnimationPlan>(new SVGAnimationPlan());
  if (document == nullptr) {
    return plan;
  }

  for (const auto* node : document->animations) {
    if (node != nullptr && node->nodeType() == NodeType::StateMachine) {
      AddWarning(warnings,
                 "document declares a StateMachine; SMIL cannot express host-driven states so the "
                 "state machine is dropped.");
    }
  }
  if (document->animations.empty()) {
    return plan;
  }

  std::vector<const AnimationTimeline*> timelines = {};
  CollectTimelines(document->layers, &timelines);

  std::unordered_set<const Animation*> visited = {};
  std::unordered_set<std::string> reportedChannels = {};
  for (const auto* timeline : timelines) {
    const auto* animation = document->findNode<Animation>(timeline->animationId);
    if (animation == nullptr) {
      AddWarning(warnings, "animation timeline references unknown animation '" +
                               timeline->animationId + "'.");
      continue;
    }
    if (!visited.insert(animation).second) {
      AddWarning(warnings, "animation '" + animation->id +
                               "' is driven by more than one timeline; only the first timeline's "
                               "timing (offset / playing) is exported.");
      continue;
    }
    if (animation->duration <= 0 || animation->frameRate <= 0.0f) {
      plan->_skippedAnimations++;
      AddWarning(warnings, "animation '" + animation->id +
                               "' has a non-positive duration or frame rate; no track is emitted.");
      continue;
    }
    const size_t bindingsBefore = plan->_bindingCount;
    for (const auto* object : animation->objects) {
      if (object == nullptr) {
        continue;
      }
      auto* target = document->findNode(object->target);
      if (target == nullptr) {
        AddWarning(warnings, "animation '" + animation->id + "' targets '" + object->target +
                                 "' which is not resolvable in this document; its channels are "
                                 "dropped.");
        continue;
      }
      for (const auto* channel : object->channels) {
        if (channel == nullptr || channel->name.empty()) {
          continue;
        }
        if (!IsSupportedChannel(target->nodeType(), channel->name)) {
          std::string key = std::string(NodeTypeName(target->nodeType())) + ":" + channel->name;
          if (reportedChannels.insert(key).second) {
            auto type = target->nodeType();
            if (type == NodeType::Group || type == NodeType::TextBox) {
              AddWarning(warnings, "channel '" + channel->name + "' on <" + NodeTypeName(type) +
                                       "> is dropped: group opacity is folded into each descendant "
                                       "element, so there is no single element to carry the SMIL "
                                       "animation.");
            } else {
              AddWarning(warnings, "channel '" + channel->name + "' on <" + NodeTypeName(type) +
                                       "> has no SMIL equivalent and is dropped.");
            }
          }
          continue;
        }
        auto& list = plan->_bindings[target];
        bool replaced = false;
        for (auto& existing : list) {
          if (existing.channel->name == channel->name) {
            existing.channel = channel;
            existing.animation = animation;
            existing.timeline = timeline;
            replaced = true;
            break;
          }
        }
        if (!replaced) {
          list.push_back({channel, animation, timeline});
          plan->_bindingCount++;
        }
      }
    }
    if (plan->_bindingCount == bindingsBefore) {
      plan->_skippedAnimations++;
      AddWarning(warnings, "animation '" + animation->id +
                               "' has no channel the SVG exporter can express; nothing is emitted "
                               "for it.");
    }
  }
  return plan;
}

const std::vector<SVGAnimationBinding>* SVGAnimationPlan::BindingsFor(const Node* node) const {
  if (node == nullptr) {
    return nullptr;
  }
  auto iter = _bindings.find(node);
  return iter == _bindings.end() ? nullptr : &iter->second;
}

const SVGAnimationBinding* SVGAnimationPlan::Find(const Node* node, const char* channelName) const {
  auto* list = BindingsFor(node);
  if (list == nullptr) {
    return nullptr;
  }
  const SVGAnimationBinding* result = nullptr;
  for (const auto& binding : *list) {
    if (binding.channel->name == channelName) {
      result = &binding;
    }
  }
  return result;
}

bool SVGAnimationPlan::HasVisibilityAnimation(const Node* node) const {
  return Find(node, "visible") != nullptr;
}

bool SVGAnimationPlan::HasTransformAnimation(const Node* node) const {
  auto* list = BindingsFor(node);
  if (list == nullptr || node == nullptr) {
    return false;
  }
  for (const auto& binding : *list) {
    if (IsTransformChannel(node->nodeType(), binding.channel->name)) {
      return true;
    }
  }
  return false;
}

bool SVGAnimationPlan::HasAlphaAnimation(const Node* node) const {
  return Find(node, "alpha") != nullptr;
}

bool SVGAnimationPlan::HasPainterAnimation(const Node* fill, const Node* stroke) const {
  return (fill != nullptr && BindingsFor(fill) != nullptr) ||
         (stroke != nullptr && BindingsFor(stroke) != nullptr);
}

}  // namespace pagx
