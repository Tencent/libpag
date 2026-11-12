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
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Gradient.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/RadialGradient.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
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
    case NodeType::Text:
      // Text.x / Text.y translate the shaped run without reshaping it, so the exporter wraps the
      // emitted text in a <g> that carries the translation track.
      return name == "x" || name == "y";
    case NodeType::TrimPath:
      // Bakeable into a sibling shape's d track by the per-frame re-resolve sampler.
      return name == "start" || name == "end" || name == "offset";
    case NodeType::RoundCorner:
      return name == "radius";
    case NodeType::Polystar:
      // Fixed-topology geometry channels are sampled per frame and re-baked into a d track; the
      // content bounds (and therefore render scale) are recomputed from the sampled fields.
      // pointCount / reversed change the vertex list and stay unsupported.
      return name == "rotation" || name == "outerRadius" || name == "innerRadius" ||
             name == "outerRoundness" || name == "innerRoundness" || name == "position.x" ||
             name == "position.y";
    case NodeType::Rectangle:
      return name == "position.x" || name == "position.y" || name == "size.width" ||
             name == "size.height" || name == "roundness";
    case NodeType::Ellipse:
      return name == "position.x" || name == "position.y" || name == "size.width" ||
             name == "size.height";
    case NodeType::Fill:
      return name == "color" || name == "alpha";
    case NodeType::SolidColor:
      return name == "color";
    case NodeType::ColorStop:
      return name == "offset" || name == "color";
    case NodeType::LinearGradient:
      return name == "startPoint.x" || name == "startPoint.y" || name == "endPoint.x" ||
             name == "endPoint.y";
    case NodeType::RadialGradient:
      return name == "center.x" || name == "center.y" || name == "radius";
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
    case NodeType::NoiseFilter:
      // feTurbulence exposes `seed` as an animatable attribute.
      return name == "seed";
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

// A ColorStop's SMIL host is the <stop> element inside the gradient def SVG writes for a
// consumer. Conic and diamond gradients have no SVG primitive: they degrade to a radial
// approximation, so their stops are not a faithful native host. This map records which kinds of
// gradient actually reference each stop so the channel loop can classify the instance.
struct StopOwnership {
  bool nativeGradient = false;
  bool degradedGradient = false;
};

void CollectStopOwners(PAGXDocument* document,
                       std::unordered_map<const Node*, StopOwnership>* out) {
  for (const auto& node : document->nodes) {
    if (node == nullptr) {
      continue;
    }
    auto type = node->nodeType();
    bool native = type == NodeType::LinearGradient || type == NodeType::RadialGradient;
    if (!native && type != NodeType::ConicGradient && type != NodeType::DiamondGradient) {
      continue;
    }
    const auto* gradient = static_cast<const Gradient*>(node.get());
    for (const auto* stop : gradient->colorStops) {
      if (stop == nullptr) {
        continue;
      }
      auto& owner = (*out)[stop];
      if (native) {
        owner.nativeGradient = true;
      } else {
        owner.degradedGradient = true;
      }
    }
  }
}

// Parses a `point{N}.x` / `point{N}.y` channel name. The family is table-external: it addresses
// the PathData point array directly instead of a reflective node field, so it never appears in
// ListChannels and must not change the 175-entry ledger.
bool ParsePathPointChannel(const std::string& channel, size_t* index) {
  if (channel.rfind("point", 0) != 0) {
    return false;
  }
  auto dot = channel.rfind('.');
  if (dot == std::string::npos || dot + 2 != channel.size() || dot <= 5) {
    return false;
  }
  char axis = channel[dot + 1];
  if (axis != 'x' && axis != 'y') {
    return false;
  }
  size_t value = 0;
  for (size_t i = 5; i < dot; i++) {
    char c = channel[i];
    if (c < '0' || c > '9') {
      return false;
    }
    value = value * 10 + static_cast<size_t>(c - '0');
  }
  *index = value;
  return true;
}

// SVG collapses a zero-length linear gradient to its first stop and a zero-radius radial gradient
// to its last stop. That static collapse cannot follow a moving endpoint, so a gradient whose
// authored geometry is already degenerate stays a static collapsed paint instead of gaining a
// track the browser would evaluate with different semantics.
bool IsDegenerateGradient(const Node* target) {
  if (target->nodeType() == NodeType::LinearGradient) {
    const auto* gradient = static_cast<const LinearGradient*>(target);
    if (gradient->colorStops.size() <= 1) {
      return false;
    }
    float dx = gradient->endPoint.x - gradient->startPoint.x;
    float dy = gradient->endPoint.y - gradient->startPoint.y;
    return dx * dx + dy * dy < 1e-8f;
  }
  if (target->nodeType() == NodeType::RadialGradient) {
    const auto* gradient = static_cast<const RadialGradient*>(target);
    return gradient->colorStops.size() > 1 && gradient->radius < 1e-4f &&
           gradient->radius >= 0.0f;
  }
  return false;
}

}  // namespace

