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

#include <cmath>
#include <string>
#include "base/PAGTest.h"
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/PAGXOptimizer.h"
#include "pagx/PAGXOptimizerOptions.h"
#include "pagx/nodes/BlurFilter.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/DropShadowStyle.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/GlyphRun.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/Text.h"
#include "pagx/utils/VerifyUtils.h"
#include "utils/ProjectPath.h"

namespace pag {

using pagx::Alignment;
using pagx::Arrangement;
using pagx::BlendMode;
using pagx::BlurFilter;
using pagx::Color;
using pagx::Composition;
using pagx::DropShadowStyle;
using pagx::Element;
using pagx::Ellipse;
using pagx::Fill;
using pagx::Font;
using pagx::Glyph;
using pagx::GlyphRun;
using pagx::Group;
using pagx::HasLayerOnlyFeatures;
using pagx::HasPainter;
using pagx::Image;
using pagx::ImagePattern;
using pagx::IsLayerShell;
using pagx::IsPainter;
using pagx::Layer;
using pagx::LayoutMode;
using pagx::MaskType;
using pagx::Matrix;
using pagx::Matrix3D;
using pagx::NodeType;
using pagx::OptimizeWithOptions;
using pagx::Padding;
using pagx::PAGXDocument;
using pagx::PAGXExporter;
using pagx::PAGXImporter;
using pagx::PAGXOptimizer;
using pagx::PAGXOptimizerOptions;
using pagx::Path;
using pagx::PathData;
using pagx::Rectangle;
using pagx::SolidColor;
using pagx::Stroke;
using pagx::Text;

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

static PAGXOptimizerOptions OnlyRuleOptions() {
  PAGXOptimizerOptions options;
  options.pruneEmpty = false;
  options.downgradeShellChildren = false;
  options.mergeAdjacentShellLayers = false;
  options.collapseSingleChildLayers = false;
  options.unwrapRedundantFirstGroup = false;
  options.mergeAdjacentGroups = false;
  options.canonicalizePaths = false;
  options.rectMaskToScrollRect = false;
  options.dedupPathData = false;
  options.pruneUnreferencedResources = false;
  return options;
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
// Whole-rule-set stability against the resources/cli/optimizer_all_rules.pagx
// fixture. The fixture is hand-crafted so that EVERY optimizer rule fires at
// least once during the first pass; the two tests below pin down the two
// invariants the optimizer must therefore satisfy:
//
//   1. In-memory idempotence: running Optimize repeatedly on the same document
//      changes nothing after the first call (the rule set has reached a fixed
//      point and subsequent calls produce byte-identical XML).
//
//   2. Round-trip stability: exporting the optimized document, re-importing
//      it, and optimizing again still produces byte-identical XML. This is
//      stronger than (1) because it also proves that the exporter writes
//      enough state for the importer to reproduce a fixed-point document --
//      no rule should "discover" something new on the re-imported tree.
//
// Both tests use the fully-public PAGXImporter / PAGXExporter pair so a CI
// failure here points either at a non-determinism in the optimizer itself or
// at an importer/exporter that drops or rewrites optimization-relevant state.
// ---------------------------------------------------------------------------

static std::string OptimizerFixturePath() {
  return ProjectPath::Absolute("resources/cli/optimizer_all_rules.pagx");
}

CLI_TEST(PAGXOptimizerTest, AllRulesFixtureIsIdempotentAcrossRepeatedOptimize) {
  auto doc = PAGXImporter::FromFile(OptimizerFixturePath());
  ASSERT_TRUE(doc != nullptr);

  auto firstResult = PAGXOptimizer::Optimize(doc.get());
  EXPECT_TRUE(firstResult.converged);
  auto firstXml = PAGXExporter::ToXML(*doc);
  ASSERT_FALSE(firstXml.empty());

  // A second optimize on the same in-memory document must be a no-op.
  auto secondResult = PAGXOptimizer::Optimize(doc.get());
  EXPECT_TRUE(secondResult.converged);
  auto secondXml = PAGXExporter::ToXML(*doc);
  EXPECT_EQ(firstXml, secondXml);

  // Run a third time so a regression that needs >1 extra iteration to settle
  // (e.g. two rules oscillating once before reaching a new fixed point) still
  // surfaces here rather than silently passing the second-call check.
  auto thirdResult = PAGXOptimizer::Optimize(doc.get());
  EXPECT_TRUE(thirdResult.converged);
  auto thirdXml = PAGXExporter::ToXML(*doc);
  EXPECT_EQ(firstXml, thirdXml);
}

CLI_TEST(PAGXOptimizerTest, AllRulesFixtureIsStableAcrossExportImportRoundTrip) {
  auto sourceDoc = PAGXImporter::FromFile(OptimizerFixturePath());
  ASSERT_TRUE(sourceDoc != nullptr);
  auto firstResult = PAGXOptimizer::Optimize(sourceDoc.get());
  EXPECT_TRUE(firstResult.converged);
  auto firstXml = PAGXExporter::ToXML(*sourceDoc);
  ASSERT_FALSE(firstXml.empty());

  // Round-trip 1: re-import the optimized XML and optimize again. The output
  // must match the previous optimized XML byte-for-byte.
  auto secondDoc = PAGXImporter::FromXML(firstXml);
  ASSERT_TRUE(secondDoc != nullptr);
  auto secondResult = PAGXOptimizer::Optimize(secondDoc.get());
  EXPECT_TRUE(secondResult.converged);
  auto secondXml = PAGXExporter::ToXML(*secondDoc);
  EXPECT_EQ(firstXml, secondXml);

  // Round-trip 2: feed the second round's output back through the loop one
  // more time. Two complete iterations are enough to expose any drift that
  // takes more than a single round to manifest.
  auto thirdDoc = PAGXImporter::FromXML(secondXml);
  ASSERT_TRUE(thirdDoc != nullptr);
  auto thirdResult = PAGXOptimizer::Optimize(thirdDoc.get());
  EXPECT_TRUE(thirdResult.converged);
  auto thirdXml = PAGXExporter::ToXML(*thirdDoc);
  EXPECT_EQ(secondXml, thirdXml);
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

  PAGXOptimizerOptions options;
  options.pruneEmpty = false;
  options.mergeAdjacentShellLayers = false;
  options.downgradeShellChildren = false;
  OptimizeWithOptions(doc.get(), options);
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

  PAGXOptimizerOptions options;
  options.downgradeShellChildren = false;
  // mergeAdjacentShellLayers would otherwise collapse the two children into a single sibling —
  // disabling it here keeps the test focused on the downgrade toggle.
  options.mergeAdjacentShellLayers = false;
  OptimizeWithOptions(doc.get(), options);
  EXPECT_EQ(parent->children.size(), 2u);
  EXPECT_TRUE(parent->contents.empty());
}

CLI_TEST(PAGXOptimizerTest, MergeAdjacentShellLayersDisabled) {
  auto doc = PAGXDocument::Make(100, 100);
  AddShellLayerWithFill(doc.get(), 10, 10, 1, 0, 0);
  AddShellLayerWithFill(doc.get(), 12, 12, 0, 1, 0);
  AddShellLayerWithFill(doc.get(), 14, 14, 0, 0, 1);

  PAGXOptimizerOptions options;
  options.mergeAdjacentShellLayers = false;
  OptimizeWithOptions(doc.get(), options);
  EXPECT_EQ(doc->layers.size(), 3u);
}

CLI_TEST(PAGXOptimizerTest, UnwrapRedundantFirstGroupDisabled) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = AddTopLayer(doc.get());
  auto* outer = MakeFilledRectGroup(doc.get(), 50, 50, 1, 0, 0);
  layer->contents.push_back(outer);

  PAGXOptimizerOptions options;
  options.unwrapRedundantFirstGroup = false;
  OptimizeWithOptions(doc.get(), options);
  ASSERT_EQ(layer->contents.size(), 1u);
  EXPECT_EQ(layer->contents[0]->nodeType(), NodeType::Group);
}

CLI_TEST(PAGXOptimizerTest, MergeAdjacentGroupsDisabled) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = AddTopLayer(doc.get());
  layer->contents.push_back(MakeFilledRectGroup(doc.get(), 10, 10, 1, 0, 0));
  layer->contents.push_back(MakeFilledRectGroup(doc.get(), 12, 12, 1, 0, 0));

