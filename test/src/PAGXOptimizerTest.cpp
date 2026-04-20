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

#include <functional>
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
// SplitLongPaths
// ---------------------------------------------------------------------------

namespace {

// Build a PathData that packs `count` unit squares along the x axis. Each square contributes
// 5 verbs (M, L, L, L, Z), so the total verb count is exactly `count * 5`. Squares live at
// distinct x positions so their bounding boxes never overlap — the clustering pass will therefore
// treat each square as its own standalone cluster.
PathData* MakeRepeatedSquares(PAGXDocument* doc, int count) {
  auto* data = doc->makeNode<PathData>();
  for (int i = 0; i < count; i++) {
    float x = static_cast<float>(i * 10);
    data->moveTo(x, 0);
    data->lineTo(x + 5, 0);
    data->lineTo(x + 5, 5);
    data->lineTo(x, 5);
    data->close();
  }
  return data;
}

// Glyph-with-hole geometry: a large outer square followed by a small square completely inside
// it (the "hole"). Any split must keep the hole attached to its outer, which means this path
// packs into a single cluster. With a smaller second outer+hole separated from the first
// horizontally, we expose a two-cluster layout that can be split between them but never
// between an outer and its inner hole.
PathData* MakeGlyphsWithHoles(PAGXDocument* doc, int pairCount, int extraSubpathsPerPair) {
  auto* data = doc->makeNode<PathData>();
  for (int i = 0; i < pairCount; i++) {
    float x = static_cast<float>(i * 100);
    // Outer.
    data->moveTo(x, 0);
    data->lineTo(x + 50, 0);
    data->lineTo(x + 50, 50);
    data->lineTo(x, 50);
    data->close();
    // Hole (fully inside outer).
    data->moveTo(x + 20, 20);
    data->lineTo(x + 30, 20);
    data->lineTo(x + 30, 30);
    data->lineTo(x + 20, 30);
    data->close();
    // Extra small squares inside the outer to pad up the verb count without creating new
    // clusters (they are each contained by the outer, so they attach as holes).
    for (int k = 0; k < extraSubpathsPerPair; k++) {
      float ox = x + 5 + k * 0.5f;
      float oy = 5 + k * 0.5f;
      data->moveTo(ox, oy);
      data->lineTo(ox + 1, oy);
      data->lineTo(ox + 1, oy + 1);
      data->lineTo(ox, oy + 1);
      data->close();
    }
  }
  return data;
}

int TotalVerbCount(const PathData* data) {
  return static_cast<int>(data->verbs().size());
}

// Default Options + splitLongPaths enabled. The pass is opt-in by default, so every
// SplitLongPaths* test must request it explicitly.
PAGXOptimizer::Options SplitOptions(bool enableChordSplit = false) {
  PAGXOptimizer::Options options;
  options.splitLongPaths = true;
  options.enableChordSplit = enableChordSplit;
  return options;
}

}  // namespace

CLI_TEST(PAGXOptimizerTest, SplitLongPathsLeavesSmallPathAlone) {
  auto doc = PAGXDocument::Make(200, 50);
  auto* layer = AddTopLayer(doc.get());
  auto* path = doc->makeNode<Path>();
  path->data = MakeRepeatedSquares(doc.get(), 10);  // 50 verbs — well under threshold
  layer->contents.push_back(path);
  layer->contents.push_back(MakeSolidFill(doc.get(), 1, 0, 0));

  PAGXOptimizer::Optimize(doc.get(), SplitOptions());
  ASSERT_EQ(doc->layers.size(), 1u);
  ASSERT_EQ(doc->layers[0]->contents.size(), 2u);
  EXPECT_EQ(doc->layers[0]->contents[0]->nodeType(), NodeType::Path);
}

