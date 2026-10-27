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
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <algorithm>
#include "base/PAGTest.h"
#include "pagx/SVGImporter.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextPath.h"

namespace pag {
namespace {

void CollectElements(pagx::Element* element, pagx::NodeType type,
                     std::vector<pagx::Element*>& result) {
  if (element->nodeType() == type) result.push_back(element);
  if (element->nodeType() == pagx::NodeType::Group) {
    for (auto* child : static_cast<pagx::Group*>(element)->elements)
      CollectElements(child, type, result);
  }
}

std::vector<pagx::Element*> Elements(pagx::Layer* layer, pagx::NodeType type) {
  std::vector<pagx::Element*> result;
  for (auto* element : layer->contents) CollectElements(element, type, result);
  for (auto* child : layer->children) {
    auto elements = Elements(child, type);
    result.insert(result.end(), elements.begin(), elements.end());
  }
  return result;
}

bool HasDiagnostic(const pagx::PAGXDocument& document, const std::string& message) {
  return std::any_of(document.errors.begin(), document.errors.end(),
                     [&](const auto& error) { return error.find(message) != std::string::npos; });
}

void ExpectMatrix(const pagx::Matrix& matrix, float a, float b, float c, float d, float x,
                  float y) {
  EXPECT_NEAR(matrix.a, a, 1e-4f);
  EXPECT_NEAR(matrix.b, b, 1e-4f);
  EXPECT_NEAR(matrix.c, c, 1e-4f);
  EXPECT_NEAR(matrix.d, d, 1e-4f);
  EXPECT_NEAR(matrix.tx, x, 1e-4f);
  EXPECT_NEAR(matrix.ty, y, 1e-4f);
}

}  // namespace

PAG_TEST(PAGXSVGReferenceTest, PatternTemplatesInheritAttributesAndDefinitionStylesIndependently) {
  auto doc = pagx::SVGImporter::ParseString(R"SVG(
    <svg width="100" height="80"><defs fill="red">
      <pattern id="base" patternUnits="userSpaceOnUse" width="20" height="20"
               viewBox="0 0 10 10"><rect width="50%" height="100%"/></pattern>
      <pattern id="middle" href="#base" width="40"><title>not tile content</title></pattern>
      <pattern id="tile" href="#middle" height="40" patternTransform="translate(5 7)"/>
    </defs><rect width="80" height="40" fill="url(#tile)" fill-opacity=".25"/></svg>)SVG");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(doc->errors.empty());
  ASSERT_EQ(doc->layers.size(), 1u);
  ASSERT_GE(doc->layers[0]->children.size(), 1u);
  auto* paint = doc->layers[0]->children[0];
  ASSERT_EQ(paint->children.size(), 2u);
  auto* tiles = paint->children[1];
  EXPECT_EQ(tiles->mask, paint->children[0]);
  EXPECT_FLOAT_EQ(tiles->alpha, .25f);
  ASSERT_EQ(tiles->children.size(), 6u);
  auto* first = tiles->children[0];
  EXPECT_TRUE(first->hasScrollRect);
  EXPECT_FLOAT_EQ(first->width, 40);
  EXPECT_FLOAT_EQ(first->height, 40);
  ExpectMatrix(first->matrix, 1, 0, 0, 1, -35, -33);
  ASSERT_EQ(first->children.size(), 1u);
  ExpectMatrix(first->children[0]->matrix, 4, 0, 0, 4, 0, 0);
  auto rectangles = Elements(first, pagx::NodeType::Rectangle);
  ASSERT_EQ(rectangles.size(), 1u);
  EXPECT_FLOAT_EQ(static_cast<pagx::Rectangle*>(rectangles[0])->size.width, 5);
  EXPECT_FLOAT_EQ(static_cast<pagx::Rectangle*>(rectangles[0])->size.height, 10);
  auto fills = Elements(first, pagx::NodeType::Fill);
  ASSERT_EQ(fills.size(), 1u);
  auto* color = static_cast<pagx::Fill*>(fills[0])->color;
  ASSERT_EQ(color->nodeType(), pagx::NodeType::SolidColor);
  EXPECT_FLOAT_EQ(static_cast<pagx::SolidColor*>(color)->color.red, 1);
  EXPECT_FLOAT_EQ(static_cast<pagx::SolidColor*>(color)->color.green, 0);
}

PAG_TEST(PAGXSVGReferenceTest, ObjectBoundingBoxPatternContentUsesTheReferencingShape) {
  auto doc = pagx::SVGImporter::ParseString(R"SVG(
    <svg width="200" height="100"><defs>
      <pattern id="p" width="50%" height="100%" patternContentUnits="objectBoundingBox">
        <rect width="25%" height="50%"/>
      </pattern></defs><rect x="10" y="20" width="80" height="40" fill="url(#p)"/>
    </svg>)SVG");
  ASSERT_NE(doc, nullptr);
  ASSERT_EQ(doc->layers.size(), 1u);
  ASSERT_GE(doc->layers[0]->children.size(), 1u);
  auto* paint = doc->layers[0]->children[0];
  ASSERT_EQ(paint->children.size(), 2u);
  auto* tiles = paint->children[1];
  ASSERT_EQ(tiles->children.size(), 2u);
  auto* tile = tiles->children[0];
  ExpectMatrix(tile->matrix, 1, 0, 0, 1, 10, 20);
  EXPECT_FLOAT_EQ(tile->width, 40);
  EXPECT_FLOAT_EQ(tile->height, 40);
  ASSERT_EQ(tile->children.size(), 1u);
  ExpectMatrix(tile->children[0]->matrix, 80, 0, 0, 40, 0, 0);
  auto rectangles = Elements(tile, pagx::NodeType::Rectangle);
  ASSERT_EQ(rectangles.size(), 1u);
  EXPECT_FLOAT_EQ(static_cast<pagx::Rectangle*>(rectangles[0])->size.width, .25f);
  EXPECT_FLOAT_EQ(static_cast<pagx::Rectangle*>(rectangles[0])->size.height, .5f);
}

PAG_TEST(PAGXSVGReferenceTest, InvalidPatternGeometryPreservesIndependentSolidStroke) {
  for (const auto* attributes :
       {"width='0' height='1'", "width='-1' height='1'", "width='1' height='1' viewBox='0 0 0 1'",
        "width='1' height='1' patternTransform='scale(0)'"}) {
    SCOPED_TRACE(attributes);
    auto doc = pagx::SVGImporter::ParseString(
        std::string("<svg width='100' height='100'><defs><pattern id='p' ") + attributes +
        "><rect width='1' height='1' fill='red'/></pattern></defs>"
        "<rect width='80' height='40' fill='url(#p)' stroke='blue' stroke-width='2'/></svg>");
    ASSERT_NE(doc, nullptr);
    ASSERT_EQ(doc->layers.size(), 1u);
    EXPECT_TRUE(Elements(doc->layers[0], pagx::NodeType::Fill).empty());
    auto strokes = Elements(doc->layers[0], pagx::NodeType::Stroke);
    ASSERT_EQ(strokes.size(), 1u);
    auto* stroke = static_cast<pagx::Stroke*>(strokes[0]);
    EXPECT_FLOAT_EQ(stroke->width, 2);
    ASSERT_EQ(stroke->color->nodeType(), pagx::NodeType::SolidColor);
    EXPECT_FLOAT_EQ(static_cast<pagx::SolidColor*>(stroke->color)->color.blue, 1);
  }
}

PAG_TEST(PAGXSVGReferenceTest, VisiblePatternOverflowIsClippedWithADiagnostic) {
  auto doc = pagx::SVGImporter::ParseString(R"SVG(
    <svg width="20" height="20"><defs>
      <pattern id="p" width="1" height="1" overflow="visible">
        <circle r="40"/></pattern></defs><rect width="20" height="20" fill="url(#p)"/>
    </svg>)SVG");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnostic(*doc, "visible pattern overflow"));
  ASSERT_EQ(doc->layers.size(), 1u);
  ASSERT_GE(doc->layers[0]->children.size(), 1u);
  auto* paint = doc->layers[0]->children[0];
  ASSERT_EQ(paint->children.size(), 2u);
  ASSERT_EQ(paint->children[1]->children.size(), 1u);
  auto* tile = paint->children[1]->children[0];
  EXPECT_TRUE(tile->hasScrollRect);
  EXPECT_FLOAT_EQ(tile->scrollRect.width, 20);
  EXPECT_FLOAT_EQ(tile->scrollRect.height, 20);
}

PAG_TEST(PAGXSVGReferenceTest, UserSpaceGradientCacheSeparatesViewportDimensions) {
  auto doc = pagx::SVGImporter::ParseString(R"SVG(
    <svg width="200" height="100"><defs>
      <linearGradient id="g" gradientUnits="userSpaceOnUse" x2="100%"><stop stop-color="red"/></linearGradient>
    </defs><rect width="20" height="20" fill="url(#g)"/>
    <svg width="80" height="40" viewBox="0 0 40 20">
      <rect width="20" height="20" fill="url(#g)"/><rect width="10" height="10" fill="url(#g)"/>
    </svg></svg>)SVG");
  ASSERT_NE(doc, nullptr);
  ASSERT_EQ(doc->layers.size(), 2u);
  auto rootFills = Elements(doc->layers[0], pagx::NodeType::Fill);
  auto nestedFills = Elements(doc->layers[1], pagx::NodeType::Fill);
  ASSERT_EQ(rootFills.size(), 1u);
  ASSERT_EQ(nestedFills.size(), 2u);
  auto* root = static_cast<pagx::Fill*>(rootFills[0])->color;
  auto* nested = static_cast<pagx::Fill*>(nestedFills[0])->color;
  ASSERT_EQ(root->nodeType(), pagx::NodeType::LinearGradient);
  ASSERT_EQ(nested->nodeType(), pagx::NodeType::LinearGradient);
  EXPECT_NE(root, nested);
  EXPECT_EQ(nested, static_cast<pagx::Fill*>(nestedFills[1])->color);
  EXPECT_FLOAT_EQ(static_cast<pagx::LinearGradient*>(root)->endPoint.x, 200);
  EXPECT_FLOAT_EQ(static_cast<pagx::LinearGradient*>(nested)->endPoint.x, 40);
}

PAG_TEST(PAGXSVGReferenceTest, MarkerShorthandUsesAuthoredCornersAndAutoStartReverse) {
  auto doc = pagx::SVGImporter::ParseString(R"SVG(
    <svg width="100" height="100"><defs>
      <marker id="m" orient="auto-start-reverse" markerUnits="userSpaceOnUse">
        <path d="M0 0L3 1L0 2Z"/></marker>
    </defs><path d="M10 20L30 20L30 40" marker="url(#m)" stroke="red" fill="none"/>
    </svg>)SVG");
  ASSERT_NE(doc, nullptr);
  ASSERT_EQ(doc->layers.size(), 1u);
  auto* owner = doc->layers[0];
  ASSERT_EQ(owner->children.size(), 4u);
  ExpectMatrix(owner->children[1]->matrix, -1, 0, 0, -1, 10, 20);
  const float diagonal = std::sqrt(.5f);
  ExpectMatrix(owner->children[2]->matrix, diagonal, diagonal, -diagonal, diagonal, 30, 20);
  ExpectMatrix(owner->children[3]->matrix, 0, 1, -1, 0, 30, 40);
}

PAG_TEST(PAGXSVGReferenceTest, MarkerOrientAcceptsAngleUnitsAndScalesReferencePoint) {
  for (const auto* orient : {"90deg", "100grad", "0.25turn", "1.57079632679rad"}) {
    SCOPED_TRACE(orient);
    auto doc = pagx::SVGImporter::ParseString(
        std::string("<svg width='100' height='100'><defs><marker id='m' orient='") + orient +
        "' refX='1' refY='2' markerWidth='10' markerHeight='10'><rect width='3' height='3'/>"
        "</marker></defs><line x1='10' y1='20' x2='30' y2='40' stroke='red' stroke-width='2'"
        " marker-end='url(#m)'/></svg>");
    ASSERT_NE(doc, nullptr);
    ASSERT_EQ(doc->layers.size(), 1u);
    ASSERT_EQ(doc->layers[0]->children.size(), 2u);
    ExpectMatrix(doc->layers[0]->children[1]->matrix, 0, 2, -2, 0, 34, 38);
  }
}

PAG_TEST(PAGXSVGReferenceTest, DegenerateMarkerSegmentsBorrowAdjacentDirections) {
  auto doc = pagx::SVGImporter::ParseString(R"SVG(
    <svg width="100" height="100"><defs><marker id="m" orient="auto" markerUnits="userSpaceOnUse">
      <rect width="2" height="2"/></marker></defs>
      <path d="M10 10L10 10L10 30L10 30" marker="url(#m)"/></svg>)SVG");
  ASSERT_NE(doc, nullptr);
  ASSERT_EQ(doc->layers.size(), 1u);
  ASSERT_EQ(doc->layers[0]->children.size(), 5u);
  for (size_t i = 1; i < 5; ++i) {
    ExpectMatrix(doc->layers[0]->children[i]->matrix, 0, 1, -1, 0, 10, i <= 2 ? 10 : 30);
  }
}

PAG_TEST(PAGXSVGReferenceTest, MarkerContextGradientKeepsOwnerCoordinatesThroughNestedTransforms) {
  auto doc = pagx::SVGImporter::ParseString(R"SVG(
    <svg width="100" height="100"><defs>
      <linearGradient id="g" gradientUnits="userSpaceOnUse" x2="100%"><stop stop-color="red"/></linearGradient>
      <marker id="m" orient="90" markerUnits="userSpaceOnUse" markerWidth="10" markerHeight="10">
        <rect width="3" height="3" transform="translate(1 2) scale(2)" fill="context-stroke"/>
      </marker></defs><line x1="10" y1="20" x2="80" y2="20" stroke="url(#g)" marker-end="url(#m)"/>
    </svg>)SVG");
  ASSERT_NE(doc, nullptr);
  ASSERT_EQ(doc->layers.size(), 1u);
  ASSERT_EQ(doc->layers[0]->children.size(), 2u);
  auto* marker = doc->layers[0]->children[1];
  ASSERT_EQ(marker->children.size(), 1u);
  auto* content = marker->children[0];
  ASSERT_EQ(content->children.size(), 1u);
  auto* rect = content->children[0];
  auto fills = Elements(rect, pagx::NodeType::Fill);
  ASSERT_EQ(fills.size(), 1u);
  auto* color = static_cast<pagx::Fill*>(fills[0])->color;
  ASSERT_EQ(color->nodeType(), pagx::NodeType::LinearGradient);
  auto* gradient = static_cast<pagx::LinearGradient*>(color);
  EXPECT_FLOAT_EQ(gradient->endPoint.x, 100);
  ExpectMatrix(marker->matrix * content->matrix * rect->matrix * gradient->matrix, 1, 0, 0, 1, 0,
               0);
}

PAG_TEST(PAGXSVGReferenceTest, CyclicMarkersStopExpansionAndKeepTheOwnerGeometry) {
  auto doc = pagx::SVGImporter::ParseString(R"SVG(
    <svg width="100" height="100"><defs><marker id="m">
      <path d="M0 0L1 1" marker-end="url(#m)"/>
    </marker></defs><path d="M0 0L80 80" marker-end="url(#m)"/></svg>)SVG");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnostic(*doc, "cyclic or excessively nested marker"));
  EXPECT_LT(doc->nodes.size(), 50u);
  ASSERT_EQ(doc->layers.size(), 1u);
  EXPECT_EQ(Elements(doc->layers[0], pagx::NodeType::Path).size(), 2u);
}

PAG_TEST(PAGXSVGReferenceTest, MaskRegionAndContentUnitsResolveInSeparateCoordinateSystems) {
  auto doc = pagx::SVGImporter::ParseString(R"SVG(
    <svg width="200" height="100"><defs>
      <mask id="m" maskUnits="userSpaceOnUse" maskContentUnits="objectBoundingBox"
            x="10%" y="20%" width="50%" height="60%" mask-type="alpha">
        <rect width="50%" height="100%" fill="white"/>
      </mask></defs><rect x="10" y="20" width="80" height="40" mask="url(#m)" opacity=".4"
                          transform="translate(3 5)"/></svg>)SVG");
  ASSERT_NE(doc, nullptr);
  ASSERT_EQ(doc->layers.size(), 1u);
  auto* wrapper = doc->layers[0];
  ExpectMatrix(wrapper->matrix, 1, 0, 0, 1, 3, 5);
  EXPECT_FLOAT_EQ(wrapper->alpha, .4f);
  ASSERT_EQ(wrapper->children.size(), 2u);
  auto* mask = wrapper->children[0];
  auto* owner = wrapper->children[1];
  EXPECT_EQ(owner->mask, mask);
  EXPECT_EQ(owner->maskType, pagx::MaskType::Alpha);
  EXPECT_FLOAT_EQ(owner->alpha, 1);
  EXPECT_TRUE(owner->matrix.isIdentity());
  ASSERT_EQ(mask->children.size(), 1u);
  auto* region = mask->children[0];
  ExpectMatrix(region->matrix, 1, 0, 0, 1, 20, 20);
  EXPECT_FLOAT_EQ(region->scrollRect.width, 100);
  EXPECT_FLOAT_EQ(region->scrollRect.height, 60);
  ASSERT_EQ(region->children.size(), 1u);
  ExpectMatrix(region->children[0]->matrix, 80, 0, 0, 40, -10, 0);
  auto rectangles = Elements(mask, pagx::NodeType::Rectangle);
  ASSERT_EQ(rectangles.size(), 1u);
  EXPECT_FLOAT_EQ(static_cast<pagx::Rectangle*>(rectangles[0])->size.width, .5f);
  EXPECT_FLOAT_EQ(static_cast<pagx::Rectangle*>(rectangles[0])->size.height, 1);
}

PAG_TEST(PAGXSVGReferenceTest, EmptyMaskRegionsRemainAttachedInsteadOfRevealingTheOwner) {
  for (const auto* region : {"width='0'", "height='-1'"}) {
    SCOPED_TRACE(region);
    auto doc = pagx::SVGImporter::ParseString(
        std::string("<svg width='100' height='100'><defs><mask id='m' ") + region +
        "><rect width='100' height='100' fill='white'/></mask></defs>"
        "<rect width='80' height='40' mask='url(#m)'/></svg>");
    ASSERT_NE(doc, nullptr);
    ASSERT_EQ(doc->layers.size(), 1u);
    ASSERT_EQ(doc->layers[0]->children.size(), 2u);
    auto* owner = doc->layers[0]->children[1];
    ASSERT_NE(owner->mask, nullptr);
    EXPECT_TRUE(owner->mask->children.empty());
    EXPECT_EQ(owner->maskType, pagx::MaskType::Luminance);
  }
}

PAG_TEST(PAGXSVGReferenceTest, ClipGeometryIgnoresPaintOpacityAndPreservesEvenOddRule) {
  auto doc = pagx::SVGImporter::ParseString(R"SVG(
    <svg width="100" height="100"><defs><clipPath id="c" clipPathUnits="objectBoundingBox">
      <path d="M0 0H1V1H0Z M.25 .25H.75V.75H.25Z" clip-rule="evenodd" fill="none" opacity="0"/>
    </clipPath></defs><rect width="80" height="40" clip-path="url(#c)"/></svg>)SVG");
  ASSERT_NE(doc, nullptr);
  ASSERT_EQ(doc->layers.size(), 1u);
  ASSERT_EQ(doc->layers[0]->children.size(), 2u);
  auto* mask = doc->layers[0]->children[0];
  auto fills = Elements(mask, pagx::NodeType::Fill);
  ASSERT_EQ(fills.size(), 1u);
  auto* fill = static_cast<pagx::Fill*>(fills[0]);
  EXPECT_EQ(fill->fillRule, pagx::FillRule::EvenOdd);
  EXPECT_FLOAT_EQ(fill->alpha, 1);
  ASSERT_EQ(mask->children.size(), 1u);
  auto* content = mask->children[0];
  ExpectMatrix(content->matrix, 80, 0, 0, 40, 0, 0);
  ASSERT_EQ(content->children.size(), 1u);
  EXPECT_FLOAT_EQ(content->children[0]->alpha, 1);
}

PAG_TEST(PAGXSVGReferenceTest, UseOfSymbolKeepsItsViewportLocalAndRestoresSiblingPercentages) {
  auto doc = pagx::SVGImporter::ParseString(R"SVG(
    <svg width="200" height="100"><defs><symbol id="s" viewBox="0 0 10 20" overflow="visible"
      preserveAspectRatio="defer xMaxYMin meet"><rect width="100%" height="100%"/></symbol></defs>
      <use href="#s" x="10" y="20" width="80" height="40"/>
      <rect width="50%" height="50%"/>
    </svg>)SVG");
  ASSERT_NE(doc, nullptr);
  ASSERT_EQ(doc->layers.size(), 2u);
  ASSERT_EQ(doc->layers[0]->children.size(), 1u);
  auto* viewport = doc->layers[0]->children[0];
  EXPECT_FALSE(viewport->clipToBounds);
  EXPECT_FLOAT_EQ(viewport->width, 80);
  EXPECT_FLOAT_EQ(viewport->height, 40);
  ExpectMatrix(viewport->matrix, 1, 0, 0, 1, 10, 20);
  ASSERT_EQ(viewport->children.size(), 1u);
  ExpectMatrix(viewport->children[0]->matrix, 2, 0, 0, 2, 60, 0);
  auto symbolRects = Elements(viewport, pagx::NodeType::Rectangle);
  auto siblingRects = Elements(doc->layers[1], pagx::NodeType::Rectangle);
  ASSERT_EQ(symbolRects.size(), 1u);
  ASSERT_EQ(siblingRects.size(), 1u);
  EXPECT_FLOAT_EQ(static_cast<pagx::Rectangle*>(symbolRects[0])->size.width, 10);
  EXPECT_FLOAT_EQ(static_cast<pagx::Rectangle*>(symbolRects[0])->size.height, 20);
  EXPECT_FLOAT_EQ(static_cast<pagx::Rectangle*>(siblingRects[0])->size.width, 100);
  EXPECT_FLOAT_EQ(static_cast<pagx::Rectangle*>(siblingRects[0])->size.height, 50);
}

PAG_TEST(PAGXSVGReferenceTest, TextChunksMergeNeutralSpansButKeepAbsolutePositionBoundaries) {
  auto doc = pagx::SVGImporter::ParseString(R"SVG(
    <svg width="200" height="100"><text x="10" y="20" font-size="12">
      f<tspan>fi</tspan><tspan display="none">hidden</tspan><tspan x="100" y="40" dx=".5em"> B </tspan>
    </text></svg>)SVG");
  ASSERT_NE(doc, nullptr);
  ASSERT_EQ(doc->layers.size(), 1u);
  auto texts = Elements(doc->layers[0], pagx::NodeType::Text);
  ASSERT_EQ(texts.size(), 2u);
  auto* first = static_cast<pagx::Text*>(texts[0]);
  auto* second = static_cast<pagx::Text*>(texts[1]);
  EXPECT_EQ(first->text, "ffi");
  EXPECT_EQ(second->text, " B");
  EXPECT_FLOAT_EQ(first->position.x, 10);
  EXPECT_FLOAT_EQ(first->position.y, 20);
  EXPECT_FLOAT_EQ(second->position.x, 106);
  EXPECT_FLOAT_EQ(second->position.y, 40);
}

PAG_TEST(PAGXSVGReferenceTest, TextPathCalibratesAbsoluteOffsetsAndAnchorsTheWholeChunk) {
  auto doc = pagx::SVGImporter::ParseString(R"SVG(
    <svg width="200" height="100"><defs><path id="p" d="M0 0H100" pathLength="10"
      transform="scale(2)"/></defs><text text-anchor="middle" textLength="40">
      <textPath href="#p" startOffset="2">a<tspan fill="red">b</tspan></textPath>
    </text></svg>)SVG");
  ASSERT_NE(doc, nullptr);
  ASSERT_EQ(doc->layers.size(), 1u);
  auto paths = Elements(doc->layers[0], pagx::NodeType::TextPath);
  ASSERT_EQ(paths.size(), 1u);
  auto* path = static_cast<pagx::TextPath*>(paths[0]);
  EXPECT_TRUE(path->forceAlignment);
  EXPECT_FLOAT_EQ(path->firstMargin, 20);
  EXPECT_FLOAT_EQ(path->lastMargin, -140);
  EXPECT_EQ(Elements(doc->layers[0], pagx::NodeType::Text).size(), 2u);
}

PAG_TEST(PAGXSVGReferenceTest, PatternedStrokeRetainsCalibratedDashesAndIndependentFill) {
  auto doc = pagx::SVGImporter::ParseString(R"SVG(
    <svg width="100" height="100"><defs>
      <pattern id="p" patternUnits="userSpaceOnUse" width="10" height="10">
        <rect width="5" height="10" fill="red"/></pattern>
    </defs><rect x="10" y="10" width="20" height="20" fill="blue" stroke="url(#p)"
      stroke-width="4" stroke-dasharray="1 1" pathLength="8" stroke-opacity=".4"/>
    </svg>)SVG");
  ASSERT_NE(doc, nullptr);
  ASSERT_EQ(doc->layers.size(), 1u);
  ASSERT_EQ(doc->layers[0]->children.size(), 2u);
  auto solid = Elements(doc->layers[0]->children[0], pagx::NodeType::Fill);
  ASSERT_EQ(solid.size(), 1u);
  auto* color = static_cast<pagx::Fill*>(solid[0])->color;
  ASSERT_EQ(color->nodeType(), pagx::NodeType::SolidColor);
  EXPECT_FLOAT_EQ(static_cast<pagx::SolidColor*>(color)->color.blue, 1);
  auto* paint = doc->layers[0]->children[1];
  ASSERT_EQ(paint->children.size(), 2u);
  auto strokes = Elements(paint->children[0], pagx::NodeType::Stroke);
  ASSERT_EQ(strokes.size(), 1u);
  auto* stroke = static_cast<pagx::Stroke*>(strokes[0]);
  EXPECT_FLOAT_EQ(stroke->width, 4);
  EXPECT_FLOAT_EQ(stroke->alpha, 1);
  EXPECT_TRUE(stroke->id.empty());
  EXPECT_EQ(stroke->dashes, (std::vector<float>{10, 10}));
  auto* tiles = paint->children[1];
  EXPECT_FLOAT_EQ(tiles->alpha, .4f);
  EXPECT_EQ(tiles->children.size(), 16u);
}

PAG_TEST(PAGXSVGReferenceTest, MarkerContextPatternUsesTheOwnerTileGrid) {
  auto doc = pagx::SVGImporter::ParseString(R"SVG(
    <svg width="100" height="100"><defs>
      <pattern id="p" patternUnits="userSpaceOnUse" width="10" height="10">
        <rect width="5" height="10" fill="red"/></pattern>
      <marker id="m" markerUnits="userSpaceOnUse" orient="90" markerWidth="10" markerHeight="10">
        <rect width="3" height="3" fill="context-stroke"/>
      </marker></defs><line x1="10" y1="20" x2="75" y2="20" stroke="url(#p)" marker-end="url(#m)"/>
    </svg>)SVG");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(doc->errors.empty());
  ASSERT_EQ(doc->layers.size(), 1u);
  ASSERT_EQ(doc->layers[0]->children.size(), 2u);
  auto* marker = doc->layers[0]->children[1];
  ASSERT_EQ(marker->children.size(), 1u);
  auto* content = marker->children[0];
  ASSERT_EQ(content->children.size(), 1u);
  auto* rect = content->children[0];
  ASSERT_GE(rect->children.size(), 1u);
  auto* paint = rect->children[0];
  ASSERT_EQ(paint->children.size(), 2u);
  ASSERT_EQ(paint->children[1]->children.size(), 1u);
  auto* tile = paint->children[1]->children[0];
  ExpectMatrix(marker->matrix * content->matrix * rect->matrix * tile->matrix, 1, 0, 0, 1, 70, 20);
}

PAG_TEST(PAGXSVGReferenceTest, ClosedPolygonMarkersIncludeClosingVertexAndViewportMapping) {
  auto doc = pagx::SVGImporter::ParseString(R"SVG(
    <svg width="100" height="100"><defs>
      <marker id="m" markerUnits="userSpaceOnUse" orient="0" viewBox="0 0 2 2"
        markerWidth="4" markerHeight="4" refX="1" refY="1"><rect width="2" height="2"/></marker>
    </defs><polygon points="10,10 30,10 30,30" marker="url(#m)"/></svg>)SVG");
  ASSERT_NE(doc, nullptr);
  ASSERT_EQ(doc->layers.size(), 1u);
  ASSERT_EQ(doc->layers[0]->children.size(), 5u);
  std::vector<pagx::Point> origins = {{8, 8}, {28, 8}, {28, 28}, {8, 8}};
  for (size_t i = 0; i < origins.size(); ++i) {
    auto* marker = doc->layers[0]->children[i + 1];
    ExpectMatrix(marker->matrix, 1, 0, 0, 1, origins[i].x, origins[i].y);
    ASSERT_EQ(marker->children.size(), 1u);
    ExpectMatrix(marker->children[0]->matrix, 2, 0, 0, 2, 0, 0);
  }
}

PAG_TEST(PAGXSVGReferenceTest, MarkerBisectorCrossesTheAngleDiscontinuityInBothDirections) {
  for (const auto* points : {"80,10 40,20 0,10", "80,30 40,20 0,30"}) {
    SCOPED_TRACE(points);
    auto doc = pagx::SVGImporter::ParseString(
        std::string("<svg width='100' height='100'><defs><marker id='m' orient='auto' ") +
        "markerUnits='userSpaceOnUse'><rect width='2' height='2'/></marker></defs>"
        "<polyline points='" +
        points + "' marker-mid='url(#m)'/></svg>");
    ASSERT_NE(doc, nullptr);
    ASSERT_EQ(doc->layers.size(), 1u);
    ASSERT_EQ(doc->layers[0]->children.size(), 2u);
    ExpectMatrix(doc->layers[0]->children[1]->matrix, -1, 0, 0, -1, 40, 20);
  }
}

PAG_TEST(PAGXSVGReferenceTest, UseGeometryBoundsIncludeItsSymbolViewportForObjectMasks) {
  auto doc = pagx::SVGImporter::ParseString(R"SVG(
    <svg width="200" height="100"><defs>
      <symbol id="s" viewBox="0 0 10 20" preserveAspectRatio="none">
        <rect width="10" height="20"/></symbol>
      <mask id="m" maskContentUnits="objectBoundingBox"><rect width="1" height="1" fill="white"/></mask>
    </defs><use href="#s" x="10" y="20" width="80" height="40" mask="url(#m)"/></svg>)SVG");
  ASSERT_NE(doc, nullptr);
  ASSERT_EQ(doc->layers.size(), 1u);
  ASSERT_EQ(doc->layers[0]->children.size(), 2u);
  auto* mask = doc->layers[0]->children[0];
  ASSERT_EQ(mask->children.size(), 1u);
  auto* region = mask->children[0];
  ExpectMatrix(region->matrix, 1, 0, 0, 1, 2, 16);
  EXPECT_NEAR(region->scrollRect.width, 96, 1e-4f);
  EXPECT_NEAR(region->scrollRect.height, 48, 1e-4f);
  ASSERT_EQ(region->children.size(), 1u);
  ExpectMatrix(region->matrix * region->children[0]->matrix, 80, 0, 0, 40, 10, 20);
}

PAG_TEST(PAGXSVGReferenceTest, CyclicMaskReferencesHideTheRecursiveBranchAndKeepSiblings) {
  auto doc = pagx::SVGImporter::ParseString(R"SVG(
    <svg width="100" height="100"><defs><mask id="m">
      <rect width="80" height="40" mask="url(#m)" fill="white"/>
    </mask></defs><rect width="80" height="40" mask="url(#m)"/>
    <rect id="sibling" width="10" height="10" fill="red"/></svg>)SVG");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnostic(*doc, "cyclic or excessively nested mask"));
  EXPECT_LT(doc->nodes.size(), 50u);
  ASSERT_EQ(doc->layers.size(), 2u);
  EXPECT_EQ(doc->layers[1]->id, "sibling");
  EXPECT_TRUE(doc->layers[1]->visible);
  auto* mask = doc->layers[0]->children[0];
  ASSERT_EQ(mask->children.size(), 1u);
  ASSERT_EQ(mask->children[0]->children.size(), 1u);
  auto* content = mask->children[0]->children[0];
  ASSERT_EQ(content->children.size(), 1u);
  EXPECT_FALSE(content->children[0]->visible);
}

PAG_TEST(PAGXSVGReferenceTest, PatternNodeBudgetStopsDenseTilesAndKeepsFollowingShapes) {
  std::string shapes;
  for (int i = 0; i < 20; ++i) shapes += "<rect width='1' height='1'/>";
  auto doc = pagx::SVGImporter::ParseString(
      "<svg width='100' height='100'><defs><pattern id='p' width='1' height='1' "
      "patternUnits='userSpaceOnUse'>" +
      shapes +
      "</pattern></defs><rect width='100' height='100' fill='url(#p)'/>"
      "<circle id='after' r='10'/></svg>");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnostic(*doc, "pattern node expansion limit"));
  EXPECT_LE(doc->nodes.size(), 200150u);
  ASSERT_EQ(doc->layers.size(), 2u);
  EXPECT_EQ(doc->layers[1]->id, "after");
  EXPECT_EQ(Elements(doc->layers[1], pagx::NodeType::Ellipse).size(), 1u);
}