  PAGXOptimizerOptions options;
  options.mergeAdjacentGroups = false;
  options.unwrapRedundantFirstGroup = false;
  OptimizeWithOptions(doc.get(), options);
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

  PAGXOptimizerOptions options;
  options.canonicalizePaths = false;
  options.rectMaskToScrollRect = false;
  options.unwrapRedundantFirstGroup = false;
  OptimizeWithOptions(doc.get(), options);
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

  PAGXOptimizerOptions options;
  options.rectMaskToScrollRect = false;
  OptimizeWithOptions(doc.get(), options);
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

  PAGXOptimizerOptions options;
  options.dedupPathData = true;
  options.pruneUnreferencedResources = true;
  options.canonicalizePaths = false;
  options.unwrapRedundantFirstGroup = false;
  options.mergeAdjacentGroups = false;
  OptimizeWithOptions(doc.get(), options);

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

  PAGXOptimizerOptions options;
  options.dedupPathData = false;
  options.pruneUnreferencedResources = false;
  options.canonicalizePaths = false;
  OptimizeWithOptions(doc.get(), options);

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

  PAGXOptimizerOptions options;
  options.pruneUnreferencedResources = true;
  OptimizeWithOptions(doc.get(), options);
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

  PAGXOptimizerOptions options;
  options.pruneUnreferencedResources = false;
  OptimizeWithOptions(doc.get(), options);
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

  PAGXOptimizerOptions options;
  options.pruneUnreferencedResources = true;
  options.canonicalizePaths = false;
  options.unwrapRedundantFirstGroup = false;
  OptimizeWithOptions(doc.get(), options);

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