CLI_TEST(PAGXOptimizerTest, SplitLongPathsSplitsIntoChunks) {
  auto doc = PAGXDocument::Make(2000, 50);
  auto* layer = AddTopLayer(doc.get());
  auto* path = doc->makeNode<Path>();
  // 240 squares × 5 verbs = 1200 verbs — above default threshold of 1024.
  path->data = MakeRepeatedSquares(doc.get(), 240);
  layer->contents.push_back(path);
  layer->contents.push_back(MakeSolidFill(doc.get(), 1, 0, 0));
  int originalVerbs = TotalVerbCount(path->data);

  PAGXOptimizer::Optimize(doc.get(), SplitOptions());

  ASSERT_EQ(doc->layers.size(), 1u);
  auto& contents = doc->layers[0]->contents;
  // Collect every resulting Path; the terminating Fill keeps its slot.
  std::vector<Path*> resultingPaths;
  for (auto* el : contents) {
    if (el->nodeType() == NodeType::Path) resultingPaths.push_back(static_cast<Path*>(el));
  }
  ASSERT_GE(resultingPaths.size(), 2u);
  int totalVerbs = 0;
  for (auto* p : resultingPaths) {
    ASSERT_NE(p->data, nullptr);
    int verbs = TotalVerbCount(p->data);
    EXPECT_LE(verbs, 900);  // default maxVerbsPerPath
    totalVerbs += verbs;
  }
  EXPECT_EQ(totalVerbs, originalVerbs);
}

CLI_TEST(PAGXOptimizerTest, SplitLongPathsKeepsHolesAttached) {
  auto doc = PAGXDocument::Make(2000, 50);
  auto* layer = AddTopLayer(doc.get());
  auto* path = doc->makeNode<Path>();
  // Each pair = 1 outer + 1 main hole + 20 padding holes = 22 sub-paths (110 verbs). With 10
  // pairs we sit at 1100 verbs — above the 1024 threshold and enough that the first-fit packer
  // must close the bucket at 8 pairs (880 verbs) and open a second for the remaining 2.
  path->data = MakeGlyphsWithHoles(doc.get(), 10, 20);
  int subPathsPerPair = 22;  // 1 outer + 1 main hole + 20 padding holes
  layer->contents.push_back(path);
  layer->contents.push_back(MakeSolidFill(doc.get(), 1, 0, 0));
  int originalVerbs = TotalVerbCount(path->data);

  PAGXOptimizer::Optimize(doc.get(), SplitOptions());

  std::vector<Path*> resultingPaths;
  for (auto* el : doc->layers[0]->contents) {
    if (el->nodeType() == NodeType::Path) resultingPaths.push_back(static_cast<Path*>(el));
  }
  ASSERT_GE(resultingPaths.size(), 2u);
  int totalVerbs = 0;
  for (auto* p : resultingPaths) {
    totalVerbs += TotalVerbCount(p->data);
    // Every pair contributes exactly `subPathsPerPair` Move verbs; since clusters stay intact,
    // each chunk's Move count must be a multiple of `subPathsPerPair`.
    int moveCount = 0;
    for (auto v : p->data->verbs()) {
      if (v == pagx::PathVerb::Move) moveCount++;
    }
    EXPECT_EQ(moveCount % subPathsPerPair, 0);
  }
  EXPECT_EQ(totalVerbs, originalVerbs);
}

CLI_TEST(PAGXOptimizerTest, SplitLongPathsSkipsWhenCustomDataSet) {
  auto doc = PAGXDocument::Make(2000, 50);
  auto* layer = AddTopLayer(doc.get());
  auto* path = doc->makeNode<Path>();
  path->data = MakeRepeatedSquares(doc.get(), 240);  // 1200 verbs, above threshold
  path->customData["role"] = "icon-outline";
  layer->contents.push_back(path);
  layer->contents.push_back(MakeSolidFill(doc.get(), 1, 0, 0));

  PAGXOptimizer::Optimize(doc.get(), SplitOptions());

  int pathCount = 0;
  for (auto* el : doc->layers[0]->contents) {
    if (el->nodeType() == NodeType::Path) pathCount++;
  }
  // customData must not be duplicated across splits; the path is preserved as-is.
  EXPECT_EQ(pathCount, 1);
}

