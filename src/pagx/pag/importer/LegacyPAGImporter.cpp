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
#include "base/utils/MathUtil.h"
#include "pag/file.h"
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Animation.h"
#include "pagx/nodes/AnimationObject.h"
#include "pagx/nodes/BlurFilter.h"
#include "pagx/nodes/BrightnessContrastFilter.h"
#include "pagx/nodes/BulgeFilter.h"
#include "pagx/nodes/Channel.h"
#include "pagx/nodes/ColorStop.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/CornerPinFilter.h"
#include "pagx/nodes/DataBind.h"
#include "pagx/nodes/DisplacementMapFilter.h"
#include "pagx/nodes/DropShadowStyle.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/GlowFilter.h"
#include "pagx/nodes/Gradient.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/HueSaturationFilter.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/LevelsIndividualFilter.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/MaskPath.h"
#include "pagx/nodes/MergePath.h"
#include "pagx/nodes/MosaicFilter.h"
#include "pagx/nodes/MotionTileFilter.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/Polystar.h"
#include "pagx/nodes/RadialBlurFilter.h"
#include "pagx/nodes/RadialGradient.h"
#include "pagx/nodes/RangeSelector.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/Repeater.h"
#include "pagx/nodes/RoundCorner.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/StrokeLayerStyle.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/nodes/TextModifier.h"
#include "pagx/nodes/TextPath.h"
#include "pagx/nodes/TrimPath.h"
#include "pagx/nodes/ViewModel.h"
#include "pagx/nodes/ViewModelProperty.h"
#include "pagx/pag/SequenceCodec.h"
#include "pagx/types/ColorSpace.h"
#include "tgfx/core/PathMeasure.h"