  PAGXOptimizerOptions options;
  options.maxIterations = 1;
  auto result = OptimizeWithOptions(doc.get(), options);
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

// ---------------------------------------------------------------------------
// Coverage-focused guards for canonicalization, mask conversion, and resources.
// ---------------------------------------------------------------------------

CLI_TEST(PAGXOptimizerTest, OptimizeNullDocumentIsANoOp) {
  auto result = OptimizeWithOptions(nullptr, OnlyRuleOptions());
  EXPECT_TRUE(result.converged);
  EXPECT_EQ(result.iterationsUsed, 0);
}

CLI_TEST(PAGXOptimizerTest, CanonicalizesEllipseAndPreservesIneligiblePaths) {
  auto doc = PAGXDocument::Make(120, 120);
  auto* layer = AddTopLayer(doc.get());

  auto* ellipseData = doc->makeNode<PathData>();
  ellipseData->moveTo(50, 0);
  ellipseData->cubicTo(77.614235f, 0, 100, 22.385765f, 100, 50);
  ellipseData->cubicTo(100, 77.614235f, 77.614235f, 100, 50, 100);
  ellipseData->cubicTo(22.385765f, 100, 0, 77.614235f, 0, 50);
  ellipseData->cubicTo(0, 22.385765f, 22.385765f, 0, 50, 0);
  ellipseData->close();
  auto* ellipsePath = doc->makeNode<Path>();
  ellipsePath->data = ellipseData;
  ellipsePath->position = {3, 4};
  layer->contents.push_back(ellipsePath);

  auto* emptyPath = doc->makeNode<Path>();
  emptyPath->data = doc->makeNode<PathData>();
  layer->contents.push_back(emptyPath);

  auto* rectData = doc->makeNode<PathData>();
  rectData->moveTo(0, 0);
  rectData->lineTo(10, 0);
  rectData->lineTo(10, 10);
  rectData->lineTo(0, 10);
  rectData->close();
  auto* reversedPath = doc->makeNode<Path>();
  reversedPath->data = rectData;
  reversedPath->reversed = true;
  layer->contents.push_back(reversedPath);
  auto* constrainedPath = doc->makeNode<Path>();
  constrainedPath->data = rectData;
  constrainedPath->right = 0;
  layer->contents.push_back(constrainedPath);
  auto* annotatedPath = doc->makeNode<Path>();
  annotatedPath->data = rectData;
  annotatedPath->customData["data-role"] = "shape";
  layer->contents.push_back(annotatedPath);

  auto options = OnlyRuleOptions();
  options.canonicalizePaths = true;
  OptimizeWithOptions(doc.get(), options);

  ASSERT_EQ(layer->contents.size(), 5u);
  ASSERT_EQ(layer->contents[0]->nodeType(), NodeType::Ellipse);
  auto* ellipse = static_cast<Ellipse*>(layer->contents[0]);
  EXPECT_FLOAT_EQ(ellipse->position.x, 53.0f);
  EXPECT_FLOAT_EQ(ellipse->position.y, 54.0f);
  EXPECT_FLOAT_EQ(ellipse->size.width, 100.0f);
  EXPECT_FLOAT_EQ(ellipse->size.height, 100.0f);
  for (size_t i = 1; i < layer->contents.size(); i++) {
    EXPECT_EQ(layer->contents[i]->nodeType(), NodeType::Path);
  }
}

CLI_TEST(PAGXOptimizerTest, NonDefaultGroupsKeepTheirTransformAndConstraintFrame) {
  auto options = OnlyRuleOptions();
  options.unwrapRedundantFirstGroup = true;
  for (int guard = 0; guard < 11; guard++) {
    auto doc = PAGXDocument::Make(100, 100);
    auto* layer = AddTopLayer(doc.get());
    auto* group = doc->makeNode<Group>();
    group->elements.push_back(doc->makeNode<Rectangle>());
    switch (guard) {
      case 0:
        group->alpha = 0.5f;
        break;
      case 1:
        group->position.x = 1;
        break;
      case 2:
        group->anchor.y = 1;
        break;
      case 3:
        group->rotation = 5;
        break;
      case 4:
        group->scale.x = 2;
        break;
      case 5:
        group->skew = 3;
        break;
      case 6:
        group->skewAxis = 15;
        break;
      case 7:
        group->width = 20;
        break;
      case 8:
        group->percentHeight = 100;
        break;
      case 9:
        group->padding.left = 2;
        break;
      case 10:
        group->right = 0;
        break;
    }
    layer->contents.push_back(group);
    OptimizeWithOptions(doc.get(), options);
    ASSERT_EQ(layer->contents.size(), 1u) << "guard=" << guard;
    EXPECT_EQ(layer->contents[0], group) << "guard=" << guard;
  }
}

CLI_TEST(PAGXOptimizerTest, RectMaskConversionRejectsSemanticAndGeometryChanges) {
  struct Fixture {
    std::shared_ptr<PAGXDocument> doc;
    Layer* mask = nullptr;
    Layer* user = nullptr;
    Rectangle* rect = nullptr;
    Fill* fill = nullptr;
  };
  auto makeFixture = []() {
    Fixture f;
    f.doc = PAGXDocument::Make(100, 100);
    f.mask = AddTopLayer(f.doc.get());
    f.rect = f.doc->makeNode<Rectangle>();
    f.rect->position = {25, 25};
    f.rect->size = {50, 50};
    f.fill = MakeSolidFill(f.doc.get(), 1, 1, 1);
    f.mask->contents = {f.rect, f.fill};
    f.user = AddTopLayer(f.doc.get());
    f.user->mask = f.mask;
    f.user->maskType = MaskType::Alpha;
    f.user->contents.push_back(MakeFilledRectGroup(f.doc.get(), 100, 100, 1, 0, 0));
    return f;
  };

  auto options = OnlyRuleOptions();
  options.rectMaskToScrollRect = true;
  for (int guard = 0; guard < 22; guard++) {
    auto f = makeFixture();
    switch (guard) {
      case 0:
        f.user->hasScrollRect = true;
        break;
      case 1:
        f.mask->x = 1;
        break;
      case 2:
        f.mask->matrix = Matrix::Scale(2, 2);
        break;
      case 3:
        f.mask->matrix3D.setRowColumn(0, 3, 1);
        break;
      case 4:
        f.mask->alpha = 0.5f;
        break;
      case 5:
        f.mask->blendMode = BlendMode::Multiply;
        break;
      case 6:
        f.mask->styles.push_back(f.doc->makeNode<DropShadowStyle>());
        break;
      case 7:
        f.mask->customData["data-mask"] = "kept";
        break;
      case 8:
        f.mask->contents.pop_back();
        break;
      case 9:
        f.mask->contents[0] = f.doc->makeNode<Ellipse>();
        break;
      case 10:
        f.mask->contents[1] = f.doc->makeNode<Stroke>();
        break;
      case 11:
        f.rect->reversed = true;
        break;
      case 12:
        f.rect->right = 0;
        break;
      case 13:
        f.rect->customData["data-rect"] = "kept";
        break;
      case 14:
        f.fill->alpha = 0.5f;
        break;
      case 15:
        f.fill->blendMode = BlendMode::Multiply;
        break;
      case 16:
        f.fill->color = f.doc->makeNode<ImagePattern>();
        break;
      case 17:
        static_cast<SolidColor*>(f.fill->color)->color.alpha = 0.5f;
        break;
      case 18:
        f.user->matrix = Matrix::Scale(2, 1);
        break;
      case 19:
        f.user->matrix3D.setRowColumn(1, 3, 2);
        break;
      case 20:
        f.user->right = 0;
        break;
      case 21:
        f.user->width = 100;
        break;
    }
    OptimizeWithOptions(f.doc.get(), options);
    EXPECT_EQ(f.user->mask, f.mask) << "guard=" << guard;
  }
}

CLI_TEST(PAGXOptimizerTest, PruneResourcesFollowsPatternsTextAndFontGlyphs) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = AddTopLayer(doc.get());