CLI_TEST(PAGXOptimizerTest, SplitLongPathsSkipsWhenPathDataIsShared) {
  auto doc = PAGXDocument::Make(2000, 50);
  auto* layer = AddTopLayer(doc.get());
  // Simulate a shared PathData resource by giving it an id. Per contract of the optimizer,
  // shared resources are left untouched so other Path references stay valid.
  auto* shared = MakeRepeatedSquares(doc.get(), 240);  // 1200 verbs, above threshold
  shared->id = "shared-long-path";
  auto* path = doc->makeNode<Path>();
  path->data = shared;
  layer->contents.push_back(path);
  layer->contents.push_back(MakeSolidFill(doc.get(), 1, 0, 0));

  PAGXOptimizer::Optimize(doc.get(), SplitOptions());

  int pathCount = 0;
  for (auto* el : doc->layers[0]->contents) {
    if (el->nodeType() == NodeType::Path) pathCount++;
  }
  EXPECT_EQ(pathCount, 1);
}

// Regression: an earlier rebuild-into-result implementation lost siblings that lived before
// a Group whose deep descendant was the only thing that actually changed. Mirrors the
// in-place tree walk in `tools/optimize_svg_paths.py::_walk_and_split`.
CLI_TEST(PAGXOptimizerTest, SplitLongPathsPreservesSiblingsAroundChangedGroup) {
  auto doc = PAGXDocument::Make(2000, 50);
  auto* layer = AddTopLayer(doc.get());

  // Sibling 1 — short Path that the splitter must leave in place.
  auto* sibling1 = doc->makeNode<Path>();
  sibling1->data = MakeRepeatedSquares(doc.get(), 10);  // 50 verbs
  layer->contents.push_back(sibling1);

  // Group whose nested child contains the only oversized path.
  auto* outer = doc->makeNode<Group>();
  auto* inner = doc->makeNode<Group>();
  auto* longPath = doc->makeNode<Path>();
  longPath->data = MakeRepeatedSquares(doc.get(), 240);  // 1200 verbs — splits
  inner->elements.push_back(longPath);
  inner->elements.push_back(MakeSolidFill(doc.get(), 1, 0, 0));
  outer->elements.push_back(inner);
  layer->contents.push_back(outer);

  // Sibling 2 — short Path after the Group; must also survive.
  auto* sibling2 = doc->makeNode<Path>();
  sibling2->data = MakeRepeatedSquares(doc.get(), 10);
  layer->contents.push_back(sibling2);

  PAGXOptimizer::Optimize(doc.get(), SplitOptions());

  // Outer layer.contents should keep all three slots: the Group + both sibling Paths in
  // their original relative order. Some passes may merge fills around the Group, but the
  // Path siblings and the Group itself must be present.
  ASSERT_EQ(doc->layers.size(), 1u);
  auto& contents = doc->layers[0]->contents;
  int siblingPathCount = 0;
  bool foundGroup = false;
  for (auto* el : contents) {
    if (el->nodeType() == NodeType::Path) siblingPathCount++;
    if (el->nodeType() == NodeType::Group) foundGroup = true;
  }
  EXPECT_EQ(siblingPathCount, 2);
  EXPECT_TRUE(foundGroup);

  // The deep oversized path must have been split.
  std::function<void(Group*, std::vector<Path*>&)> collectInner = [&](Group* g,
                                                                      std::vector<Path*>& out) {
    for (auto* el : g->elements) {
      if (el->nodeType() == NodeType::Path) out.push_back(static_cast<Path*>(el));
      if (el->nodeType() == NodeType::Group) collectInner(static_cast<Group*>(el), out);
    }
  };
  std::vector<Path*> deepPaths;
  for (auto* el : contents) {
    if (el->nodeType() == NodeType::Group) collectInner(static_cast<Group*>(el), deepPaths);
  }
  EXPECT_GE(deepPaths.size(), 2u);
}