namespace pagx {
// Keyframe migration (P3-5): implemented in KeyframeConverter.cpp.
void MigrateTextModifierAnimation(pag::TextAnimator* animator, float frameRate,
                                  PAGXDocument* document, AnimationObject* object);
void MigrateRangeSelectorAnimation(pag::TextRangeSelector* legacy, float frameRate,
                                   PAGXDocument* document, AnimationObject* object);
void MigrateTransform2DAnimation(pag::Transform2D* transform, float frameRate,
                                 PAGXDocument* document, AnimationObject* object);
void MigrateLayerVisibility(pag::Layer* legacyLayer, PAGXDocument* document,
                            AnimationObject* object);
void MigrateGroupTransformAnimation(pag::ShapeTransform* transform, float frameRate,
                                    PAGXDocument* document, AnimationObject* object);
void MigrateShapeElementAnimation(pag::ShapeElement* element, float frameRate,
                                  PAGXDocument* document, AnimationObject* object,
                                  AnimationObject* colorObject);
void MigrateGlowEffectAnimation(pag::GlowEffect* effect, float frameRate, PAGXDocument* document,
                                AnimationObject* object);
void MigrateMosaicEffectAnimation(pag::MosaicEffect* effect, float frameRate,
                                  PAGXDocument* document, AnimationObject* object);
void MigrateRadialBlurEffectAnimation(pag::RadialBlurEffect* effect, float frameRate,
                                      PAGXDocument* document, AnimationObject* object);
void MigrateCornerPinEffectAnimation(pag::CornerPinEffect* effect, float frameRate,
                                     PAGXDocument* document, AnimationObject* object);
void MigrateBulgeEffectAnimation(pag::BulgeEffect* effect, float frameRate, PAGXDocument* document,
                                 AnimationObject* object);
void MigrateMotionTileEffectAnimation(pag::MotionTileEffect* effect, float frameRate,
                                      PAGXDocument* document, AnimationObject* object);
void MigrateLevelsIndividualEffectAnimation(pag::LevelsIndividualEffect* effect, float frameRate,
                                            PAGXDocument* document, AnimationObject* object);
void MigrateBrightnessContrastEffectAnimation(pag::BrightnessContrastEffect* effect,
                                              float frameRate, PAGXDocument* document,
                                              AnimationObject* object);
void MigrateDisplacementMapEffectAnimation(pag::DisplacementMapEffect* effect, float frameRate,
                                           PAGXDocument* document, AnimationObject* object);
void MigrateHueSaturationEffectAnimation(pag::HueSaturationEffect* effect, float frameRate,
                                         PAGXDocument* document, AnimationObject* object);
void MigrateGradientOverlayStyleAnimation(pag::GradientOverlayStyle* style, float frameRate,
                                          PAGXDocument* document, AnimationObject* object);
void MigrateDropShadowStyleAnimation(pag::DropShadowStyle* style, float frameRate,
                                     PAGXDocument* document, AnimationObject* object);
void MigrateOuterGlowStyleAnimation(pag::OuterGlowStyle* style, float frameRate,
                                    PAGXDocument* document, AnimationObject* object);
void MigrateStrokeStyleAnimation(pag::StrokeStyle* style, float frameRate, PAGXDocument* document,
                                 AnimationObject* object);
PathData* ConvertLegacyPathData(PAGXDocument* document, const pag::PathData& legacyData,
                                const std::string& id = "");
void MigratePathDataChannel(pag::Property<pag::PathHandle>* legacy, const std::string& channelName,
                            const std::string& idPrefix, PAGXDocument* document,
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
  int elementIdCounter = 0;
  Animation* currentAnimation = nullptr;
  float currentFrameRate = 30.0f;
  std::vector<ConvertedTextEntry> convertedTexts = {};
  std::vector<ConvertedImageEntry> convertedImages = {};
  Composition* currentComposition = nullptr;
  int textIdCounter = 0;
  int imageIdCounter = 0;
  // Shape elements with animated paths get targetable ids (the Path node carries the "data"
  // channel, spec 6.1) drawn from this deterministic counter.
  int shapePathIdCounter = 0;
  // Displacement map references waiting for the map layer's node to convert (resolved in the
  // composition post-pass, like the track mattes).
  std::vector<std::pair<DisplacementMapFilter*, pag::Layer*>> pendingDisplacementMaps = {};

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
  return ConvertLegacyPathData(state->document, legacyData);
}

// Converts one legacy color (0-255 components) to the PAGX Color (0-1 floats).
Color ConvertColor(const pag::Color& color) {
  return {color.red / 255.0f, color.green / 255.0f, color.blue / 255.0f, 1.0f, ColorSpace::SRGB};
}

// Converts a legacy layer blend mode to the PAGX equivalent. The first sixteen values share
// the legacy order; the legacy Add mode maps to PlusLighter (linear dodge, matching
// TGFXCast.cpp).
// Maps the legacy paragraph justification onto the box's inline alignment. A NaN-width point
// box reproduces the legacy centered/right drawX offsets; the FullJustify last-line variants
// beyond Left have no PAGX equivalent and approximate as Justify (whose last line is Start).
SelectorUnit ConvertSelectorUnit(pag::TextRangeSelectorUnits units) {
  return units == pag::TextRangeSelectorUnits::Index ? SelectorUnit::Index
                                                     : SelectorUnit::Percentage;
}

SelectorShape ConvertSelectorShape(pag::TextRangeSelectorShape shape) {
  switch (shape) {
    case pag::TextRangeSelectorShape::RampUp:
      return SelectorShape::RampUp;
    case pag::TextRangeSelectorShape::RampDown:
      return SelectorShape::RampDown;
    case pag::TextRangeSelectorShape::Triangle:
      return SelectorShape::Triangle;
    case pag::TextRangeSelectorShape::Round:
      return SelectorShape::Round;
    case pag::TextRangeSelectorShape::Smooth:
      return SelectorShape::Smooth;
    case pag::TextRangeSelectorShape::Square:
    default:
      return SelectorShape::Square;
  }
}

SelectorMode ConvertSelectorMode(pag::TextSelectorMode mode) {
  switch (mode) {
    case pag::TextSelectorMode::Subtract:
      return SelectorMode::Subtract;
    case pag::TextSelectorMode::Intersect:
      return SelectorMode::Intersect;
    case pag::TextSelectorMode::Min:
      return SelectorMode::Min;
    case pag::TextSelectorMode::Max:
      return SelectorMode::Max;
    case pag::TextSelectorMode::Difference:
      return SelectorMode::Difference;
    case pag::TextSelectorMode::Add:
    case pag::TextSelectorMode::None:
    default:
      return SelectorMode::Add;
  }
}

// Legacy text animators (AE per-character animation) migrate as TextModifier elements (spec
// 6.5.3): one modifier per animator, its RangeSelectors as child selector nodes. The modifier
// sits after the Text element and before the painters, so tgfx applies it to the accumulated
// glyphs before the fill/stroke painters capture them. Unsupported capabilities (tracking,
// wiggly selectors, non-character basedOn) warn and drop; the legacy renderer itself ignores
// smoothness, so it is not migrated either.
static void MigrateTextAnimators(ConvertState* state, pag::TextLayer* legacyText,
                                 TextBox* textBox) {
  auto startFrame = legacyText->startTime;
  auto frameRate = state->currentFrameRate;
  auto baseId = LegacyLayerTargetId(legacyText) + "-animator-";
  auto textDocument = legacyText->getTextDocument();
  // The tracking widens each line; the justification keeps the line's fixed edge where the
  // legacy CalculateOffsetByJustification puts it (right = -length, center = -length/2, left = 0).
  float justifyFactor = 0.0f;
  if (textDocument != nullptr) {
    switch (textDocument->justification) {
      case pag::ParagraphJustification::RightJustify:
        justifyFactor = -1.0f;
        break;
      case pag::ParagraphJustification::LeftJustify:
        justifyFactor = 0.0f;
        break;
      default:
        justifyFactor = -0.5f;
        break;
    }
  }
  for (size_t animatorIndex = 0; animatorIndex < legacyText->animators.size(); animatorIndex++) {
    auto* animator = legacyText->animators[animatorIndex];
    auto* modifier =
        state->document->makeNode<TextModifier>(baseId + std::to_string(animatorIndex));
    auto* typo = animator->typographyProperties;
    if (typo != nullptr) {
      if (typo->trackingAmount != nullptr) {
        // The legacy tracking type splits the amount around the glyph: BeforeAndAfter halves it
        // onto both sides, Before / After put all of it on one side.
        auto trackingAmount = PropertyValueAtStart(typo->trackingAmount, startFrame, nullptr);
        auto trackingType = pag::TextAnimatorTrackingType::BeforeAndAfter;
        if (typo->trackingType != nullptr) {
          if (typo->trackingType->animatable()) {
            state->warn(PAGExportWarning::UnsupportedColorSource,
                        "animated animator trackingType approximated as static");
          }
          trackingType = PropertyValueAtStart(typo->trackingType, startFrame, nullptr);
        }
        switch (trackingType) {
          case pag::TextAnimatorTrackingType::Before:
            modifier->trackingBefore = trackingAmount;
            modifier->trackingAfter = 0.0f;
            break;
          case pag::TextAnimatorTrackingType::After:
            modifier->trackingBefore = 0.0f;
            modifier->trackingAfter = trackingAmount;
            break;
          default:
            modifier->trackingBefore = trackingAmount * 0.5f;
            modifier->trackingAfter = trackingAmount * 0.5f;
            break;
        }
        modifier->trackingJustify = justifyFactor;
      }
      if (typo->position != nullptr) {
        auto position = PropertyValueAtStart(typo->position, startFrame, nullptr);
        modifier->position = {position.x, position.y};
      }
      if (typo->scale != nullptr) {
        auto scale = PropertyValueAtStart(typo->scale, startFrame, nullptr);
        modifier->scale = {scale.x, scale.y};
      }
      if (typo->rotation != nullptr) {
        modifier->rotation = PropertyValueAtStart(typo->rotation, startFrame, nullptr);
      }
      if (typo->opacity != nullptr) {
        modifier->alpha = PropertyValueAtStart(typo->opacity, startFrame, nullptr) / 255.0f;
      }
    }
    auto* colorProps = animator->colorProperties;
    if (colorProps != nullptr) {
      if (colorProps->fillColor != nullptr) {
        modifier->fillColor =
            ConvertColor(PropertyValueAtStart(colorProps->fillColor, startFrame, nullptr));
      }
      if (colorProps->strokeColor != nullptr) {
        modifier->strokeColor =
            ConvertColor(PropertyValueAtStart(colorProps->strokeColor, startFrame, nullptr));
      }
    }
    for (size_t selectorIndex = 0; selectorIndex < animator->selectors.size(); selectorIndex++) {
      auto* selector = animator->selectors[selectorIndex];
      if (selector->type() != pag::TextSelectorType::Range) {
        state->warn(PAGExportWarning::UnsupportedColorSource, "wiggly text selector not migrated");
        continue;
      }
      auto* legacySelector = static_cast<pag::TextRangeSelector*>(selector);
      if (legacySelector->basedOn != pag::TextSelectorBasedOn::Characters) {
        state->warn(PAGExportWarning::UnsupportedColorSource, "text selector basedOn not migrated");
      }
      auto* range = state->document->makeNode<RangeSelector>(
          baseId + std::to_string(animatorIndex) + "-selector-" + std::to_string(selectorIndex));
      range->start = PropertyValueAtStart(legacySelector->start, startFrame, nullptr);
      range->end = PropertyValueAtStart(legacySelector->end, startFrame, nullptr);
      range->offset = PropertyValueAtStart(legacySelector->offset, startFrame, nullptr);
      range->unit = ConvertSelectorUnit(legacySelector->units);
      range->shape = ConvertSelectorShape(legacySelector->shape);
      range->mode =
          ConvertSelectorMode(PropertyValueAtStart(legacySelector->mode, startFrame, nullptr));
      range->weight = PropertyValueAtStart(legacySelector->amount, startFrame, nullptr);
      range->easeIn = PropertyValueAtStart(legacySelector->easeLow, startFrame, nullptr);
      range->easeOut = PropertyValueAtStart(legacySelector->easeHigh, startFrame, nullptr);
      range->randomOrder = legacySelector->randomizeOrder;
      range->randomSeed =
          static_cast<int>(PropertyValueAtStart(legacySelector->randomSeed, startFrame, nullptr));
      modifier->selectors.push_back(range);
      if (state->currentAnimation != nullptr) {
        auto* object = state->document->makeNode<AnimationObject>();
        object->target = range->id;
        MigrateRangeSelectorAnimation(legacySelector, frameRate, state->document, object);
        if (!object->channels.empty()) {
          state->currentAnimation->objects.push_back(object);
        }
      }
    }
    textBox->elements.push_back(modifier);
    if (state->currentAnimation != nullptr) {
      auto* object = state->document->makeNode<AnimationObject>();
      object->target = modifier->id;
      MigrateTextModifierAnimation(animator, frameRate, state->document, object);
      if (!object->channels.empty()) {
        state->currentAnimation->objects.push_back(object);
      }
    }
  }
}

// Legacy path text (a TextLayer with pathOption) migrates as a TextPath element after the Text
// (and any TextModifier): tgfx places the accumulated glyphs along the curve, matching the legacy
// TextPathRender. The legacy justification positions the whole run relative to the path
// (left = start, right = end, center = middle); tgfx has no justification, so the offset folds
// into firstMargin (the legacy formulas are all position + constant, so the fold is exact):
//   left:               firstMargin
//   right:              pathLength - layoutWidth + lastMargin
//   center (and full):  (pathLength - layoutWidth) / 2 + firstMargin + lastMargin
static void MigrateTextPathOptions(ConvertState* state, pag::TextLayer* legacyText,
                                   TextBox* textBox) {
  auto* options = legacyText->pathOption;
  if (options == nullptr || options->path == nullptr || options->path->maskPath == nullptr) {
    return;
  }
  auto startFrame = legacyText->startTime;
  auto* textPath =
      state->document->makeNode<TextPath>(LegacyLayerTargetId(legacyText) + "-textpath");
  auto handle = PropertyValueAtStart(options->path->maskPath, startFrame, nullptr);
  if (handle != nullptr) {
    textPath->path = ConvertPathData(state, *handle);
  }
  auto reversed = PropertyValueAtStart(options->reversedPath, startFrame, nullptr);
  auto firstMargin = PropertyValueAtStart(options->firstMargin, startFrame, nullptr);
  auto lastMargin = PropertyValueAtStart(options->lastMargin, startFrame, nullptr);
  textPath->reversed = reversed;
  textPath->perpendicular = PropertyValueAtStart(options->perpendicularToPath, startFrame, nullptr);
  auto forceAlignment = PropertyValueAtStart(options->forceAlignment, startFrame, nullptr);
  textPath->forceAlignment = forceAlignment;
  // Fold the justification into firstMargin (only the non-forceAlignment mode uses it; the
  // forceAlignment branch of the legacy MapToPathPosition ignores justification).
  if (!forceAlignment) {
    auto textDocument = legacyText->getTextDocument();
    float layoutWidth = 0.0f;
    if (textDocument != nullptr && textDocument->boxText) {
      layoutWidth = textDocument->boxTextSize.x;
    }
    auto justification = textDocument != nullptr ? textDocument->justification
                                                 : pag::ParagraphJustification::LeftJustify;
    float pathLength = 0.0f;
    if (handle != nullptr) {
      tgfx::Path path = {};
      size_t pointIndex = 0;
      const auto& points = handle->points;
      for (auto verb : handle->verbs) {
        switch (verb) {
          case pag::PathDataVerb::MoveTo:
            if (pointIndex < points.size()) {
              path.moveTo(points[pointIndex].x, points[pointIndex].y);
              pointIndex++;
            }
            break;
          case pag::PathDataVerb::LineTo:
            if (pointIndex < points.size()) {
              path.lineTo(points[pointIndex].x, points[pointIndex].y);
              pointIndex++;
            }
            break;
          case pag::PathDataVerb::CurveTo:
            if (pointIndex + 2 < points.size() + 1 && pointIndex + 2 <= points.size()) {
              path.cubicTo(points[pointIndex].x, points[pointIndex].y, points[pointIndex + 1].x,
                           points[pointIndex + 1].y, points[pointIndex + 2].x,
                           points[pointIndex + 2].y);
              pointIndex += 3;
            }
            break;
          case pag::PathDataVerb::Close:
            path.close();
            break;
        }
      }
      auto measure = tgfx::PathMeasure::MakeFrom(path);
      if (measure != nullptr) {
        pathLength = measure->getLength();
      }
    }
    switch (justification) {
      case pag::ParagraphJustification::RightJustify:
        textPath->firstMargin = pathLength - layoutWidth + lastMargin;
        break;
      case pag::ParagraphJustification::CenterJustify:
      case pag::ParagraphJustification::FullJustifyLastLineLeft:
      case pag::ParagraphJustification::FullJustifyLastLineRight:
      case pag::ParagraphJustification::FullJustifyLastLineCenter:
      case pag::ParagraphJustification::FullJustifyLastLineFull:
        textPath->firstMargin = (pathLength - layoutWidth) / 2 + firstMargin + lastMargin;
        break;
      case pag::ParagraphJustification::LeftJustify:
      default:
        textPath->firstMargin = firstMargin;
        break;
    }
  } else {
    textPath->firstMargin = firstMargin;
    textPath->lastMargin = lastMargin;
  }
  // Animated path options have no PAGX channels yet (margins are layout-only, booleans are
  // static); warn instead of silently dropping.
  if ((options->firstMargin != nullptr && options->firstMargin->animatable()) ||
      (options->lastMargin != nullptr && options->lastMargin->animatable()) ||
      (options->reversedPath != nullptr && options->reversedPath->animatable()) ||
      (options->perpendicularToPath != nullptr && options->perpendicularToPath->animatable()) ||
      (options->forceAlignment != nullptr && options->forceAlignment->animatable())) {
    state->warn(PAGExportWarning::UnsupportedColorSource,
                "animated text path options not migrated");
  }
  textBox->elements.push_back(textPath);
}

TextAlign ConvertJustification(pag::ParagraphJustification justification) {
  switch (justification) {
    case pag::ParagraphJustification::CenterJustify:
      return TextAlign::Center;
    case pag::ParagraphJustification::RightJustify:
      return TextAlign::End;
    case pag::ParagraphJustification::FullJustifyLastLineLeft:
    case pag::ParagraphJustification::FullJustifyLastLineCenter:
    case pag::ParagraphJustification::FullJustifyLastLineRight:
    case pag::ParagraphJustification::FullJustifyLastLineFull:
      return TextAlign::Justify;
    case pag::ParagraphJustification::LeftJustify:
    default:
      return TextAlign::Start;
  }
}

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
                                        const std::shared_ptr<pag::GradientColor>& colors,
                                        const std::string& id = "") {
  Gradient* gradient = nullptr;
  if (fillType == pag::GradientFillType::Linear) {
    auto* linear = state->document->makeNode<LinearGradient>(id);
    linear->startPoint = {startPoint.x, startPoint.y};
    linear->endPoint = {endPoint.x, endPoint.y};
    gradient = linear;
  } else {
    auto* radial = state->document->makeNode<RadialGradient>(id);
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

// Returns a targetable id for a shape element whose properties vary over time (so its animated
// channels can attach to the node), or an empty id for a static element.
static std::string AnimatedElementId(ConvertState* state, pag::ShapeElement* element) {
  if (state->currentAnimation == nullptr) {
    return {};
  }
  std::vector<pag::TimeRange> ranges = {{0, std::numeric_limits<pag::Frame>::max()}};
  element->excludeVaryingRanges(&ranges);
  if (ranges.size() == 1 && ranges[0].start == 0 &&
      ranges[0].end == std::numeric_limits<pag::Frame>::max()) {
    return {};
  }
  return "pag-legacy-elem-" + std::to_string(state->elementIdCounter++);
}

// Migrates the animated properties of a shape element onto the node with the given id; paint
// colors and gradient geometry animate the color source node (colorId).
static void AttachShapeElementAnimation(ConvertState* state, pag::ShapeElement* element,
                                        const std::string& id, const std::string& colorId) {
  if (id.empty() || state->currentAnimation == nullptr) {
    return;
  }
  auto* object = state->document->makeNode<AnimationObject>();
  object->target = id;
  AnimationObject* colorObject = nullptr;
  if (!colorId.empty()) {
    colorObject = state->document->makeNode<AnimationObject>();
    colorObject->target = colorId;
  }
  MigrateShapeElementAnimation(element, state->currentFrameRate, state->document, object,
                               colorObject);
  if (!object->channels.empty()) {
    state->currentAnimation->objects.push_back(object);
  }
  if (colorObject != nullptr && !colorObject->channels.empty()) {
    state->currentAnimation->objects.push_back(colorObject);
  }
}

// Adds a translation to the "<baseName>.x" / "<baseName>.y" channels animating the given node, so
// a translation baked into static geometry also applies to animated values.
static void ShiftAnimatedPoint(ConvertState* state, const std::string& targetId,
                               const std::string& baseName, float dx, float dy) {
  if (targetId.empty() || state->currentAnimation == nullptr) {
    return;
  }
  for (auto* object : state->currentAnimation->objects) {
    if (object == nullptr || object->target != targetId) {
      continue;
    }
    for (auto* channel : object->channels) {
      if (channel == nullptr || channel->valueType() != ChannelValueType::Float) {
        continue;
      }
      float delta = 0.0f;
      if (channel->name == baseName + ".x") {
        delta = dx;
      } else if (channel->name == baseName + ".y") {
        delta = dy;
      } else {
        continue;
      }
      for (auto& keyframe : static_cast<TypedChannel<float>*>(channel)->keyframes) {
        keyframe.value += delta;
      }
    }
  }
}

static bool IsLegacyPaint(pag::ShapeType type) {
  return type == pag::ShapeType::Fill || type == pag::ShapeType::Stroke ||
         type == pag::ShapeType::GradientFill || type == pag::ShapeType::GradientStroke;
}

static pag::CompositeOrder LegacyPaintOrder(pag::ShapeElement* element) {
  switch (element->type()) {
    case pag::ShapeType::Fill:
      return static_cast<pag::FillElement*>(element)->composite;
    case pag::ShapeType::Stroke:
      return static_cast<pag::StrokeElement*>(element)->composite;
    case pag::ShapeType::GradientFill:
      return static_cast<pag::GradientFillElement*>(element)->composite;
    case pag::ShapeType::GradientStroke:
      return static_cast<pag::GradientStrokeElement*>(element)->composite;
    default:
      return pag::CompositeOrder::BelowPreviousInSameGroup;
  }
}

// Trim and merge consume the accumulated geometry in order, so the order in which child groups
// contribute their geometry matters to them.
static bool IsOrderSensitiveModifier(pag::ShapeType type) {
  return type == pag::ShapeType::TrimPaths || type == pag::ShapeType::MergePaths;
}

static void ConvertShapeElementList(ConvertState* state,
                                    const std::vector<pag::ShapeElement*>& elements,
                                    std::vector<Element*>* contents, pag::Frame startFrame,
                                    bool orderSensitiveAfter);

// Converts one legacy shape element. orderSensitiveAfter tells a group whether a trim or merge
// later in an enclosing list consumes its geometry in order (see ConvertShapeElementList).
void ConvertShapeElement(ConvertState* state, pag::ShapeElement* element,
                         std::vector<Element*>* contents, pag::Frame startFrame,
                         bool orderSensitiveAfter) {
  if (element == nullptr) {
    return;
  }
  switch (element->type()) {
    case pag::ShapeType::Rectangle: {
      auto* legacy = static_cast<pag::RectangleElement*>(element);
      auto id = AnimatedElementId(state, element);
      auto* rect = state->document->makeNode<Rectangle>(id);
      auto size = PropertyValueAtStart(legacy->size, startFrame, nullptr);
      rect->size = {size.x, size.y};
      auto rpos = PropertyValueAtStart(legacy->position, startFrame, nullptr);
      rect->position = {rpos.x, rpos.y};
      rect->roundness = PropertyValueAtStart(legacy->roundness, startFrame, nullptr);
      rect->reversed = legacy->reversed;
      contents->push_back(rect);
      AttachShapeElementAnimation(state, element, id, "");
      break;
    }
    case pag::ShapeType::Ellipse: {
      auto* legacy = static_cast<pag::EllipseElement*>(element);
      auto id = AnimatedElementId(state, element);
      auto* ellipse = state->document->makeNode<Ellipse>(id);
      auto esize = PropertyValueAtStart(legacy->size, startFrame, nullptr);
      ellipse->size = {esize.x, esize.y};
      auto epos = PropertyValueAtStart(legacy->position, startFrame, nullptr);
      ellipse->position = {epos.x, epos.y};
      ellipse->reversed = legacy->reversed;
      contents->push_back(ellipse);
      AttachShapeElementAnimation(state, element, id, "");
      break;
    }
    case pag::ShapeType::PolyStar: {
      auto* legacy = static_cast<pag::PolyStarElement*>(element);
      auto id = AnimatedElementId(state, element);
      auto* polystar = state->document->makeNode<Polystar>(id);
      polystar->type = legacy->polyType == pag::PolyStarType::Polygon ? PolystarType::Polygon
                                                                      : PolystarType::Star;
      polystar->reversed = legacy->reversed;
      polystar->pointCount =
          static_cast<int>(PropertyValueAtStart(legacy->points, startFrame, nullptr));
      auto ppos = PropertyValueAtStart(legacy->position, startFrame, nullptr);
      polystar->position = {ppos.x, ppos.y};
      polystar->rotation = PropertyValueAtStart(legacy->rotation, startFrame, nullptr);
      polystar->innerRadius = PropertyValueAtStart(legacy->innerRadius, startFrame, nullptr);
      polystar->outerRadius = PropertyValueAtStart(legacy->outerRadius, startFrame, nullptr);
      // Legacy roundness is a Percent already in the 0-1 range, matching PAGX.
      polystar->innerRoundness = PropertyValueAtStart(legacy->innerRoundness, startFrame, nullptr);
      polystar->outerRoundness = PropertyValueAtStart(legacy->outerRoundness, startFrame, nullptr);
      contents->push_back(polystar);
      AttachShapeElementAnimation(state, element, id, "");
      break;
    }
    case pag::ShapeType::ShapePath: {
      auto* legacy = static_cast<pag::ShapePathElement*>(element);
      bool pathAnimated = false;
      auto handle = PropertyValueAtStart(legacy->shapePath, startFrame, &pathAnimated);
      if (handle != nullptr) {
        // An animated outline rides the Path node's "data" channel (spec 6.1); the node needs a
        // targetable id for the channel object, while static outlines keep the id-less inline
        // serialization.
        auto shapeId = pathAnimated
                           ? "pag-legacy-shape-" + std::to_string(state->shapePathIdCounter++)
                           : std::string();
        auto* path = state->document->makeNode<Path>(shapeId);
        path->data = ConvertPathData(state, *handle);
        contents->push_back(path);
        if (pathAnimated && state->currentAnimation != nullptr) {
          auto* object = state->document->makeNode<AnimationObject>();
          object->target = shapeId;
          MigratePathDataChannel(legacy->shapePath, "data", shapeId, state->document, object);
          if (!object->channels.empty()) {
            state->currentAnimation->objects.push_back(object);
          }
        }
      }
      break;
    }
    case pag::ShapeType::Fill: {
      auto* legacy = static_cast<pag::FillElement*>(element);
      auto id = AnimatedElementId(state, element);
      auto colorId = id.empty() ? std::string() : id + "-color";
      auto* fill = state->document->makeNode<Fill>(id);
      auto* solid = state->document->makeNode<SolidColor>(colorId);
      auto color = PropertyValueAtStart(legacy->color, startFrame, nullptr);
      solid->color = ConvertColor(color);
      auto opacity = PropertyValueAtStart(legacy->opacity, startFrame, nullptr);
      fill->alpha = opacity / 255.0f;
      fill->color = solid;
      fill->fillRule =
          legacy->fillRule == pag::FillRule::NonZeroWinding ? FillRule::Winding : FillRule::EvenOdd;
      contents->push_back(fill);
      AttachShapeElementAnimation(state, element, id, colorId);
      break;
    }
    case pag::ShapeType::Stroke: {
      auto* legacy = static_cast<pag::StrokeElement*>(element);
      auto id = AnimatedElementId(state, element);
      auto colorId = id.empty() ? std::string() : id + "-color";
      auto* stroke = state->document->makeNode<Stroke>(id);
      auto* solid = state->document->makeNode<SolidColor>(colorId);
      solid->color = ConvertColor(PropertyValueAtStart(legacy->color, startFrame, nullptr));
      stroke->color = solid;
      stroke->alpha = PropertyValueAtStart(legacy->opacity, startFrame, nullptr) / 255.0f;
      stroke->width = PropertyValueAtStart(legacy->strokeWidth, startFrame, nullptr);
      // Dash pattern, cap, join and miter limit complete the stroke style; the enums share
      // their order across the legacy and PAGX definitions.
      for (auto* dash : legacy->dashes) {
        stroke->dashes.push_back(PropertyValueAtStart(dash, startFrame, nullptr));
      }
      stroke->dashOffset = PropertyValueAtStart(legacy->dashOffset, startFrame, nullptr);
      stroke->cap = static_cast<LineCap>(static_cast<int>(legacy->lineCap));
      stroke->join = static_cast<LineJoin>(static_cast<int>(legacy->lineJoin));
      if (legacy->miterLimit != nullptr) {
        stroke->miterLimit = PropertyValueAtStart(legacy->miterLimit, startFrame, nullptr);
      }
      contents->push_back(stroke);
      AttachShapeElementAnimation(state, element, id, colorId);
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
      ConvertShapeElementList(state, legacy->elements, &group->elements, startFrame,
                              orderSensitiveAfter);
      if (bakeTranslate && (bakeX != 0.0f || bakeY != 0.0f)) {
        for (auto* element : group->elements) {
          if (element == nullptr) {
            continue;
          }
          // Animated positions override the static field, so the bake shifts their channels too.
          ShiftAnimatedPoint(state, element->id, "position", bakeX, bakeY);
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
              ShiftAnimatedPoint(state, colorSource->id, "startPoint", bakeX, bakeY);
              ShiftAnimatedPoint(state, colorSource->id, "endPoint", bakeX, bakeY);
              ShiftAnimatedPoint(state, colorSource->id, "center", bakeX, bakeY);
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
      auto id = AnimatedElementId(state, element);
      auto colorId = id.empty() ? std::string() : id + "-color";
      auto* fill = state->document->makeNode<Fill>(id);
      fill->color = ConvertGradientColorSource(
          state, legacy->fillType, PropertyValueAtStart(legacy->startPoint, startFrame, nullptr),
          PropertyValueAtStart(legacy->endPoint, startFrame, nullptr),
          PropertyValueAtStart(legacy->colors, startFrame, nullptr), colorId);
      fill->alpha = PropertyValueAtStart(legacy->opacity, startFrame, nullptr) / 255.0f;
      fill->fillRule =
          legacy->fillRule == pag::FillRule::NonZeroWinding ? FillRule::Winding : FillRule::EvenOdd;
      contents->push_back(fill);
      AttachShapeElementAnimation(state, element, id, colorId);
      break;
    }
    case pag::ShapeType::GradientStroke: {
      auto* legacy = static_cast<pag::GradientStrokeElement*>(element);
      auto id = AnimatedElementId(state, element);
      auto colorId = id.empty() ? std::string() : id + "-color";
      auto* stroke = state->document->makeNode<Stroke>(id);
      stroke->color = ConvertGradientColorSource(
          state, legacy->fillType, PropertyValueAtStart(legacy->startPoint, startFrame, nullptr),
          PropertyValueAtStart(legacy->endPoint, startFrame, nullptr),
          PropertyValueAtStart(legacy->colors, startFrame, nullptr), colorId);
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
      AttachShapeElementAnimation(state, element, id, colorId);
      break;
    }
    case pag::ShapeType::MergePaths: {
      auto* legacy = static_cast<pag::MergePathsElement*>(element);
      auto* merge = state->document->makeNode<MergePath>();
      // The enum orders differ (legacy Merge/Add/Subtract/Intersect/ExcludeIntersections), so the
      // modes map by meaning, matching the path ops of the legacy ApplyMergePaths.
      switch (legacy->mode) {
        case pag::MergePathsMode::Merge:
          merge->mode = MergePathMode::Append;
          break;
        case pag::MergePathsMode::Add:
          merge->mode = MergePathMode::Union;
          break;
        case pag::MergePathsMode::Subtract:
          merge->mode = MergePathMode::Difference;
          break;
        case pag::MergePathsMode::Intersect:
          merge->mode = MergePathMode::Intersect;
          break;
        case pag::MergePathsMode::ExcludeIntersections:
          merge->mode = MergePathMode::Xor;
          break;
      }
      contents->push_back(merge);
      break;
    }
    case pag::ShapeType::TrimPaths: {
      auto* legacy = static_cast<pag::TrimPathsElement*>(element);
      auto id = AnimatedElementId(state, element);
      auto* trim = state->document->makeNode<TrimPath>(id);
      // Legacy start/end are Percent values already in the 0-1 range, matching PAGX.
      trim->start = PropertyValueAtStart(legacy->start, startFrame, nullptr);
      trim->end = PropertyValueAtStart(legacy->end, startFrame, nullptr);
      trim->offset = PropertyValueAtStart(legacy->offset, startFrame, nullptr);
      trim->type = legacy->trimType == pag::TrimPathsType::Individually ? TrimType::Separate
                                                                        : TrimType::Continuous;
      contents->push_back(trim);
      AttachShapeElementAnimation(state, element, id, "");
      break;
    }
    case pag::ShapeType::Repeater: {
      auto* legacy = static_cast<pag::RepeaterElement*>(element);
      auto id = AnimatedElementId(state, element);
      auto* repeater = state->document->makeNode<Repeater>(id);
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
        repeater->scale = {scale.x, scale.y};
        repeater->rotation = PropertyValueAtStart(legacy->transform->rotation, startFrame, nullptr);
        repeater->startAlpha =
            PropertyValueAtStart(legacy->transform->startOpacity, startFrame, nullptr) / 255.0f;
        repeater->endAlpha =
            PropertyValueAtStart(legacy->transform->endOpacity, startFrame, nullptr) / 255.0f;
      }
      contents->push_back(repeater);
      AttachShapeElementAnimation(state, element, id, "");
      break;
    }
    case pag::ShapeType::RoundCorners: {
      auto* legacy = static_cast<pag::RoundCornersElement*>(element);
      auto id = AnimatedElementId(state, element);
      auto* round = state->document->makeNode<RoundCorner>(id);
      round->radius = PropertyValueAtStart(legacy->radius, startFrame, nullptr);
      contents->push_back(round);
      AttachShapeElementAnimation(state, element, id, "");
      break;
    }
    default:
      state->warn(PAGExportWarning::UnsupportedColorSource, "unsupported shape element");
      break;
  }
}

// Converts a legacy shape content list and restores the legacy paint stacking. The legacy renderer
// stacks the render items of a list (paints and child groups) by insertion: a child group and a
// BelowPreviousInSameGroup paint go to the BOTTOM, an AbovePreviousInSameGroup paint to the top.
// PAGX paints in document order (later on top) with otherwise identical accumulation semantics,
// so the converted elements are permuted into the legacy bottom-to-top order where that is exact:
// a contiguous run of paints after all geometry and modifiers (every paint sees the same
// geometry), or a list whose only render items are child groups (no painter consumes their
// geometry here, and no trim or merge consumes it in order further up). Other mixes would need a
// paint to sit below a group while still painting its geometry, which document order cannot
// express; they keep document order and are reported.
static void ConvertShapeElementList(ConvertState* state,
                                    const std::vector<pag::ShapeElement*>& elements,
                                    std::vector<Element*>* contents, pag::Frame startFrame,
                                    bool orderSensitiveAfter) {
  std::vector<bool> sensitiveAfter(elements.size(), false);
  bool laterSensitive = false;
  for (size_t i = elements.size(); i > 0; i--) {
    sensitiveAfter[i - 1] = orderSensitiveAfter || laterSensitive;
    if (elements[i - 1] != nullptr && IsOrderSensitiveModifier(elements[i - 1]->type())) {
      laterSensitive = true;
    }
  }
  std::vector<std::pair<size_t, size_t>> ranges = {};
  for (size_t i = 0; i < elements.size(); i++) {
    auto begin = contents->size();
    ConvertShapeElement(state, elements[i], contents, startFrame, sensitiveAfter[i]);
    ranges.emplace_back(begin, contents->size());
  }

  std::vector<size_t> renderItems = {};
  std::vector<size_t> stack = {};
  bool hasGroup = false;
  bool hasPaint = false;
  bool hasModifier = false;
  size_t firstPaint = elements.size();
  size_t lastPaint = 0;
  for (size_t i = 0; i < elements.size(); i++) {
    if (elements[i] == nullptr) {
      continue;
    }
    auto type = elements[i]->type();
    if (type == pag::ShapeType::MergePaths) {
      // A merge replaces everything accumulated so far with the merged path: earlier groups and
      // paints stop being render items in both renderers, so stacking restarts after it.
      renderItems.clear();
      stack.clear();
      hasGroup = false;
      hasPaint = false;
      firstPaint = elements.size();
      lastPaint = 0;
      hasModifier = true;
    } else if (type == pag::ShapeType::ShapeGroup) {
      hasGroup = true;
      renderItems.push_back(i);
      stack.insert(stack.begin(), i);
    } else if (IsLegacyPaint(type)) {
      hasPaint = true;
      firstPaint = std::min(firstPaint, i);
      lastPaint = i;
      renderItems.push_back(i);
      if (LegacyPaintOrder(elements[i]) == pag::CompositeOrder::AbovePreviousInSameGroup) {
        stack.push_back(i);
      } else {
        stack.insert(stack.begin(), i);
      }
    } else if (type != pag::ShapeType::Rectangle && type != pag::ShapeType::Ellipse &&
               type != pag::ShapeType::PolyStar && type != pag::ShapeType::ShapePath) {
      hasModifier = true;
    }
  }
  if (stack == renderItems) {
    return;
  }
  bool paintRunContiguous = hasPaint;
  for (size_t i = firstPaint; hasPaint && i <= lastPaint; i++) {
    if (elements[i] == nullptr || !IsLegacyPaint(elements[i]->type())) {
      paintRunContiguous = false;
    }
  }
  bool groupsOnly = !hasPaint && !hasModifier && !orderSensitiveAfter;
  if ((hasGroup || !paintRunContiguous) && !groupsOnly) {
    state->warn(PAGExportWarning::AnimatedContentSkipped,
                "legacy shape paint stacking order approximated");
    return;
  }
  // Both exact cases permute the render items between their own slots: collect the converted
  // elements in the legacy bottom-to-top order, then write them back over the render-item slots
  // in document order (geometry and modifiers between groups stay where they are).
  std::vector<Element*> stacked = {};
  for (auto index : stack) {
    for (auto k = ranges[index].first; k < ranges[index].second; k++) {
      stacked.push_back((*contents)[k]);
    }
  }
  size_t next = 0;
  for (auto index : renderItems) {
    for (auto k = ranges[index].first; k < ranges[index].second; k++) {
      (*contents)[k] = stacked[next++];
    }
  }
}

// Collects the animated channels of one legacy layer into a new AnimationObject under the
// given animation. Returns the object, or nullptr when the layer has no animation at all.
// Legacy matrix of one transform at a frame, matching the legacy RenderTransform order:
// translate(-anchor) * scale * rotate * translate(position).
static void BuildLegacyTransformMatrix(pag::Transform2D* transform, pag::Frame frame,
                                       tgfx::Matrix* out) {
  out->setIdentity();
  if (transform == nullptr) {
    return;
  }
  auto anchor = transform->anchorPoint->getValueAt(frame);
  auto scale = transform->scale->getValueAt(frame);
  pag::Point position = {};
  if (transform->position != nullptr) {
    position = transform->position->getValueAt(frame);
  } else {
    position.x = transform->xPosition->getValueAt(frame);
    position.y = transform->yPosition->getValueAt(frame);
  }
  out->postTranslate(-anchor.x, -anchor.y);
  out->postScale(scale.x, scale.y);
  out->postRotate(transform->rotation->getValueAt(frame));
  out->postTranslate(position.x, position.y);
}

static bool TransformIsAnimated(pag::Transform2D* transform) {
  if (transform == nullptr) {
    return false;
  }
  auto animated = [](auto* property) { return property != nullptr && property->animatable(); };
  return animated(transform->anchorPoint) || animated(transform->position) ||
         animated(transform->xPosition) || animated(transform->yPosition) ||
         animated(transform->scale) || animated(transform->rotation);
}

// The full world matrix of a parented layer at a frame, in the LEGACY composition order:
// TransformCache::createCache renders the child matrix first, then post-concats each parent
// walking outward (direct parent first), i.e. child * direct * grandparent * ... * outermost.
static tgfx::Matrix ParentedWorldMatrixAt(pag::Layer* legacyLayer, pag::Frame frame) {
  auto world = tgfx::Matrix::I();
  BuildLegacyTransformMatrix(legacyLayer->transform, frame, &world);
  for (auto* parent = legacyLayer->parent; parent != nullptr; parent = parent->parent) {
    tgfx::Matrix matrix = tgfx::Matrix::I();
    BuildLegacyTransformMatrix(parent->transform, frame, &matrix);
    world.postConcat(matrix);
  }
  return world;
}

static Matrix3D ToMatrix3D(const tgfx::Matrix& matrix) {
  auto result = Matrix3D();
  result.setRowColumn(0, 0, matrix.getScaleX());
  result.setRowColumn(1, 0, matrix.getSkewY());
  result.setRowColumn(0, 1, matrix.getSkewX());
  result.setRowColumn(1, 1, matrix.getScaleY());
  result.setRowColumn(0, 3, matrix.getTranslateX());
  result.setRowColumn(1, 3, matrix.getTranslateY());
  return result;
}

// Parented layers fold the whole parent-chain transform into the layer matrix (flattening
// option A): the matrix wins over the decomposed fields (priority matrix3D > matrix >
// decomposed, spec 6.2) and the layer keeps its own timeline slot, so the rendering order
// stays exactly the legacy array order.
static void ApplyParentedStaticMatrix(pag::Layer* legacyLayer, Layer* child) {
  if (legacyLayer->parent == nullptr || child == nullptr) {
    return;
  }
  auto world = ParentedWorldMatrixAt(legacyLayer, legacyLayer->startTime);
  // The matrix carries the FULL world transform (linear + translation). The renderer
  // composes renderPosition() (resolved from x/y) ON TOP of the effective matrix, so the
  // authored x/y must be zeroed or the layer translation applies twice.
  child->x = 0.0f;
  child->y = 0.0f;
  if (!world.isIdentity()) {
    child->matrix = {world.getScaleX(), world.getSkewY(),      world.getSkewX(),
                     world.getScaleY(), world.getTranslateX(), world.getTranslateY()};
  }
}

// Bakes the parented layer world matrix per frame into a matrix3D Hold channel: the legacy
// renderer evaluates every property per frame, so per-frame holds are exactly equivalent and
// survive any parent/child animation combination. Only the geometric channels are baked;
// alpha and visible stay decomposed on the same object.
static void BakeParentedMatrixAnimation(ConvertState* state, pag::Layer* legacyLayer,
                                        float frameRate, AnimationObject* object) {
  (void)frameRate;
  bool animated = TransformIsAnimated(legacyLayer->transform);
  for (auto* parent = legacyLayer->parent; parent != nullptr && !animated;
       parent = parent->parent) {
    animated = TransformIsAnimated(parent->transform);
  }
  if (!animated) {
    return;
  }
  auto start = legacyLayer->startTime;
  auto end = start + legacyLayer->duration;
  auto* channel = state->document->makeNode<TypedChannel<Matrix3D>>();
  channel->name = "matrix3D";
  for (auto frame = start; frame <= end; frame++) {
    auto world = ParentedWorldMatrixAt(legacyLayer, frame);
    Keyframe<Matrix3D> matrixKey = {};
    matrixKey.time = frame;
    matrixKey.value = ToMatrix3D(world);
    matrixKey.interpolation = KeyframeInterpolationType::Hold;
    channel->keyframes.push_back(matrixKey);
  }
  if (!channel->keyframes.empty()) {
    object->channels.push_back(channel);
  }
}

AnimationObject* MigrateLayerAnimation(ConvertState* state, pag::Layer* legacyLayer,
                                       Animation* animation, float frameRate) {
  auto* object = state->document->makeNode<AnimationObject>();
  object->target = LegacyLayerTargetId(legacyLayer);
  if (legacyLayer->parent != nullptr) {
    // Parented layers bake the whole transform chain into matrix3D (the decomposed channels
    // would animate in the wrong space after flattening); alpha/visible stay decomposed.
    BakeParentedMatrixAnimation(state, legacyLayer, frameRate, object);
  } else {
    MigrateTransform2DAnimation(legacyLayer->transform, frameRate, state->document, object);
  }
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
  // The composition animation is ensured BEFORE the layer loop: layers convert their effects
  // (filter channels) and shape groups (group transform channels) inside ConvertLayer, and a
  // null currentAnimation there silently dropped the first layer's animated channels.
  auto* animation = EnsureCompositionAnimation(state, legacy);
  state->currentAnimation = animation;
  for (auto it = legacy->layers.rbegin(); it != legacy->layers.rend(); ++it) {
    auto* legacyLayer = *it;
    auto* child = ConvertLayer(state, legacyLayer, legacy->frameRate);
    if (child != nullptr) {
      ApplyParentedStaticMatrix(legacyLayer, child);
      composition->layers.push_back(child);
      // P3-5: migrate this layer's animated channels into the composition animation. Shape
      // groups converted inside the layer append their channels to the same animation.
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
  // Displacement map layer references resolve now that every layer of this composition has a
  // converted node (the reference targets a sibling layer by its deterministic legacy id).
  for (auto& pending : state->pendingDisplacementMaps) {
    auto* mapLayer = state->document->findNode<Layer>(LegacyLayerTargetId(pending.second));
    if (mapLayer != nullptr) {
      pending.first->displacementMapLayer = mapLayer;
    } else {
      state->warn(PAGExportWarning::UnsupportedColorSource, "unresolved displacement map layer");
    }
  }
  state->pendingDisplacementMaps.clear();

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

// Converts legacy layer styles into PAGX style nodes. The GradientOverlay style maps to the
// GradientOverlayStyle node: the gradient rides a shared Gradient resource built with neutral
// unit geometry, because the renderer recomputes the real geometry from the style's
// angle/scale/offset each frame exactly like the legacy GradientOverlayFilter derives it from
// the content image size. The gradient colors, fill type and blend mode are static-only
// (animatable values report an issue); the remaining animated properties join the composition
// animation as channels. Other style types report an issue until their rounds land.
void ConvertLayerStyles(ConvertState* state, pag::Layer* legacyLayer, Layer* layer) {
  for (auto* style : legacyLayer->layerStyles) {
    if (style == nullptr) {
      continue;
    }
    if (style->type() == pag::LayerStyleType::DropShadow) {
      auto* legacy = static_cast<pag::DropShadowStyle*>(style);
      auto startFrame = legacyLayer->startTime;
      auto shadow = state->document->makeNode<DropShadowStyle>("pag-legacy-drop-shadow-" +
                                                               std::to_string(style->uniqueID));
      auto distance = PropertyValueAtStart(legacy->distance, startFrame, nullptr);
      if (distance > 0.0f) {
        auto angle = PropertyValueAtStart(legacy->angle, startFrame, nullptr);
        auto radians = pag::DegreesToRadians(angle - 180.0f);
        shadow->offsetX = cosf(radians) * distance;
        shadow->offsetY = -sinf(radians) * distance;
      }
      auto size = PropertyValueAtStart(legacy->size, startFrame, nullptr);
      auto spread = PropertyValueAtStart(legacy->spread, startFrame, nullptr);
      // The node stores the pre-spread blur radius (half the legacy size): the renderer splits
      // it into the solid core and the blur falloff from the spread, which keeps the size
      // recoverable at every spread value (a baked size*(1-spread)/2 blur would lose it at
      // spread 1, where the shadow is a pure solid silhouette).
      shadow->blurX = size / 2.0f;
      shadow->blurY = shadow->blurX;
      shadow->spread = spread * 100.0f;
      auto color = PropertyValueAtStart(legacy->color, startFrame, nullptr);
      auto opacity = PropertyValueAtStart(legacy->opacity, startFrame, nullptr);
      shadow->color = ConvertColor(color);
      shadow->color.alpha = opacity / 255.0f;
      shadow->blendMode =
          ConvertBlendMode(PropertyValueAtStart(legacy->blendMode, startFrame, nullptr));
      shadow->showBehindLayer = true;
      layer->styles.push_back(shadow);
      if (state->currentAnimation != nullptr) {
        auto* object = state->document->makeNode<AnimationObject>();
        object->target = shadow->id;
        MigrateDropShadowStyleAnimation(legacy, state->currentFrameRate, state->document, object);
        if (!object->channels.empty()) {
          state->currentAnimation->objects.push_back(object);
        }
      }
    } else if (style->type() == pag::LayerStyleType::GradientOverlay) {
      auto* legacy = static_cast<pag::GradientOverlayStyle*>(style);
      auto startFrame = legacyLayer->startTime;
      auto overlay = state->document->makeNode<GradientOverlayStyle>(
          "pag-legacy-gradient-overlay-" + std::to_string(style->uniqueID));
      overlay->alpha = PropertyValueAtStart(legacy->opacity, startFrame, nullptr) / 255.0f;
      overlay->angle = PropertyValueAtStart(legacy->angle, startFrame, nullptr);
      auto fillType = PropertyValueAtStart(legacy->style, startFrame, nullptr);
      switch (fillType) {
        case pag::GradientFillType::Radial:
          overlay->overlayType = GradientOverlayType::Radial;
          break;
        case pag::GradientFillType::Angle:
          overlay->overlayType = GradientOverlayType::Angle;
          break;
        case pag::GradientFillType::Reflected:
          overlay->overlayType = GradientOverlayType::Reflected;
          break;
        case pag::GradientFillType::Linear:
        case pag::GradientFillType::Diamond:
          overlay->overlayType = GradientOverlayType::Linear;
          break;
      }
      overlay->reverse = PropertyValueAtStart(legacy->reverse, startFrame, nullptr);
      overlay->alignWithLayer = PropertyValueAtStart(legacy->alignWithLayer, startFrame, nullptr);
      overlay->scale = PropertyValueAtStart(legacy->scale, startFrame, nullptr);
      auto offset = PropertyValueAtStart(legacy->offset, startFrame, nullptr);
      overlay->offsetX = offset.x;
      overlay->offsetY = offset.y;
      overlay->blendMode =
          ConvertBlendMode(PropertyValueAtStart(legacy->blendMode, startFrame, nullptr));
      auto colors = PropertyValueAtStart(legacy->colors, startFrame, nullptr);
      overlay->gradient = static_cast<Gradient*>(ConvertGradientColorSource(
          state, fillType, pag::Point{0.0f, 0.0f}, pag::Point{1.0f, 0.0f}, colors));
      layer->styles.push_back(overlay);
      if (fillType == pag::GradientFillType::Diamond) {
        state->warn(PAGExportWarning::UnsupportedColorSource,
                    "diamond gradient overlay approximated as linear");
      }
      if (legacy->colors != nullptr && legacy->colors->animatable()) {
        state->warn(PAGExportWarning::UnsupportedColorSource, "animated gradient overlay colors");
      }
      if (legacy->style != nullptr && legacy->style->animatable()) {
        state->warn(PAGExportWarning::UnsupportedColorSource,
                    "animated gradient overlay fill type");
      }
      if (legacy->blendMode != nullptr && legacy->blendMode->animatable()) {
        state->warn(PAGExportWarning::UnsupportedColorSource,
                    "animated gradient overlay blend mode");
      }
      if (state->currentAnimation != nullptr) {
        auto* object = state->document->makeNode<AnimationObject>();
        object->target = overlay->id;
        MigrateGradientOverlayStyleAnimation(legacy, state->currentFrameRate, state->document,
                                             object);
        if (!object->channels.empty()) {
          state->currentAnimation->objects.push_back(object);
        }
      }
    } else if (style->type() == pag::LayerStyleType::OuterGlow) {
      auto* legacy = static_cast<pag::OuterGlowStyle*>(style);
      auto startFrame = legacyLayer->startTime;
      auto glow = state->document->makeNode<OuterGlowStyle>("pag-legacy-outer-glow-" +
                                                            std::to_string(style->uniqueID));
      glow->color = ConvertColor(PropertyValueAtStart(legacy->color, startFrame, nullptr));
      glow->alpha = PropertyValueAtStart(legacy->opacity, startFrame, nullptr) / 255.0f;
      glow->size = PropertyValueAtStart(legacy->size, startFrame, nullptr);
      glow->spread = PropertyValueAtStart(legacy->spread, startFrame, nullptr) * 100.0f;
      glow->range = PropertyValueAtStart(legacy->range, startFrame, nullptr) * 100.0f;
      glow->blendMode =
          ConvertBlendMode(PropertyValueAtStart(legacy->blendMode, startFrame, nullptr));
      layer->styles.push_back(glow);
      // The legacy renderer only reads the solid color, spread, size and range: the gradient
      // color type, noise and jitter inputs never reach its shader, so the migrated render
      // degrades exactly the way the legacy one does.
      if (PropertyValueAtStart(legacy->colorType, startFrame, nullptr) ==
          pag::GlowColorType::Gradient) {
        state->warn(PAGExportWarning::UnsupportedColorSource,
                    "gradient outer glow approximated as solid color");
      }
      if (legacy->blendMode != nullptr && legacy->blendMode->animatable()) {
        state->warn(PAGExportWarning::UnsupportedColorSource, "animated outer glow blend mode");
      }
      if (state->currentAnimation != nullptr) {
        auto* object = state->document->makeNode<AnimationObject>();
        object->target = glow->id;
        MigrateOuterGlowStyleAnimation(legacy, state->currentFrameRate, state->document, object);
        if (!object->channels.empty()) {
          state->currentAnimation->objects.push_back(object);
        }
      }
    } else if (style->type() == pag::LayerStyleType::Stroke) {
      auto* legacy = static_cast<pag::StrokeStyle*>(style);
      auto startFrame = legacyLayer->startTime;
      auto stroke = state->document->makeNode<StrokeLayerStyle>("pag-legacy-stroke-" +
                                                                std::to_string(style->uniqueID));
      stroke->color = ConvertColor(PropertyValueAtStart(legacy->color, startFrame, nullptr));
      stroke->alpha = PropertyValueAtStart(legacy->opacity, startFrame, nullptr) / 255.0f;
      stroke->size = PropertyValueAtStart(legacy->size, startFrame, nullptr);
      // The legacy and PAGX stroke position enums order their values differently (legacy:
      // Outside/Inside/Center, PAGX: Outside/Center/Inside), so map by name rather than value.
      switch (PropertyValueAtStart(legacy->position, startFrame, nullptr)) {
        case pag::StrokePosition::Center:
          stroke->position = StrokePosition::Center;
          break;
        case pag::StrokePosition::Inside:
          stroke->position = StrokePosition::Inside;
          break;
        case pag::StrokePosition::Outside:
        default:
          stroke->position = StrokePosition::Outside;
          break;
      }
      stroke->blendMode =
          ConvertBlendMode(PropertyValueAtStart(legacy->blendMode, startFrame, nullptr));
      layer->styles.push_back(stroke);
      if (legacy->blendMode != nullptr && legacy->blendMode->animatable()) {
        state->warn(PAGExportWarning::UnsupportedColorSource, "animated stroke blend mode");
      }
      if (state->currentAnimation != nullptr) {
        auto* object = state->document->makeNode<AnimationObject>();
        object->target = stroke->id;
        MigrateStrokeStyleAnimation(legacy, state->currentFrameRate, state->document, object);
        if (!object->channels.empty()) {
          state->currentAnimation->objects.push_back(object);
        }
      }
    } else {
      state->warn(PAGExportWarning::UnsupportedColorSource, "unsupported layer style");
    }
  }
}

// Converts legacy layer effects into PAGX filters. The FastBlur effect maps to BlurFilter
// (blurriness becomes blurX/blurY per the blur direction, repeatEdgePixels selects the tile
// mode), and the Glow, Mosaic, RadialBlur, CornerPin, Bulge, MotionTile, LevelsIndividual,
// HueSaturation, BrightnessContrast and DisplacementMap effects map to their filter nodes
// (animated properties join the composition animation). Other effect types report an issue
// until their phase-3 filters land.
void ConvertLayerEffects(ConvertState* state, pag::Layer* legacyLayer, Layer* layer) {
  for (auto* effect : legacyLayer->effects) {
    if (effect == nullptr) {
      continue;
    }
    if (effect->type() == pag::EffectType::DisplacementMap) {
      auto* legacy = static_cast<pag::DisplacementMapEffect*>(effect);
      auto* displacement = state->document->makeNode<DisplacementMapFilter>(
          "pag-legacy-displacement-" + std::to_string(legacy->uniqueID));
      displacement->useForHorizontalDisplacement =
          static_cast<DisplacementMapSource>(PropertyValueAtStart(
              legacy->useForHorizontalDisplacement, legacyLayer->startTime, nullptr));
      displacement->maxHorizontalDisplacement =
          PropertyValueAtStart(legacy->maxHorizontalDisplacement, legacyLayer->startTime, nullptr);
      displacement->useForVerticalDisplacement =
          static_cast<DisplacementMapSource>(PropertyValueAtStart(
              legacy->useForVerticalDisplacement, legacyLayer->startTime, nullptr));
      displacement->maxVerticalDisplacement =
          PropertyValueAtStart(legacy->maxVerticalDisplacement, legacyLayer->startTime, nullptr);
      displacement->displacementMapBehavior = static_cast<DisplacementMapBehavior>(
          PropertyValueAtStart(legacy->displacementMapBehavior, legacyLayer->startTime, nullptr));
      displacement->edgeBehavior =
          PropertyValueAtStart(legacy->edgeBehavior, legacyLayer->startTime, nullptr) != 0;
      displacement->expandOutput =
          PropertyValueAtStart(legacy->expandOutput, legacyLayer->startTime, nullptr) != 0;
      layer->filters.push_back(displacement);
      // The map layer reference resolves after the whole composition converts (the map layer
      // may come later in the list); track it for the post-pass next to the track mattes.
      if (legacy->displacementMapLayer != nullptr) {
        state->pendingDisplacementMaps.push_back({displacement, legacy->displacementMapLayer});
      }
      if (state->currentAnimation != nullptr) {
        auto* object = state->document->makeNode<AnimationObject>();
        object->target = displacement->id;
        MigrateDisplacementMapEffectAnimation(legacy, state->currentFrameRate, state->document,
                                              object);
        if (!object->channels.empty()) {
          state->currentAnimation->objects.push_back(object);
        }
      }
    } else if (effect->type() == pag::EffectType::BrightnessContrast) {
      auto* legacy = static_cast<pag::BrightnessContrastEffect*>(effect);
      auto* bc = state->document->makeNode<BrightnessContrastFilter>(
          "pag-legacy-brightness-" + std::to_string(legacy->uniqueID));
      bc->brightness = PropertyValueAtStart(legacy->brightness, legacyLayer->startTime, nullptr);
      bc->contrast = PropertyValueAtStart(legacy->contrast, legacyLayer->startTime, nullptr);
      bc->useOldVersion =
          PropertyValueAtStart(legacy->useOldVersion, legacyLayer->startTime, nullptr) != 0;
      layer->filters.push_back(bc);
      if (state->currentAnimation != nullptr) {
        auto* object = state->document->makeNode<AnimationObject>();
        object->target = bc->id;
        MigrateBrightnessContrastEffectAnimation(legacy, state->currentFrameRate, state->document,
                                                 object);
        if (!object->channels.empty()) {
          state->currentAnimation->objects.push_back(object);
        }
      }
    } else if (effect->type() == pag::EffectType::HueSaturation) {
      auto* legacy = static_cast<pag::HueSaturationEffect*>(effect);
      auto* hs = state->document->makeNode<HueSaturationFilter>("pag-legacy-huesat-" +
                                                                std::to_string(legacy->uniqueID));
      // The legacy stores per-channel-control slots but its renderer only consumes the
      // selected slot, so the migration flattens to the selected values.
      auto channelControl = static_cast<int>(legacy->channelControl);
      if (channelControl >= 0 && channelControl < static_cast<int>(legacy->hue.size())) {
        hs->hue = legacy->hue[channelControl];
        hs->saturation = legacy->saturation[channelControl];
        hs->lightness = legacy->lightness[channelControl];
      }
      hs->colorize = legacy->colorize;
      hs->colorizeHue = PropertyValueAtStart(legacy->colorizeHue, legacyLayer->startTime, nullptr);
      hs->colorizeSaturation =
          PropertyValueAtStart(legacy->colorizeSaturation, legacyLayer->startTime, nullptr);
      hs->colorizeLightness =
          PropertyValueAtStart(legacy->colorizeLightness, legacyLayer->startTime, nullptr);
      layer->filters.push_back(hs);
      if (state->currentAnimation != nullptr) {
        auto* object = state->document->makeNode<AnimationObject>();
        object->target = hs->id;
        MigrateHueSaturationEffectAnimation(legacy, state->currentFrameRate, state->document,
                                            object);
        if (!object->channels.empty()) {
          state->currentAnimation->objects.push_back(object);
        }
      }
    } else if (effect->type() == pag::EffectType::LevelsIndividual) {
      auto* legacy = static_cast<pag::LevelsIndividualEffect*>(effect);
      auto* levels = state->document->makeNode<LevelsIndividualFilter>(
          "pag-legacy-levels-" + std::to_string(legacy->uniqueID));
      levels->inputBlack =
          PropertyValueAtStart(legacy->inputBlack, legacyLayer->startTime, nullptr);
      levels->inputWhite =
          PropertyValueAtStart(legacy->inputWhite, legacyLayer->startTime, nullptr);
      levels->gamma = PropertyValueAtStart(legacy->gamma, legacyLayer->startTime, nullptr);
      levels->outputBlack =
          PropertyValueAtStart(legacy->outputBlack, legacyLayer->startTime, nullptr);
      levels->outputWhite =
          PropertyValueAtStart(legacy->outputWhite, legacyLayer->startTime, nullptr);
      levels->redInputBlack =
          PropertyValueAtStart(legacy->redInputBlack, legacyLayer->startTime, nullptr);
      levels->redInputWhite =
          PropertyValueAtStart(legacy->redInputWhite, legacyLayer->startTime, nullptr);
      levels->redGamma = PropertyValueAtStart(legacy->redGamma, legacyLayer->startTime, nullptr);
      levels->redOutputBlack =
          PropertyValueAtStart(legacy->redOutputBlack, legacyLayer->startTime, nullptr);
      levels->redOutputWhite =
          PropertyValueAtStart(legacy->redOutputWhite, legacyLayer->startTime, nullptr);
      levels->greenInputBlack =
          PropertyValueAtStart(legacy->greenInputBlack, legacyLayer->startTime, nullptr);
      levels->greenInputWhite =
          PropertyValueAtStart(legacy->greenInputWhite, legacyLayer->startTime, nullptr);
      levels->greenGamma =
          PropertyValueAtStart(legacy->greenGamma, legacyLayer->startTime, nullptr);
      levels->greenOutputBlack =
          PropertyValueAtStart(legacy->greenOutputBlack, legacyLayer->startTime, nullptr);
      levels->greenOutputWhite =
          PropertyValueAtStart(legacy->greenOutputWhite, legacyLayer->startTime, nullptr);
      levels->blueInputBlack =
          PropertyValueAtStart(legacy->blueInputBlack, legacyLayer->startTime, nullptr);
      levels->blueInputWhite =
          PropertyValueAtStart(legacy->blueInputWhite, legacyLayer->startTime, nullptr);
      levels->blueGamma = PropertyValueAtStart(legacy->blueGamma, legacyLayer->startTime, nullptr);
      levels->blueOutputBlack =
          PropertyValueAtStart(legacy->blueOutputBlack, legacyLayer->startTime, nullptr);
      levels->blueOutputWhite =
          PropertyValueAtStart(legacy->blueOutputWhite, legacyLayer->startTime, nullptr);
      layer->filters.push_back(levels);
      if (state->currentAnimation != nullptr) {
        auto* object = state->document->makeNode<AnimationObject>();
        object->target = levels->id;
        MigrateLevelsIndividualEffectAnimation(legacy, state->currentFrameRate, state->document,
                                               object);
        if (!object->channels.empty()) {
          state->currentAnimation->objects.push_back(object);
        }
      }
    } else if (effect->type() == pag::EffectType::MotionTile) {
      auto* legacy = static_cast<pag::MotionTileEffect*>(effect);
      auto* tile = state->document->makeNode<MotionTileFilter>("pag-legacy-motiontile-" +
                                                               std::to_string(legacy->uniqueID));
      auto center = PropertyValueAtStart(legacy->tileCenter, legacyLayer->startTime, nullptr);
      tile->tileCenter = {center.x, center.y};
      tile->tileWidth = PropertyValueAtStart(legacy->tileWidth, legacyLayer->startTime, nullptr);
      tile->tileHeight = PropertyValueAtStart(legacy->tileHeight, legacyLayer->startTime, nullptr);
      tile->outputWidth =
          PropertyValueAtStart(legacy->outputWidth, legacyLayer->startTime, nullptr);
      tile->outputHeight =
          PropertyValueAtStart(legacy->outputHeight, legacyLayer->startTime, nullptr);
      tile->mirrorEdges =
          PropertyValueAtStart(legacy->mirrorEdges, legacyLayer->startTime, nullptr) != 0;
      tile->phase = PropertyValueAtStart(legacy->phase, legacyLayer->startTime, nullptr);
      tile->horizontalPhaseShift =
          PropertyValueAtStart(legacy->horizontalPhaseShift, legacyLayer->startTime, nullptr) != 0;
      layer->filters.push_back(tile);
      if (state->currentAnimation != nullptr) {
        auto* object = state->document->makeNode<AnimationObject>();
        object->target = tile->id;
        MigrateMotionTileEffectAnimation(legacy, state->currentFrameRate, state->document, object);
        if (!object->channels.empty()) {
          state->currentAnimation->objects.push_back(object);
        }
      }
    } else if (effect->type() == pag::EffectType::Bulge) {
      auto* legacy = static_cast<pag::BulgeEffect*>(effect);
      auto* bulge = state->document->makeNode<BulgeFilter>("pag-legacy-bulge-" +
                                                           std::to_string(legacy->uniqueID));
      bulge->horizontalRadius =
          PropertyValueAtStart(legacy->horizontalRadius, legacyLayer->startTime, nullptr);
      bulge->verticalRadius =
          PropertyValueAtStart(legacy->verticalRadius, legacyLayer->startTime, nullptr);
      auto center = PropertyValueAtStart(legacy->bulgeCenter, legacyLayer->startTime, nullptr);
      bulge->bulgeCenter = {center.x, center.y};
      bulge->bulgeHeight =
          PropertyValueAtStart(legacy->bulgeHeight, legacyLayer->startTime, nullptr);
      bulge->taperRadius =
          PropertyValueAtStart(legacy->taperRadius, legacyLayer->startTime, nullptr);
      bulge->pinning = PropertyValueAtStart(legacy->pinning, legacyLayer->startTime, nullptr) != 0;
      layer->filters.push_back(bulge);
      if (state->currentAnimation != nullptr) {
        auto* object = state->document->makeNode<AnimationObject>();
        object->target = bulge->id;
        MigrateBulgeEffectAnimation(legacy, state->currentFrameRate, state->document, object);
        if (!object->channels.empty()) {
          state->currentAnimation->objects.push_back(object);
        }
      }
    } else if (effect->type() == pag::EffectType::CornerPin) {
      auto* legacy = static_cast<pag::CornerPinEffect*>(effect);
      auto* cornerPin = state->document->makeNode<CornerPinFilter>(
          "pag-legacy-cornerpin-" + std::to_string(legacy->uniqueID));
      auto upperLeft = PropertyValueAtStart(legacy->upperLeft, legacyLayer->startTime, nullptr);
      auto upperRight = PropertyValueAtStart(legacy->upperRight, legacyLayer->startTime, nullptr);
      auto lowerLeft = PropertyValueAtStart(legacy->lowerLeft, legacyLayer->startTime, nullptr);
      auto lowerRight = PropertyValueAtStart(legacy->lowerRight, legacyLayer->startTime, nullptr);
      cornerPin->upperLeft = {upperLeft.x, upperLeft.y};
      cornerPin->upperRight = {upperRight.x, upperRight.y};
      cornerPin->lowerLeft = {lowerLeft.x, lowerLeft.y};
      cornerPin->lowerRight = {lowerRight.x, lowerRight.y};
      layer->filters.push_back(cornerPin);
      if (state->currentAnimation != nullptr) {
        auto* object = state->document->makeNode<AnimationObject>();
        object->target = cornerPin->id;
        MigrateCornerPinEffectAnimation(legacy, state->currentFrameRate, state->document, object);
        if (!object->channels.empty()) {
          state->currentAnimation->objects.push_back(object);
        }
      }
    } else if (effect->type() == pag::EffectType::RadialBlur) {
      auto* legacy = static_cast<pag::RadialBlurEffect*>(effect);
      auto* radial = state->document->makeNode<RadialBlurFilter>("pag-legacy-radial-" +
                                                                 std::to_string(legacy->uniqueID));
      radial->amount = PropertyValueAtStart(legacy->amount, legacyLayer->startTime, nullptr);
      auto center = PropertyValueAtStart(legacy->center, legacyLayer->startTime, nullptr);
      radial->center = {center.x, center.y};
      radial->mode = static_cast<RadialBlurMode>(
          PropertyValueAtStart(legacy->mode, legacyLayer->startTime, nullptr));
      radial->antialias = static_cast<RadialBlurAntialias>(
          PropertyValueAtStart(legacy->antialias, legacyLayer->startTime, nullptr));
      layer->filters.push_back(radial);
      if (state->currentAnimation != nullptr) {
        auto* object = state->document->makeNode<AnimationObject>();
        object->target = radial->id;
        MigrateRadialBlurEffectAnimation(legacy, state->currentFrameRate, state->document, object);
        if (!object->channels.empty()) {
          state->currentAnimation->objects.push_back(object);
        }
      }
    } else if (effect->type() == pag::EffectType::Mosaic) {
      auto* legacy = static_cast<pag::MosaicEffect*>(effect);
      auto* mosaic = state->document->makeNode<MosaicFilter>("pag-legacy-mosaic-" +
                                                             std::to_string(legacy->uniqueID));
      mosaic->horizontalBlocks = static_cast<float>(
          PropertyValueAtStart(legacy->horizontalBlocks, legacyLayer->startTime, nullptr));
      mosaic->verticalBlocks = static_cast<float>(
          PropertyValueAtStart(legacy->verticalBlocks, legacyLayer->startTime, nullptr));
      mosaic->sharpColors =
          PropertyValueAtStart(legacy->sharpColors, legacyLayer->startTime, nullptr) != 0;
      layer->filters.push_back(mosaic);
      if (state->currentAnimation != nullptr) {
        auto* object = state->document->makeNode<AnimationObject>();
        object->target = mosaic->id;
        MigrateMosaicEffectAnimation(legacy, state->currentFrameRate, state->document, object);
        if (!object->channels.empty()) {
          state->currentAnimation->objects.push_back(object);
        }
      }
    } else if (effect->type() == pag::EffectType::Glow) {
      auto* legacy = static_cast<pag::GlowEffect*>(effect);
      auto* glow = state->document->makeNode<GlowFilter>("pag-legacy-glow-" +
                                                         std::to_string(legacy->uniqueID));
      glow->glowThreshold =
          PropertyValueAtStart(legacy->glowThreshold, legacyLayer->startTime, nullptr);
      glow->glowRadius = PropertyValueAtStart(legacy->glowRadius, legacyLayer->startTime, nullptr);
      glow->glowIntensity =
          PropertyValueAtStart(legacy->glowIntensity, legacyLayer->startTime, nullptr);
      layer->filters.push_back(glow);
      if (state->currentAnimation != nullptr) {
        auto* object = state->document->makeNode<AnimationObject>();
        object->target = glow->id;
        MigrateGlowEffectAnimation(legacy, state->currentFrameRate, state->document, object);
        if (!object->channels.empty()) {
          state->currentAnimation->objects.push_back(object);
        }
      }
    } else if (effect->type() == pag::EffectType::FastBlur) {
      auto* legacy = static_cast<pag::FastBlurEffect*>(effect);
      auto* blur = state->document->makeNode<BlurFilter>();
      auto blurriness = PropertyValueAtStart(legacy->blurriness, legacyLayer->startTime, nullptr);
      auto direction =
          PropertyValueAtStart(legacy->blurDimensions, legacyLayer->startTime, nullptr);
      // The legacy GaussianBlurFilter divides blurriness by 2 before passing to tgfx (see
      // src/rendering/filters/gaussianblur/GaussianBlurFilter.cpp), and repeatEdgePixels maps
      // to Clamp (not Repeat) per the same implementation.
      auto radius = blurriness / 2.0f;
      blur->blurX = direction == pag::BlurDimensionsDirection::Vertical ? 0.0f : radius;
      blur->blurY = direction == pag::BlurDimensionsDirection::Horizontal ? 0.0f : radius;
      auto repeatEdge =
          PropertyValueAtStart(legacy->repeatEdgePixels, legacyLayer->startTime, nullptr);
      blur->tileMode = repeatEdge ? TileMode::Clamp : TileMode::Decal;
      layer->filters.push_back(blur);
    } else {
      state->warn(PAGExportWarning::UnsupportedColorSource, "unsupported layer effect");
    }
  }
}

// Layer masks (spec 7.6): every legacy MaskData entry becomes a MaskPath on the PAGX layer.
// Static fields take their content-start values; an animated mask path rides the "path" channel
// (spec 6.1) with key times shifted onto the composition clock by the layer's startTime, the
// same content-to-composition mapping the visibility windows use.
void ConvertLayerMasks(ConvertState* state, pag::Layer* legacyLayer, Layer* layer) {
  if (legacyLayer->masks.empty()) {
    return;
  }
  auto maskIndex = 0;
  for (auto* legacyMask : legacyLayer->masks) {
    if (legacyMask == nullptr) {
      continue;
    }
    auto targetId = LegacyLayerTargetId(legacyLayer) + "-mask-" + std::to_string(maskIndex);
    maskIndex++;
    auto* maskPath = state->document->makeNode<MaskPath>(targetId);
    bool pathAnimated = false;
    auto startPath =
        PropertyValueAtStart(legacyMask->maskPath, legacyLayer->startTime, &pathAnimated);
    if (startPath != nullptr) {
      maskPath->path = ConvertLegacyPathData(state->document, *startPath, targetId + "-shape");
    }
    maskPath->maskMode = static_cast<MaskMode>(static_cast<int>(legacyMask->maskMode));
    maskPath->inverted = legacyMask->inverted;
    auto feather = PropertyValueAtStart(legacyMask->maskFeather, legacyLayer->startTime, nullptr);
    maskPath->feather = {feather.x, feather.y};
    auto opacity = PropertyValueAtStart(legacyMask->maskOpacity, legacyLayer->startTime, nullptr);
    maskPath->opacity = opacity / 255.0f;
    maskPath->expansion =
        PropertyValueAtStart(legacyMask->maskExpansion, legacyLayer->startTime, nullptr);
    layer->maskPaths.push_back(maskPath);
    if (pathAnimated && state->currentAnimation != nullptr) {
      auto* object = state->document->makeNode<AnimationObject>();
      object->target = targetId;
      MigratePathDataChannel(legacyMask->maskPath, "path", targetId, state->document, object);
      if (!object->channels.empty()) {
        state->currentAnimation->objects.push_back(object);
      }
    }
  }
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
        // Time-lock mount (spec 6.3): the legacy mapping
        // compositionFrame = (parentFrame - compositionStartTime) * (childRate / parentRate)
        // only converts between the two frame grids; in seconds it is
        // childTime = parentTime - compositionStartTime / parentRate, so the lock carries the
        // start offset with a unit scale (the PAGX clocks are in seconds). The legacy layer
        // stretch is not consumed by the legacy renderer (PreComposeLayer::getCompositionFrame
        // ignores it), so it does not scale the child clock either.
        if (parentFrameRate > 0.0f) {
          layer->startTime = static_cast<float>(legacy->compositionStartTime) / parentFrameRate;
        }
      }
      // The legacy renderer applies layer effects to precompose layers the same way (e.g. a
      // HueSaturation over a nested composition).
      ConvertLayerEffects(state, legacyLayer, layer);
      ConvertLayerStyles(state, legacyLayer, layer);
      ConvertLayerMasks(state, legacyLayer, layer);
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
      // The legacy renderer applies layer effects and styles to solid layers the same as every
      // other layer type.
      ConvertLayerEffects(state, legacyLayer, layer);
      ConvertLayerStyles(state, legacyLayer, layer);
      ConvertLayerMasks(state, legacyLayer, layer);
      return layer;
    }
    case pag::LayerType::Shape: {
      auto* legacy = static_cast<pag::ShapeLayer*>(legacyLayer);
      auto* layer = state->document->makeNode<Layer>(LegacyLayerTargetId(legacyLayer));
      layer->name = legacyLayer->name;
      layer->visible = legacyLayer->isActive;
      layer->blendMode = ConvertBlendMode(legacyLayer->blendMode);
      ConvertTransform2D(state, legacyLayer->transform, layer, legacyLayer->startTime);
      ConvertShapeElementList(state, legacy->contents, &layer->contents, legacyLayer->startTime,
                              false);
      ConvertLayerMasks(state, legacyLayer, layer);
      ConvertLayerEffects(state, legacyLayer, layer);
      ConvertLayerStyles(state, legacyLayer, layer);
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
        // Legacy tracking is 1/1000 em, the same convention as the PAGX Text field.
        text->tracking = textDocument->tracking;
        if (!textDocument->boxText) {
          // Legacy point text anchors the first line's baseline at the layer origin; combined
          // with the box's NaN width, Center/End become the legacy centered/right drawX offsets.
          text->baseline = TextBaseline::Alphabetic;
        }
        // The legacy baselineShift lifts every line's baseline (drawY = baseline - shift) and the
        // box-text firstBaseLine anchors the first baseline relative to the layer origin; the
        // box-local conversion subtracts boxTextPos.y. Both act on the rotated frame's baseline
        // axis for vertical text (a column-position shift in visual space), which the native
        // vertical layout places differently - defer those to the vertical-geometry round.
        auto isVerticalText = textDocument->direction == pag::TextDirection::Vertical;
        if (textDocument->baselineShift != 0) {
          if (isVerticalText) {
            state->warn(PAGExportWarning::UnsupportedColorSource,
                        "vertical text baselineShift (column offset) not migrated");
          } else {
            text->baselineShift = textDocument->baselineShift;
          }
        }
        if (textDocument->boxText && textDocument->firstBaseLine != 0) {
          if (isVerticalText) {
            state->warn(PAGExportWarning::UnsupportedColorSource,
                        "vertical text firstBaseLine (column baseline) not migrated");
          } else {
            text->firstBaseLine = textDocument->firstBaseLine - textDocument->boxTextPos.y;
          }
        }
        if (textDocument->backgroundAlpha > 0) {
          if (isVerticalText) {
            state->warn(PAGExportWarning::UnsupportedColorSource,
                        "vertical text background not migrated");
          } else {
            text->backgroundColor = ConvertColor(textDocument->backgroundColor);
            text->backgroundAlpha = textDocument->backgroundAlpha / 255.0f;
          }
        }
        textBox->elements.push_back(text);
        MigrateTextAnimators(state, legacyText, textBox);
        MigrateTextPathOptions(state, legacyText, textBox);
        // The legacy derives line spacing from the document leading (auto = 1.2 x fontSize); the
        // PAGX auto mode uses font metrics instead, so the spacing is always mapped explicitly.
        textBox->lineHeight = textDocument->leading == 0 ? roundf(textDocument->fontSize * 1.2f)
                                                         : textDocument->leading;
        textBox->textAlign = ConvertJustification(textDocument->justification);
        if (textDocument->direction == pag::TextDirection::Vertical) {
          textBox->writingMode = WritingMode::Vertical;
        }
        if (textDocument->justification == pag::ParagraphJustification::FullJustifyLastLineCenter ||
            textDocument->justification == pag::ParagraphJustification::FullJustifyLastLineRight ||
            textDocument->justification == pag::ParagraphJustification::FullJustifyLastLineFull) {
          state->warn(PAGExportWarning::UnsupportedColorSource,
                      "justified text last-line alignment approximated as start");
        }
        // The legacy paints StrokeAndFill text as two passes over the glyph paths: fill first,
        // then the stroke when strokeOverFill is set (the stroke covers the fill edge), or the
        // reverse otherwise. The element order encodes the same stacking (M0: earlier painters
        // render below later ones), and the legacy's CopyLines drops the stroke pass entirely
        // for a stroke-and-fill text whose width is under 0.1px.
        Element* fillElement = nullptr;
        if (textDocument->applyFill) {
          auto* fill = state->document->makeNode<Fill>();
          auto* solid = state->document->makeNode<SolidColor>();
          solid->color = ConvertColor(textDocument->fillColor);
          fill->color = solid;
          fillElement = fill;
        }
        Element* strokeElement = nullptr;
        if (textDocument->applyStroke && textDocument->strokeWidth >= 0.1f) {
          auto* stroke = state->document->makeNode<Stroke>();
          auto* solid = state->document->makeNode<SolidColor>();
          solid->color = ConvertColor(textDocument->strokeColor);
          stroke->color = solid;
          stroke->width = textDocument->strokeWidth;
          strokeElement = stroke;
        }
        if (textDocument->strokeOverFill || strokeElement == nullptr) {
          if (fillElement != nullptr) {
            textBox->elements.push_back(fillElement);
          }
          if (strokeElement != nullptr) {
            textBox->elements.push_back(strokeElement);
          }
        } else {
          textBox->elements.push_back(strokeElement);
          if (fillElement != nullptr) {
            textBox->elements.push_back(fillElement);
          }
        }
        layer->contents.push_back(textBox);
        state->convertedTexts.push_back({legacyText, text, state->currentComposition});
      }
      // The legacy renderer applies layer effects to every layer type; text layers carry them
      // the same way (the runtime applies node->filters uniformly in buildLayerInto).
      ConvertLayerEffects(state, legacyLayer, layer);
      ConvertLayerStyles(state, legacyLayer, layer);
      ConvertLayerMasks(state, legacyLayer, layer);
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
      ConvertLayerEffects(state, legacyLayer, layer);
      ConvertLayerStyles(state, legacyLayer, layer);
      ConvertLayerMasks(state, legacyLayer, layer);
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
  // pag::File::duration() already returns FRAMES (the root composition's frame count), so the
  // duration maps directly - multiplying by the frame rate again would stretch the timeline
  // by that factor.
  animation->duration = file->duration();
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