  auto* fillImage = doc->makeNode<Image>("fill-image");
  auto* fillPattern = doc->makeNode<ImagePattern>("fill-pattern");
  fillPattern->image = fillImage;
  auto* fill = doc->makeNode<Fill>();
  fill->color = fillPattern;
  layer->contents.push_back(fill);

  auto* strokeImage = doc->makeNode<Image>("stroke-image");
  auto* strokePattern = doc->makeNode<ImagePattern>("stroke-pattern");
  strokePattern->image = strokeImage;
  auto* stroke = doc->makeNode<Stroke>();
  stroke->color = strokePattern;
  layer->contents.push_back(stroke);

  auto* glyphPath = doc->makeNode<PathData>("glyph-path");
  auto* glyphImage = doc->makeNode<Image>("glyph-image");
  auto* pathGlyph = doc->makeNode<Glyph>();
  pathGlyph->path = glyphPath;
  auto* imageGlyph = doc->makeNode<Glyph>();
  imageGlyph->image = glyphImage;
  auto* font = doc->makeNode<Font>("embedded-font");
  font->glyphs = {nullptr, pathGlyph, imageGlyph};
  auto* run = doc->makeNode<GlyphRun>();
  run->font = font;
  auto* text = doc->makeNode<Text>();
  text->glyphRuns = {nullptr, run};
  layer->contents.push_back(text);

  auto options = OnlyRuleOptions();
  options.pruneUnreferencedResources = true;
  OptimizeWithOptions(doc.get(), options);

  EXPECT_EQ(fillPattern->id, "fill-pattern");
  EXPECT_EQ(fillImage->id, "fill-image");
  EXPECT_EQ(strokePattern->id, "stroke-pattern");
  EXPECT_EQ(strokeImage->id, "stroke-image");
  EXPECT_EQ(font->id, "embedded-font");
  EXPECT_EQ(glyphPath->id, "glyph-path");
  EXPECT_EQ(glyphImage->id, "glyph-image");
}

// ---------------------------------------------------------------------------
// VerifyUtils: HasLayerOnlyFeatures — a freshly created Layer with all defaults
// must return false. Every subsequent test mutates exactly one attribute to
// prove each branch of the predicate.
// ---------------------------------------------------------------------------

static Layer* MakeDefaultLayer(PAGXDocument* doc) {
  return doc->makeNode<Layer>();
}

CLI_TEST(VerifyUtilsTest, HasLayerOnlyFeaturesDefaultIsFalse) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = MakeDefaultLayer(doc.get());
  EXPECT_FALSE(HasLayerOnlyFeatures(layer));
}

CLI_TEST(VerifyUtilsTest, HasLayerOnlyFeaturesIdSet) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = MakeDefaultLayer(doc.get());
  layer->id = "layer-id";
  EXPECT_TRUE(HasLayerOnlyFeatures(layer));
}

CLI_TEST(VerifyUtilsTest, HasLayerOnlyFeaturesNameSet) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = MakeDefaultLayer(doc.get());
  layer->name = "decoration";
  EXPECT_TRUE(HasLayerOnlyFeatures(layer));
}

CLI_TEST(VerifyUtilsTest, HasLayerOnlyFeaturesInvisible) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = MakeDefaultLayer(doc.get());
  layer->visible = false;
  EXPECT_TRUE(HasLayerOnlyFeatures(layer));
}

CLI_TEST(VerifyUtilsTest, HasLayerOnlyFeaturesAlphaNotOne) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = MakeDefaultLayer(doc.get());
  layer->alpha = 0.5f;
  EXPECT_TRUE(HasLayerOnlyFeatures(layer));
}

CLI_TEST(VerifyUtilsTest, HasLayerOnlyFeaturesNonNormalBlendMode) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = MakeDefaultLayer(doc.get());
  layer->blendMode = BlendMode::Multiply;
  EXPECT_TRUE(HasLayerOnlyFeatures(layer));
}

CLI_TEST(VerifyUtilsTest, HasLayerOnlyFeaturesMatrix3DNonIdentity) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = MakeDefaultLayer(doc.get());
  layer->matrix3D.setRowColumn(0, 3, 10);
  EXPECT_TRUE(HasLayerOnlyFeatures(layer));
}

