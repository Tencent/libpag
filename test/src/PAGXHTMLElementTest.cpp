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

#include <algorithm>
#include <string>
#include <vector>
#include "base/PAGTest.h"
#include "pagx/HTMLImporter.h"
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Animation.h"
#include "pagx/nodes/AnimationObject.h"
#include "pagx/nodes/Channel.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/TextBox.h"

namespace pag {
namespace {

constexpr char kRasterSource[] =
    "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAQAAAACCAYAAAB/qH1j"
    "AAAAE0lEQVR42mP8z8DwnwEJMDGgAQA/JwICXm3wVAAAAABJRU5ErkJggg==";
constexpr char kVectorSource[] =
    "data:image/svg+xml,%3Csvg%20xmlns='http://www.w3.org/2000/svg'%20width='40'%20height='20'%3E"
    "%3Crect%20width='40'%20height='20'%20fill='red'/%3E%3C/svg%3E";

std::shared_ptr<pagx::PAGXDocument> ParseElements(const std::string& content) {
  pagx::HTMLImporter::Options options;
  options.autoNormalize = false;
  return pagx::HTMLImporter::ParseString(
      "<html><body style=\"width:120px;height:80px\">" + content + "</body></html>", options);
}

std::shared_ptr<pagx::PAGXDocument> ParseBackground(const std::string& source,
                                                    const std::string& properties) {
  // Percentage dimensions exercise the fallback used before layout has resolved a box's size.
  return ParseElements(
      "<div id=\"box\" style=\"width:100%;height:100%;background-image:url(&quot;" + source +
      "&quot;);" + properties + "\"></div>");
}

bool HasWarning(const std::shared_ptr<pagx::PAGXDocument>& document, const std::string& text) {
  return std::any_of(document->errors.begin(), document->errors.end(),
                     [&](const auto& error) { return error.find(text) != std::string::npos; });
}

template <typename T>
std::vector<T*> Nodes(const std::shared_ptr<pagx::PAGXDocument>& document, pagx::NodeType type) {
  std::vector<T*> nodes;
  for (const auto& node : document->nodes) {
    if (node->nodeType() == type) nodes.push_back(static_cast<T*>(node.get()));
  }
  return nodes;
}

pagx::Layer* NamedLayer(const std::shared_ptr<pagx::PAGXDocument>& document,
                        const std::string& id = "box") {
  for (auto* layer : Nodes<pagx::Layer>(document, pagx::NodeType::Layer)) {
    if (layer->id == id) return layer;
  }
  return nullptr;
}

}  // namespace

PAG_TEST(PAGXHTMLElementTest, PercentageRasterBackgroundUsesFittedScaleModes) {
  struct Case {
    const char* size;
    pagx::ScaleMode mode;
  };
  for (const auto& test :
       {Case{"contain", pagx::ScaleMode::LetterBox}, Case{"cover", pagx::ScaleMode::Zoom},
        Case{"100% 100%", pagx::ScaleMode::Stretch}}) {
    SCOPED_TRACE(test.size);
    auto document =
        ParseBackground(kRasterSource, std::string("background-size:") + test.size +
                                           ";background-color:red;background-blend-mode:multiply");
    ASSERT_NE(document, nullptr);
    auto patterns = Nodes<pagx::ImagePattern>(document, pagx::NodeType::ImagePattern);
    ASSERT_EQ(patterns.size(), 1u);
    EXPECT_EQ(patterns[0]->scaleMode, test.mode);
    ASSERT_NE(patterns[0]->image, nullptr);
    auto* layer = NamedLayer(document);
    ASSERT_NE(layer, nullptr);
    ASSERT_FALSE(layer->contents.empty());
    ASSERT_EQ(layer->contents.back()->nodeType(), pagx::NodeType::Fill);
    auto* fill = static_cast<pagx::Fill*>(layer->contents.back());
    EXPECT_EQ(fill->color, patterns[0]);
    EXPECT_EQ(fill->blendMode, pagx::BlendMode::Multiply);
  }
}

PAG_TEST(PAGXHTMLElementTest, PercentageRasterBackgroundPositionsSingleTileBeforeContent) {
  auto document = ParseBackground(
      kRasterSource,
      "background-size:20px 10px;background-repeat:no-repeat;background-position:right bottom;"
      "background-color:red;background-blend-mode:screen");
  ASSERT_NE(document, nullptr);
  auto* layer = NamedLayer(document);
  ASSERT_NE(layer, nullptr);
  ASSERT_EQ(layer->children.size(), 1u);
  auto* tile = layer->children[0];
  EXPECT_FLOAT_EQ(tile->width, 20);
  EXPECT_FLOAT_EQ(tile->height, 10);
  EXPECT_FLOAT_EQ(tile->left, 100);
  EXPECT_FLOAT_EQ(tile->top, 70);
  EXPECT_FALSE(tile->includeInLayout);
  EXPECT_EQ(tile->blendMode, pagx::BlendMode::Screen);
  auto patterns = Nodes<pagx::ImagePattern>(document, pagx::NodeType::ImagePattern);
  ASSERT_EQ(patterns.size(), 1u);
  EXPECT_EQ(patterns[0]->scaleMode, pagx::ScaleMode::Stretch);
}

PAG_TEST(PAGXHTMLElementTest, OversizedPercentageBackgroundClipsOnlyTheImage) {
  auto document = ParseBackground(
      kRasterSource,
      "background-size:160px 100px;background-repeat:no-repeat;background-position:center");
  ASSERT_NE(document, nullptr);
  auto* layer = NamedLayer(document);
  ASSERT_NE(layer, nullptr);
  ASSERT_EQ(layer->children.size(), 1u);
  auto* clip = layer->children.front();
  EXPECT_TRUE(clip->clipToBounds);
  EXPECT_FLOAT_EQ(clip->width, 120);
  EXPECT_FLOAT_EQ(clip->height, 80);
  ASSERT_EQ(clip->children.size(), 1u);
  auto* tile = clip->children.front();
  EXPECT_FLOAT_EQ(tile->width, 160);
  EXPECT_FLOAT_EQ(tile->height, 100);
  EXPECT_FLOAT_EQ(tile->left, -20);
  EXPECT_FLOAT_EQ(tile->top, -10);
  EXPECT_FALSE(layer->clipToBounds);
}

PAG_TEST(PAGXHTMLElementTest, PercentageBackgroundRepeatControlsEachAxisAndNativeScale) {
  struct Case {
    const char* repeat;
    pagx::TileMode x;
    pagx::TileMode y;
  };
  const Case cases[] = {
      {"", pagx::TileMode::Repeat, pagx::TileMode::Repeat},
      {"repeat-x", pagx::TileMode::Repeat, pagx::TileMode::Decal},
      {"repeat-y", pagx::TileMode::Decal, pagx::TileMode::Repeat},
      {"repeat no-repeat", pagx::TileMode::Repeat, pagx::TileMode::Decal},
      {"no-repeat repeat", pagx::TileMode::Decal, pagx::TileMode::Repeat},
  };
  for (const auto& test : cases) {
    SCOPED_TRACE(test.repeat);
    auto document = ParseBackground(
        kRasterSource,
        std::string("background-size:8px 4px;background-position:3px 5px;background-repeat:") +
            test.repeat);
    ASSERT_NE(document, nullptr);
    auto patterns = Nodes<pagx::ImagePattern>(document, pagx::NodeType::ImagePattern);
    ASSERT_EQ(patterns.size(), 1u);
    auto* pattern = patterns[0];
    EXPECT_EQ(pattern->scaleMode, pagx::ScaleMode::None);
    EXPECT_EQ(pattern->tileModeX, test.x);
    EXPECT_EQ(pattern->tileModeY, test.y);
    EXPECT_FLOAT_EQ(pattern->matrix.a, 2);
    EXPECT_FLOAT_EQ(pattern->matrix.d, 2);
    EXPECT_FLOAT_EQ(pattern->matrix.tx, 3);
    EXPECT_FLOAT_EQ(pattern->matrix.ty, 5);
  }
}

PAG_TEST(PAGXHTMLElementTest, UnavailableRasterNativeSizeKeepsUnresolvedPattern) {
  auto document = ParseBackground("missing-background.png", "background-repeat:no-repeat");
  ASSERT_NE(document, nullptr);
  auto patterns = Nodes<pagx::ImagePattern>(document, pagx::NodeType::ImagePattern);
  ASSERT_EQ(patterns.size(), 1u);
  EXPECT_EQ(patterns[0]->scaleMode, pagx::ScaleMode::None);
  EXPECT_EQ(patterns[0]->tileModeX, pagx::TileMode::Decal);
  EXPECT_EQ(patterns[0]->tileModeY, pagx::TileMode::Decal);
  EXPECT_TRUE(patterns[0]->matrix.isIdentity());
  ASSERT_NE(patterns[0]->image, nullptr);
  EXPECT_EQ(patterns[0]->image->filePath, "missing-background.png");
}

PAG_TEST(PAGXHTMLElementTest, PercentageVectorBackgroundResolvesFittingAndPosition) {
  struct Case {
    const char* size;
    float width;
    float height;
    float sx;
    float sy;
    float x;
    float y;
    bool clip;
  };
  const Case cases[] = {
      {"contain", 120, 60, 1, 1, 0, 10, false}, {"cover", 160, 80, 1, 1, -20, 0, true},
      {"100% 100%", 40, 20, 3, 4, 0, 0, false}, {"20px 10px", 40, 20, .5f, .5f, 50, 35, false},
      {"auto", 40, 20, 1, 1, 40, 30, false},
  };
  for (const auto& test : cases) {
    SCOPED_TRACE(test.size);
    auto document = ParseBackground(kVectorSource,
                                    std::string("background-size:") + test.size +
                                        ";background-repeat:no-repeat;background-position:center");
    ASSERT_NE(document, nullptr);
    auto* layer = NamedLayer(document);
    ASSERT_NE(layer, nullptr);
    ASSERT_EQ(layer->children.size(), 1u);
    auto* tile = layer->children.front();
    if (test.clip) {
      EXPECT_TRUE(tile->clipToBounds);
      EXPECT_FLOAT_EQ(tile->width, 120);
      EXPECT_FLOAT_EQ(tile->height, 80);
      ASSERT_EQ(tile->children.size(), 1u);
      tile = tile->children.front();
    }
    EXPECT_EQ(tile->importDirective.format, "svg");
    EXPECT_NE(tile->importDirective.content.find("<svg"), std::string::npos);
    EXPECT_FLOAT_EQ(tile->width, test.width);
    EXPECT_FLOAT_EQ(tile->height, test.height);
    EXPECT_FLOAT_EQ(tile->matrix.a, test.sx);
    EXPECT_FLOAT_EQ(tile->matrix.d, test.sy);
    EXPECT_FLOAT_EQ(std::isnan(tile->left) ? 0 : tile->left, test.x);
    EXPECT_FLOAT_EQ(std::isnan(tile->top) ? 0 : tile->top, test.y);
    EXPECT_TRUE(Nodes<pagx::Image>(document, pagx::NodeType::Image).empty());
  }
}

PAG_TEST(PAGXHTMLElementTest, UnsizedVectorSourceUsesAuthoredTileOrCanvasFallback) {
  const std::string source =
      "data:image/svg+xml,%3Csvg%3E%3Cpath%20d='M0%200L10%2010'/%3E%3C/svg%3E";
  for (const std::string& size : {std::string("20px 10px"), std::string("auto")}) {
    SCOPED_TRACE(size);
    auto document = ParseBackground(source, "background-repeat:no-repeat;background-size:" + size);
    ASSERT_NE(document, nullptr);
    auto* layer = NamedLayer(document);
    ASSERT_NE(layer, nullptr);
    ASSERT_EQ(layer->children.size(), 1u);
    auto* tile = layer->children.front();
    EXPECT_EQ(tile->importDirective.format, "svg");
    EXPECT_FLOAT_EQ(tile->width, size == "auto" ? 120 : 20);
    EXPECT_FLOAT_EQ(tile->height, size == "auto" ? 80 : 10);
  }
}

PAG_TEST(PAGXHTMLElementTest, UnresolvedVectorRepeatFallsBackWithDiagnostic) {
  auto document =
      ParseBackground(kVectorSource, "background-size:20px 10px;background-repeat:repeat");
  ASSERT_NE(document, nullptr);
  EXPECT_TRUE(HasWarning(document, "tiled SVG background"));
  auto patterns = Nodes<pagx::ImagePattern>(document, pagx::NodeType::ImagePattern);
  ASSERT_EQ(patterns.size(), 1u);
  EXPECT_EQ(patterns[0]->tileModeX, pagx::TileMode::Repeat);
  EXPECT_EQ(patterns[0]->tileModeY, pagx::TileMode::Repeat);
}

PAG_TEST(PAGXHTMLElementTest, BodyBackgroundUsesExplicitCanvasOptions) {
  pagx::HTMLImporter::Options options;
  options.autoNormalize = false;
  options.targetWidth = 120;
  options.targetHeight = 80;
  auto document = pagx::HTMLImporter::ParseString(
      std::string("<html><body style=\"background-image:url(") + kRasterSource +
          ");background-size:20px 10px;background-repeat:no-repeat\"></body></html>",
      options);
  ASSERT_NE(document, nullptr);
  EXPECT_FLOAT_EQ(document->width, 120);
  EXPECT_FLOAT_EQ(document->height, 80);
  ASSERT_EQ(document->layers.size(), 1u);
  ASSERT_EQ(document->layers.front()->children.size(), 1u);
  EXPECT_FLOAT_EQ(document->layers.front()->children.front()->width, 20);
  EXPECT_FLOAT_EQ(document->layers.front()->children.front()->height, 10);
}

PAG_TEST(PAGXHTMLElementTest, BackgroundLayoutRejectsUnboundedTilesAndUnsupportedImages) {
  for (const auto& image : {"linear-gradient(red,blue)", "unsupported(foo)"}) {
    SCOPED_TRACE(image);
    auto document =
        ParseElements(std::string("<div style=\"width:100px;height:100px;background-image:") +
                      image + ";background-size:0.001px 0.001px\"></div>");
    ASSERT_NE(document, nullptr);
    EXPECT_TRUE(HasWarning(document, "repeat budget exceeded"));
    EXPECT_LT(document->nodes.size(), 20u);
  }
  auto document = ParseElements(
      "<div "
      "style=\"width:100px;height:100px;background-image:unsupported(foo);background-size:cover\"><"
      "/div>");
  ASSERT_NE(document, nullptr);
  EXPECT_TRUE(HasWarning(document, "unsupported background image"));
}

PAG_TEST(PAGXHTMLElementTest, BasicShapeClipSupportsKeywordAndPercentageRadii) {
  for (const auto& shape : {"circle(closest-side at 25% 50%)", "circle(farthest-side at 25% 50%)",
                            "circle(50% at 20px 30px)", "ellipse(20px 10px at 40px 30px)",
                            "polygon(evenodd, 0 0, 100% 0, 50% 100%)"}) {
    SCOPED_TRACE(shape);
    auto document =
        ParseElements(std::string("<div id=\"box\" style=\"width:100px;height:80px;clip-path:") +
                      shape + "\"></div>");
    ASSERT_NE(document, nullptr);
    auto* layer = NamedLayer(document);
    ASSERT_NE(layer, nullptr);
    ASSERT_NE(layer->mask, nullptr);
    EXPECT_EQ(layer->maskType, pagx::MaskType::Contour);
    EXPECT_FALSE(HasWarning(document, "unsupported clip-path"));
    if (std::string(shape).find("polygon") == 0) {
      auto paths = Nodes<pagx::Path>(document, pagx::NodeType::Path);
      ASSERT_FALSE(paths.empty());
      ASSERT_NE(paths.front()->data, nullptr);
      EXPECT_EQ(paths.front()->data->points().size(), 3u);
    } else {
      auto ellipses = Nodes<pagx::Ellipse>(document, pagx::NodeType::Ellipse);
      ASSERT_EQ(ellipses.size(), 1u);
      EXPECT_GT(ellipses.front()->size.width, 0);
      EXPECT_GT(ellipses.front()->size.height, 0);
    }
  }
}

PAG_TEST(PAGXHTMLElementTest, InvalidBasicShapesDoNotCreateBrokenMasks) {
  for (const auto& shape : {"circle(0px)", "circle(bad)", "circle(20px at bad top)",
                            "ellipse(10px 0px)", "polygon(bad)", "inset(60px)", "unknown(2px)"}) {
    SCOPED_TRACE(shape);
    auto document =
        ParseElements(std::string("<div id=\"box\" style=\"width:100px;height:80px;clip-path:") +
                      shape + "\"></div>");
    ASSERT_NE(document, nullptr);
    auto* layer = NamedLayer(document);
    ASSERT_NE(layer, nullptr);
    EXPECT_EQ(layer->mask, nullptr);
    EXPECT_TRUE(HasWarning(document, "unsupported clip-path"));
  }
}

PAG_TEST(PAGXHTMLElementTest, RoundedOverflowWithExplicitMaskReportsApproximation) {
  auto document = ParseElements(
      "<div id=\"box\" style=\"width:100px;height:80px;border-radius:12px;overflow:hidden;"
      "clip-path:circle(20px)\"></div>");
  ASSERT_NE(document, nullptr);
  auto* layer = NamedLayer(document);
  ASSERT_NE(layer, nullptr);
  EXPECT_NE(layer->mask, nullptr);
  EXPECT_TRUE(HasWarning(document, "border-radius overflow clip combined"));
}

PAG_TEST(PAGXHTMLElementTest, AnimatedSvgShapesReceiveStableIdsAndScaledDashOffsets) {
  pagx::HTMLImporter::Options options;
  options.autoNormalize = false;
  auto document = pagx::HTMLImporter::ParseString(R"HTML(
    <html><head><style>@keyframes draw { from {stroke-dashoffset:10} to {stroke-dashoffset:0} }</style></head>
    <body style="width:120px;height:80px"><svg width="120" height="80"><g>
      <path id="" d="M0 0 L40 0 M0 10 L60 10" pathLength="10"
            style="animation:draw 1s linear both"/>
      <path d="M0 0 L20 0" pathLength="0" style="animation:draw 1s linear both"/>
    </g></svg></body></html>)HTML",
                                                  options);
  ASSERT_NE(document, nullptr);
  ASSERT_EQ(document->animations.size(), 1u);
  auto* animation = static_cast<pagx::Animation*>(document->animations.front());
  ASSERT_EQ(animation->objects.size(), 2u);
  const float starts[] = {100, 10};
  std::vector<std::string> targets;
  for (size_t i = 0; i < 2; i++) {
    auto* object = animation->objects[i];
    targets.push_back(object->target);
    EXPECT_NE(object->target.find("svgshape"), std::string::npos);
    ASSERT_EQ(object->channels.size(), 1u);
    auto* channel = object->channels.front();
    EXPECT_EQ(channel->name, "dashOffset");
    EXPECT_FLOAT_EQ(std::get<float>(channel->evaluateAt(0)), starts[i]);
    EXPECT_FLOAT_EQ(std::get<float>(channel->evaluateAt(30)), starts[i] / 2);
  }
  EXPECT_NE(targets[0], targets[1]);
  auto layers = Nodes<pagx::Layer>(document, pagx::NodeType::Layer);
  auto imported = std::find_if(layers.begin(), layers.end(), [](const auto* layer) {
    return layer->importDirective.format == "svg";
  });
  ASSERT_NE(imported, layers.end());
  for (const auto& target : targets) {
    auto id = target.substr(0, target.find("__stroke"));
    EXPECT_NE((*imported)->importDirective.content.find(id), std::string::npos);
  }
}

