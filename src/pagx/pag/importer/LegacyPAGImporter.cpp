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
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "pagx/LegacyPAGImporter.h"
#include <unordered_map>
#include "pag/file.h"
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Animation.h"
#include "pagx/nodes/AnimationObject.h"
#include "pagx/nodes/Channel.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/Polystar.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/pag/SequenceCodec.h"
#include "pagx/types/ColorSpace.h"

namespace pagx {
// Keyframe migration (P3-5): implemented in KeyframeConverter.cpp.
void MigrateTransform2DAnimation(pag::Transform2D* transform, float frameRate,
                                 PAGXDocument* document, AnimationObject* object);
void MigrateLayerVisibility(pag::Layer* legacyLayer, PAGXDocument* document,
                            AnimationObject* object);

namespace {

// Deterministic target id for a converted layer: legacy layers have no string ids, so the
// importer assigns pag-legacy-N and the RetainedIdTable restores it after the binary hop.
std::string LegacyLayerTargetId(pag::Layer* legacyLayer) {
  return "pag-legacy-" + std::to_string(legacyLayer->uniqueID);
}

std::string LegacyCompositionAnimationId(pag::VectorComposition* legacy) {
  return "pag-legacy-anim-" + std::to_string(legacy->uniqueID);
}

// Conversion state shared across the tree walk.
struct ConvertState {
  PAGXDocument* document = nullptr;
  std::vector<PAGExportIssue>* issues = nullptr;
  // Every legacy composition gets exactly one converted Composition node; layers reference it
  // through this map so shared compositions convert once.
  std::unordered_map<const pag::Composition*, Composition*> compositionNodes = {};
  // Per-composition animation nodes for migrated keyframes (P3-5).
  std::unordered_map<const pag::VectorComposition*, Animation*> compositionAnimations = {};
  int layerIdCounter = 0;
  // Set when any converted channel carries keyframes (P3-5 extends this); until then it stays
  // false and the root animation fallback is unconditional for files with duration.
  bool hasKeyframes = false;