CLI_TEST(VerifyUtilsTest, HasLayerOnlyFeaturesPreserve3D) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = MakeDefaultLayer(doc.get());
  layer->preserve3D = true;
  EXPECT_TRUE(HasLayerOnlyFeatures(layer));
}

CLI_TEST(VerifyUtilsTest, HasLayerOnlyFeaturesAntiAliasDisabled) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = MakeDefaultLayer(doc.get());
  layer->antiAlias = false;
  EXPECT_TRUE(HasLayerOnlyFeatures(layer));
}

CLI_TEST(VerifyUtilsTest, HasLayerOnlyFeaturesGroupOpacityDisabled) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = MakeDefaultLayer(doc.get());
  layer->groupOpacity = false;
  EXPECT_TRUE(HasLayerOnlyFeatures(layer));
}

CLI_TEST(VerifyUtilsTest, HasLayerOnlyFeaturesPassThroughBackgroundDisabled) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = MakeDefaultLayer(doc.get());
  layer->passThroughBackground = false;
  EXPECT_TRUE(HasLayerOnlyFeatures(layer));
}

CLI_TEST(VerifyUtilsTest, HasLayerOnlyFeaturesHasScrollRect) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = MakeDefaultLayer(doc.get());
  layer->hasScrollRect = true;
  EXPECT_TRUE(HasLayerOnlyFeatures(layer));
}

CLI_TEST(VerifyUtilsTest, HasLayerOnlyFeaturesClipToBounds) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = MakeDefaultLayer(doc.get());
  layer->clipToBounds = true;
  EXPECT_TRUE(HasLayerOnlyFeatures(layer));
}

CLI_TEST(VerifyUtilsTest, HasLayerOnlyFeaturesMaskSet) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* target = MakeDefaultLayer(doc.get());
  auto* mask = MakeDefaultLayer(doc.get());
  target->mask = mask;
  EXPECT_TRUE(HasLayerOnlyFeatures(target));
}

CLI_TEST(VerifyUtilsTest, HasLayerOnlyFeaturesNonAlphaMaskType) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = MakeDefaultLayer(doc.get());
  layer->maskType = MaskType::Luminance;
  EXPECT_TRUE(HasLayerOnlyFeatures(layer));
}

CLI_TEST(VerifyUtilsTest, HasLayerOnlyFeaturesCompositionSet) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = MakeDefaultLayer(doc.get());
  layer->composition = doc->makeNode<Composition>();
  EXPECT_TRUE(HasLayerOnlyFeatures(layer));
}

CLI_TEST(VerifyUtilsTest, HasLayerOnlyFeaturesStylesNonEmpty) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = MakeDefaultLayer(doc.get());
  layer->styles.push_back(doc->makeNode<DropShadowStyle>());
  EXPECT_TRUE(HasLayerOnlyFeatures(layer));
}

CLI_TEST(VerifyUtilsTest, HasLayerOnlyFeaturesFiltersNonEmpty) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = MakeDefaultLayer(doc.get());
  layer->filters.push_back(doc->makeNode<BlurFilter>());
  EXPECT_TRUE(HasLayerOnlyFeatures(layer));
}

CLI_TEST(VerifyUtilsTest, HasLayerOnlyFeaturesLayoutNotNone) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = MakeDefaultLayer(doc.get());
  layer->layout = LayoutMode::Horizontal;
  EXPECT_TRUE(HasLayerOnlyFeatures(layer));
}

CLI_TEST(VerifyUtilsTest, HasLayerOnlyFeaturesGapNonZero) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = MakeDefaultLayer(doc.get());
  layer->gap = 4.0f;
  EXPECT_TRUE(HasLayerOnlyFeatures(layer));
}

CLI_TEST(VerifyUtilsTest, HasLayerOnlyFeaturesFlexNonZero) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = MakeDefaultLayer(doc.get());
  layer->flex = 1.0f;
  EXPECT_TRUE(HasLayerOnlyFeatures(layer));
}

CLI_TEST(VerifyUtilsTest, HasLayerOnlyFeaturesAlignmentNotStretch) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = MakeDefaultLayer(doc.get());
  layer->alignment = Alignment::Center;
  EXPECT_TRUE(HasLayerOnlyFeatures(layer));
}

CLI_TEST(VerifyUtilsTest, HasLayerOnlyFeaturesArrangementNotStart) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = MakeDefaultLayer(doc.get());
  layer->arrangement = Arrangement::Center;
  EXPECT_TRUE(HasLayerOnlyFeatures(layer));
}

CLI_TEST(VerifyUtilsTest, HasLayerOnlyFeaturesIncludeInLayoutFalse) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = MakeDefaultLayer(doc.get());
  layer->includeInLayout = false;
  EXPECT_TRUE(HasLayerOnlyFeatures(layer));
}

// ---------------------------------------------------------------------------
// VerifyUtils: IsLayerShell — a plain shell Layer returns true; any non-default
// geometry, matrix, size, padding, or constraint attribute forces it to false.
// Anything already covered by HasLayerOnlyFeatures also forces it to false.
// ---------------------------------------------------------------------------

CLI_TEST(VerifyUtilsTest, IsLayerShellDefaultIsTrue) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = MakeDefaultLayer(doc.get());
  EXPECT_TRUE(IsLayerShell(layer));
}

CLI_TEST(VerifyUtilsTest, IsLayerShellWithLayerOnlyFeature) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = MakeDefaultLayer(doc.get());
  layer->blendMode = BlendMode::Multiply;
  EXPECT_FALSE(IsLayerShell(layer));
}