PAG_TEST(PAGXHTMLElementTest, MissingImageDimensionsPreserveSlotAndReportNaturalSize) {
  auto document = ParseElements(
      "<img id=\"box\" src=\"missing.png\" style=\"object-fit:contain;object-position:left "
      "top\"/>");
  ASSERT_NE(document, nullptr);
  auto* layer = NamedLayer(document);
  ASSERT_NE(layer, nullptr);
  EXPECT_GT(layer->width, 0);
  EXPECT_GT(layer->height, 0);
  EXPECT_TRUE(HasWarning(document, "image natural size unavailable"));
}

PAG_TEST(PAGXHTMLElementTest, GenericDataUriTypesAreSniffedFromThePayload) {
  const std::string png(kRasterSource);
  const auto payload = png.substr(png.find(',') + 1);
  for (const auto& mime : {"application/octet-stream", ""}) {
    SCOPED_TRACE(mime);
    auto document = ParseElements(std::string("<img src=\"data:") + mime + ";base64," + payload +
                                  "\" style=\"width:20px;height:10px\"/>");
    ASSERT_NE(document, nullptr);
    EXPECT_FALSE(HasWarning(document, "outside the <Image> supported set"));
    EXPECT_EQ(Nodes<pagx::Image>(document, pagx::NodeType::Image).size(), 1u);
  }
  auto unknown = ParseElements("<img src=\"data:application/octet-stream;base64,YmFk\"/>");
  ASSERT_NE(unknown, nullptr);
  EXPECT_TRUE(HasWarning(unknown, "an unrecognised format"));
}

