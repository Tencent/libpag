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

#include "base/PAGTest.h"
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXOptimizer.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"

namespace pag {

using pagx::Color;
using pagx::Composition;
using pagx::Element;
using pagx::Ellipse;
using pagx::Fill;
using pagx::Group;
using pagx::Layer;
using pagx::MaskType;
using pagx::NodeType;
using pagx::PAGXDocument;
using pagx::PAGXOptimizer;
using pagx::Path;
using pagx::PathData;
using pagx::Rectangle;
using pagx::SolidColor;

// ---------------------------------------------------------------------------
// Builders kept tiny so each test stays focused on the rule under inspection.
// ---------------------------------------------------------------------------

static Fill* MakeSolidFill(PAGXDocument* doc, float r, float g, float b, float a = 1.0f) {
  auto* fill = doc->makeNode<Fill>();
  auto* color = doc->makeNode<SolidColor>();
  color->color = {r, g, b, a};
  fill->color = color;
  return fill;
}

static Group* MakeFilledRectGroup(PAGXDocument* doc, float w, float h, float r, float g, float b) {
  auto* group = doc->makeNode<Group>();
  auto* rect = doc->makeNode<Rectangle>();
  rect->position = {w / 2.0f, h / 2.0f};
  rect->size = {w, h};
  group->elements.push_back(rect);
  group->elements.push_back(MakeSolidFill(doc, r, g, b));
  return group;
}

static Layer* AddTopLayer(PAGXDocument* doc) {
  auto* layer = doc->makeNode<Layer>();
  doc->layers.push_back(layer);
  return layer;
}

// ---------------------------------------------------------------------------
// PruneEmpty
// ---------------------------------------------------------------------------

CLI_TEST(PAGXOptimizerTest, PruneEmptyLayer) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* good = AddTopLayer(doc.get());
  good->contents.push_back(MakeFilledRectGroup(doc.get(), 50, 50, 1, 0, 0));
  AddTopLayer(doc.get());  // empty
  AddTopLayer(doc.get());  // empty

  PAGXOptimizer::Optimize(doc.get());
  ASSERT_EQ(doc->layers.size(), 1u);
  EXPECT_EQ(doc->layers[0], good);
}

CLI_TEST(PAGXOptimizerTest, PruneEmptyGroup) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = AddTopLayer(doc.get());
  auto* emptyGroup = doc->makeNode<Group>();
  layer->contents.push_back(emptyGroup);
  // Two siblings keep the wrapper group from being unwrapped (painter scope must stay isolated),
  // so the only structural change should be removal of `emptyGroup`.
  layer->contents.push_back(MakeFilledRectGroup(doc.get(), 50, 50, 1, 0, 0));
  layer->contents.push_back(MakeFilledRectGroup(doc.get(), 30, 30, 0, 1, 0));

  PAGXOptimizer::Optimize(doc.get());
  ASSERT_EQ(doc->layers.size(), 1u);
  ASSERT_EQ(doc->layers[0]->contents.size(), 2u);
  EXPECT_EQ(doc->layers[0]->contents[0]->nodeType(), NodeType::Group);
}

CLI_TEST(PAGXOptimizerTest, PruneEmptyDoesNotRemoveMaskTarget) {
  auto doc = PAGXDocument::Make(100, 100);
  // Use an Ellipse so the rect-mask -> scrollRect conversion does not apply: the mask reference
  // must then be preserved verbatim.
  auto* maskLayer = AddTopLayer(doc.get());
  auto* maskEllipse = doc->makeNode<Ellipse>();
  maskEllipse->position = {25, 25};
  maskEllipse->size = {50, 50};
  maskLayer->contents.push_back(maskEllipse);
  maskLayer->contents.push_back(MakeSolidFill(doc.get(), 1, 1, 1));

  auto* user = AddTopLayer(doc.get());
  user->mask = maskLayer;
  user->maskType = MaskType::Alpha;
  user->contents.push_back(MakeFilledRectGroup(doc.get(), 50, 50, 0, 0, 1));

  PAGXOptimizer::Optimize(doc.get());
  bool maskKept = false;
  for (auto* l : doc->layers) {
    if (l == maskLayer) maskKept = true;
  }
  EXPECT_TRUE(maskKept);
  EXPECT_EQ(user->mask, maskLayer);
}