CLI_TEST(VerifyUtilsTest, IsLayerShellXSet) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = MakeDefaultLayer(doc.get());
  layer->x = 5.0f;
  EXPECT_FALSE(IsLayerShell(layer));
}

CLI_TEST(VerifyUtilsTest, IsLayerShellYSet) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = MakeDefaultLayer(doc.get());
  layer->y = 5.0f;
  EXPECT_FALSE(IsLayerShell(layer));
}

CLI_TEST(VerifyUtilsTest, IsLayerShellMatrixNonIdentity) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = MakeDefaultLayer(doc.get());
  layer->matrix = Matrix::Translate(3.0f, 4.0f);
  EXPECT_FALSE(IsLayerShell(layer));
}

CLI_TEST(VerifyUtilsTest, IsLayerShellWidthSet) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = MakeDefaultLayer(doc.get());
  layer->width = 50.0f;
  EXPECT_FALSE(IsLayerShell(layer));
}

CLI_TEST(VerifyUtilsTest, IsLayerShellHeightSet) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = MakeDefaultLayer(doc.get());
  layer->height = 60.0f;
  EXPECT_FALSE(IsLayerShell(layer));
}

CLI_TEST(VerifyUtilsTest, IsLayerShellPercentWidthSet) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = MakeDefaultLayer(doc.get());
  layer->percentWidth = 50.0f;
  EXPECT_FALSE(IsLayerShell(layer));
}

CLI_TEST(VerifyUtilsTest, IsLayerShellPercentHeightSet) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = MakeDefaultLayer(doc.get());
  layer->percentHeight = 50.0f;
  EXPECT_FALSE(IsLayerShell(layer));
}

CLI_TEST(VerifyUtilsTest, IsLayerShellPaddingSet) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = MakeDefaultLayer(doc.get());
  layer->padding.left = 8.0f;
  EXPECT_FALSE(IsLayerShell(layer));
}

CLI_TEST(VerifyUtilsTest, IsLayerShellLeftConstraintSet) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = MakeDefaultLayer(doc.get());
  layer->left = 10.0f;
  EXPECT_FALSE(IsLayerShell(layer));
}

CLI_TEST(VerifyUtilsTest, IsLayerShellRightConstraintSet) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = MakeDefaultLayer(doc.get());
  layer->right = 10.0f;
  EXPECT_FALSE(IsLayerShell(layer));
}

CLI_TEST(VerifyUtilsTest, IsLayerShellTopConstraintSet) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = MakeDefaultLayer(doc.get());
  layer->top = 10.0f;
  EXPECT_FALSE(IsLayerShell(layer));
}

CLI_TEST(VerifyUtilsTest, IsLayerShellBottomConstraintSet) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = MakeDefaultLayer(doc.get());
  layer->bottom = 10.0f;
  EXPECT_FALSE(IsLayerShell(layer));
}

CLI_TEST(VerifyUtilsTest, IsLayerShellCenterXConstraintSet) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = MakeDefaultLayer(doc.get());
  layer->centerX = 0.0f;
  EXPECT_FALSE(IsLayerShell(layer));
}

CLI_TEST(VerifyUtilsTest, IsLayerShellCenterYConstraintSet) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = MakeDefaultLayer(doc.get());
  layer->centerY = 0.0f;
  EXPECT_FALSE(IsLayerShell(layer));
}

// ---------------------------------------------------------------------------
// VerifyUtils: HasPainter / IsPainter
// ---------------------------------------------------------------------------

CLI_TEST(VerifyUtilsTest, IsPainterFillTrue) {
  EXPECT_TRUE(IsPainter(NodeType::Fill));
}

CLI_TEST(VerifyUtilsTest, IsPainterStrokeTrue) {
  EXPECT_TRUE(IsPainter(NodeType::Stroke));
}

CLI_TEST(VerifyUtilsTest, IsPainterOtherTypesFalse) {
  EXPECT_FALSE(IsPainter(NodeType::Rectangle));
  EXPECT_FALSE(IsPainter(NodeType::Ellipse));
  EXPECT_FALSE(IsPainter(NodeType::Group));
  EXPECT_FALSE(IsPainter(NodeType::Layer));
  EXPECT_FALSE(IsPainter(NodeType::Document));
}

CLI_TEST(VerifyUtilsTest, HasPainterEmptyVector) {
  std::vector<Element*> empty;
  EXPECT_FALSE(HasPainter(empty));
}

CLI_TEST(VerifyUtilsTest, HasPainterOnlyShapes) {
  auto doc = PAGXDocument::Make(100, 100);
  std::vector<Element*> elements;
  elements.push_back(doc->makeNode<Rectangle>());
  elements.push_back(doc->makeNode<Ellipse>());
  elements.push_back(doc->makeNode<Group>());
  EXPECT_FALSE(HasPainter(elements));
}

CLI_TEST(VerifyUtilsTest, HasPainterWithFill) {
  auto doc = PAGXDocument::Make(100, 100);
  std::vector<Element*> elements;
  elements.push_back(doc->makeNode<Rectangle>());
  elements.push_back(doc->makeNode<Fill>());
  EXPECT_TRUE(HasPainter(elements));
}

CLI_TEST(VerifyUtilsTest, HasPainterWithStroke) {
  auto doc = PAGXDocument::Make(100, 100);
  std::vector<Element*> elements;
  elements.push_back(doc->makeNode<Rectangle>());
  elements.push_back(doc->makeNode<Stroke>());
  EXPECT_TRUE(HasPainter(elements));
}