PAG_TEST(PAGXHTMLElementTest, SvgImageDimensionsUseAbsoluteUnitsAndViewBoxRatio) {
  const char* source =
      "data:image/svg+xml,%3Csvg%20xmlns='http://www.w3.org/2000/svg'%20"
      "style='width:40px;height:20px'%20viewBox='0%200%2040%2020'%3E"
      "%3Crect%20width='40'%20height='20'/%3E%3C/svg%3E";
  for (const auto& dimension : {"width:60px", "height:30px"}) {
    SCOPED_TRACE(dimension);
    auto document = ParseElements(std::string("<img id=\"box\" src=\"") + source + "\" style=\"" +
                                  dimension + ";object-fit:none\"/>");
    ASSERT_NE(document, nullptr);
    auto* layer = NamedLayer(document);
    ASSERT_NE(layer, nullptr);
    EXPECT_FLOAT_EQ(layer->width, 60);
    EXPECT_FLOAT_EQ(layer->height, 30);
    EXPECT_FALSE(layer->children.empty());
  }
}

PAG_TEST(PAGXHTMLElementTest, FlexImagesReleaseTheirIntrinsicMainAxisSize) {
  for (const auto& direction : {"row", "column"}) {
    SCOPED_TRACE(direction);
    auto document = ParseElements(
        std::string("<div style=\"width:120px;height:80px;display:flex;flex-direction:") +
        direction + "\"><img id=\"box\" src=\"" + kRasterSource +
        "\" style=\"flex:1;object-fit:contain\"/></div>");
    ASSERT_NE(document, nullptr);
    auto* layer = NamedLayer(document);
    ASSERT_NE(layer, nullptr);
    auto bounds = layer->layoutBounds();
    EXPECT_FLOAT_EQ(std::string(direction) == "row" ? bounds.width : bounds.height,
                    std::string(direction) == "row" ? 120 : 80);
    EXPECT_FALSE(layer->children.empty());
  }
}