// ---------------------------------------------------------------------------
// CanonicalizePaths
// ---------------------------------------------------------------------------

CLI_TEST(PAGXOptimizerTest, CanonicalizePathToRectangle) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = AddTopLayer(doc.get());
  auto* group = doc->makeNode<Group>();
  auto* path = doc->makeNode<Path>();
  auto* data = doc->makeNode<PathData>();
  data->moveTo(10, 10);
  data->lineTo(40, 10);
  data->lineTo(40, 30);
  data->lineTo(10, 30);
  data->close();
  path->data = data;
  group->elements.push_back(path);
  group->elements.push_back(MakeSolidFill(doc.get(), 1, 0, 0));
  layer->contents.push_back(group);

  PAGXOptimizer::Optimize(doc.get());
  ASSERT_EQ(doc->layers.size(), 1u);
  ASSERT_FALSE(doc->layers[0]->contents.empty());
  // The wrapper Group may be unwrapped (single child, no painter leak risk), so look across both
  // possible representations: contents = [Rect, Fill] OR contents = [Group{Rect, Fill}].
  bool foundRectangle = false;
  for (auto* el : doc->layers[0]->contents) {
    if (el->nodeType() == NodeType::Rectangle) {
      foundRectangle = true;
      break;
    }
    if (el->nodeType() == NodeType::Group) {
      auto* g = static_cast<Group*>(el);
      for (auto* inner : g->elements) {
        if (inner->nodeType() == NodeType::Rectangle) {
          foundRectangle = true;
          break;
        }
      }
    }
  }
  EXPECT_TRUE(foundRectangle);
}

// ---------------------------------------------------------------------------
// Rect mask -> scrollRect
// ---------------------------------------------------------------------------

CLI_TEST(PAGXOptimizerTest, RectMaskToScrollRect) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* mask = AddTopLayer(doc.get());
  auto* maskRect = doc->makeNode<Rectangle>();
  maskRect->position = {25, 25};
  maskRect->size = {50, 50};
  mask->contents.push_back(maskRect);
  mask->contents.push_back(MakeSolidFill(doc.get(), 1, 1, 1));

  auto* user = AddTopLayer(doc.get());
  user->mask = mask;
  user->maskType = MaskType::Alpha;
  user->contents.push_back(MakeFilledRectGroup(doc.get(), 100, 100, 0, 1, 0));

  PAGXOptimizer::Optimize(doc.get());

  EXPECT_TRUE(user->hasScrollRect);
  EXPECT_EQ(user->mask, nullptr);
  EXPECT_FLOAT_EQ(user->scrollRect.x, 0.0f);
  EXPECT_FLOAT_EQ(user->scrollRect.y, 0.0f);
  EXPECT_FLOAT_EQ(user->scrollRect.width, 50.0f);
  EXPECT_FLOAT_EQ(user->scrollRect.height, 50.0f);
}

CLI_TEST(PAGXOptimizerTest, RectMaskWithRoundnessIsKept) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* mask = AddTopLayer(doc.get());
  auto* maskRect = doc->makeNode<Rectangle>();
  maskRect->position = {25, 25};
  maskRect->size = {50, 50};
  maskRect->roundness = 8;
  mask->contents.push_back(maskRect);
  mask->contents.push_back(MakeSolidFill(doc.get(), 1, 1, 1));

  auto* user = AddTopLayer(doc.get());
  user->mask = mask;
  user->maskType = MaskType::Alpha;
  user->contents.push_back(MakeFilledRectGroup(doc.get(), 100, 100, 0, 1, 0));

  PAGXOptimizer::Optimize(doc.get());

  EXPECT_FALSE(user->hasScrollRect);
  EXPECT_EQ(user->mask, mask);
}

// ---------------------------------------------------------------------------
// MergeAdjacentGroups (identical painters)
// ---------------------------------------------------------------------------