CLI_TEST(VerifyUtilsTest, HasPainterPainterAsFirstElement) {
  auto doc = PAGXDocument::Make(100, 100);
  std::vector<Element*> elements;
  elements.push_back(doc->makeNode<Fill>());
  elements.push_back(doc->makeNode<Rectangle>());
  EXPECT_TRUE(HasPainter(elements));
}

// ---------------------------------------------------------------------------
// CollapseSingleChildLayers
// ---------------------------------------------------------------------------

// Only the collapse rule enabled, so a test observes it in isolation from the other structural
// passes (which could otherwise downgrade/merge the same layers).
static PAGXOptimizerOptions CollapseOnly() {
  PAGXOptimizerOptions options;
  options.pruneEmpty = false;
  options.downgradeShellChildren = false;
  options.mergeAdjacentShellLayers = false;
  options.unwrapRedundantFirstGroup = false;
  options.mergeAdjacentGroups = false;
  options.canonicalizePaths = false;
  options.rectMaskToScrollRect = false;
  options.dedupPathData = false;
  options.pruneUnreferencedResources = false;
  options.collapseSingleChildLayers = true;
  return options;
}

static void AddRectFill(PAGXDocument* doc, Layer* layer, float w, float h) {
  auto* rect = doc->makeNode<Rectangle>();
  rect->position = {w / 2.0f, h / 2.0f};
  rect->size = {w, h};
  layer->contents.push_back(rect);
  layer->contents.push_back(MakeSolidFill(doc, 1, 0, 0));
}

// A clipping/sizing parent whose single child exactly fills it and only carries paint content:
// the child's content is absorbed and the parent keeps its frame + clip.
CLI_TEST(PAGXOptimizerTest, CollapseAbsorbsFillingChild) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* parent = AddTopLayer(doc.get());
  parent->width = 16;
  parent->height = 16;
  parent->clipToBounds = true;
  auto* child = doc->makeNode<Layer>();
  child->width = 16;
  child->height = 16;
  AddRectFill(doc.get(), child, 16, 16);
  parent->children.push_back(child);

  OptimizeWithOptions(doc.get(), CollapseOnly());

  ASSERT_EQ(doc->layers.size(), 1u);
  EXPECT_EQ(doc->layers[0], parent);
  EXPECT_TRUE(parent->children.empty());
  EXPECT_TRUE(parent->clipToBounds);
  EXPECT_FLOAT_EQ(parent->width, 16.0f);
  ASSERT_EQ(parent->contents.size(), 2u);
  EXPECT_EQ(parent->contents[0]->nodeType(), NodeType::Rectangle);
  EXPECT_EQ(parent->contents[1]->nodeType(), NodeType::Fill);
}

// A shell parent (all attributes default, no contents) is dropped in favour of its only child,
// which carries the real frame.
CLI_TEST(PAGXOptimizerTest, CollapseDropsShellParent) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* parent = AddTopLayer(doc.get());
  auto* child = doc->makeNode<Layer>();
  child->width = 40;
  child->height = 20;
  child->left = 5;
  child->top = 5;
  AddRectFill(doc.get(), child, 40, 20);
  parent->children.push_back(child);
  ASSERT_TRUE(IsLayerShell(parent));

  OptimizeWithOptions(doc.get(), CollapseOnly());

  ASSERT_EQ(doc->layers.size(), 1u);
  EXPECT_EQ(doc->layers[0], child);
  EXPECT_FLOAT_EQ(child->width, 40.0f);
  EXPECT_FLOAT_EQ(child->left, 5.0f);
}

// A child that applies its own opacity must NOT be absorbed — the alpha would be lost.
CLI_TEST(PAGXOptimizerTest, CollapseKeepsChildWithAlpha) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* parent = AddTopLayer(doc.get());
  parent->width = 16;
  parent->height = 16;
  parent->clipToBounds = true;
  auto* child = doc->makeNode<Layer>();
  child->width = 16;
  child->height = 16;
  child->alpha = 0.5f;
  AddRectFill(doc.get(), child, 16, 16);
  parent->children.push_back(child);

  OptimizeWithOptions(doc.get(), CollapseOnly());

  ASSERT_EQ(parent->children.size(), 1u);
  EXPECT_EQ(parent->children[0], child);
  EXPECT_TRUE(parent->contents.empty());
}

// Padding changes the constraint frame used to lay out the child's contents. Dissolving this layer
// would make percentage/constrained elements resolve against the parent's unpadded frame.
CLI_TEST(PAGXOptimizerTest, CollapseKeepsChildWithPadding) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* parent = AddTopLayer(doc.get());
  parent->width = 100;
  parent->height = 100;
  parent->clipToBounds = true;
  auto* child = doc->makeNode<Layer>();
  child->width = 100;
  child->height = 100;
  child->padding = {10, 10, 10, 10};
  auto* rect = doc->makeNode<Rectangle>();
  rect->percentWidth = 100;
  rect->percentHeight = 100;
  child->contents.push_back(rect);
  child->contents.push_back(MakeSolidFill(doc.get(), 1, 0, 0));
  parent->children.push_back(child);

  OptimizeWithOptions(doc.get(), CollapseOnly());

  ASSERT_EQ(parent->children.size(), 1u);
  EXPECT_EQ(parent->children[0], child);
  EXPECT_TRUE(parent->contents.empty());
}