  void warn(PAGExportWarning code, const std::string& message) {
    if (issues == nullptr) {
      return;
    }
    PAGExportIssue issue = {};
    issue.code = code;
    issue.message = message;
    issues->push_back(issue);
  }
};

// Reads the property's value at the layer's content start frame. Animated properties take
// their first-frame value in the skeleton (P3-5 migrates the keyframes themselves); the
// caller reports that through an issue when animatable() is true.
template <typename T>
T PropertyValueAtStart(pag::Property<T>* property, pag::Frame startFrame, bool* animated) {
  if (animated != nullptr) {
    *animated = property != nullptr && property->animatable();
  }
  if (property == nullptr) {
    return {};
  }
  if (property->animatable()) {
    return property->getValueAt(startFrame);
  }
  return property->value;
}

// Copies a legacy PathData (verbs + points) into a freshly created PAGX PathData node.
PathData* ConvertPathData(ConvertState* state, const pag::PathData& legacyData) {
  auto* data = state->document->makeNode<PathData>();
  size_t pointIndex = 0;
  const auto& points = legacyData.points;
  for (auto verb : legacyData.verbs) {
    switch (verb) {
      case pag::PathDataVerb::MoveTo:
        if (pointIndex + 1 < points.size() + 1 && pointIndex < points.size()) {
          data->moveTo(points[pointIndex].x, points[pointIndex].y);
          pointIndex++;
        }
        break;
      case pag::PathDataVerb::LineTo:
        if (pointIndex < points.size()) {
          data->lineTo(points[pointIndex].x, points[pointIndex].y);
          pointIndex++;
        }
        break;
      case pag::PathDataVerb::CurveTo:
        if (pointIndex + 2 < points.size() + 1 && pointIndex + 2 <= points.size()) {
          data->cubicTo(points[pointIndex].x, points[pointIndex].y, points[pointIndex + 1].x,
                        points[pointIndex + 1].y, points[pointIndex + 2].x,
                        points[pointIndex + 2].y);
          pointIndex += 3;
        }
        break;
      case pag::PathDataVerb::Close:
        data->close();
        break;
    }
  }
  return data;
}

// Converts one legacy color (0-255 components) to the PAGX Color (0-1 floats).
Color ConvertColor(const pag::Color& color) {
  return {color.red / 255.0f, color.green / 255.0f, color.blue / 255.0f, 1.0f, ColorSpace::SRGB};
}

// Shape element conversion (skeleton scope): Rectangle / Ellipse / Polystar / Path geometry
// and solid Fill / Stroke painters with static values. Trim paths, merge paths, and gradient
// painters report an issue and are skipped until their phase-3 tasks.
void ConvertShapeElement(ConvertState* state, pag::ShapeElement* element,
                         std::vector<Element*>* contents, pag::Frame startFrame) {
  if (element == nullptr) {
    return;
  }
  switch (element->type()) {
    case pag::ShapeType::Rectangle: {
      auto* legacy = static_cast<pag::RectangleElement*>(element);
      auto* rect = state->document->makeNode<Rectangle>();
      bool animated = false;
      auto size = PropertyValueAtStart(legacy->size, startFrame, &animated);
      if (animated) {
        state->warn(PAGExportWarning::AnimatedContentSkipped, "animated shape size");
      }
      rect->size = {size.x, size.y};
      auto rpos = PropertyValueAtStart(legacy->position, startFrame, nullptr);
      rect->position = {rpos.x, rpos.y};
      rect->roundness = PropertyValueAtStart(legacy->roundness, startFrame, nullptr);
      rect->reversed = legacy->reversed;
      contents->push_back(rect);
      break;
    }
    case pag::ShapeType::Ellipse: {
      auto* legacy = static_cast<pag::EllipseElement*>(element);
      auto* ellipse = state->document->makeNode<Ellipse>();
      auto esize = PropertyValueAtStart(legacy->size, startFrame, nullptr);
      ellipse->size = {esize.x, esize.y};
      auto epos = PropertyValueAtStart(legacy->position, startFrame, nullptr);
      ellipse->position = {epos.x, epos.y};
      ellipse->reversed = legacy->reversed;
      contents->push_back(ellipse);
      break;
    }
    case pag::ShapeType::PolyStar: {
      auto* legacy = static_cast<pag::PolyStarElement*>(element);
      auto* polystar = state->document->makeNode<Polystar>();
      polystar->pointCount =
          static_cast<int>(PropertyValueAtStart(legacy->points, startFrame, nullptr));
      auto ppos = PropertyValueAtStart(legacy->position, startFrame, nullptr);
      polystar->position = {ppos.x, ppos.y};
      polystar->rotation = PropertyValueAtStart(legacy->rotation, startFrame, nullptr);
      polystar->innerRadius = PropertyValueAtStart(legacy->innerRadius, startFrame, nullptr);
      polystar->outerRadius = PropertyValueAtStart(legacy->outerRadius, startFrame, nullptr);
      polystar->innerRoundness =
          PropertyValueAtStart(legacy->innerRoundness, startFrame, nullptr) / 100.0f;
      polystar->outerRoundness =
          PropertyValueAtStart(legacy->outerRoundness, startFrame, nullptr) / 100.0f;
      contents->push_back(polystar);
      break;
    }
    case pag::ShapeType::ShapePath: {
      auto* legacy = static_cast<pag::ShapePathElement*>(element);
      auto handle = PropertyValueAtStart(legacy->shapePath, startFrame, nullptr);
      if (handle != nullptr) {
        auto* path = state->document->makeNode<Path>();
        path->data = ConvertPathData(state, *handle);
        contents->push_back(path);
      }
      break;
    }
    case pag::ShapeType::Fill: {
      auto* legacy = static_cast<pag::FillElement*>(element);
      auto* fill = state->document->makeNode<Fill>();
      auto* solid = state->document->makeNode<SolidColor>();
      auto color = PropertyValueAtStart(legacy->color, startFrame, nullptr);
      solid->color = ConvertColor(color);
      auto opacity = PropertyValueAtStart(legacy->opacity, startFrame, nullptr);
      fill->alpha = opacity / 255.0f;
      fill->color = solid;
      fill->fillRule =
          legacy->fillRule == pag::FillRule::NonZeroWinding ? FillRule::Winding : FillRule::EvenOdd;
      contents->push_back(fill);
      break;
    }
    case pag::ShapeType::Stroke: {
      auto* legacy = static_cast<pag::StrokeElement*>(element);
      auto* stroke = state->document->makeNode<Stroke>();
      auto* solid = state->document->makeNode<SolidColor>();
      solid->color = ConvertColor(PropertyValueAtStart(legacy->color, startFrame, nullptr));
      stroke->color = solid;
      stroke->alpha = PropertyValueAtStart(legacy->opacity, startFrame, nullptr) / 255.0f;
      stroke->width = PropertyValueAtStart(legacy->strokeWidth, startFrame, nullptr);
      contents->push_back(stroke);
      break;
    }
    case pag::ShapeType::ShapeGroup: {
      auto* legacy = static_cast<pag::ShapeGroupElement*>(element);
      auto* group = state->document->makeNode<Group>();
      for (auto* child : legacy->elements) {
        ConvertShapeElement(state, child, &group->elements, startFrame);
      }
      contents->push_back(group);
      break;
    }
    default:
      state->warn(PAGExportWarning::UnsupportedColorSource, "unsupported shape element");
      break;
  }
}

// Collects the animated channels of one legacy layer into a new AnimationObject under the
// given animation. Returns the object, or nullptr when the layer has no animation at all.
AnimationObject* MigrateLayerAnimation(ConvertState* state, pag::Layer* legacyLayer,
                                       Animation* animation, float frameRate) {
  auto* object = state->document->makeNode<AnimationObject>();
  object->target = LegacyLayerTargetId(legacyLayer);
  MigrateTransform2DAnimation(legacyLayer->transform, frameRate, state->document, object);
  MigrateLayerVisibility(legacyLayer, state->document, object);
  if (object->channels.empty()) {
    return nullptr;
  }
  animation->objects.push_back(object);
  return object;
}

// Creates the per-composition animation holding every layer's migrated channels; the
// composition scope auto-plays it (spec 5.4) and time-locked parents position it (6.3).
Animation* EnsureCompositionAnimation(ConvertState* state, pag::VectorComposition* legacy) {
  auto it = state->compositionAnimations.find(legacy);
  if (it != state->compositionAnimations.end()) {
    return it->second;
  }
  auto* animation = state->document->makeNode<Animation>();
  animation->id = LegacyCompositionAnimationId(legacy);
  animation->frameRate = legacy->frameRate;
  animation->duration = legacy->duration;
  animation->loop = LoopMode::Once;
  state->compositionAnimations[legacy] = animation;
  return animation;
}

// Forward declaration for the precompose conversion.
Layer* ConvertLayer(ConvertState* state, pag::Layer* legacyLayer, float parentFrameRate);
Composition* ConvertComposition(ConvertState* state, pag::VectorComposition* legacy,
                                float parentFrameRate);
Composition* ConvertSequenceComposition(ConvertState* state, pag::Composition* legacy);
Image* MakeSequenceImage(ConvertState* state, const std::shared_ptr<Data>& data, float frameRate,
                         uint32_t frameCount, uint32_t width, uint32_t height, uint8_t mediaType);

// Wraps an encoded sequence container into an Image node carrying the scheduling metadata.
Image* MakeSequenceImage(ConvertState* state, const std::shared_ptr<Data>& data, float frameRate,
                         uint32_t frameCount, uint32_t width, uint32_t height, uint8_t mediaType) {
  auto* image = state->document->makeNode<Image>();
  image->isSequence = true;
  image->sequenceMediaType = mediaType;
  image->data = data;
  image->sequenceFrameRate = frameRate;
  image->sequenceFrameCount = frameCount;
  image->logicalWidth = width;
  image->logicalHeight = height;
  return image;
}

// Converts a legacy video/bitmap sequence composition (spec 3.3): every sequence becomes one
// ImageTable sequence entry with its frames stored as-is; the last sequence - the one the
// legacy runtime renders (Sequence::Get picks the back for best quality) - drives a
// full-frame image pattern layer. Runtime frame scheduling lands with the sequence player;
// until then the entry carries the data and the pattern presents the structure.
Composition* ConvertSequenceComposition(ConvertState* state, pag::Composition* legacy) {
  if (legacy == nullptr) {
    return nullptr;
  }
  auto it = state->compositionNodes.find(legacy);
  if (it != state->compositionNodes.end()) {
    return it->second;
  }
  auto* composition = state->document->makeNode<Composition>();
  composition->width = legacy->width;
  composition->height = legacy->height;
  state->compositionNodes[legacy] = composition;
  Image* primaryImage = nullptr;
  if (legacy->type() == pag::CompositionType::Video) {
    auto* video = static_cast<pag::VideoComposition*>(legacy);
    for (auto* sequence : video->sequences) {
      if (sequence == nullptr) {
        continue;
      }
      VideoSequenceContainer container = {};
      container.frameRate = sequence->frameRate;
      container.width = static_cast<uint32_t>(sequence->width);
      container.height = static_cast<uint32_t>(sequence->height);
      container.alphaStartX = sequence->alphaStartX;
      container.alphaStartY = sequence->alphaStartY;
      for (auto* header : sequence->headers) {
        if (header != nullptr) {
          container.headers.push_back(Data::MakeWithCopy(header->data(), header->length()));
        }
      }
      for (auto* frame : sequence->frames) {
        VideoSequenceFrame containerFrame = {};
        containerFrame.isKeyframe = frame->isKeyframe;
        containerFrame.frameIndex = static_cast<uint32_t>(frame->frame);
        if (frame->fileBytes != nullptr) {
          containerFrame.data =
              Data::MakeWithCopy(frame->fileBytes->data(), frame->fileBytes->length());
        }
        container.frames.push_back(containerFrame);
      }
      auto* image = MakeSequenceImage(
          state, SequenceCodec::EncodeVideoSequence(container), container.frameRate,
          static_cast<uint32_t>(container.frames.size()), container.width, container.height, 2);
      if (sequence == video->sequences.back()) {
        primaryImage = image;
      }
    }
  } else if (legacy->type() == pag::CompositionType::Bitmap) {
    auto* bitmap = static_cast<pag::BitmapComposition*>(legacy);
    for (auto* sequence : bitmap->sequences) {
      if (sequence == nullptr) {
        continue;
      }
      BitmapSequenceContainer container = {};
      container.frameRate = sequence->frameRate;
      container.width = static_cast<uint32_t>(sequence->width);
      container.height = static_cast<uint32_t>(sequence->height);
      for (auto* frame : sequence->frames) {
        BitmapSequenceFrame containerFrame = {};
        containerFrame.isKeyframe = frame->isKeyframe;
        for (auto* rect : frame->bitmaps) {
          BitmapSequenceBlock block = {};
          block.x = rect->x;
          block.y = rect->y;
          if (rect->fileBytes != nullptr) {
            block.data = Data::MakeWithCopy(rect->fileBytes->data(), rect->fileBytes->length());
          }
          containerFrame.bitmaps.push_back(block);
        }
        container.frames.push_back(containerFrame);
      }
      auto* image = MakeSequenceImage(
          state, SequenceCodec::EncodeBitmapSequence(container), container.frameRate,
          static_cast<uint32_t>(container.frames.size()), container.width, container.height, 3);
      if (sequence == bitmap->sequences.back()) {
        primaryImage = image;
      }
    }
  }
  if (primaryImage != nullptr) {
    auto* layer = state->document->makeNode<Layer>();
    layer->width = composition->width;
    layer->height = composition->height;
    auto* rect = state->document->makeNode<Rectangle>();
    rect->size.width = composition->width;
    rect->size.height = composition->height;
    layer->contents.push_back(rect);
    auto* fill = state->document->makeNode<Fill>();
    auto* pattern = state->document->makeNode<ImagePattern>();
    pattern->image = primaryImage;
    pattern->scaleMode = ScaleMode::Stretch;
    fill->color = pattern;
    layer->contents.push_back(fill);
    composition->layers.push_back(layer);
  }
  return composition;
}

// Converts a legacy composition into a Composition node, recursively converting its layers.
Composition* ConvertComposition(ConvertState* state, pag::VectorComposition* legacy,
                                float parentFrameRate) {
  if (legacy == nullptr) {
    return nullptr;
  }
  auto it = state->compositionNodes.find(legacy);
  if (it != state->compositionNodes.end()) {
    return it->second;
  }
  auto* composition = state->document->makeNode<Composition>();
  composition->width = legacy->width;
  composition->height = legacy->height;
  state->compositionNodes[legacy] = composition;
  // Composition-scoped animations (spec 5.4) auto-play in the instance; the migrated
  // keyframe animation is attached here (or stays absent for fully static compositions).
  auto animIt = state->compositionAnimations.find(legacy);
  if (animIt != state->compositionAnimations.end()) {
    composition->animations.push_back(animIt->second);
  }
  for (auto* legacyLayer : legacy->layers) {
    auto* child = ConvertLayer(state, legacyLayer, legacy->frameRate);
    if (child != nullptr) {
      composition->layers.push_back(child);
      // P3-5: migrate this layer's animated channels into the composition animation.
      auto* animation = EnsureCompositionAnimation(state, legacy);
      MigrateLayerAnimation(state, legacyLayer, animation, legacy->frameRate);
    }
  }
  // The wrapper layer is stored on the composition through the caller's layer conversion;
  // the composition itself only holds the width/height for now, layers attach via Layer.
  (void)parentFrameRate;
  return composition;
}

// Maps the legacy Transform2D static values onto the PAGX Layer's decomposed fields
// (spec 6.2): anchor/position/scale/rotation map one-to-one, opacity becomes alpha.
void ConvertTransform2D(ConvertState* state, pag::Transform2D* transform, Layer* layer,
                        pag::Frame startFrame) {
  if (transform == nullptr) {
    return;
  }
  bool animated = false;
  auto anchor = PropertyValueAtStart(transform->anchorPoint, startFrame, &animated);
  layer->anchor = {anchor.x, anchor.y};
  if (animated) {
    state->warn(PAGExportWarning::AnimatedContentSkipped, "animated anchor");
  }
  // Position: the separated x/y properties win over the point property when present.
  if (transform->xPosition != nullptr) {
    layer->x = PropertyValueAtStart(transform->xPosition, startFrame, nullptr);
  } else if (transform->position != nullptr) {
    auto position = PropertyValueAtStart(transform->position, startFrame, nullptr);
    layer->x = position.x;
    layer->y = position.y;
  }
  auto scale = PropertyValueAtStart(transform->scale, startFrame, nullptr);
  layer->scale = {scale.x / 100.0f, scale.y / 100.0f};
  layer->rotation = PropertyValueAtStart(transform->rotation, startFrame, nullptr);
  auto opacity = PropertyValueAtStart(transform->opacity, startFrame, nullptr);
  layer->alpha = opacity / 255.0f;
}

Layer* ConvertLayer(ConvertState* state, pag::Layer* legacyLayer, float parentFrameRate) {
  if (legacyLayer == nullptr) {
    return nullptr;
  }
  switch (legacyLayer->type()) {
    case pag::LayerType::PreCompose: {
      auto* legacy = static_cast<pag::PreComposeLayer*>(legacyLayer);
      auto* layer = state->document->makeNode<Layer>(LegacyLayerTargetId(legacyLayer));
      layer->name = legacyLayer->name;
      layer->visible = legacyLayer->isActive;
      ConvertTransform2D(state, legacyLayer->transform, layer, legacyLayer->startTime);
      Composition* composition = nullptr;
      if (legacy->composition != nullptr) {
        if (legacy->composition->type() == pag::CompositionType::Vector) {
          composition = ConvertComposition(
              state, static_cast<pag::VectorComposition*>(legacy->composition), parentFrameRate);
        } else if (legacy->composition->type() == pag::CompositionType::Video ||
                   legacy->composition->type() == pag::CompositionType::Bitmap) {
          // Sequence compositions (spec 3.3): frames migrate into ImageTable entries.
          composition = ConvertSequenceComposition(state, legacy->composition);
        }
      }
      if (composition != nullptr) {
        layer->composition = composition;
        // Time-lock mount (spec 6.3): the legacy mapping is
        // compositionFrame = (parentFrame - compositionStartTime) * (childRate / parentRate),
        // which maps to startTime = compositionStartTime / parentRate and
        // timeScale = childRate / parentRate.
        auto childRate = legacy->composition != nullptr ? legacy->composition->frameRate : 60.0f;
        if (parentFrameRate > 0.0f && childRate > 0.0f) {
          layer->startTime = static_cast<float>(legacy->compositionStartTime) / parentFrameRate;
          layer->timeScale = childRate / parentFrameRate;
        }
        // The legacy layer time stretch (Ratio, 1 = none) multiplies the lock scale.
        auto stretch = legacyLayer->stretch.value();
        if (stretch > 0.0f && stretch != 1.0f) {
          layer->timeScale *= stretch;
        }
      }
      return layer;
    }
    case pag::LayerType::Solid: {
      auto* legacy = static_cast<pag::SolidLayer*>(legacyLayer);
      auto* layer = state->document->makeNode<Layer>(LegacyLayerTargetId(legacyLayer));
      layer->name = legacyLayer->name;
      layer->visible = legacyLayer->isActive;
      ConvertTransform2D(state, legacyLayer->transform, layer, legacyLayer->startTime);
      auto* rect = state->document->makeNode<Rectangle>();
      rect->size = {static_cast<float>(legacy->width), static_cast<float>(legacy->height)};
      rect->position = {static_cast<float>(legacy->width) / 2.0f,
                        static_cast<float>(legacy->height) / 2.0f};
      layer->contents.push_back(rect);
      auto* fill = state->document->makeNode<Fill>();
      auto* solid = state->document->makeNode<SolidColor>();
      solid->color = ConvertColor(legacy->solidColor);
      fill->color = solid;
      layer->contents.push_back(fill);
      return layer;
    }
    case pag::LayerType::Shape: {
      auto* legacy = static_cast<pag::ShapeLayer*>(legacyLayer);
      auto* layer = state->document->makeNode<Layer>(LegacyLayerTargetId(legacyLayer));
      layer->name = legacyLayer->name;
      layer->visible = legacyLayer->isActive;
      ConvertTransform2D(state, legacyLayer->transform, layer, legacyLayer->startTime);
      for (auto* element : legacy->contents) {
        ConvertShapeElement(state, element, &layer->contents, legacyLayer->startTime);
      }
      if (legacyLayer->masks.empty() == false) {
        state->warn(PAGExportWarning::UnsupportedColorSource, "layer masks");
      }
      if (legacyLayer->effects.empty() == false) {
        state->warn(PAGExportWarning::UnsupportedColorSource, "layer effects");
      }
      if (legacyLayer->layerStyles.empty() == false) {
        state->warn(PAGExportWarning::UnsupportedColorSource, "layer styles");
      }
      return layer;
    }
    default:
      state->warn(PAGExportWarning::UnsupportedColorSource,
                  "unsupported legacy layer type: " + legacyLayer->name);
      return nullptr;
  }
}

// Root animation fallback (design doc 7.3): a legacy file always has a duration, so the root
// document gets a same-length same-framerate Animation placed first, becoming the default
// timeline even when no keyframes exist.
void EnsureRootAnimation(ConvertState* state, pag::File* file) {
  auto* animation = state->document->makeNode<Animation>();
  animation->id = "pag-legacy-root";
  animation->frameRate = file->frameRate();
  animation->duration =
      static_cast<Frame>(llround(static_cast<double>(file->duration()) * file->frameRate()));
  animation->loop = LoopMode::Once;
  state->document->animations.push_back(animation);
}

}  // namespace

std::shared_ptr<PAGXDocument> LegacyPAGImporter::Convert(const std::shared_ptr<pag::File>& file,
                                                         std::vector<PAGExportIssue>* issues) {
  if (file == nullptr) {
    return nullptr;
  }
  auto document = PAGXDocument::Make(file->width(), file->height());
  ConvertState state = {};
  state.document = document.get();
  state.issues = issues;

  auto* rootLayer = file->getRootLayer();
  if (rootLayer == nullptr || rootLayer->composition == nullptr) {
    return nullptr;
  }
  Composition* composition = nullptr;
  if (rootLayer->composition->type() == pag::CompositionType::Vector) {
    composition = ConvertComposition(
        &state, static_cast<pag::VectorComposition*>(rootLayer->composition), file->frameRate());
  } else if (rootLayer->composition->type() == pag::CompositionType::Video ||
             rootLayer->composition->type() == pag::CompositionType::Bitmap) {
    composition = ConvertSequenceComposition(&state, rootLayer->composition);
  }
  if (composition == nullptr) {
    return nullptr;
  }
  // The converted tree hangs off a single root layer that references the composition.
  auto* topLayer = document->makeNode<Layer>();
  topLayer->composition = composition;
  document->layers.push_back(topLayer);

  // The root fallback animation always exists (design doc 7.3): legacy files have a single
  // time axis, so the document gets a same-length same-framerate root Animation placed first,
  // becoming the default timeline. Migrated composition-scoped animations auto-play on top of
  // it, and time-locked children read this timeline as the parent clock.
  EnsureRootAnimation(&state, file.get());
  return document;
}

}  // namespace pagx