CLI_TEST(PAGXOptimizerTest, MergeAdjacentGroupsIdenticalSolidColor) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = AddTopLayer(doc.get());

  layer->contents.push_back(MakeFilledRectGroup(doc.get(), 10, 10, 1, 0, 0));
  layer->contents.push_back(MakeFilledRectGroup(doc.get(), 12, 12, 1, 0, 0));
  layer->contents.push_back(MakeFilledRectGroup(doc.get(), 14, 14, 0, 0, 1));

  PAGXOptimizer::Optimize(doc.get());

  ASSERT_EQ(doc->layers.size(), 1u);
  // Two reds collapse into one Group with both rects + a single Fill; blue stays separate.
  ASSERT_EQ(doc->layers[0]->contents.size(), 2u);
}

CLI_TEST(PAGXOptimizerTest, MergeDoesNotCollapseDifferentColors) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = AddTopLayer(doc.get());
  layer->contents.push_back(MakeFilledRectGroup(doc.get(), 10, 10, 1, 0, 0));
  layer->contents.push_back(MakeFilledRectGroup(doc.get(), 12, 12, 0, 1, 0));

  PAGXOptimizer::Optimize(doc.get());
  ASSERT_EQ(doc->layers[0]->contents.size(), 2u);
}

// ---------------------------------------------------------------------------
// UnwrapRedundantFirstChildGroup
// ---------------------------------------------------------------------------

CLI_TEST(PAGXOptimizerTest, UnwrapRedundantFirstChildGroup) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = AddTopLayer(doc.get());
  auto* outer = doc->makeNode<Group>();
  auto* rect = doc->makeNode<Rectangle>();
  rect->position = {25, 25};
  rect->size = {50, 50};
  outer->elements.push_back(rect);
  outer->elements.push_back(MakeSolidFill(doc.get(), 1, 0, 0));
  layer->contents.push_back(outer);

  PAGXOptimizer::Optimize(doc.get());
  // The wrapper group is unwrapped because there are no following siblings to leak onto.
  ASSERT_EQ(doc->layers[0]->contents.size(), 2u);
  EXPECT_EQ(doc->layers[0]->contents[0]->nodeType(), NodeType::Rectangle);
}

CLI_TEST(PAGXOptimizerTest, DoesNotUnwrapWhenSiblingsWouldLeakPaint) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = AddTopLayer(doc.get());
  layer->contents.push_back(MakeFilledRectGroup(doc.get(), 10, 10, 1, 0, 0));
  layer->contents.push_back(MakeFilledRectGroup(doc.get(), 20, 20, 0, 0, 1));

  PAGXOptimizer::Optimize(doc.get());
  // Two distinct painters → cannot unwrap either group without re-coloring siblings.
  ASSERT_EQ(doc->layers[0]->contents.size(), 2u);
}

// ---------------------------------------------------------------------------
// MergeAdjacentShellLayers
// ---------------------------------------------------------------------------

CLI_TEST(PAGXOptimizerTest, MergeAdjacentShellSiblings) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* a = AddTopLayer(doc.get());
  a->contents.push_back(MakeFilledRectGroup(doc.get(), 10, 10, 1, 0, 0));
  auto* b = AddTopLayer(doc.get());
  b->contents.push_back(MakeFilledRectGroup(doc.get(), 12, 12, 0, 1, 0));
  auto* c = AddTopLayer(doc.get());
  c->contents.push_back(MakeFilledRectGroup(doc.get(), 14, 14, 0, 0, 1));

  PAGXOptimizer::Optimize(doc.get());

  // Three shell layers collapse into one layer holding three groups (one per source).
  ASSERT_EQ(doc->layers.size(), 1u);
  EXPECT_EQ(doc->layers[0]->contents.size(), 3u);
  EXPECT_TRUE(doc->layers[0]->children.empty());
}