// An id makes the child externally observable through animations, data binds, host lookups, and
// future edits. Moving that id to a parent that may have its own contents changes the target scope.
CLI_TEST(PAGXOptimizerTest, CollapseKeepsIdentifiedChild) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* parent = AddTopLayer(doc.get());
  parent->width = 100;
  parent->height = 100;
  AddRectFill(doc.get(), parent, 100, 100);
  auto* child = doc->makeNode<Layer>("animated");
  child->width = 100;
  child->height = 100;
  AddRectFill(doc.get(), child, 50, 50);
  parent->children.push_back(child);

  OptimizeWithOptions(doc.get(), CollapseOnly());

  ASSERT_EQ(parent->children.size(), 1u);
  EXPECT_EQ(parent->children[0], child);
  EXPECT_EQ(doc->findNode("animated"), child);
  EXPECT_EQ(parent->contents.size(), 2u);
}

// Two auto/content-sized axes are not evidence that the resolved boxes match: parent and child can
// measure from different payloads, and reparenting percentage descendants would change their frame.
CLI_TEST(PAGXOptimizerTest, CollapseKeepsUnprovenAutoSizedChild) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* parent = AddTopLayer(doc.get());
  parent->clipToBounds = true;
  auto* child = doc->makeNode<Layer>();
  AddRectFill(doc.get(), child, 16, 16);
  parent->children.push_back(child);

  OptimizeWithOptions(doc.get(), CollapseOnly());

  ASSERT_EQ(parent->children.size(), 1u);
  EXPECT_EQ(parent->children[0], child);
}

// Fill proof is per-axis: an exact concrete width plus a 100%-height constraint is as safe as two
// concrete matches and should retain the useful optimisation.
CLI_TEST(PAGXOptimizerTest, CollapseAbsorbsMixedConcreteAndPercentFill) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* parent = AddTopLayer(doc.get());
  parent->width = 16;
  parent->height = 16;
  parent->clipToBounds = true;
  auto* child = doc->makeNode<Layer>();
  child->width = 16;
  child->percentHeight = 100;
  AddRectFill(doc.get(), child, 16, 16);
  parent->children.push_back(child);

  OptimizeWithOptions(doc.get(), CollapseOnly());

  EXPECT_TRUE(parent->children.empty());
  ASSERT_EQ(parent->contents.size(), 2u);
}

// Percentage constraints take precedence over absolute dimensions. A malformed/programmatically
// built node can carry both, so matching absolute dimensions must not hide a non-filling percent.
CLI_TEST(PAGXOptimizerTest, CollapseKeepsNonFillingPercentDespiteMatchingAbsoluteSize) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* parent = AddTopLayer(doc.get());
  parent->width = 16;
  parent->height = 16;
  parent->clipToBounds = true;
  auto* child = doc->makeNode<Layer>();
  child->width = 16;
  child->height = 16;
  child->percentWidth = 50;
  AddRectFill(doc.get(), child, 16, 16);
  parent->children.push_back(child);

  OptimizeWithOptions(doc.get(), CollapseOnly());

  ASSERT_EQ(parent->children.size(), 1u);
  EXPECT_EQ(parent->children[0], child);
  EXPECT_TRUE(parent->contents.empty());
}

// A filling child that carries non-geometric semantics (an external composition, a databind
// context, ...) must NOT be absorbed — dropping the child layer would discard that content/binding.
CLI_TEST(PAGXOptimizerTest, CollapseKeepsChildWithComposition) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* parent = AddTopLayer(doc.get());
  parent->width = 16;
  parent->height = 16;
  parent->clipToBounds = true;
  auto* child = doc->makeNode<Layer>();
  child->width = 16;
  child->height = 16;
  child->compositionFilePath = "icon.pagx";
  parent->children.push_back(child);

  OptimizeWithOptions(doc.get(), CollapseOnly());

  ASSERT_EQ(parent->children.size(), 1u);
  EXPECT_EQ(parent->children[0], child);
  EXPECT_EQ(child->compositionFilePath, "icon.pagx");
}

// A shell parent that carries a databind context is not a pure structural wrapper: dropping it
// would discard the vmContext, so it must be kept even though its geometry looks like a shell.
CLI_TEST(PAGXOptimizerTest, CollapseKeepsShellParentWithVmContext) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* parent = AddTopLayer(doc.get());
  parent->vmContext = "$vm.item";
  auto* child = doc->makeNode<Layer>();
  child->width = 40;
  child->height = 20;
  AddRectFill(doc.get(), child, 40, 20);
  parent->children.push_back(child);

  OptimizeWithOptions(doc.get(), CollapseOnly());

  ASSERT_EQ(doc->layers.size(), 1u);
  EXPECT_EQ(doc->layers[0], parent);
  ASSERT_EQ(parent->children.size(), 1u);
  EXPECT_EQ(parent->children[0], child);
  EXPECT_EQ(parent->vmContext, "$vm.item");
}

// A child that does not fill its parent (smaller + offset) is left alone: the parent's frame/clip
// is doing real work positioning it.
CLI_TEST(PAGXOptimizerTest, CollapseKeepsNonFillingChild) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* parent = AddTopLayer(doc.get());
  parent->width = 20;
  parent->height = 20;
  parent->clipToBounds = true;
  auto* child = doc->makeNode<Layer>();
  child->width = 10;
  child->height = 10;
  child->left = 5;
  child->top = 5;
  AddRectFill(doc.get(), child, 10, 10);
  parent->children.push_back(child);

  OptimizeWithOptions(doc.get(), CollapseOnly());

  ASSERT_EQ(parent->children.size(), 1u);
  EXPECT_EQ(parent->children[0], child);
}

}  // namespace pag