PAG_TEST(PAGXHTMLElementTest, PercentageSvgUnderlayKeepsTheImportViewportRelative) {
  auto document = ParseElements(
      "<svg id=\"box\" style=\"width:50%;height:50%;background-color:red\">"
      "<rect width=\"10\" height=\"10\"/></svg>");
  ASSERT_NE(document, nullptr);
  auto* layer = NamedLayer(document);
  ASSERT_NE(layer, nullptr);
  ASSERT_EQ(layer->children.size(), 1u);
  auto* imported = layer->children.front();
  EXPECT_FLOAT_EQ(imported->percentWidth, 100);
  EXPECT_FLOAT_EQ(imported->percentHeight, 100);
  EXPECT_EQ(imported->importDirective.format, "svg");
}

PAG_TEST(PAGXHTMLElementTest, EllipticalBoxShadowBuildsCurvedGeometry) {
  auto document = ParseElements(
      "<div id=\"box\" style=\"width:100px;height:80px;border-radius:50%;"
      "background-color:red;box-shadow:2px 3px 4px 5px blue\"></div>");
  ASSERT_NE(document, nullptr);
  auto paths = Nodes<pagx::Path>(document, pagx::NodeType::Path);
  ASSERT_FALSE(paths.empty());
  bool hasCurve = false;
  for (auto* path : paths) {
    ASSERT_NE(path->data, nullptr);
    const auto& verbs = path->data->verbs();
    if (std::find(verbs.begin(), verbs.end(), pagx::PathVerb::Cubic) != verbs.end())
      hasCurve = true;
  }
  EXPECT_TRUE(hasCurve);
  EXPECT_FALSE(HasWarning(document, "spread requires"));
}

PAG_TEST(PAGXHTMLElementTest, VectorTileBudgetIsBoundedBeforeCreatingThousandsOfImports) {
  auto document = ParseElements(
      std::string("<div style=\"width:100px;height:100px;background-image:url(&quot;") +
      kVectorSource + "&quot;);background-size:1px 1px;background-repeat:repeat\"></div>");
  ASSERT_NE(document, nullptr);
  EXPECT_TRUE(HasWarning(document, "background tile budget exceeded"));
  EXPECT_LT(document->nodes.size(), 20u);
}

}  // namespace pag