CLI_TEST(PAGXOptimizerTest, DoesNotMergeWhenLayerHasOpacity) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* a = AddTopLayer(doc.get());
  a->contents.push_back(MakeFilledRectGroup(doc.get(), 10, 10, 1, 0, 0));
  auto* b = AddTopLayer(doc.get());
  b->alpha = 0.5f;
  b->contents.push_back(MakeFilledRectGroup(doc.get(), 12, 12, 0, 1, 0));

  PAGXOptimizer::Optimize(doc.get());
  EXPECT_EQ(doc->layers.size(), 2u);
}

// ---------------------------------------------------------------------------
// CustomData / id / mask references must be preserved
// ---------------------------------------------------------------------------

CLI_TEST(PAGXOptimizerTest, PreservesCustomDataOnGroup) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = AddTopLayer(doc.get());
  auto* outer = MakeFilledRectGroup(doc.get(), 50, 50, 1, 0, 0);
  outer->customData["data-tag"] = "icon";
  layer->contents.push_back(outer);

  PAGXOptimizer::Optimize(doc.get());

  // The Group must NOT be unwrapped: it carries metadata.
  ASSERT_EQ(doc->layers.size(), 1u);
  ASSERT_EQ(doc->layers[0]->contents.size(), 1u);
  ASSERT_EQ(doc->layers[0]->contents[0]->nodeType(), NodeType::Group);
  auto* g = static_cast<Group*>(doc->layers[0]->contents[0]);
  EXPECT_EQ(g->customData.count("data-tag"), 1u);
}

// ---------------------------------------------------------------------------
// Idempotence — optimizing twice yields the same structure as optimizing once.
// ---------------------------------------------------------------------------

CLI_TEST(PAGXOptimizerTest, OptimizeIsIdempotent) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = AddTopLayer(doc.get());
  layer->contents.push_back(MakeFilledRectGroup(doc.get(), 10, 10, 1, 0, 0));
  layer->contents.push_back(MakeFilledRectGroup(doc.get(), 12, 12, 1, 0, 0));
  AddTopLayer(doc.get());  // empty; will be pruned

  PAGXOptimizer::Optimize(doc.get());
  size_t topCount = doc->layers.size();
  size_t firstContents = doc->layers[0]->contents.size();

  PAGXOptimizer::Optimize(doc.get());
  EXPECT_EQ(doc->layers.size(), topCount);
  EXPECT_EQ(doc->layers[0]->contents.size(), firstContents);
}

// ---------------------------------------------------------------------------
// Options toggle matrix: every bool must have an on/off pair so a regression
// that accidentally short-circuits a switch gets caught by CI.
// ---------------------------------------------------------------------------

// A shell Layer whose only job is to hold a single filled rect group. Two of these stacked as top
// level siblings exercise pruneEmpty (empty shell disappears), downgradeShellChildren (shell child
// folds into parent), mergeAdjacentShellLayers (consecutive shells merge), and the wrap/unwrap
// rules.
static Layer* AddShellLayerWithFill(PAGXDocument* doc, float w, float h, float r, float g,
                                    float b) {
  auto* layer = AddTopLayer(doc);
  layer->contents.push_back(MakeFilledRectGroup(doc, w, h, r, g, b));
  return layer;
}

CLI_TEST(PAGXOptimizerTest, PruneEmptyDisabledKeepsEmptyLayer) {
  auto doc = PAGXDocument::Make(100, 100);
  AddShellLayerWithFill(doc.get(), 20, 20, 1, 0, 0);
  AddTopLayer(doc.get());  // empty

  PAGXOptimizer::Options options;
  options.pruneEmpty = false;
  options.mergeAdjacentShellLayers = false;
  options.downgradeShellChildren = false;
  PAGXOptimizer::Optimize(doc.get(), options);
  EXPECT_EQ(doc->layers.size(), 2u);
}

CLI_TEST(PAGXOptimizerTest, DowngradeShellChildrenEnabled) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* parent = AddTopLayer(doc.get());
  auto* childA = doc->makeNode<Layer>();
  childA->contents.push_back(MakeFilledRectGroup(doc.get(), 20, 20, 1, 0, 0));
  auto* childB = doc->makeNode<Layer>();
  childB->contents.push_back(MakeFilledRectGroup(doc.get(), 30, 30, 0, 1, 0));
  parent->children.push_back(childA);
  parent->children.push_back(childB);

  PAGXOptimizer::Optimize(doc.get());
  EXPECT_TRUE(parent->children.empty());
  EXPECT_FALSE(parent->contents.empty());
}

