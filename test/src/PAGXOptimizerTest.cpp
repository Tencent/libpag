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

}  // namespace pag