std::unique_ptr<SVGAnimationPlan> SVGAnimationPlan::Build(PAGXDocument* document,
                                                          std::vector<std::string>* warnings,
                                                          const SVGAnimationOptions& options) {
  auto plan = std::unique_ptr<SVGAnimationPlan>(new SVGAnimationPlan());
  if (document == nullptr) {
    return plan;
  }
  bool strict = options.policy == SVGAnimationPolicy::Strict;

  for (const auto* node : document->animations) {
    if (node != nullptr && node->nodeType() == NodeType::StateMachine) {
      AddWarning(warnings,
                 "document declares a StateMachine; SMIL cannot express host-driven states so the "
                 "state machine is dropped.");
      plan->_rejected = plan->_rejected || strict;
    }
  }
  if (document->animations.empty()) {
    return plan;
  }

  std::vector<const AnimationTimeline*> timelines = {};
  CollectTimelines(document->layers, &timelines);
  std::unordered_map<std::string, const AnimationTimeline*> timelineById = {};
  for (const auto* timeline : timelines) {
    if (timeline == nullptr) {
      continue;
    }
    if (!timelineById.emplace(timeline->animationId, timeline).second) {
      AddWarning(warnings, "animation '" + timeline->animationId +
                               "' has multiple timelines; only the first timeline's timing is "
                               "exported.");
    }
  }

  std::vector<const Animation*> declared = {};
  for (const auto* node : document->animations) {
    if (node != nullptr && node->nodeType() == NodeType::Animation) {
      declared.push_back(static_cast<const Animation*>(node));
    }
  }
  std::vector<const Animation*> selected = {};
  switch (options.selection) {
    case SVGAnimationSelection::Named: {
      const Animation* match = nullptr;
      for (const auto* animation : declared) {
        if (animation->id == options.animationId) {
          match = animation;
          break;
        }
      }
      if (match == nullptr) {
        AddWarning(warnings, "animation '" + options.animationId +
                                 "' is not declared in this document; nothing is emitted for it.");
        plan->_rejected = plan->_rejected || strict;
      } else {
        selected.push_back(match);
      }
      break;
    }
    case SVGAnimationSelection::All:
      selected = declared;
      break;
    case SVGAnimationSelection::Default:
    default:
      if (!declared.empty()) {
        selected.push_back(declared.front());
      }
      break;
  }
  if (selected.empty() && options.selection == SVGAnimationSelection::Default) {
    // Compatibility fallback for documents that only mount timelines on layers.
    for (const auto* timeline : timelines) {
      const auto* animation = document->findNode<Animation>(timeline->animationId);
      if (animation != nullptr) {
        selected.push_back(animation);
      }
    }
  }

  std::unordered_set<const Animation*> visited = {};
  std::unordered_set<std::string> reportedChannels = {};
  std::unordered_map<const Node*, StopOwnership> stopOwners = {};
  CollectStopOwners(document, &stopOwners);
  for (const auto* animation : selected) {
    if (animation == nullptr || !visited.insert(animation).second) {
      continue;
    }
    const auto* timeline =
        timelineById.count(animation->id) > 0 ? timelineById.find(animation->id)->second : nullptr;
    if (animation->duration <= 0 || animation->frameRate <= 0.0f) {
      plan->_skippedAnimations++;
      plan->_rejected = plan->_rejected || strict;
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
        plan->_rejected = plan->_rejected || strict;
        continue;
      }
      for (const auto* channel : object->channels) {
        if (channel == nullptr || channel->name.empty()) {
          continue;
        }
        if (target->nodeType() == NodeType::ColorStop) {
          auto owner = stopOwners.find(target);
          bool supported = owner != stopOwners.end() && owner->second.nativeGradient;
          if (!supported) {
            plan->_rejected = plan->_rejected || strict;
            std::string key = std::string("ColorStop:") + channel->name;
            if (reportedChannels.insert(key).second) {
              if (owner == stopOwners.end()) {
                AddWarning(warnings, "channel '" + channel->name + "' on <ColorStop> '" +
                                         target->id +
                                         "' is dropped: the stop is not referenced by any "
                                         "gradient, so no <stop> host exists.");
              } else {
                AddWarning(warnings, "channel '" + channel->name + "' on <ColorStop> '" +
                                         target->id +
                                         "' is dropped: conic and diamond gradients degrade to a "
                                         "radial approximation, which is not a native stop host.");
              }
            }
            continue;
          }
          if (owner->second.degradedGradient) {
            plan->_rejected = plan->_rejected || strict;
            std::string key = std::string("ColorStop:degraded:") + channel->name;
            if (reportedChannels.insert(key).second) {
              AddWarning(warnings, "channel '" + channel->name + "' on <ColorStop> '" + target->id +
                                       "' is exported for its linear/radial gradients only; the "
                                       "conic/diamond consumers keep static stops.");
            }
          }
        }
        if ((target->nodeType() == NodeType::LinearGradient ||
             target->nodeType() == NodeType::RadialGradient) &&
            IsDegenerateGradient(target)) {
          plan->_rejected = plan->_rejected || strict;
          std::string key = std::string(NodeTypeName(target->nodeType())) + ":degenerate:" +
                            channel->name;
          if (reportedChannels.insert(key).second) {
            AddWarning(warnings, "channel '" + channel->name + "' on <" +
                                     NodeTypeName(target->nodeType()) + "> '" + target->id +
                                     "' is dropped: the gradient is degenerate before the "
                                     "animation starts, so SVG cannot keep the static collapse "
                                     "semantics while its geometry moves.");
          }
          continue;
        }
        bool pathPointChannel = false;
        if (target->nodeType() == NodeType::Path) {
          size_t pointIndex = 0;
          if (ParsePathPointChannel(channel->name, &pointIndex)) {
            pathPointChannel = true;
            const auto* path = static_cast<const Path*>(target);
            if (channel->valueType() != ChannelValueType::Float || path->data == nullptr ||
                pointIndex >= path->data->countPoints()) {
              plan->_rejected = plan->_rejected || strict;
              std::string key = "Path:point:" + channel->name;
              if (reportedChannels.insert(key).second) {
                AddWarning(warnings, "channel '" + channel->name + "' on <Path> '" + target->id +
                                         "' is dropped: the point index is outside the authored "
                                         "path or the channel is not a float track.");
              }
              continue;
            }
          }
        }
        if (!pathPointChannel && !IsSupportedChannel(target->nodeType(), channel->name)) {
          plan->_rejected = plan->_rejected || strict;
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
        bool multiClock = false;
        for (auto& existing : list) {
          if (existing.channel->name == channel->name) {
            multiClock = existing.animation != animation;
            existing.channel = channel;
            existing.animation = animation;
            existing.timeline = timeline;
            replaced = true;
            break;
          }
        }
        if (multiClock) {
          // Two animations driving the same attribute cannot both be expressed by one SMIL
          // track. Keep the runtime's last-writer-wins order in BestEffort and reject in Strict.
          plan->_rejected = plan->_rejected || strict;
          std::string key = "multi-clock:" + target->id + ":" + channel->name;
          if (reportedChannels.insert(key).second) {
            AddWarning(warnings, "channel '" + channel->name + "' on <" +
                                     NodeTypeName(target->nodeType()) + "> '" + target->id +
                                     "' is driven by multiple animations; the later animation "
                                     "wins and the earlier track is dropped.");
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
      plan->_rejected = plan->_rejected || strict;
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
  if (fill != nullptr) {
    if (BindingsFor(fill) != nullptr) {
      return true;
    }
    if (fill->nodeType() == NodeType::Fill) {
      auto* source = static_cast<const Fill*>(fill)->color;
      if (source != nullptr && source->nodeType() == NodeType::SolidColor &&
          BindingsFor(source) != nullptr) {
        return true;
      }
    }
  }
  if (stroke != nullptr) {
    if (BindingsFor(stroke) != nullptr) {
      return true;
    }
    if (stroke->nodeType() == NodeType::Stroke) {
      auto* source = static_cast<const Stroke*>(stroke)->color;
      if (source != nullptr && source->nodeType() == NodeType::SolidColor &&
          BindingsFor(source) != nullptr) {
        return true;
      }
    }
  }
  return false;
}

bool SVGAnimationPlan::SupportsChannel(NodeType nodeType, const std::string& channelName) {
  if (nodeType == NodeType::Path) {
    size_t pointIndex = 0;
    if (ParsePathPointChannel(channelName, &pointIndex)) {
      return true;
    }
  }
  return IsSupportedChannel(nodeType, channelName);
}

}  // namespace pagx