PAG_TEST(PAGXSVGReferenceTest, TextPatternPaintReportsUnsupportedVectorExpansion) {
  auto doc = pagx::SVGImporter::ParseString(R"SVG(
    <svg width="100" height="100"><defs><pattern id="p" width="1" height="1">
      <rect width="1" height="1"/></pattern></defs><text fill="url(#p)">text</text>
    </svg>)SVG");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnostic(*doc, "pattern paint on this geometry requires vector expansion"));
  ASSERT_EQ(doc->layers.size(), 1u);
  EXPECT_EQ(Elements(doc->layers[0], pagx::NodeType::Text).size(), 1u);
  EXPECT_TRUE(Elements(doc->layers[0], pagx::NodeType::Fill).empty());
}

PAG_TEST(PAGXSVGReferenceTest, UnexpandedImageUseAddsBothOffsetsAndRetainsStretchFallback) {
  pagx::SVGImporter::Options options;
  options.expandUseReferences = false;
  auto doc = pagx::SVGImporter::ParseString(R"SVG(
    <svg width="100" height="100"><defs><image id="image" href="missing-image.png"
      x="3" y="5" width="20" height="10" preserveAspectRatio="none"/></defs>
      <use href="#image" x="7" y="11"/>
    </svg>)SVG",
                                            options);
  ASSERT_NE(doc, nullptr);
  ASSERT_EQ(doc->layers.size(), 1u);
  auto rectangles = Elements(doc->layers[0], pagx::NodeType::Rectangle);
  auto fills = Elements(doc->layers[0], pagx::NodeType::Fill);
  ASSERT_EQ(rectangles.size(), 1u);
  ASSERT_EQ(fills.size(), 1u);
  auto* rectangle = static_cast<pagx::Rectangle*>(rectangles[0]);
  EXPECT_FLOAT_EQ(rectangle->position.x, 20);
  EXPECT_FLOAT_EQ(rectangle->position.y, 21);
  EXPECT_FLOAT_EQ(rectangle->size.width, 20);
  EXPECT_FLOAT_EQ(rectangle->size.height, 10);
  auto* color = static_cast<pagx::Fill*>(fills[0])->color;
  ASSERT_EQ(color->nodeType(), pagx::NodeType::ImagePattern);
  EXPECT_EQ(static_cast<pagx::ImagePattern*>(color)->scaleMode, pagx::ScaleMode::Stretch);
}