CLI_TEST(PAGXOptimizerTest, DowngradeShellChildrenDisabled) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* parent = AddTopLayer(doc.get());
  auto* childA = doc->makeNode<Layer>();
  childA->contents.push_back(MakeFilledRectGroup(doc.get(), 20, 20, 1, 0, 0));
  auto* childB = doc->makeNode<Layer>();
  childB->contents.push_back(MakeFilledRectGroup(doc.get(), 30, 30, 0, 1, 0));
  parent->children.push_back(childA);
  parent->children.push_back(childB);

  PAGXOptimizer::Options options;
  options.downgradeShellChildren = false;
  // mergeAdjacentShellLayers would otherwise collapse the two children into a single sibling —
  // disabling it here keeps the test focused on the downgrade toggle.
  options.mergeAdjacentShellLayers = false;
  PAGXOptimizer::Optimize(doc.get(), options);
  EXPECT_EQ(parent->children.size(), 2u);
  EXPECT_TRUE(parent->contents.empty());
}

CLI_TEST(PAGXOptimizerTest, MergeAdjacentShellLayersDisabled) {
  auto doc = PAGXDocument::Make(100, 100);
  AddShellLayerWithFill(doc.get(), 10, 10, 1, 0, 0);
  AddShellLayerWithFill(doc.get(), 12, 12, 0, 1, 0);
  AddShellLayerWithFill(doc.get(), 14, 14, 0, 0, 1);

  PAGXOptimizer::Options options;
  options.mergeAdjacentShellLayers = false;
  PAGXOptimizer::Optimize(doc.get(), options);
  EXPECT_EQ(doc->layers.size(), 3u);
}

CLI_TEST(PAGXOptimizerTest, UnwrapRedundantFirstGroupDisabled) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = AddTopLayer(doc.get());
  auto* outer = MakeFilledRectGroup(doc.get(), 50, 50, 1, 0, 0);
  layer->contents.push_back(outer);

  PAGXOptimizer::Options options;
  options.unwrapRedundantFirstGroup = false;
  PAGXOptimizer::Optimize(doc.get(), options);
  ASSERT_EQ(layer->contents.size(), 1u);
  EXPECT_EQ(layer->contents[0]->nodeType(), NodeType::Group);
}

CLI_TEST(PAGXOptimizerTest, MergeAdjacentGroupsDisabled) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = AddTopLayer(doc.get());
  layer->contents.push_back(MakeFilledRectGroup(doc.get(), 10, 10, 1, 0, 0));
  layer->contents.push_back(MakeFilledRectGroup(doc.get(), 12, 12, 1, 0, 0));

  PAGXOptimizer::Options options;
  options.mergeAdjacentGroups = false;
  options.unwrapRedundantFirstGroup = false;
  PAGXOptimizer::Optimize(doc.get(), options);
  EXPECT_EQ(layer->contents.size(), 2u);
}

CLI_TEST(PAGXOptimizerTest, CanonicalizePathsDisabled) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = AddTopLayer(doc.get());
  auto* group = doc->makeNode<Group>();
  auto* path = doc->makeNode<Path>();
  auto* data = doc->makeNode<PathData>();
  data->moveTo(10, 10);
  data->lineTo(40, 10);
  data->lineTo(40, 30);
  data->lineTo(10, 30);
  data->close();
  path->data = data;
  group->elements.push_back(path);
  group->elements.push_back(MakeSolidFill(doc.get(), 1, 0, 0));
  layer->contents.push_back(group);

  PAGXOptimizer::Options options;
  options.canonicalizePaths = false;
  options.rectMaskToScrollRect = false;
  options.unwrapRedundantFirstGroup = false;
  PAGXOptimizer::Optimize(doc.get(), options);
  ASSERT_EQ(layer->contents.size(), 1u);
  auto* outGroup = static_cast<Group*>(layer->contents[0]);
  bool hasRectangle = false;
  for (auto* el : outGroup->elements) {
    if (el->nodeType() == NodeType::Rectangle) hasRectangle = true;
  }
  EXPECT_FALSE(hasRectangle);
}

