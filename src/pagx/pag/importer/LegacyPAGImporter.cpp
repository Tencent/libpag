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
#include "pagx/nodes/ColorStop.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/DataBind.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Gradient.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/MergePath.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/Polystar.h"
#include "pagx/nodes/RadialGradient.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/Repeater.h"
#include "pagx/nodes/RoundCorner.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/nodes/TrimPath.h"
#include "pagx/nodes/ViewModel.h"
#include "pagx/nodes/ViewModelProperty.h"
#include "pagx/pag/SequenceCodec.h"
#include "pagx/types/ColorSpace.h"

namespace pagx {
// Keyframe migration (P3-5): implemented in KeyframeConverter.cpp.
void MigrateTransform2DAnimation(pag::Transform2D* transform, float frameRate,
                                 PAGXDocument* document, AnimationObject* object);
void MigrateLayerVisibility(pag::Layer* legacyLayer, PAGXDocument* document,
                            AnimationObject* object);
void MigrateGroupTransformAnimation(pag::ShapeTransform* transform, float frameRate,
                                    PAGXDocument* document, AnimationObject* object);

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
  // Editable mapping (spec 7.2): converted text/image nodes keyed by their legacy layers, so
  // the editable index lists resolve onto the migrated nodes after the tree walk. Each entry
  // records the composition the layer lives in — DataBinds must attach to that composition so
  // the runtime scope check finds the target in the same binding.
  struct ConvertedTextEntry {
    pag::TextLayer* legacy = nullptr;
    Text* node = nullptr;
    Composition* composition = nullptr;
  };
  struct ConvertedImageEntry {
    pag::ImageLayer* legacy = nullptr;
    ImagePattern* node = nullptr;
    Composition* composition = nullptr;
  };
  // Shape-group conversion state: every converted Group gets a targetable id, and the current
  // composition's animation (plus its frame rate) receives the group's animated transform
  // channels.
  int groupIdCounter = 0;
  Animation* currentAnimation = nullptr;
  float currentFrameRate = 30.0f;
  std::vector<ConvertedTextEntry> convertedTexts = {};
  std::vector<ConvertedImageEntry> convertedImages = {};
  Composition* currentComposition = nullptr;
  int textIdCounter = 0;
  int imageIdCounter = 0;

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

// Converts a legacy layer blend mode to the PAGX equivalent. The first sixteen values share
// the legacy order; the legacy Add mode maps to PlusLighter (linear dodge, matching
// TGFXCast.cpp).
BlendMode ConvertBlendMode(pag::BlendMode mode) {
  switch (mode) {
    case pag::BlendMode::Multiply:
      return BlendMode::Multiply;
    case pag::BlendMode::Screen:
      return BlendMode::Screen;
    case pag::BlendMode::Overlay:
      return BlendMode::Overlay;
    case pag::BlendMode::Darken:
      return BlendMode::Darken;
    case pag::BlendMode::Lighten:
      return BlendMode::Lighten;
    case pag::BlendMode::ColorDodge:
      return BlendMode::ColorDodge;
    case pag::BlendMode::ColorBurn:
      return BlendMode::ColorBurn;
    case pag::BlendMode::HardLight:
      return BlendMode::HardLight;
    case pag::BlendMode::SoftLight:
      return BlendMode::SoftLight;
    case pag::BlendMode::Difference:
      return BlendMode::Difference;
    case pag::BlendMode::Exclusion:
      return BlendMode::Exclusion;
    case pag::BlendMode::Hue:
      return BlendMode::Hue;
    case pag::BlendMode::Saturation:
      return BlendMode::Saturation;
    case pag::BlendMode::Color:
      return BlendMode::Color;
    case pag::BlendMode::Luminosity:
      return BlendMode::Luminosity;
    case pag::BlendMode::Add:
      return BlendMode::PlusLighter;
    case pag::BlendMode::Normal:
      break;
  }
  return BlendMode::Normal;
}

// Shape element conversion (skeleton scope): Rectangle / Ellipse / Polystar / Path geometry
// and solid Fill / Stroke painters with static values. Trim paths, merge paths, and gradient
// painters report an issue and are skipped until their phase-3 tasks.
// Builds a PAGX gradient color source (Linear or Radial) from a legacy gradient element's
// start/end points and color stops. Alpha stops fold into the color stop alpha values so a
// single color-stop list carries both. Legacy gradients use absolute layer-space coordinates,
// so fitsToGeometry stays false.
ColorSource* ConvertGradientColorSource(ConvertState* state, pag::GradientFillType fillType,
                                        const pag::Point& startPoint, const pag::Point& endPoint,
                                        const std::shared_ptr<pag::GradientColor>& colors) {
  Gradient* gradient = nullptr;
  if (fillType == pag::GradientFillType::Linear) {
    auto* linear = state->document->makeNode<LinearGradient>();
    linear->startPoint = {startPoint.x, startPoint.y};
    linear->endPoint = {endPoint.x, endPoint.y};
    gradient = linear;
  } else {
    auto* radial = state->document->makeNode<RadialGradient>();
    radial->center = {startPoint.x, startPoint.y};
    radial->radius = sqrtf((endPoint.x - startPoint.x) * (endPoint.x - startPoint.x) +
                           (endPoint.y - startPoint.y) * (endPoint.y - startPoint.y));
    gradient = radial;
  }
  gradient->fitsToGeometry = false;
  if (colors == nullptr) {
    return gradient;
  }
  for (const auto& stop : colors->colorStops) {
    auto* colorStop = state->document->makeNode<ColorStop>();
    colorStop->offset = stop.position;
    colorStop->color = ConvertColor(stop.color);
    // The legacy color carries no alpha; the gradient alpha comes from the alpha stops only.
    // The alpha stops form an opacity ramp; the color stop's alpha is the ramp's interpolated
    // value at the stop's position. Multiplying the qualifying stops instead would zero out
    // every stop after a transparent one (e.g. ramp 0 -> 127 made the far stop alpha 0).
    float alpha = 1.0f;
    if (!colors->alphaStops.empty()) {
      const auto& first = colors->alphaStops.front();
      const auto& last = colors->alphaStops.back();
      if (stop.position <= first.position) {
        alpha = first.opacity / 255.0f;
      } else if (stop.position >= last.position) {
        alpha = last.opacity / 255.0f;
      } else {
        for (size_t i = 0; i + 1 < colors->alphaStops.size(); i++) {
          const auto& a = colors->alphaStops[i];
          const auto& b = colors->alphaStops[i + 1];
          if (stop.position >= a.position && stop.position <= b.position) {
            auto span = b.position - a.position;
            auto t = span > 0.0f ? (stop.position - a.position) / span : 0.0f;
            alpha = (a.opacity + (b.opacity - a.opacity) * t) / 255.0f;
            break;
          }
        }
      }
    }
    colorStop->color.alpha = alpha;
    gradient->colorStops.push_back(colorStop);
  }
  return gradient;
}

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
      auto* group = state->document->makeNode<Group>("pag-legacy-group-" +
                                                     std::to_string(state->groupIdCounter++));
      // The group's own ShapeTransform places/scales its contents; dropping it would render
      // the children at identity. The PAGX Group position is layout-driven (the content bounds
      // land at the position), which does not reproduce the legacy content-space translation,
      // so a static translation-only transform bakes directly into the child geometry; other
      // transforms map onto the group's decomposed fields with the position passed through.
      bool bakeTranslate = false;
      float bakeX = 0.0f;
      float bakeY = 0.0f;
      if (legacy->transform != nullptr) {
        auto* t = legacy->transform;
        auto anchor = PropertyValueAtStart(t->anchorPoint, startFrame, nullptr);
        auto position = PropertyValueAtStart(t->position, startFrame, nullptr);
        auto scale = PropertyValueAtStart(t->scale, startFrame, nullptr);
        group->rotation = PropertyValueAtStart(t->rotation, startFrame, nullptr);
        group->skew = PropertyValueAtStart(t->skew, startFrame, nullptr);
        group->skewAxis = PropertyValueAtStart(t->skewAxis, startFrame, nullptr);
        auto opacity = PropertyValueAtStart(t->opacity, startFrame, nullptr);
        group->alpha = opacity / 255.0f;
        auto identityScale = scale.x == 1.0f && scale.y == 1.0f;
        auto identityRest =
            group->rotation == 0.0f && group->skew == 0.0f && anchor.x == 0.0f && anchor.y == 0.0f;
        auto transformAnimated = (t->position != nullptr && t->position->animatable()) ||
                                 (t->scale != nullptr && t->scale->animatable()) ||
                                 (t->rotation != nullptr && t->rotation->animatable()) ||
                                 (t->anchorPoint != nullptr && t->anchorPoint->animatable());
        if (identityScale && identityRest && !transformAnimated) {
          bakeTranslate = true;
          bakeX = position.x;
          bakeY = position.y;
        } else {
          group->anchor = {anchor.x, anchor.y};
          group->position = {position.x, position.y};
          group->scale = {scale.x, scale.y};
          if (state->currentAnimation != nullptr) {
            auto* object = state->document->makeNode<AnimationObject>();
            object->target = group->id;
            MigrateGroupTransformAnimation(t, state->currentFrameRate, state->document, object);
            if (!object->channels.empty()) {
              state->currentAnimation->objects.push_back(object);
            }
          }
        }
      }
      for (auto* child : legacy->elements) {
        ConvertShapeElement(state, child, &group->elements, startFrame);
      }
      if (bakeTranslate && (bakeX != 0.0f || bakeY != 0.0f)) {
        for (auto* element : group->elements) {
          if (element == nullptr) {
            continue;
          }
          switch (element->nodeType()) {
            case NodeType::Rectangle:
              static_cast<Rectangle*>(element)->position.x += bakeX;
              static_cast<Rectangle*>(element)->position.y += bakeY;
              break;
            case NodeType::Ellipse:
              static_cast<Ellipse*>(element)->position.x += bakeX;
              static_cast<Ellipse*>(element)->position.y += bakeY;
              break;
            case NodeType::Polystar:
              static_cast<Polystar*>(element)->position.x += bakeX;
              static_cast<Polystar*>(element)->position.y += bakeY;
              break;
            case NodeType::Path:
              static_cast<Path*>(element)->position.x += bakeX;
              static_cast<Path*>(element)->position.y += bakeY;
              break;
            case NodeType::Group:
              static_cast<Group*>(element)->position.x += bakeX;
              static_cast<Group*>(element)->position.y += bakeY;
              break;
            case NodeType::Fill:
            case NodeType::Stroke: {
              // A fitsToGeometry=false gradient lives in the same space as the geometry, so
              // the baked translation must shift its anchor points too, or the gradient
              // stays behind while the geometry moves and degenerates to a flat color.
              auto* colorSource = element->nodeType() == NodeType::Fill
                                      ? static_cast<Fill*>(element)->color
                                      : static_cast<Stroke*>(element)->color;
              if (colorSource == nullptr || colorSource->nodeType() == NodeType::SolidColor ||
                  colorSource->nodeType() == NodeType::ImagePattern) {
                break;
              }
              if (colorSource->nodeType() == NodeType::LinearGradient) {
                auto* linear = static_cast<LinearGradient*>(colorSource);
                linear->startPoint.x += bakeX;
                linear->startPoint.y += bakeY;
                linear->endPoint.x += bakeX;
                linear->endPoint.y += bakeY;
              } else if (colorSource->nodeType() == NodeType::RadialGradient) {
                auto* radial = static_cast<RadialGradient*>(colorSource);
                radial->center.x += bakeX;
                radial->center.y += bakeY;
              }
              break;
            }
            default:
              break;
          }
        }
      }
      contents->push_back(group);
      break;
    }
    case pag::ShapeType::GradientFill: {
      auto* legacy = static_cast<pag::GradientFillElement*>(element);
      auto* fill = state->document->makeNode<Fill>();
      fill->color = ConvertGradientColorSource(
          state, legacy->fillType, PropertyValueAtStart(legacy->startPoint, startFrame, nullptr),
          PropertyValueAtStart(legacy->endPoint, startFrame, nullptr),
          PropertyValueAtStart(legacy->colors, startFrame, nullptr));
      fill->alpha = PropertyValueAtStart(legacy->opacity, startFrame, nullptr) / 255.0f;
      fill->fillRule =
          legacy->fillRule == pag::FillRule::NonZeroWinding ? FillRule::Winding : FillRule::EvenOdd;
      contents->push_back(fill);
      break;
    }
    case pag::ShapeType::GradientStroke: {
      auto* legacy = static_cast<pag::GradientStrokeElement*>(element);
      auto* stroke = state->document->makeNode<Stroke>();
      stroke->color = ConvertGradientColorSource(
          state, legacy->fillType, PropertyValueAtStart(legacy->startPoint, startFrame, nullptr),
          PropertyValueAtStart(legacy->endPoint, startFrame, nullptr),
          PropertyValueAtStart(legacy->colors, startFrame, nullptr));
      stroke->alpha = PropertyValueAtStart(legacy->opacity, startFrame, nullptr) / 255.0f;
      stroke->width = PropertyValueAtStart(legacy->strokeWidth, startFrame, nullptr);
      stroke->cap = static_cast<LineCap>(legacy->lineCap);
      stroke->join = static_cast<LineJoin>(legacy->lineJoin);
      stroke->miterLimit = PropertyValueAtStart(legacy->miterLimit, startFrame, nullptr);
      if (legacy->dashOffset != nullptr) {
        stroke->dashOffset = PropertyValueAtStart(legacy->dashOffset, startFrame, nullptr);
      }
      for (auto* dash : legacy->dashes) {
        stroke->dashes.push_back(PropertyValueAtStart(dash, startFrame, nullptr));
      }
      contents->push_back(stroke);
      break;
    }
    case pag::ShapeType::MergePaths: {
      auto* legacy = static_cast<pag::MergePathsElement*>(element);
      auto* merge = state->document->makeNode<MergePath>();
      merge->mode = static_cast<MergePathMode>(legacy->mode);
      contents->push_back(merge);
      break;
    }
    case pag::ShapeType::TrimPaths: {
      auto* legacy = static_cast<pag::TrimPathsElement*>(element);
      auto* trim = state->document->makeNode<TrimPath>();
      trim->start = PropertyValueAtStart(legacy->start, startFrame, nullptr) / 100.0f;
      trim->end = PropertyValueAtStart(legacy->end, startFrame, nullptr) / 100.0f;
      trim->offset = PropertyValueAtStart(legacy->offset, startFrame, nullptr);
      trim->type = legacy->trimType == pag::TrimPathsType::Individually ? TrimType::Separate
                                                                        : TrimType::Continuous;
      contents->push_back(trim);
      break;
    }
    case pag::ShapeType::Repeater: {
      auto* legacy = static_cast<pag::RepeaterElement*>(element);
      auto* repeater = state->document->makeNode<Repeater>();
      repeater->copies = PropertyValueAtStart(legacy->copies, startFrame, nullptr);
      repeater->offset = PropertyValueAtStart(legacy->offset, startFrame, nullptr);
      repeater->order = legacy->composite == pag::RepeaterOrder::Above
                            ? RepeaterOrder::AboveOriginal
                            : RepeaterOrder::BelowOriginal;
      if (legacy->transform != nullptr) {
        auto anchorPoint =
            PropertyValueAtStart(legacy->transform->anchorPoint, startFrame, nullptr);
        repeater->anchor = {anchorPoint.x, anchorPoint.y};
        auto position = PropertyValueAtStart(legacy->transform->position, startFrame, nullptr);
        repeater->position = {position.x, position.y};
        auto scale = PropertyValueAtStart(legacy->transform->scale, startFrame, nullptr);
        repeater->scale = {scale.x / 100.0f, scale.y / 100.0f};
        repeater->rotation = PropertyValueAtStart(legacy->transform->rotation, startFrame, nullptr);
        repeater->startAlpha =
            PropertyValueAtStart(legacy->transform->startOpacity, startFrame, nullptr) / 255.0f;
        repeater->endAlpha =
            PropertyValueAtStart(legacy->transform->endOpacity, startFrame, nullptr) / 255.0f;
      }
      contents->push_back(repeater);
      break;
    }
    case pag::ShapeType::RoundCorners: {
      auto* legacy = static_cast<pag::RoundCornersElement*>(element);
      auto* round = state->document->makeNode<RoundCorner>();
      round->radius = PropertyValueAtStart(legacy->radius, startFrame, nullptr);
      contents->push_back(round);
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
  auto* composition =
      state->document->makeNode<Composition>("pag-legacy-comp-" + std::to_string(legacy->uniqueID));
  composition->width = legacy->width;
  composition->height = legacy->height;
  state->compositionNodes[legacy] = composition;
  // Audio (spec 3.4): same carry as vector compositions.
  if (legacy->audioBytes != nullptr) {
    composition->audioData =
        Data::MakeWithCopy(legacy->audioBytes->data(), legacy->audioBytes->length());
    composition->audioStartTime =
        legacy->frameRate > 0 ? static_cast<float>(legacy->audioStartTime) / legacy->frameRate
                              : 0.0f;
  }
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
    // Center the rectangle on the composition so it covers [0, width] x [0, height]; the
    // default position (0, 0) would leave the geometry centered on the origin and only the
    // bottom-right quadrant of the composition would be covered.
    rect->position.x = composition->width / 2.0f;
    rect->position.y = composition->height / 2.0f;
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
  auto* composition =
      state->document->makeNode<Composition>("pag-legacy-comp-" + std::to_string(legacy->uniqueID));
  composition->width = legacy->width;
  composition->height = legacy->height;
  state->compositionNodes[legacy] = composition;
  // The composition being filled is the editable-attachment scope for its layers (spec 7.2).
  auto* previousComposition = state->currentComposition;
  state->currentComposition = composition;
  // Audio (spec 3.4): the AAC stream is carried as-is; the start time converts from legacy
  // frames to seconds on the composition's frame rate.
  if (legacy->audioBytes != nullptr) {
    composition->audioData =
        Data::MakeWithCopy(legacy->audioBytes->data(), legacy->audioBytes->length());
    composition->audioStartTime =
        legacy->frameRate > 0 ? static_cast<float>(legacy->audioStartTime) / legacy->frameRate
                              : 0.0f;
  }
  // Legacy files follow the AE stacking convention (the first layer in the list is the
  // topmost), while PAGX paints layers in document order (the first layer is the bottom,
  // matching CSS). Push the converted layers in reverse so the visual stacking is preserved.
  // topmost), while PAGX paints layers in document order (the first layer is the bottom,
  // matching CSS). Push the converted layers in reverse so the visual stacking is preserved.
  auto* previousAnimation = state->currentAnimation;
  auto previousFrameRate = state->currentFrameRate;
  state->currentFrameRate = legacy->frameRate;
  for (auto it = legacy->layers.rbegin(); it != legacy->layers.rend(); ++it) {
    auto* legacyLayer = *it;
    auto* child = ConvertLayer(state, legacyLayer, legacy->frameRate);
    if (child != nullptr) {
      composition->layers.push_back(child);
      // P3-5: migrate this layer's animated channels into the composition animation. Shape
      // groups converted inside the layer append their channels to the same animation.
      auto* animation = EnsureCompositionAnimation(state, legacy);
      state->currentAnimation = animation;
      MigrateLayerAnimation(state, legacyLayer, animation, legacy->frameRate);
    }
  }
  state->currentAnimation = previousAnimation;
  state->currentFrameRate = previousFrameRate;
  // Legacy track mattes (spec 6.4): the matted layer consumes its trackMatteLayer as an
  // alpha/luminance mask. The matte source converts as a regular layer - keeping it in the
  // composition's layer list preserves the legacy sibling coordinate space, and the renderer
  // suppresses its own drawing once the mask is attached (maskOwner). The reference resolves by
  // the deterministic legacy target id.
  for (auto* legacyLayer : legacy->layers) {
    if (legacyLayer->trackMatteType == pag::TrackMatteType::None ||
        legacyLayer->trackMatteLayer == nullptr) {
      continue;
    }
    auto* matted = state->document->findNode<Layer>(LegacyLayerTargetId(legacyLayer));
    auto* source =
        state->document->findNode<Layer>(LegacyLayerTargetId(legacyLayer->trackMatteLayer));
    if (matted == nullptr || source == nullptr) {
      state->warn(PAGExportWarning::UnsupportedColorSource, "unresolved track matte");
      continue;
    }
    matted->mask = source;
    switch (legacyLayer->trackMatteType) {
      case pag::TrackMatteType::Alpha:
        matted->maskType = MaskType::Alpha;
        break;
      case pag::TrackMatteType::AlphaInverted:
        matted->maskType = MaskType::AlphaInverted;
        break;
      case pag::TrackMatteType::Luma:
        matted->maskType = MaskType::Luminance;
        break;
      case pag::TrackMatteType::LumaInverted:
        matted->maskType = MaskType::LuminanceInverted;
        break;
      case pag::TrackMatteType::None:
        break;
    }
  }
  // Composition-scoped animations (spec 5.4) auto-play in the instance. The attachment runs
  // AFTER the layer loop: EnsureCompositionAnimation creates the animation lazily while the
  // layers convert, so looking it up before the loop would always miss it and the runtime
  // would never spawn the composition timeline.
  auto animIt = state->compositionAnimations.find(legacy);
  if (animIt != state->compositionAnimations.end()) {
    composition->animations.push_back(animIt->second);
  }
  // The wrapper layer is stored on the composition through the caller's layer conversion;
  // the composition itself only holds the width/height for now, layers attach via Layer.
  (void)parentFrameRate;
  state->currentComposition = previousComposition;
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
  layer->scale = {scale.x, scale.y};
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
      layer->blendMode = ConvertBlendMode(legacyLayer->blendMode);
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
      layer->blendMode = ConvertBlendMode(legacyLayer->blendMode);
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
      layer->blendMode = ConvertBlendMode(legacyLayer->blendMode);
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
    case pag::LayerType::Text: {
      // Text layers migrate as a TextBox with a runtime-shaped Text element (spec 7.2): the
      // document's text string and font properties carry over; glyph-exact migration lands with
      // the legacy glyph exporter. The text channel of the Text node is the editable target.
      auto* legacyText = static_cast<pag::TextLayer*>(legacyLayer);
      auto* layer = state->document->makeNode<Layer>(LegacyLayerTargetId(legacyLayer));
      layer->name = legacyLayer->name;
      layer->visible = legacyLayer->isActive;
      layer->blendMode = ConvertBlendMode(legacyLayer->blendMode);
      ConvertTransform2D(state, legacyLayer->transform, layer, legacyLayer->startTime);
      auto textDocument = legacyText->getTextDocument();
      if (textDocument != nullptr) {
        auto* textBox = state->document->makeNode<TextBox>();
        textBox->position.x = textDocument->boxTextPos.x;
        textBox->position.y = textDocument->boxTextPos.y;
        if (textDocument->boxText) {
          textBox->width = textDocument->boxTextSize.x;
          textBox->height = textDocument->boxTextSize.y;
        }
        auto* text = state->document->makeNode<Text>("pag-editable-text-" +
                                                     std::to_string(state->textIdCounter++));
        text->text = textDocument->text;
        text->fontFamily = textDocument->fontFamily;
        text->fontStyle = textDocument->fontStyle;
        text->fontSize = textDocument->fontSize;
        text->fauxBold = textDocument->fauxBold;
        text->fauxItalic = textDocument->fauxItalic;
        textBox->elements.push_back(text);
        if (textDocument->applyFill) {
          auto* fill = state->document->makeNode<Fill>();
          auto* solid = state->document->makeNode<SolidColor>();
          solid->color = ConvertColor(textDocument->fillColor);
          fill->color = solid;
          textBox->elements.push_back(fill);
        }
        layer->contents.push_back(textBox);
        state->convertedTexts.push_back({legacyText, text, state->currentComposition});
      }
      if (legacyLayer->masks.empty() == false) {
        state->warn(PAGExportWarning::UnsupportedColorSource, "layer masks");
      }
      return layer;
    }
    case pag::LayerType::Image: {
      auto* legacyImage = static_cast<pag::ImageLayer*>(legacyLayer);
      auto* layer = state->document->makeNode<Layer>(LegacyLayerTargetId(legacyLayer));
      layer->name = legacyLayer->name;
      layer->visible = legacyLayer->isActive;
      layer->blendMode = ConvertBlendMode(legacyLayer->blendMode);
      // The layer mirrors the image bounds so the layout system derives its size from the
      // content instead of a default box (a default-sized layer clips its pattern fill).
      auto imageBounds = legacyImage->getBounds();
      layer->width = imageBounds.right - imageBounds.left;
      layer->height = imageBounds.bottom - imageBounds.top;
      ConvertTransform2D(state, legacyLayer->transform, layer, legacyLayer->startTime);
      if (legacyImage->imageBytes != nullptr && legacyImage->imageBytes->fileBytes != nullptr) {
        auto* imageBytes = legacyImage->imageBytes;
        auto bounds = legacyImage->getBounds();
        auto* rect = state->document->makeNode<Rectangle>();
        rect->size.width = bounds.right - bounds.left;
        rect->size.height = bounds.bottom - bounds.top;
        rect->position.x = (bounds.right + bounds.left) / 2;
        rect->position.y = (bounds.bottom + bounds.top) / 2;
        layer->contents.push_back(rect);
        auto* fill = state->document->makeNode<Fill>();
        auto* pattern = state->document->makeNode<ImagePattern>(
            "pag-editable-image-" + std::to_string(state->imageIdCounter++));
        auto* image = state->document->makeNode<Image>();
        image->data =
            Data::MakeWithCopy(imageBytes->fileBytes->data(), imageBytes->fileBytes->length());
        pattern->image = image;
        // Legacy image bytes store the original image cropped to the anchor and scaled by
        // scaleFactor; the legacy renderer maps stored pixels with
        // translate(-anchor) * scale(1 / scaleFactor) (ImageContentCache.cpp). Mirror that
        // mapping on the pattern matrix and disable per-geometry fitting so the bitmap lands
        // exactly where the legacy renderer put it; the default Decal tile mode keeps the
        // stripped transparent border empty. Files without the metadata (width == 0, V1)
        // keep stretching the bitmap over the bounds.
        if (imageBytes->width > 0 && imageBytes->height > 0) {
          auto scaleFactor = imageBytes->scaleFactor > 0.0f ? imageBytes->scaleFactor : 1.0f;
          if (imageBytes->anchorX != 0 || imageBytes->anchorY != 0 || scaleFactor != 1.0f) {
            pattern->scaleMode = ScaleMode::None;
            pattern->matrix.a = 1.0f / scaleFactor;
            pattern->matrix.d = 1.0f / scaleFactor;
            pattern->matrix.tx = -static_cast<float>(imageBytes->anchorX) / scaleFactor;
            pattern->matrix.ty = -static_cast<float>(imageBytes->anchorY) / scaleFactor;
          } else {
            pattern->scaleMode = ScaleMode::Stretch;
          }
        } else {
          pattern->scaleMode = ScaleMode::Stretch;
        }
        fill->color = pattern;
        layer->contents.push_back(fill);
        state->convertedImages.push_back({legacyImage, pattern, state->currentComposition});
      }
      return layer;
    }
    default:
      state->warn(PAGExportWarning::UnsupportedColorSource,
                  "unsupported legacy layer type: " + legacyLayer->name);
      return nullptr;
  }
}