CLI_TEST(PAGXOptimizerTest, SplitLongPathsChordSplitDisabledByDefault) {
  // A single closed sub-path with > threshold verbs and no natural break points: only the
  // chord-split fallback could shrink it, and that fallback is off by default. We turn on
  // splitLongPaths to actually exercise the splitter — chord-split is the only thing that
  // could touch this path, so an unchanged path count proves the chord-split gate held.
  auto doc = PAGXDocument::Make(1000, 1000);
  auto* layer = AddTopLayer(doc.get());
  auto* data = doc->makeNode<PathData>();
  data->moveTo(0, 0);
  // 1100 cubic segments (1 Move + 1100 Cubic + 1 Close = 1102 verbs).
  for (int i = 0; i < 1100; i++) {
    float t = static_cast<float>(i + 1);
    data->cubicTo(t, 0, t, t, t, t);
  }
  data->close();
  auto* path = doc->makeNode<Path>();
  path->data = data;
  layer->contents.push_back(path);
  layer->contents.push_back(MakeSolidFill(doc.get(), 0, 0, 1));

  PAGXOptimizer::Optimize(doc.get(), SplitOptions(/*enableChordSplit=*/false));

  int pathCount = 0;
  for (auto* el : doc->layers[0]->contents) {
    if (el->nodeType() == NodeType::Path) pathCount++;
  }
  EXPECT_EQ(pathCount, 1);  // untouched — chord-split is opt-in
}

CLI_TEST(PAGXOptimizerTest, SplitLongPathsChordSplitEnabledHalvesLargeClosedSubPath) {
  auto doc = PAGXDocument::Make(1000, 1000);
  auto* layer = AddTopLayer(doc.get());
  auto* data = doc->makeNode<PathData>();
  data->moveTo(0, 0);
  // 1100 cubic segments (1 Move + 1100 Cubic + 1 Close = 1102 verbs).
  for (int i = 0; i < 1100; i++) {
    float t = static_cast<float>(i + 1);
    data->cubicTo(t, 0, t, t, t, t);
  }
  data->close();
  auto* path = doc->makeNode<Path>();
  path->data = data;
  layer->contents.push_back(path);
  layer->contents.push_back(MakeSolidFill(doc.get(), 0, 0, 1));

  PAGXOptimizer::Optimize(doc.get(), SplitOptions(/*enableChordSplit=*/true));

  std::vector<Path*> resultingPaths;
  for (auto* el : doc->layers[0]->contents) {
    if (el->nodeType() == NodeType::Path) resultingPaths.push_back(static_cast<Path*>(el));
  }
  ASSERT_EQ(resultingPaths.size(), 2u);
  for (auto* p : resultingPaths) {
    ASSERT_NE(p->data, nullptr);
    int verbs = TotalVerbCount(p->data);
    EXPECT_LE(verbs, 900);
    // Each emitted half must start with Move and end with Close.
    EXPECT_EQ(p->data->verbs().front(), pagx::PathVerb::Move);
    EXPECT_EQ(p->data->verbs().back(), pagx::PathVerb::Close);
  }
}

CLI_TEST(PAGXOptimizerTest, SplitLongPathsChordSplitSkipsOpenSubPath) {
  // Open (un-closed) single sub-path with > threshold verbs: chord split needs a closed
  // contour to keep the fill area unchanged, so the splitter must leave it alone even
  // when chord-split is enabled.
  auto doc = PAGXDocument::Make(1000, 1000);
  auto* layer = AddTopLayer(doc.get());
  auto* data = doc->makeNode<PathData>();
  data->moveTo(0, 0);
  // 1100 cubic segments (1 Move + 1100 Cubic = 1101 verbs, no Close).
  for (int i = 0; i < 1100; i++) {
    float t = static_cast<float>(i + 1);
    data->cubicTo(t, 0, t, t, t, t);
  }
  // Note: no close().
  auto* path = doc->makeNode<Path>();
  path->data = data;
  layer->contents.push_back(path);
  layer->contents.push_back(MakeSolidFill(doc.get(), 0, 0, 1));

  PAGXOptimizer::Optimize(doc.get(), SplitOptions(/*enableChordSplit=*/true));

  int pathCount = 0;
  for (auto* el : doc->layers[0]->contents) {
    if (el->nodeType() == NodeType::Path) pathCount++;
  }
  EXPECT_EQ(pathCount, 1);
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