CLI_TEST(PAGXOptimizerTest, RectMaskToScrollRectDisabled) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* mask = AddTopLayer(doc.get());
  auto* maskRect = doc->makeNode<Rectangle>();
  maskRect->position = {25, 25};
  maskRect->size = {50, 50};
  mask->contents.push_back(maskRect);
  mask->contents.push_back(MakeSolidFill(doc.get(), 1, 1, 1));

  auto* user = AddTopLayer(doc.get());
  user->mask = mask;
  user->maskType = MaskType::Alpha;
  user->contents.push_back(MakeFilledRectGroup(doc.get(), 100, 100, 0, 1, 0));

  PAGXOptimizer::Options options;
  options.rectMaskToScrollRect = false;
  PAGXOptimizer::Optimize(doc.get(), options);
  EXPECT_FALSE(user->hasScrollRect);
  EXPECT_EQ(user->mask, mask);
}

CLI_TEST(PAGXOptimizerTest, DedupPathDataEnabledCollapsesDuplicates) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = AddTopLayer(doc.get());

  auto* data1 = doc->makeNode<PathData>("p1");
  data1->moveTo(0, 0);
  data1->lineTo(20, 0);
  data1->lineTo(20, 10);
  data1->lineTo(0, 10);
  data1->close();
  auto* data2 = doc->makeNode<PathData>("p2");
  data2->moveTo(0, 0);
  data2->lineTo(20, 0);
  data2->lineTo(20, 10);
  data2->lineTo(0, 10);
  data2->close();

  auto* g1 = doc->makeNode<Group>();
  auto* p1 = doc->makeNode<Path>();
  p1->data = data1;
  g1->elements.push_back(p1);
  g1->elements.push_back(MakeSolidFill(doc.get(), 1, 0, 0));
  layer->contents.push_back(g1);

  auto* g2 = doc->makeNode<Group>();
  auto* p2 = doc->makeNode<Path>();
  p2->data = data2;
  g2->elements.push_back(p2);
  g2->elements.push_back(MakeSolidFill(doc.get(), 0, 0, 1));
  layer->contents.push_back(g2);

  size_t beforePathData = 0;
  for (auto& n : doc->nodes) {
    if (n->nodeType() == NodeType::PathData) beforePathData++;
  }

  PAGXOptimizer::Options options;
  options.dedupPathData = true;
  options.pruneUnreferencedResources = true;
  options.canonicalizePaths = false;
  options.unwrapRedundantFirstGroup = false;
  options.mergeAdjacentGroups = false;
  PAGXOptimizer::Optimize(doc.get(), options);

  // After dedup, exactly one of the two PathData resources loses its id (the prune pass clears
  // the id of any resource that is no longer referenced).
  size_t withId = 0;
  for (auto& n : doc->nodes) {
    if (n->nodeType() == NodeType::PathData && !n->id.empty()) withId++;
  }
  EXPECT_EQ(beforePathData, 2u);
  EXPECT_EQ(withId, 1u);
}