// Collects the file's text layers in File::updateEditables order (main composition first,
// recursing into pre-composes, de-duplicated), so the editableTexts indices resolve onto the
// same layers the legacy accessor used.
void CollectLegacyTextLayers(pag::Composition* composition,
                             std::vector<pag::TextLayer*>* outLayers) {
  if (composition == nullptr || composition->type() != pag::CompositionType::Vector) {
    return;
  }
  for (auto* layer : static_cast<pag::VectorComposition*>(composition)->layers) {
    if (layer->type() == pag::LayerType::Text) {
      auto* textLayer = static_cast<pag::TextLayer*>(layer);
      if (std::find(outLayers->begin(), outLayers->end(), textLayer) == outLayers->end()) {
        outLayers->push_back(textLayer);
      }
    } else if (layer->type() == pag::LayerType::PreCompose) {
      CollectLegacyTextLayers(static_cast<pag::PreComposeLayer*>(layer)->composition, outLayers);
    }
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

void BuildEditableViewModel(ConvertState* state, pag::File* file, Composition* composition);

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
  // The slot layer mirrors the composition size so the layout system derives its bounds from
  // the content instead of a default 4x4 box (a clipped slot would cut the content).
  topLayer->width = composition->width;
  topLayer->height = composition->height;
  document->layers.push_back(topLayer);

  // The root fallback animation always exists (design doc 7.3): legacy files have a single
  // time axis, so the document gets a same-length same-framerate root Animation placed first,
  // becoming the default timeline. Migrated composition-scoped animations auto-play on top of
  // it, and time-locked children read this timeline as the parent clock.
  EnsureRootAnimation(&state, file.get());

  // Editable mapping (spec 7.2): the legacy editable index lists resolve onto the converted
  // nodes; each entry becomes a ViewModel property (text<i> / image<i>) bound to the migrated
  // node's channel. A null list means every layer of that type is editable.
  BuildEditableViewModel(&state, file.get(), composition);
  return document;
}

// Builds the editable ViewModel (spec 7.2). The legacy editableImages indices address groups
// of image layers sharing one resource, so every converted pattern of a group binds to the
// same image<i> property. The schema attaches at the document level (every nested data context
// reaches $vm.* through its parent chain), while each DataBind attaches to the composition
// owning its target so the runtime scope check resolves inside the same binding.
void BuildEditableViewModel(ConvertState* state, pag::File* file, Composition* composition) {
  if (composition == nullptr) {
    return;
  }
  auto* viewModel = state->document->makeNode<ViewModel>("pag-editable-vm");
  std::unordered_map<Composition*, std::vector<DataBind*>> bindsByComposition = {};

  // Text: the editable set is the index list, or every converted text when the list is null.
  std::vector<ConvertState::ConvertedTextEntry> editableTexts = {};
  if (file->editableTexts != nullptr) {
    std::vector<pag::TextLayer*> legacyTextLayers = {};
    CollectLegacyTextLayers(file->getRootLayer()->composition, &legacyTextLayers);
    for (auto index : *file->editableTexts) {
      if (index < 0 || index >= static_cast<int>(legacyTextLayers.size())) {
        continue;
      }
      for (auto& entry : state->convertedTexts) {
        if (entry.legacy == legacyTextLayers[index]) {
          editableTexts.push_back(entry);
          break;
        }
      }
    }
  } else {
    editableTexts = state->convertedTexts;
  }
  for (size_t i = 0; i < editableTexts.size(); i++) {
    auto* property = state->document->makeNode<ViewModelProperty>();
    property->name = "text" + std::to_string(i);
    property->propertyType = ViewModelPropertyType::String;
    property->defaultString = editableTexts[i].node->text;
    viewModel->properties.push_back(property);
    auto* bind = state->document->makeNode<DataBind>();
    bind->source = "$vm.text" + std::to_string(i);
    bind->target = "@" + editableTexts[i].node->id;
    bind->channel = "text";
    bindsByComposition[editableTexts[i].composition].push_back(bind);
  }

  // Images: editableImages indices address resource groups; every converted pattern sharing
  // the group's resource binds to the same property.
  std::vector<std::vector<ConvertState::ConvertedImageEntry>> editableImages = {};
  if (file->editableImages != nullptr) {
    for (auto index : *file->editableImages) {
      if (index < 0 || index >= file->numImages()) {
        continue;
      }
      std::vector<ConvertState::ConvertedImageEntry> entries = {};
      for (auto* legacyImage : file->getImageAt(index)) {
        for (auto& entry : state->convertedImages) {
          if (entry.legacy == legacyImage) {
            entries.push_back(entry);
            break;
          }
        }
      }
      if (!entries.empty()) {
        editableImages.push_back(std::move(entries));
      }
    }
  } else {
    for (auto& entry : state->convertedImages) {
      editableImages.push_back({entry});
    }
  }
  for (size_t i = 0; i < editableImages.size(); i++) {
    auto* property = state->document->makeNode<ViewModelProperty>();
    property->name = "image" + std::to_string(i);
    property->propertyType = ViewModelPropertyType::Image;
    property->defaultImage = editableImages[i][0].node->image;
    viewModel->properties.push_back(property);
    for (auto& entry : editableImages[i]) {
      auto* bind = state->document->makeNode<DataBind>();
      bind->source = "$vm.image" + std::to_string(i);
      bind->target = "@" + entry.node->id;
      bind->channel = "image";
      bindsByComposition[entry.composition].push_back(bind);
    }
  }

  if (viewModel->properties.empty()) {
    return;
  }
  state->document->viewModel = viewModel;
  for (auto& pair : bindsByComposition) {
    if (pair.first == nullptr) {
      continue;
    }
    for (auto* bind : pair.second) {
      pair.first->dataBinds.push_back(bind);
    }
  }
}

}  // namespace pagx