PAG_TEST(PAGXSVGReferenceTest, InvalidNestedViewportsOmitTheirContentAndKeepSiblings) {
  for (const auto* attributes :
       {"width='0'", "height='-1'", "viewBox='0 0 0 10'", "viewBox='0 0 10 -1'"}) {
    SCOPED_TRACE(attributes);
    auto doc = pagx::SVGImporter::ParseString(
        std::string("<svg width='100' height='100'><svg ") + attributes +
        "><circle r='10'/></svg><rect id='sibling' width='20' height='20'/></svg>");
    ASSERT_NE(doc, nullptr);
    ASSERT_EQ(doc->layers.size(), 1u);
    EXPECT_EQ(doc->layers[0]->id, "sibling");
    EXPECT_TRUE(Elements(doc->layers[0], pagx::NodeType::Ellipse).empty());
  }
}

PAG_TEST(PAGXSVGReferenceTest, MarkerSolidContextPaintUsesOwnerColorWithoutInheritingOpacity) {
  auto doc = pagx::SVGImporter::ParseString(R"SVG(
    <svg width="100" height="100"><defs><marker id="m" markerUnits="userSpaceOnUse">
      <rect width="3" height="3" fill="context-fill" stroke="context-stroke"/>
    </marker></defs><line x2="80" y2="20" fill="red" stroke="currentColor" color="#336699"
      stroke-opacity=".25" marker-end="url(#m)"/></svg>)SVG");
  ASSERT_NE(doc, nullptr);
  ASSERT_EQ(doc->layers.size(), 1u);
  ASSERT_EQ(doc->layers[0]->children.size(), 2u);
  auto* marker = doc->layers[0]->children[1];
  auto fills = Elements(marker, pagx::NodeType::Fill);
  auto strokes = Elements(marker, pagx::NodeType::Stroke);
  ASSERT_EQ(fills.size(), 1u);
  ASSERT_EQ(strokes.size(), 1u);
  auto* fill = static_cast<pagx::Fill*>(fills[0]);
  auto* stroke = static_cast<pagx::Stroke*>(strokes[0]);
  ASSERT_EQ(fill->color->nodeType(), pagx::NodeType::SolidColor);
  ASSERT_EQ(stroke->color->nodeType(), pagx::NodeType::SolidColor);
  EXPECT_FLOAT_EQ(static_cast<pagx::SolidColor*>(fill->color)->color.red, 1);
  EXPECT_FLOAT_EQ(static_cast<pagx::SolidColor*>(stroke->color)->color.blue, .6f);
  EXPECT_FLOAT_EQ(stroke->alpha, 1);
}

}  // namespace pag