CLI_TEST(PAGXOptimizerTest, DedupPathDataDisabledKeepsDuplicates) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = AddTopLayer(doc.get());

  auto* data1 = doc->makeNode<PathData>("p1");
  data1->moveTo(0, 0);
  data1->lineTo(20, 0);
  data1->lineTo(20, 10);
  data1->lineTo(0, 10);
  data1->close();
  auto* data2 = doc->makeNode<PathData>("p2");
  data2->moveTo(0, 0);
  data2->lineTo(20, 0);
  data2->lineTo(20, 10);
  data2->lineTo(0, 10);
  data2->close();

  auto* g1 = doc->makeNode<Group>();
  auto* p1 = doc->makeNode<Path>();
  p1->data = data1;
  g1->elements.push_back(p1);
  g1->elements.push_back(MakeSolidFill(doc.get(), 1, 0, 0));
  layer->contents.push_back(g1);

  auto* g2 = doc->makeNode<Group>();
  auto* p2 = doc->makeNode<Path>();
  p2->data = data2;
  g2->elements.push_back(p2);
  g2->elements.push_back(MakeSolidFill(doc.get(), 0, 0, 1));
  layer->contents.push_back(g2);

  size_t beforePathData = 0;
  for (auto& n : doc->nodes) {
    if (n->nodeType() == NodeType::PathData) beforePathData++;
  }

  PAGXOptimizer::Options options;
  options.dedupPathData = false;
  options.pruneUnreferencedResources = false;
  options.canonicalizePaths = false;
  PAGXOptimizer::Optimize(doc.get(), options);

  size_t afterPathData = 0;
  for (auto& n : doc->nodes) {
    if (n->nodeType() == NodeType::PathData) afterPathData++;
  }
  EXPECT_EQ(afterPathData, beforePathData);
}

CLI_TEST(PAGXOptimizerTest, PruneUnreferencedResourcesEnabledClearsId) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = AddTopLayer(doc.get());
  layer->contents.push_back(MakeFilledRectGroup(doc.get(), 20, 20, 1, 0, 0));

  auto* orphan = doc->makeNode<PathData>("orphan-path");
  orphan->moveTo(0, 0);
  orphan->lineTo(5, 0);
  orphan->lineTo(5, 5);
  orphan->lineTo(0, 5);
  orphan->close();

  PAGXOptimizer::Options options;
  options.pruneUnreferencedResources = true;
  PAGXOptimizer::Optimize(doc.get(), options);
  EXPECT_TRUE(orphan->id.empty());
}

CLI_TEST(PAGXOptimizerTest, PruneUnreferencedResourcesDisabledKeepsId) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = AddTopLayer(doc.get());
  layer->contents.push_back(MakeFilledRectGroup(doc.get(), 20, 20, 1, 0, 0));

  auto* orphan = doc->makeNode<PathData>("orphan-path");
  orphan->moveTo(0, 0);
  orphan->lineTo(5, 0);
  orphan->lineTo(5, 5);
  orphan->lineTo(0, 5);
  orphan->close();

  PAGXOptimizer::Options options;
  options.pruneUnreferencedResources = false;
  PAGXOptimizer::Optimize(doc.get(), options);
  EXPECT_EQ(orphan->id, "orphan-path");
}

// ---------------------------------------------------------------------------
// Edge cases requested in review M4.
// ---------------------------------------------------------------------------

// Composition-owned layer lists must also be optimized (the rule loop runs on both the document
// root layers and each Composition's layers).
CLI_TEST(PAGXOptimizerTest, OptimizesCompositionInternalLayers) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* comp = doc->makeNode<Composition>("comp1");
  comp->width = 50;
  comp->height = 50;
  auto* goodLayer = doc->makeNode<Layer>();
  goodLayer->contents.push_back(MakeFilledRectGroup(doc.get(), 20, 20, 1, 0, 0));
  comp->layers.push_back(goodLayer);
  comp->layers.push_back(doc->makeNode<Layer>());  // empty -> should be pruned

  auto* refLayer = AddTopLayer(doc.get());
  refLayer->composition = comp;

  PAGXOptimizer::Optimize(doc.get());
  EXPECT_EQ(comp->layers.size(), 1u);
  EXPECT_EQ(comp->layers[0], goodLayer);
}

// Non-Alpha masks (Luminance / Contour) cannot be collapsed to scrollRect: verify the optimizer
// leaves them alone even when the rectangular geometry would otherwise be eligible.
CLI_TEST(PAGXOptimizerTest, NonAlphaMaskIsNotCollapsedToScrollRect) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* mask = AddTopLayer(doc.get());
  auto* maskRect = doc->makeNode<Rectangle>();
  maskRect->position = {25, 25};
  maskRect->size = {50, 50};
  mask->contents.push_back(maskRect);
  mask->contents.push_back(MakeSolidFill(doc.get(), 1, 1, 1));

  auto* user = AddTopLayer(doc.get());
  user->mask = mask;
  user->maskType = MaskType::Luminance;
  user->contents.push_back(MakeFilledRectGroup(doc.get(), 100, 100, 0, 1, 0));

  PAGXOptimizer::Optimize(doc.get());

  EXPECT_FALSE(user->hasScrollRect);
  EXPECT_EQ(user->mask, mask);
}

