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
//  license is distributed on an "AS IS" basis, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "pagx/pag/exporter/LayoutFlattener.h"
#include <cmath>
#include <functional>
#include "pagx/LayoutContext.h"
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXNodeChannel.h"
#include "pagx/TextLayout.h"
#include "pagx/nodes/Animation.h"
#include "pagx/nodes/AnimationObject.h"
#include "pagx/nodes/Channel.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/DataBind.h"
#include "pagx/nodes/Element.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/LayoutNode.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/Polystar.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/nodes/TextPath.h"
#include "pagx/utils/TextUtils.h"

namespace pagx {
namespace {
// Flex containers assign BOTH axes of an included child: the main axis through flex
// allocation and the cross axis through alignment (Stretch by default). The resolved
// sizes must become explicit on flatten — otherwise the child turns into a deferred
// content-measured node whose extra layout pass re-runs setLayoutSize on already-scaled
// descendants and drifts their fit scale (Text::setLayoutSize re-derives the scale from
// the scaled shape; an unflattened document never hits this because its second pass
// re-derives from stable constraint targets, verified RELAYOUTDIFF=0).
enum class FlexAxis { None, Horizontal, Vertical };

struct FlattenState {
  FlattenOptions options = {};
  LayoutContext* context = nullptr;
  FlexAxis parentFlexAxis = FlexAxis::None;
};

bool IsNearlyOne(float value) {
  return std::abs(value - 1.0f) < 1e-6f;
}

// Constraint and percent fields exist on every LayoutNode subclass and mean nothing once the
// resolved values are written back (design doc §4.6).
void ClearConstraints(LayoutNode* node) {
  node->left = NAN;
  node->right = NAN;
  node->top = NAN;
  node->bottom = NAN;
  node->centerX = NAN;
  node->centerY = NAN;
  node->percentWidth = NAN;
  node->percentHeight = NAN;
}

void BakeScaleIntoPathData(PathData* data, float scale, float offsetX, float offsetY) {
  Matrix matrix = {};
  matrix.a = scale;
  matrix.d = scale;
  matrix.tx = offsetX;
  matrix.ty = offsetY;
  data->transform(matrix);
}

void NotifyGeometryScaled(const FlattenState& state, Node* node, float scale) {
  if (state.options.geometryScaled != nullptr) {
    state.options.geometryScaled(node, scale);
  }
}

void FlattenElement(Element* element, FlattenState* state);
void FlattenLayers(const std::vector<Layer*>& layers, FlattenState* state);

void FlattenElements(const std::vector<Element*>& elements, FlattenState* state) {
  for (auto* element : elements) {
    FlattenElement(element, state);
  }
}

// Authored sizes stay: they are explicit measurement inputs, and dropping one turns a
// single-pass layout into the deferred content-measured path, which re-runs
// setLayoutSize on already-scaled children and drifts their fit scale (Text is the
// sensitive case). Percent-sized axes take their resolved value; content-measured
// axes stay NaN and re-measure to the same extent (any multi-pass drift in the
// original is reproduced identically).
static void PreserveExplicitSize(LayoutNode* node, bool flexAllocated) {
  auto bounds = node->layoutBounds();
  bool widthAssigned = flexAllocated || !std::isnan(node->percentWidth) ||
                       (!std::isnan(node->left) && !std::isnan(node->right));
  bool heightAssigned = flexAllocated || !std::isnan(node->percentHeight) ||
                        (!std::isnan(node->top) && !std::isnan(node->bottom));
  if (widthAssigned && std::isnan(node->width) && !std::isnan(bounds.width)) {
    node->width = bounds.width;
  }
  if (heightAssigned && std::isnan(node->height) && !std::isnan(bounds.height)) {
    node->height = bounds.height;
  }
}

void FlattenLayer(Layer* layer, FlattenState* state) {
  // Flex containers allocate both axes of included children; capture the mode before it
  // is cleared so the children can pin their resolved sizes.
  bool parentIsFlex = state->parentFlexAxis != FlexAxis::None;
  auto flexAxis =
      layer->layout == LayoutMode::Horizontal
          ? FlexAxis::Horizontal
          : (layer->layout == LayoutMode::Vertical ? FlexAxis::Vertical : FlexAxis::None);
  auto position = layer->renderPosition();
  layer->x = position.x;
  layer->y = position.y;
  PreserveExplicitSize(layer, parentIsFlex && layer->includeInLayout);
  ClearConstraints(layer);
  layer->layout = LayoutMode::None;
  layer->gap = 0;
  layer->flex = 0;
  layer->padding = {};
  layer->alignment = Alignment::Stretch;
  layer->arrangement = Arrangement::Start;
  layer->includeInLayout = true;
  // clipToBounds only exists as a layout input: updateScrollRect() has already captured the
  // resolved size into scrollRect during applyLayout, and the XML exporter emits scrollRect
  // once clipToBounds is false (PAGXExporter.cpp).
  layer->clipToBounds = false;
  FlattenElements(layer->contents, state);
  auto savedFlexAxis = state->parentFlexAxis;
  state->parentFlexAxis = flexAxis;
  FlattenLayers(layer->children, state);
  state->parentFlexAxis = savedFlexAxis;
}

void FlattenRectangle(Rectangle* rectangle) {
  auto position = rectangle->renderPosition();
  auto size = rectangle->renderSize();
  rectangle->position = position;
  rectangle->size = size;
  rectangle->width = NAN;
  rectangle->height = NAN;
  ClearConstraints(rectangle);
}

void FlattenEllipse(Ellipse* ellipse) {
  auto position = ellipse->renderPosition();
  auto size = ellipse->renderSize();
  ellipse->position = position;
  ellipse->size = size;
  ellipse->width = NAN;
  ellipse->height = NAN;
  ClearConstraints(ellipse);
}

void FlattenPolystar(Polystar* polystar, FlattenState* state) {
  auto scale = polystar->renderScale();
  if (!IsNearlyOne(scale)) {
    // The layout scale is baked into the radii; the runtime radius writers then address the
    // final values directly, so key frames need no transformation (design doc §4.2).
    polystar->outerRadius *= scale;
    polystar->innerRadius *= scale;
    NotifyGeometryScaled(*state, polystar, scale);
  }
  polystar->position = polystar->renderPosition();
  polystar->width = NAN;
  polystar->height = NAN;
  ClearConstraints(polystar);
}

void FlattenPath(Path* path, FlattenState* state) {
  if (path->data != nullptr) {
    auto scale = path->renderScale();
    if (!IsNearlyOne(scale)) {
      BakeScaleIntoPathData(path->data, scale, 0, 0);
      NotifyGeometryScaled(*state, path, scale);
    }
    // Computed after baking: with the scale inside the data, renderPosition collapses to the
    // plain path-origin offset the re-layout reproduces.
    path->position = path->renderPosition();
  }
  path->width = NAN;
  path->height = NAN;
  ClearConstraints(path);
}

void FlattenTextPath(TextPath* textPath, FlattenState* state) {
  if (textPath->path != nullptr) {
    auto scale = textPath->renderScale();
    auto position = textPath->renderPosition();
    // TextPath has no position field of its own (it is constraint-positioned only), so the
    // resolved offset has to be baked into the path data together with the scale.
    if (!IsNearlyOne(scale) || position.x != 0 || position.y != 0) {
      BakeScaleIntoPathData(textPath->path, scale, position.x, position.y);
      textPath->baselineOrigin.x *= scale;
      textPath->baselineOrigin.y *= scale;
      textPath->firstMargin *= scale;
      textPath->lastMargin *= scale;
      NotifyGeometryScaled(*state, textPath, scale);
    }
  }
  textPath->width = NAN;
  textPath->height = NAN;
  ClearConstraints(textPath);
}

void FlattenStandaloneText(Text* text, FlattenState* state) {
  auto bounds = text->layoutBounds();
  auto scale = text->fontSize > 0.0f ? text->renderFontSize() / text->fontSize : 1.0f;
  if (!IsNearlyOne(scale)) {
    // The fit scale must survive the round trip as authored width/height so the re-layout
    // re-derives it. The position is solved backwards through renderPosition(): re-layout
    // computes renderPosition as position + naturalBounds.xy (the scale is 1 at measure
    // time), so subtracting the natural bounds here pins the rendered origin in place.
    // The natural bounds are re-measured with the same font context; TextLayout::Layout is
    // side-effect free (Text applies the result itself in onMeasure/setLayoutSize).
    auto params = MakeStandaloneParams(text);
    params.textScale = 1.0f;
    auto result = TextLayout::Layout({{text, MakeGlyphParams(text)}}, params, state->context);
    text->position = {bounds.x - result.bounds.x, bounds.y - result.bounds.y};
    text->width = bounds.width;
    text->height = bounds.height;
  } else {
    text->position = text->renderPosition();
    text->width = NAN;
    text->height = NAN;
  }
  ClearConstraints(text);
}

void FlattenGroup(Group* group, FlattenState* state) {
  group->position = group->renderPosition();
  PreserveExplicitSize(group, false);
  ClearConstraints(group);
  // Group padding only insets the child constraint reference frame; the children carry
  // resolved positions now (design doc §4.6).
  group->padding = {};
  FlattenElements(group->elements, state);
}

void FlattenTextBox(TextBox* textBox) {
  textBox->position = textBox->renderPosition();
  // An axis with size-assignment inputs (two-edge constraints or percentages) is typeset
  // from the RESOLVED box size in updateLayout() (TextBox.cpp: layoutWidth/layoutHeight minus
  // padding), which also overrides an authored dimension. The resolved value must therefore
  // be written back explicitly, over the authored one when present. Axes without assignment
  // inputs keep the authored value (or NaN for content-driven) and re-derive identically,
  // because the final typeset used the authored/preferred box for those.
  auto bounds = textBox->layoutBounds();
  bool widthAssigned = !std::isnan(textBox->percentWidth) ||
                       (!std::isnan(textBox->left) && !std::isnan(textBox->right));
  bool heightAssigned = !std::isnan(textBox->percentHeight) ||
                        (!std::isnan(textBox->top) && !std::isnan(textBox->bottom));
  if (widthAssigned && !std::isnan(bounds.width)) {
    textBox->width = bounds.width;
  }
  if (heightAssigned && !std::isnan(bounds.height)) {
    textBox->height = bounds.height;
  }
  ClearConstraints(textBox);
  // Typography inputs (authored width/height, padding, alignments) stay: the box re-typesets
  // at load and the constraint-assigned sizes above are now explicit. The elements subtree is
  // left untouched: the texts are positioned by typesetting.
}

void FlattenElement(Element* element, FlattenState* state) {
  switch (element->nodeType()) {
    case NodeType::Rectangle:
      FlattenRectangle(static_cast<Rectangle*>(element));
      break;
    case NodeType::Ellipse:
      FlattenEllipse(static_cast<Ellipse*>(element));
      break;
    case NodeType::Polystar:
      FlattenPolystar(static_cast<Polystar*>(element), state);
      break;
    case NodeType::Path:
      FlattenPath(static_cast<Path*>(element), state);
      break;
    case NodeType::TextPath:
      FlattenTextPath(static_cast<TextPath*>(element), state);
      break;
    case NodeType::Text:
      FlattenStandaloneText(static_cast<Text*>(element), state);
      break;
    case NodeType::Group:
      FlattenGroup(static_cast<Group*>(element), state);
      break;
    case NodeType::TextBox:
      FlattenTextBox(static_cast<TextBox*>(element));
      break;
    default:
      // Painters, styles, filters, and modifiers are not layout nodes.
      break;
  }
}

void FlattenLayers(const std::vector<Layer*>& layers, FlattenState* state) {
  for (auto* layer : layers) {
    FlattenLayer(layer, state);
  }
}
}  // namespace

void LayoutFlattener::Flatten(PAGXDocument* document, const FlattenOptions& options) {
  if (document == nullptr) {
    return;
  }
  // applyLayout() is idempotent for an already flat tree, and the flattener must never read
  // stale resolved values, so an un-applied document is laid out with its current fonts.
  if (!document->isLayoutApplied()) {
    document->applyLayout();
  }
  LayoutContext context(&document->fontConfig());
  auto state = FlattenState{options, &context};
  FlattenLayers(document->layers, &state);
  for (auto& node : document->nodes) {
    if (node->nodeType() == NodeType::Composition) {
      auto* composition = static_cast<Composition*>(node.get());
      FlattenLayers(composition->layers, &state);
    }
  }
}

bool LayoutFlattener::IsLayoutOnlyChannel(NodeType type, const std::string& channel) {
  return ChannelExists(type, channel) && RequiresLayout(type, channel) &&
         !IsAnimatableChannel(type, channel);
}

namespace {
// DataBind targets keep their leading '@' in the field (the runtime strips it at
// resolution, DataBindRuntime.cpp); AnimationObject targets are bare ids. Accept both.
static std::string ResolveTargetId(const std::string& target) {
  if (!target.empty() && target[0] == '@') {
    return target.substr(1);
  }
  return target;
}

void CollectFromAnimations(PAGXDocument* document, const std::string& scope,
                           const std::vector<Node*>& animations,
                           std::vector<PAGExportIssue>* issues) {
  for (auto* animationNode : animations) {
    if (animationNode == nullptr || animationNode->nodeType() != NodeType::Animation) {
      continue;
    }
    auto* animation = static_cast<Animation*>(animationNode);
    for (auto* object : animation->objects) {
      auto* target = document->findNode(ResolveTargetId(object->target));
      if (target == nullptr) {
        continue;
      }
      for (auto* channel : object->channels) {
        if (!LayoutFlattener::IsLayoutOnlyChannel(target->nodeType(), channel->name)) {
          continue;
        }
        PAGExportIssue issue = {};
        issue.code = PAGExportWarning::LayoutChannelDropped;
        issue.scope = scope;
        issue.nodePath = target->id;
        issue.message = "animation channel '" + channel->name + "' on node '" + target->id +
                        "' is layout-only and cannot be animated; dropped";
        issues->push_back(issue);
      }
    }
  }
}

void CollectFromDataBinds(PAGXDocument* document, const std::string& scope,
                          const std::vector<DataBind*>& dataBinds,
                          std::vector<PAGExportIssue>* issues) {
  for (auto* bind : dataBinds) {
    auto* target = document->findNode(ResolveTargetId(bind->target));
    if (target == nullptr ||
        !LayoutFlattener::IsLayoutOnlyChannel(target->nodeType(), bind->channel)) {
      continue;
    }
    PAGExportIssue issue = {};
    issue.code = PAGExportWarning::LayoutChannelDropped;
    issue.scope = scope;
    issue.nodePath = target->id;
    issue.message = "data bind channel '" + bind->channel + "' on node '" + target->id +
                    "' is layout-only and cannot be bound; dropped";
    issues->push_back(issue);
  }
}
}  // namespace

void LayoutFlattener::CollectLayoutChannelIssues(PAGXDocument* document, const std::string& scope,
                                                 std::vector<PAGExportIssue>* issues) {
  if (document == nullptr || issues == nullptr) {
    return;
  }
  CollectFromAnimations(document, scope, document->animations, issues);
  CollectFromDataBinds(document, scope, document->dataBinds, issues);
  for (auto& node : document->nodes) {
    if (node->nodeType() != NodeType::Composition) {
      continue;
    }
    auto* composition = static_cast<Composition*>(node.get());
    CollectFromAnimations(document, scope, composition->animations, issues);
    CollectFromDataBinds(document, scope, composition->dataBinds, issues);
  }
}
}  // namespace pagx