// Layers with an unresolved import (source or inline content) are awaiting `pagx resolve` and must
// never be pruned as "empty" — pruneEmpty looks at contents/children which are empty by design.
CLI_TEST(PAGXOptimizerTest, DoesNotPruneLayerWithUnresolvedImport) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* importLayer = AddTopLayer(doc.get());
  importLayer->importDirective.source = "icon.svg";

  auto* inlineImportLayer = AddTopLayer(doc.get());
  inlineImportLayer->importDirective.content = "<svg xmlns=\"http://www.w3.org/2000/svg\"/>";

  PAGXOptimizer::Optimize(doc.get());
  ASSERT_EQ(doc->layers.size(), 2u);
  EXPECT_EQ(doc->layers[0], importLayer);
  EXPECT_EQ(doc->layers[1], inlineImportLayer);
}

// A chain of references (Path -> PathData, Fill -> SolidColor) must not be pruned — the resource
// pass walks every link rather than just top-level contents.
CLI_TEST(PAGXOptimizerTest, PruneUnreferencedResourcesPreservesChainedReferences) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = AddTopLayer(doc.get());

  auto* data = doc->makeNode<PathData>("shared-path");
  data->moveTo(0, 0);
  data->lineTo(30, 0);
  data->lineTo(30, 20);
  data->lineTo(0, 20);
  data->close();

  auto* color = doc->makeNode<SolidColor>("shared-color");
  color->color = {0.25f, 0.5f, 0.75f, 1.0f};

  auto* fill = doc->makeNode<Fill>();
  fill->color = color;

  auto* path = doc->makeNode<Path>();
  path->data = data;

  auto* group = doc->makeNode<Group>();
  group->elements.push_back(path);
  group->elements.push_back(fill);
  layer->contents.push_back(group);

  PAGXOptimizer::Options options;
  options.pruneUnreferencedResources = true;
  options.canonicalizePaths = false;
  options.unwrapRedundantFirstGroup = false;
  PAGXOptimizer::Optimize(doc.get(), options);

  EXPECT_EQ(data->id, "shared-path");
  EXPECT_EQ(color->id, "shared-color");
}

// ---------------------------------------------------------------------------
// Optimize convergence reporting (Options::maxIterations + Result).
// ---------------------------------------------------------------------------

CLI_TEST(PAGXOptimizerTest, OptimizeResultReportsConvergence) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = AddTopLayer(doc.get());
  layer->contents.push_back(MakeFilledRectGroup(doc.get(), 10, 10, 1, 0, 0));

  auto result = PAGXOptimizer::Optimize(doc.get());
  EXPECT_TRUE(result.converged);
  EXPECT_GE(result.iterationsUsed, 1);
}

CLI_TEST(PAGXOptimizerTest, OptimizeResultSignalsNonConvergenceAtIterationCap) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = AddTopLayer(doc.get());
  layer->contents.push_back(MakeFilledRectGroup(doc.get(), 10, 10, 1, 0, 0));
  AddTopLayer(doc.get());  // a second shell layer so at least one rule still has something to do

  PAGXOptimizer::Options options;
  options.maxIterations = 1;
  auto result = PAGXOptimizer::Optimize(doc.get(), options);
  EXPECT_FALSE(result.converged);
  EXPECT_EQ(result.iterationsUsed, 1);
  bool warnedAboutOptimizer = false;
  for (const auto& err : doc->errors) {
    if (err.find("PAGXOptimizer") != std::string::npos) {
      warnedAboutOptimizer = true;
      break;
    }
  }
  EXPECT_TRUE(warnedAboutOptimizer);
}

}  // namespace pag
