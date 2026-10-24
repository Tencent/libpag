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
//  license is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include "base/PAGTest.h"
#include "pagx/PAGExporter.h"
#include "pagx/PAGImporter.h"
#include "pagx/PAGScene.h"
#include "pagx/PAGSurface.h"
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXExternalFiles.h"
#include "pagx/PAGXImporter.h"
#include "pagx/nodes/Animation.h"
#include "pagx/nodes/AnimationObject.h"
#include "pagx/nodes/AnimationTimeline.h"
#include "pagx/nodes/BackgroundBlurStyle.h"
#include "pagx/nodes/BlendFilter.h"
#include "pagx/nodes/BlurFilter.h"
#include "pagx/nodes/Channel.h"
#include "pagx/nodes/ColorMatrixFilter.h"
#include "pagx/nodes/ColorStop.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/DropShadowFilter.h"
#include "pagx/nodes/DropShadowStyle.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/GlassStyle.h"
#include "pagx/nodes/GlyphRun.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/InnerShadowFilter.h"
#include "pagx/nodes/InnerShadowStyle.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/Node.h"
#include "pagx/nodes/NoiseFilter.h"
#include "pagx/nodes/NoiseStyle.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/RangeSelector.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/Repeater.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/State.h"
#include "pagx/nodes/StateMachine.h"
#include "pagx/nodes/StateMachineInput.h"
#include "pagx/nodes/StateMachineTimeline.h"
#include "pagx/nodes/StateRegion.h"
#include "pagx/nodes/StateTransition.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/nodes/TextModifier.h"
#include "pagx/nodes/Timeline.h"
#include "pagx/nodes/TransitionCondition.h"
#include "pagx/pag/PAGAttributeIO.h"
#include "pagx/pag/PAGTagHeader.h"
#include "pagx/pag/TagCode.h"
#include "pagx/types/ColorSpace.h"
#include "utils/ProjectPath.h"

namespace pagx {

static std::vector<uint8_t> ReadFileBytes(const std::string& path) {
  FILE* f = fopen(path.c_str(), "rb");
  if (f == nullptr) return {};
  fseek(f, 0, SEEK_END);
  auto size = ftell(f);
  fseek(f, 0, SEEK_SET);
  std::vector<uint8_t> bytes(size);
  fread(bytes.data(), 1, size, f);
  fclose(f);
  return bytes;
}

PAGX_TEST(PAGImporterTest, ExportDecodeRoundTrip) {
  auto doc = PAGXImporter::FromFile(
      pag::ProjectPath::Absolute("resources/pagx_to_pag/static_animated/pure_static.pagx"));
  ASSERT_TRUE(doc != nullptr);

  auto exportResult = PAGExporter::ExportToPAG(doc, "/tmp/rt_export.pag");
  ASSERT_TRUE(exportResult.success);

  PAGImportResult importResult = {};
  auto decoded = PAGImporter::LoadFromPAG("/tmp/rt_export.pag", &importResult);
  ASSERT_TRUE(decoded != nullptr) << importResult.error;
  EXPECT_TRUE(importResult.success);

  EXPECT_EQ(decoded->width, doc->width);
  EXPECT_EQ(decoded->height, doc->height);
  EXPECT_EQ(decoded->layers.size(), doc->layers.size());
}

PAGX_TEST(PAGImporterTest, RoundTripByteIdentity) {
  std::vector<std::string> paths = {
      "resources/pagx_to_pag/static_animated/pure_static.pagx",
      "resources/pagx_to_pag/layout_flatten/clip_to_bounds.pagx",
      "resources/pagx_to_pag/resource/image_external_shared.pagx",
      "resources/pagx_to_pag/resource/image_data_uri.pagx",
      "resources/pagx_to_pag/text_font/embedded_vector_glyphs.pagx",
      "resources/pagx_to_pag/text_font/embedded_bitmap_glyphs.pagx",
  };
  for (auto& rel : paths) {
    auto doc = PAGXImporter::FromFile(pag::ProjectPath::Absolute(rel));
    ASSERT_TRUE(doc != nullptr) << rel;

    auto r1 = PAGExporter::ExportToPAG(doc, "/tmp/rt1.pag");
    ASSERT_TRUE(r1.success) << rel;

    auto decoded = PAGImporter::LoadFromPAG("/tmp/rt1.pag");
    ASSERT_TRUE(decoded != nullptr) << rel;

    auto r2 = PAGExporter::ExportToPAG(decoded, "/tmp/rt2.pag");
    ASSERT_TRUE(r2.success) << rel;

    auto bytes1 = ReadFileBytes("/tmp/rt1.pag");
    auto bytes2 = ReadFileBytes("/tmp/rt2.pag");
    ASSERT_EQ(bytes1.size(), bytes2.size()) << rel;
    EXPECT_EQ(memcmp(bytes1.data(), bytes2.data(), bytes1.size()), 0) << rel;
  }
}

PAGX_TEST(PAGImporterTest, DetectExportedFile) {
  auto doc = PAGXImporter::FromFile(
      pag::ProjectPath::Absolute("resources/pagx_to_pag/static_animated/pure_static.pagx"));
  ASSERT_TRUE(doc != nullptr);
  PAGExporter::ExportToPAG(doc, "/tmp/detect_v4.pag");
  PAGImportResult result = {};
  PAGImporter::LoadFromPAG("/tmp/detect_v4.pag", &result);
  EXPECT_EQ(result.format, PAGFormat::PAGv4);
  EXPECT_TRUE(result.success);
}

PAGX_TEST(PAGImporterTest, DecodeFailureReturnsNull) {
  auto doc = PAGXImporter::FromFile(
      pag::ProjectPath::Absolute("resources/pagx_to_pag/static_animated/pure_static.pagx"));
  ASSERT_TRUE(doc != nullptr);
  ASSERT_TRUE(PAGExporter::ExportToPAG(doc, "/tmp/null_contract.pag").success);
  auto bytes = ReadFileBytes("/tmp/null_contract.pag");
  ASSERT_FALSE(bytes.empty());

  PAGImportResult ok = {};
  EXPECT_TRUE(PAGImporter::LoadFromPAG("/tmp/null_contract.pag", &ok) != nullptr);
  EXPECT_TRUE(ok.success);

  for (auto cut : {static_cast<size_t>(16), bytes.size() / 2, bytes.size() - 1}) {
    FILE* f = fopen("/tmp/null_contract_trunc.pag", "wb");
    ASSERT_TRUE(f != nullptr);
    fwrite(bytes.data(), 1, cut, f);
    fclose(f);
    PAGImportResult bad = {};
    auto decoded = PAGImporter::LoadFromPAG("/tmp/null_contract_trunc.pag", &bad);
    EXPECT_EQ(decoded, nullptr);
    EXPECT_FALSE(bad.success);
    EXPECT_FALSE(bad.error.empty());
  }
}

PAGX_TEST(PAGImporterTest, DecodedDocumentLayoutAppliedAtLoad) {
  auto doc = PAGXImporter::FromFile(
      pag::ProjectPath::Absolute("resources/pagx_to_pag/static_animated/pure_static.pagx"));
  ASSERT_TRUE(doc != nullptr);
  ASSERT_TRUE(PAGExporter::ExportToPAG(doc, "/tmp/layout_at_load.pag").success);

  PAGImportResult importResult = {};
  auto decoded = PAGImporter::LoadFromPAG("/tmp/layout_at_load.pag", &importResult);
  ASSERT_TRUE(decoded != nullptr);
  EXPECT_FALSE(decoded->isLayoutApplied());
  auto scene = PAGScene::Make(decoded);
  ASSERT_TRUE(scene != nullptr);
  EXPECT_TRUE(decoded->isLayoutApplied());

  // Empty and zero-size documents are legal (spec §4.2) and still produce a scene.
  auto empty = PAGXDocument::Make(0, 0);
  ASSERT_TRUE(PAGExporter::ExportToPAG(empty, "/tmp/layout_at_load_empty.pag").success);
  auto decodedEmpty = PAGImporter::LoadFromPAG("/tmp/layout_at_load_empty.pag");
  ASSERT_TRUE(decodedEmpty != nullptr);
  EXPECT_FALSE(decodedEmpty->isLayoutApplied());
  EXPECT_TRUE(PAGScene::Make(decodedEmpty) != nullptr);
}

PAGX_TEST(PAGImporterTest, StaticSceneHasContentChanged) {
  auto doc = PAGXImporter::FromFile(
      pag::ProjectPath::Absolute("resources/pagx_to_pag/static_animated/pure_static.pagx"));
  ASSERT_TRUE(doc != nullptr);
  ASSERT_TRUE(PAGExporter::ExportToPAG(doc, "/tmp/static_content_changed.pag").success);
  PAGImportResult importResult = {};
  auto decoded = PAGImporter::LoadFromPAG("/tmp/static_content_changed.pag", &importResult);
  ASSERT_TRUE(decoded != nullptr);

  auto scene = PAGScene::Make(decoded);
  ASSERT_TRUE(scene != nullptr);
  auto surface = PAGSurface::MakeOffscreen(static_cast<int>(decoded->width),
                                           static_cast<int>(decoded->height));
  ASSERT_TRUE(surface != nullptr);
  ASSERT_TRUE(scene->draw(surface));
  // Static v4 files settle after the first draw: nothing drives the tree, so idle frames can be
  // skipped (hasContentChanged stays false, T9 static render semantics).
  EXPECT_FALSE(scene->hasContentChanged());
  EXPECT_FALSE(scene->hasContentChanged());
}

PAGX_TEST(PAGImporterTest, ImagePatternDecodeZeroCopy) {
  const std::string sharedPath =
      pag::ProjectPath::Absolute("resources/pagx_to_pag/resource/image_external_shared.pagx");
  auto doc = PAGXImporter::FromFile(sharedPath);
  ASSERT_TRUE(doc != nullptr);
  ASSERT_TRUE(PAGExporter::ExportToPAG(doc, "/tmp/image_shared.pag").success);
  auto decoded = PAGImporter::LoadFromPAG("/tmp/image_shared.pag");
  ASSERT_TRUE(decoded != nullptr);

  Image* images[2] = {nullptr, nullptr};
  int patternIndex = 0;
  for (auto* layer : decoded->layers) {
    for (auto* element : layer->contents) {
      if (element->nodeType() != NodeType::Fill) {
        continue;
      }
      auto* fill = static_cast<Fill*>(element);
      ASSERT_TRUE(fill->color != nullptr);
      ASSERT_EQ(fill->color->nodeType(), NodeType::ImagePattern);
      auto* pattern = static_cast<ImagePattern*>(fill->color);
      ASSERT_LT(patternIndex, 2);
      images[patternIndex++] = pattern->image;
    }
  }
  ASSERT_EQ(patternIndex, 2);
  ASSERT_TRUE(images[0] != nullptr);
  // Referenced by two patterns: shared through the ResourceBlock, both resolve to one node.
  EXPECT_EQ(images[0], images[1]);
  // Passthrough bytes: the entry data is the original PNG, referenced zero-copy from the file.
  auto logoBytes =
      ReadFileBytes(pag::ProjectPath::Absolute("resources/pagx_to_pag/resource/pag_logo.png"));
  ASSERT_FALSE(logoBytes.empty());
  ASSERT_TRUE(images[0]->data != nullptr);
  EXPECT_EQ(images[0]->data->size(), logoBytes.size());
  EXPECT_EQ(memcmp(images[0]->data->bytes(), logoBytes.data(), logoBytes.size()), 0);
  EXPECT_GT(images[0]->logicalWidth, 0u);
  EXPECT_GT(images[0]->logicalHeight, 0u);
  EXPECT_EQ(images[0]->scaleFactor, 1.0f);
  EXPECT_EQ(images[0]->offsetX, 0.0f);
  EXPECT_EQ(images[0]->offsetY, 0.0f);

  // Single-reference sample: the Image is inlined at the pattern's value-area end and its
  // bytes match the source data-URI image.
  const std::string inlinePath =
      pag::ProjectPath::Absolute("resources/pagx_to_pag/resource/image_data_uri.pagx");
  auto inlineDoc = PAGXImporter::FromFile(inlinePath);
  ASSERT_TRUE(inlineDoc != nullptr);
  ASSERT_TRUE(PAGExporter::ExportToPAG(inlineDoc, "/tmp/image_inline.pag").success);
  auto decodedInline = PAGImporter::LoadFromPAG("/tmp/image_inline.pag");
  ASSERT_TRUE(decodedInline != nullptr);
  Image* decodedImage = nullptr;
  Image* originalImage = nullptr;
  for (auto& node : decodedInline->nodes) {
    if (node->nodeType() == NodeType::Image) {
      decodedImage = static_cast<Image*>(node.get());
    }
  }
  for (auto& node : inlineDoc->nodes) {
    if (node->nodeType() == NodeType::Image) {
      originalImage = static_cast<Image*>(node.get());
    }
  }
  ASSERT_TRUE(decodedImage != nullptr);
  ASSERT_TRUE(originalImage != nullptr);
  ASSERT_TRUE(originalImage->data != nullptr);
  ASSERT_TRUE(decodedImage->data != nullptr);
  EXPECT_EQ(decodedImage->data->size(), originalImage->data->size());
  EXPECT_EQ(memcmp(decodedImage->data->bytes(), originalImage->data->bytes(),
                   originalImage->data->size()),
            0);
}

PAGX_TEST(PAGImporterTest, ImagePatternFieldsRoundTrip) {
  // A pattern whose every field is non-default with distinct byte values: any value-area
  // ordering mismatch between encoder and decoder desynchronizes the read and fails here,
  // instead of hiding behind symmetric bytes.
  auto doc = PAGXDocument::Make(200, 100);
  auto* layer = doc->makeNode<Layer>();
  layer->width = 200;
  layer->height = 100;
  doc->layers.push_back(layer);
  auto* rect = doc->makeNode<Rectangle>();
  rect->size.width = 100;
  rect->size.height = 100;
  layer->contents.push_back(rect);
  auto* fill = doc->makeNode<Fill>();
  auto* pattern = doc->makeNode<ImagePattern>();
  auto* image = doc->makeNode<Image>();
  auto logoBytes =
      ReadFileBytes(pag::ProjectPath::Absolute("resources/pagx_to_pag/resource/pag_logo.png"));
  ASSERT_FALSE(logoBytes.empty());
  image->data = Data::MakeWithCopy(logoBytes.data(), logoBytes.size());
  pattern->image = image;
  pattern->tileModeX = TileMode::Mirror;
  pattern->tileModeY = TileMode::Repeat;
  pattern->filterMode = FilterMode::Nearest;
  pattern->mipmapMode = MipmapMode::Nearest;
  pattern->scaleMode = ScaleMode::Zoom;
  pattern->matrix.a = 2.0f;
  pattern->matrix.d = 3.0f;
  pattern->matrix.tx = 4.0f;
  pattern->matrix.ty = 5.0f;
  pattern->customData = {{"z", "26"}, {"a", "13"}};
  fill->color = pattern;
  layer->contents.push_back(fill);

  auto r1 = PAGExporter::ExportToPAG(doc, "/tmp/pattern_fields_rt1.pag");
  ASSERT_TRUE(r1.success);
  auto decoded = PAGImporter::LoadFromPAG("/tmp/pattern_fields_rt1.pag");
  ASSERT_TRUE(decoded != nullptr);

  auto* decodedPattern =
      static_cast<ImagePattern*>(static_cast<Fill*>(decoded->layers[0]->contents[1])->color);
  ASSERT_TRUE(decodedPattern != nullptr);
  EXPECT_EQ(decodedPattern->tileModeX, TileMode::Mirror);
  EXPECT_EQ(decodedPattern->tileModeY, TileMode::Repeat);
  EXPECT_EQ(decodedPattern->filterMode, FilterMode::Nearest);
  EXPECT_EQ(decodedPattern->mipmapMode, MipmapMode::Nearest);
  EXPECT_EQ(decodedPattern->scaleMode, ScaleMode::Zoom);
  EXPECT_EQ(decodedPattern->matrix.a, 2.0f);
  EXPECT_EQ(decodedPattern->matrix.d, 3.0f);
  EXPECT_EQ(decodedPattern->matrix.tx, 4.0f);
  EXPECT_EQ(decodedPattern->matrix.ty, 5.0f);
  ASSERT_EQ(decodedPattern->customData.size(), 2u);
  EXPECT_EQ(decodedPattern->customData.at("z"), "26");
  EXPECT_EQ(decodedPattern->customData.at("a"), "13");
  ASSERT_TRUE(decodedPattern->image != nullptr);
  ASSERT_TRUE(decodedPattern->image->data != nullptr);
  EXPECT_EQ(decodedPattern->image->data->size(), logoBytes.size());

  auto r2 = PAGExporter::ExportToPAG(decoded, "/tmp/pattern_fields_rt2.pag");
  ASSERT_TRUE(r2.success);
  auto bytes1 = ReadFileBytes("/tmp/pattern_fields_rt1.pag");
  auto bytes2 = ReadFileBytes("/tmp/pattern_fields_rt2.pag");
  ASSERT_EQ(bytes1.size(), bytes2.size());
  EXPECT_EQ(memcmp(bytes1.data(), bytes2.data(), bytes1.size()), 0);
}

PAGX_TEST(PAGImporterTest, EmbeddedGlyphRunRoundTrip) {
  // Vector outlines: the font is inlined at the single glyph run's value-area end; glyph
  // advances, ids, placement, and bounds survive the round trip.
  const std::string vectorPath =
      pag::ProjectPath::Absolute("resources/pagx_to_pag/text_font/embedded_vector_glyphs.pagx");
  auto vectorDoc = PAGXImporter::FromFile(vectorPath);
  ASSERT_TRUE(vectorDoc != nullptr);
  ASSERT_TRUE(PAGExporter::ExportToPAG(vectorDoc, "/tmp/glyph_vector.pag").success);
  auto vectorDecoded = PAGImporter::LoadFromPAG("/tmp/glyph_vector.pag");
  ASSERT_TRUE(vectorDecoded != nullptr);
  auto* vectorText = static_cast<Text*>(vectorDecoded->layers[0]->contents[0]);
  ASSERT_EQ(vectorText->nodeType(), NodeType::Text);
  ASSERT_EQ(vectorText->glyphRuns.size(), 1u);
  auto* run = vectorText->glyphRuns[0];
  EXPECT_EQ(run->glyphs, (std::vector<uint16_t>{1, 2}));
  EXPECT_EQ(run->x, 120.0f);
  EXPECT_EQ(run->y, 140.0f);
  EXPECT_EQ(run->bounds.width, 160.0f);
  EXPECT_EQ(run->bounds.height, 96.0f);
  ASSERT_TRUE(run->font != nullptr);
  EXPECT_EQ(run->font->unitsPerEm, 1000);
  ASSERT_EQ(run->font->glyphs.size(), 2u);
  EXPECT_EQ(run->font->glyphs[0]->advance, 722.0f);
  EXPECT_EQ(run->font->glyphs[1]->advance, 766.0f);
  ASSERT_TRUE(run->font->glyphs[0]->path != nullptr);
  ASSERT_TRUE(run->font->glyphs[1]->path != nullptr);
  EXPECT_TRUE(run->font->glyphs[0]->image == nullptr);

  // Bitmap glyphs: the images ride the ImageTable as mediaType 1 and keep their raw bytes.
  const std::string bitmapPath =
      pag::ProjectPath::Absolute("resources/pagx_to_pag/text_font/embedded_bitmap_glyphs.pagx");
  auto bitmapDoc = PAGXImporter::FromFile(bitmapPath);
  ASSERT_TRUE(bitmapDoc != nullptr);
  ASSERT_TRUE(PAGExporter::ExportToPAG(bitmapDoc, "/tmp/glyph_bitmap.pag").success);
  auto bitmapDecoded = PAGImporter::LoadFromPAG("/tmp/glyph_bitmap.pag");
  ASSERT_TRUE(bitmapDecoded != nullptr);
  auto* bitmapText = static_cast<Text*>(bitmapDecoded->layers[0]->contents[0]);
  ASSERT_EQ(bitmapText->glyphRuns.size(), 1u);
  auto* bitmapRun = bitmapText->glyphRuns[0];
  ASSERT_TRUE(bitmapRun->font != nullptr);
  EXPECT_EQ(bitmapRun->font->unitsPerEm, 100);
  ASSERT_EQ(bitmapRun->font->glyphs.size(), 2u);
  Image* originalImages[2] = {nullptr, nullptr};
  int originalIndex = 0;
  for (auto& node : bitmapDoc->nodes) {
    if (node->nodeType() == NodeType::Glyph) {
      auto* glyph = static_cast<Glyph*>(node.get());
      if (glyph->image != nullptr && originalIndex < 2) {
        originalImages[originalIndex++] = glyph->image;
      }
    }
  }
  Image* decodedImages[2] = {nullptr, nullptr};
  int decodedIndex = 0;
  for (auto* glyph : bitmapRun->font->glyphs) {
    ASSERT_TRUE(glyph->image != nullptr);
    ASSERT_TRUE(glyph->image->data != nullptr);
    ASSERT_LT(decodedIndex, 2);
    decodedImages[decodedIndex++] = glyph->image;
  }
  ASSERT_TRUE(originalImages[0] != nullptr);
  ASSERT_TRUE(originalImages[1] != nullptr);
  EXPECT_EQ(decodedImages[0]->data->size(), originalImages[0]->data->size());
  EXPECT_EQ(memcmp(decodedImages[0]->data->bytes(), originalImages[0]->data->bytes(),
                   originalImages[0]->data->size()),
            0);
  EXPECT_EQ(decodedImages[1]->data->size(), originalImages[1]->data->size());
  EXPECT_EQ(memcmp(decodedImages[1]->data->bytes(), originalImages[1]->data->bytes(),
                   originalImages[1]->data->size()),
            0);
}

PAGX_TEST(PAGImporterTest, LayerMaskNodeRefRoundTrip) {
  // Regression: Layer.mask NodeRef (spec §5.2) survives export → decode with the mask
  // layer resolved by emission-order node index.
  const std::vector<std::string> paths = {
      "resources/cli/verify_c13_simple_rect_mask.pagx",
      "spec/samples/masking.pagx",
  };
  for (auto& rel : paths) {
    auto doc = PAGXImporter::FromFile(pag::ProjectPath::Absolute(rel));
    ASSERT_TRUE(doc != nullptr) << rel;
    ASSERT_TRUE(PAGExporter::ExportToPAG(doc, "/tmp/mask_rt.pag").success) << rel;
    auto decoded = PAGImporter::LoadFromPAG("/tmp/mask_rt.pag");
    ASSERT_TRUE(decoded != nullptr) << rel;
    // Find the layer that had a mask in the source and verify the decoded one has it too.
    Layer* sourceMasked = nullptr;
    for (auto* layer : doc->layers) {
      if (layer->mask != nullptr) {
        sourceMasked = layer;
        break;
      }
    }
    ASSERT_TRUE(sourceMasked != nullptr) << rel;
    Layer* decodedMasked = nullptr;
    for (auto* layer : decoded->layers) {
      if (layer->mask != nullptr) {
        decodedMasked = layer;
        break;
      }
    }
    ASSERT_TRUE(decodedMasked != nullptr) << rel;
    ASSERT_TRUE(decodedMasked->mask->mask != nullptr ? false : true) << rel;
    EXPECT_EQ(decodedMasked->mask->maskType, sourceMasked->maskType) << rel;
  }
}

PAGX_TEST(PAGImporterTest, StaticSamplesRenderEquivalence) {
  // T7-d acceptance: every fully supported static sample must render pixel-identically from the
  // original PAGX XML document and from the exported v4 binary (same machine, no font or image
  // divergence: embedded glyphs carry their own outlines/bitmaps, image bytes pass through).
  std::vector<std::string> paths = {
      "resources/pagx_to_pag/static_animated/pure_static.pagx",
      "resources/pagx_to_pag/layout_flatten/clip_to_bounds.pagx",
      "resources/pagx_to_pag/resource/image_external_shared.pagx",
      "resources/pagx_to_pag/resource/image_data_uri.pagx",
      "resources/pagx_to_pag/text_font/embedded_vector_glyphs.pagx",
      "resources/pagx_to_pag/text_font/embedded_bitmap_glyphs.pagx",
  };
  for (auto& rel : paths) {
    auto doc = PAGXImporter::FromFile(pag::ProjectPath::Absolute(rel));
    ASSERT_TRUE(doc != nullptr) << rel;
    ASSERT_TRUE(PAGExporter::ExportToPAG(doc, "/tmp/equiv_export.pag").success) << rel;
    auto decoded = PAGImporter::LoadFromPAG("/tmp/equiv_export.pag");
    ASSERT_TRUE(decoded != nullptr) << rel;

    auto sceneA = PAGScene::Make(doc);
    auto sceneB = PAGScene::Make(decoded);
    ASSERT_TRUE(sceneA != nullptr) << rel;
    ASSERT_TRUE(sceneB != nullptr) << rel;
    int width = static_cast<int>(doc->width);
    int height = static_cast<int>(doc->height);
    auto surfaceA = PAGSurface::MakeOffscreen(width, height);
    auto surfaceB = PAGSurface::MakeOffscreen(width, height);
    ASSERT_TRUE(surfaceA != nullptr) << rel;
    ASSERT_TRUE(surfaceB != nullptr) << rel;
    ASSERT_TRUE(sceneA->draw(surfaceA)) << rel;
    ASSERT_TRUE(sceneB->draw(surfaceB)) << rel;
    std::vector<uint8_t> pixelsA(static_cast<size_t>(width) * height * 4);
    std::vector<uint8_t> pixelsB(static_cast<size_t>(width) * height * 4);
    ASSERT_TRUE(surfaceA->readPixels(pixelsA.data(), static_cast<size_t>(width) * 4)) << rel;
    ASSERT_TRUE(surfaceB->readPixels(pixelsB.data(), static_cast<size_t>(width) * 4)) << rel;
    EXPECT_EQ(memcmp(pixelsA.data(), pixelsB.data(), pixelsA.size()), 0) << rel;
  }
}

static void RenderToPixels(const std::shared_ptr<PAGXDocument>& doc, std::vector<uint8_t>* pixels,
                           int width, int height) {
  auto scene = PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);
  auto surface = PAGSurface::MakeOffscreen(width, height);
  ASSERT_TRUE(surface != nullptr);
  ASSERT_TRUE(scene->draw(surface));
  pixels->resize(static_cast<size_t>(width) * height * 4);
  ASSERT_TRUE(surface->readPixels(pixels->data(), static_cast<size_t>(width) * 4));
}

PAGX_TEST(PAGImporterTest, SubDocumentRoundTrip) {
  struct Sample {
    const char* dir;
  };
  const std::string base = "resources/pagx_to_pag/sub_document/";
  const std::vector<std::string> dirs = {
      base + "single_reference",
      base + "multi_reference",
      base + "nested",
      base + "subdoc_with_layout",
  };
  for (auto& dir : dirs) {
    auto doc = PAGXImporter::FromFile(pag::ProjectPath::Absolute(dir + "/main.pagx"));
    ASSERT_TRUE(doc != nullptr) << dir;
    ASSERT_TRUE(PAGXExternalFiles::Load(doc.get(), pag::ProjectPath::Absolute(dir + "/"))) << dir;
    auto r1 = PAGExporter::ExportToPAG(doc, "/tmp/subdoc_rt1.pag");
    ASSERT_TRUE(r1.success) << dir;
    auto decoded = PAGImporter::LoadFromPAG("/tmp/subdoc_rt1.pag");
    ASSERT_TRUE(decoded != nullptr) << dir;
    auto r2 = PAGExporter::ExportToPAG(decoded, "/tmp/subdoc_rt2.pag");
    ASSERT_TRUE(r2.success) << dir;
    auto bytes1 = ReadFileBytes("/tmp/subdoc_rt1.pag");
    auto bytes2 = ReadFileBytes("/tmp/subdoc_rt2.pag");
    ASSERT_EQ(bytes1.size(), bytes2.size()) << dir;
    EXPECT_EQ(memcmp(bytes1.data(), bytes2.data(), bytes1.size()), 0) << dir;
  }
}

PAGX_TEST(PAGImporterTest, SubDocumentInstancesAreIndependent) {
  const std::string dir = "resources/pagx_to_pag/sub_document/multi_reference";
  auto doc = PAGXImporter::FromFile(pag::ProjectPath::Absolute(dir + "/main.pagx"));
  ASSERT_TRUE(doc != nullptr);
  ASSERT_TRUE(PAGXExternalFiles::Load(doc.get(), pag::ProjectPath::Absolute(dir + "/")));
  ASSERT_TRUE(PAGExporter::ExportToPAG(doc, "/tmp/subdoc_multi.pag").success);
  auto decoded = PAGImporter::LoadFromPAG("/tmp/subdoc_multi.pag");
  ASSERT_TRUE(decoded != nullptr);
  ASSERT_EQ(decoded->layers.size(), 2u);
  auto* first = decoded->layers[0];
  auto* second = decoded->layers[1];
  ASSERT_TRUE(first->externalDoc != nullptr);
  ASSERT_TRUE(second->externalDoc != nullptr);
  // Every referencing layer decodes its own instance (spec §6.5).
  EXPECT_NE(first->externalDoc.get(), second->externalDoc.get());
  EXPECT_TRUE(first->composition != nullptr);
  EXPECT_TRUE(second->composition != nullptr);
  // The wrapper compositions mirror the sub-document's size.
  EXPECT_EQ(first->composition->width, 80.0f);
  EXPECT_EQ(first->composition->height, 80.0f);
}

PAGX_TEST(PAGImporterTest, NestedSubDocumentDecodes) {
  const std::string dir = "resources/pagx_to_pag/sub_document/nested";
  auto doc = PAGXImporter::FromFile(pag::ProjectPath::Absolute(dir + "/main.pagx"));
  ASSERT_TRUE(doc != nullptr);
  ASSERT_TRUE(PAGXExternalFiles::Load(doc.get(), pag::ProjectPath::Absolute(dir + "/")));
  ASSERT_TRUE(PAGExporter::ExportToPAG(doc, "/tmp/subdoc_nested.pag").success);
  auto decoded = PAGImporter::LoadFromPAG("/tmp/subdoc_nested.pag");
  ASSERT_TRUE(decoded != nullptr);
  auto* rootLayer = decoded->layers[0];
  ASSERT_TRUE(rootLayer->externalDoc != nullptr);
  auto& mid = rootLayer->externalDoc;
  EXPECT_EQ(mid->width, 100.0f);
  // mid's own layers reference leaf (the second layer).
  ASSERT_GE(mid->layers.size(), 2u);
  ASSERT_TRUE(mid->layers[1]->externalDoc != nullptr);
  EXPECT_EQ(mid->layers[1]->externalDoc->width, 60.0f);
  // The diagnostic names survive for byte-identical re-export.
  EXPECT_EQ(mid->diagnosticName, "mid.pagx");
  EXPECT_EQ(mid->layers[1]->externalDoc->diagnosticName, "leaf.pagx");
}

PAGX_TEST(PAGImporterTest, SubDocumentRenderEquivalence) {
  const std::string base = "resources/pagx_to_pag/sub_document/";
  const std::vector<std::string> dirs = {
      base + "single_reference",
      base + "multi_reference",
      base + "nested",
      base + "subdoc_with_layout",
  };
  for (auto& dir : dirs) {
    auto doc = PAGXImporter::FromFile(pag::ProjectPath::Absolute(dir + "/main.pagx"));
    ASSERT_TRUE(doc != nullptr) << dir;
    ASSERT_TRUE(PAGXExternalFiles::Load(doc.get(), pag::ProjectPath::Absolute(dir + "/"))) << dir;
    ASSERT_TRUE(PAGExporter::ExportToPAG(doc, "/tmp/subdoc_equiv.pag").success) << dir;
    auto decoded = PAGImporter::LoadFromPAG("/tmp/subdoc_equiv.pag");
    ASSERT_TRUE(decoded != nullptr) << dir;
    int width = static_cast<int>(doc->width);
    int height = static_cast<int>(doc->height);
    std::vector<uint8_t> pixelsA = {};
    std::vector<uint8_t> pixelsB = {};
    RenderToPixels(doc, &pixelsA, width, height);
    RenderToPixels(decoded, &pixelsB, width, height);
    EXPECT_EQ(memcmp(pixelsA.data(), pixelsB.data(), pixelsA.size()), 0) << dir;
  }
}

PAGX_TEST(PAGImporterTest, MissingSubDocumentWarnsAndExportsEmpty) {
  auto doc = PAGXImporter::FromFile(
      pag::ProjectPath::Absolute("resources/pagx_to_pag/sub_document/missing_reference.pagx"));
  ASSERT_TRUE(doc != nullptr);
  EXPECT_FALSE(PAGXExternalFiles::Load(
      doc.get(), pag::ProjectPath::Absolute("resources/pagx_to_pag/sub_document/")));
  auto result = PAGExporter::ExportToPAG(doc, "/tmp/subdoc_missing.pag");
  ASSERT_TRUE(result.success);
  bool warned = false;
  for (auto& issue : result.warnings) {
    if (issue.code == PAGExportWarning::MissingSubDocument) {
      warned = true;
    }
  }
  EXPECT_TRUE(warned);
  // The unloaded layer exports as an empty composition layer and decodes/render identically.
  auto decoded = PAGImporter::LoadFromPAG("/tmp/subdoc_missing.pag");
  ASSERT_TRUE(decoded != nullptr);
  ASSERT_TRUE(decoded->layers[0]->externalDoc == nullptr);
  int width = static_cast<int>(doc->width);
  int height = static_cast<int>(doc->height);
  std::vector<uint8_t> pixelsA = {};
  std::vector<uint8_t> pixelsB = {};
  RenderToPixels(doc, &pixelsA, width, height);
  RenderToPixels(decoded, &pixelsB, width, height);
  EXPECT_EQ(memcmp(pixelsA.data(), pixelsB.data(), pixelsA.size()), 0);
}

PAGX_TEST(PAGImporterTest, StrokeWithDashesRoundTrip) {
  // Regression: a Stroke whose value area carries more than the color reference (dashes)
  // must keep the inline color source sub-tag at the value-area END (spec §9.0). The
  // decoder used to read the sub-tag right after the colorRef and desynced on the dashes.
  auto doc = PAGXDocument::Make(100, 100);
  auto* layer = doc->makeNode<Layer>();
  layer->width = 100;
  layer->height = 100;
  doc->layers.push_back(layer);
  auto* rect = doc->makeNode<Rectangle>();
  rect->size.width = 80;
  rect->size.height = 80;
  layer->contents.push_back(rect);
  auto* stroke = doc->makeNode<Stroke>();
  auto* solid = doc->makeNode<SolidColor>();
  solid->color = {0.8f, 0.84f, 0.88f, 1.0f, ColorSpace::SRGB};
  stroke->color = solid;
  stroke->width = 2.0f;
  stroke->dashes = {4.0f, 3.0f};
  layer->contents.push_back(stroke);

  ASSERT_TRUE(PAGExporter::ExportToPAG(doc, "/tmp/stroke_dashes.pag").success);
  auto decoded = PAGImporter::LoadFromPAG("/tmp/stroke_dashes.pag");
  ASSERT_TRUE(decoded != nullptr);
  auto* decodedStroke = static_cast<Stroke*>(decoded->layers[0]->contents[1]);
  ASSERT_TRUE(decodedStroke->color != nullptr);
  auto* decodedSolid = static_cast<SolidColor*>(decodedStroke->color);
  EXPECT_NEAR(decodedSolid->color.red, 0.8f, 0.01f);
  EXPECT_EQ(decodedStroke->width, 2.0f);
  ASSERT_EQ(decodedStroke->dashes.size(), 2u);
  EXPECT_EQ(decodedStroke->dashes[0], 4.0f);
  EXPECT_EQ(decodedStroke->dashes[1], 3.0f);

  // Fill with extra values after the color reference (alpha) hits the same layout.
  auto* fill = doc->makeNode<Fill>();
  auto* fillSolid = doc->makeNode<SolidColor>();
  fillSolid->color = {0.2f, 0.4f, 0.9f, 1.0f, ColorSpace::SRGB};
  fill->color = fillSolid;
  fill->alpha = 0.5f;
  layer->contents.push_back(fill);
  ASSERT_TRUE(PAGExporter::ExportToPAG(doc, "/tmp/fill_alpha.pag").success);
  auto decodedFill = PAGImporter::LoadFromPAG("/tmp/fill_alpha.pag");
  ASSERT_TRUE(decodedFill != nullptr);
  auto* fillBack = static_cast<Fill*>(decodedFill->layers[0]->contents[2]);
  ASSERT_TRUE(fillBack->color != nullptr);
  EXPECT_NEAR(static_cast<SolidColor*>(fillBack->color)->color.blue, 0.9f, 0.01f);
  EXPECT_NEAR(fillBack->alpha, 0.5f, 0.01f);
}

PAGX_TEST(PAGImporterTest, DecodedImageRendersIdentically) {
  auto doc = PAGXImporter::FromFile(
      pag::ProjectPath::Absolute("resources/pagx_to_pag/resource/image_data_uri.pagx"));
  ASSERT_TRUE(doc != nullptr);
  ASSERT_TRUE(PAGExporter::ExportToPAG(doc, "/tmp/image_render.pag").success);
  auto decoded = PAGImporter::LoadFromPAG("/tmp/image_render.pag");
  ASSERT_TRUE(decoded != nullptr);

  auto sceneA = PAGScene::Make(doc);
  auto sceneB = PAGScene::Make(decoded);
  ASSERT_TRUE(sceneA != nullptr);
  ASSERT_TRUE(sceneB != nullptr);
  int width = static_cast<int>(doc->width);
  int height = static_cast<int>(doc->height);
  auto surfaceA = PAGSurface::MakeOffscreen(width, height);
  auto surfaceB = PAGSurface::MakeOffscreen(width, height);
  ASSERT_TRUE(surfaceA != nullptr);
  ASSERT_TRUE(surfaceB != nullptr);
  ASSERT_TRUE(sceneA->draw(surfaceA));
  ASSERT_TRUE(sceneB->draw(surfaceB));

  // The passthrough pipeline keeps the image bytes, so the decoded document must render
  // pixel-identically to the original XML document.
  std::vector<uint8_t> pixelsA(static_cast<size_t>(width) * height * 4);
  std::vector<uint8_t> pixelsB(static_cast<size_t>(width) * height * 4);
  ASSERT_TRUE(surfaceA->readPixels(pixelsA.data(), static_cast<size_t>(width) * 4));
  ASSERT_TRUE(surfaceB->readPixels(pixelsB.data(), static_cast<size_t>(width) * 4));
  EXPECT_EQ(memcmp(pixelsA.data(), pixelsB.data(), pixelsA.size()), 0);
}

// Helper: exports → decodes → re-exports → decodes → re-exports, asserting export2 == export3
// (steady-state byte idempotency from the decoded state). Returns false when any step fails.
static bool AssertSteadyStateIdempotency(const std::shared_ptr<PAGXDocument>& doc,
                                         const std::string& tag, int* byteCount) {
  auto r1 = PAGExporter::ExportToPAG(doc, "/tmp/idem_1.pag");
  if (!r1.success) {
    ADD_FAILURE() << tag << ": export1 failed";
    return false;
  }
  auto d1 = PAGImporter::LoadFromPAG("/tmp/idem_1.pag");
  if (d1 == nullptr) {
    ADD_FAILURE() << tag << ": decode1 failed";
    return false;
  }
  auto r2 = PAGExporter::ExportToPAG(d1, "/tmp/idem_2.pag");
  if (!r2.success) {
    ADD_FAILURE() << tag << ": export2 failed";
    return false;
  }
  auto d2 = PAGImporter::LoadFromPAG("/tmp/idem_2.pag");
  if (d2 == nullptr) {
    ADD_FAILURE() << tag << ": decode2 failed";
    return false;
  }
  auto r3 = PAGExporter::ExportToPAG(d2, "/tmp/idem_3.pag");
  if (!r3.success) {
    ADD_FAILURE() << tag << ": export3 failed";
    return false;
  }
  auto bytes2 = ReadFileBytes("/tmp/idem_2.pag");
  auto bytes3 = ReadFileBytes("/tmp/idem_3.pag");
  if (bytes2.size() != bytes3.size()) {
    ADD_FAILURE() << tag << ": size mismatch " << bytes2.size() << " vs " << bytes3.size();
    return false;
  }
  if (memcmp(bytes2.data(), bytes3.data(), bytes2.size()) != 0) {
    ADD_FAILURE() << tag << ": bytes differ";
    return false;
  }
  *byteCount = static_cast<int>(bytes2.size());
  return true;
}

PAGX_TEST(PAGImporterTest, DetectFormatIdempotency) {
  // DetectFormat on the same bytes always returns the same result — across file, raw bytes,
  // and re-encoded output. This is the routing entry point for hosts deciding which loader
  // to use, so any drift here breaks file loading.
  const std::vector<std::pair<std::string, PAGFormat>> cases = {
      {"resources/pagx_to_pag/static_animated/pure_static.pagx", PAGFormat::PAGX},
  };
  for (auto& testCase : cases) {
    auto path = pag::ProjectPath::Absolute(testCase.first);
    EXPECT_EQ(PAGImporter::DetectFormat(path), PAGFormat::PAGX) << testCase.first;
  }

  // Exported v4 file → detect → decode → re-export → detect again: format stable.
  auto doc = PAGXImporter::FromFile(
      pag::ProjectPath::Absolute("resources/pagx_to_pag/static_animated/pure_static.pagx"));
  ASSERT_TRUE(doc != nullptr);
  ASSERT_TRUE(PAGExporter::ExportToPAG(doc, "/tmp/detect_idem.pag").success);
  EXPECT_EQ(PAGImporter::DetectFormat("/tmp/detect_idem.pag"), PAGFormat::PAGv4);
  auto decoded = PAGImporter::LoadFromPAG("/tmp/detect_idem.pag");
  ASSERT_TRUE(decoded != nullptr);
  ASSERT_TRUE(PAGExporter::ExportToPAG(decoded, "/tmp/detect_idem2.pag").success);
  EXPECT_EQ(PAGImporter::DetectFormat("/tmp/detect_idem2.pag"), PAGFormat::PAGv4);

  // Raw bytes variant: same result as the file variant.
  auto bytes = ReadFileBytes("/tmp/detect_idem.pag");
  ASSERT_FALSE(bytes.empty());
  EXPECT_EQ(PAGImporter::DetectFormat(bytes.data(), bytes.size()), PAGFormat::PAGv4);
  auto xmlBytes = ReadFileBytes(
      pag::ProjectPath::Absolute("resources/pagx_to_pag/static_animated/pure_static.pagx"));
  ASSERT_FALSE(xmlBytes.empty());
  EXPECT_EQ(PAGImporter::DetectFormat(xmlBytes.data(), xmlBytes.size()), PAGFormat::PAGX);

  // Edge inputs: empty, short, garbage.
  EXPECT_EQ(PAGImporter::DetectFormat(nullptr, 0), PAGFormat::Unknown);
  EXPECT_EQ(PAGImporter::DetectFormat(nullptr, 100), PAGFormat::Unknown);
  const char* garbage = "not a pag file at all";
  EXPECT_EQ(PAGImporter::DetectFormat(garbage, 21), PAGFormat::Unknown);
  // Truncated PAG header (version 4 but body length claims more than available).
  EXPECT_EQ(PAGImporter::DetectFormat(bytes.data(), 10), PAGFormat::Unknown);

  // BOM-prefixed XML still detects as PAGX.
  std::vector<uint8_t> bomXml = {0xEF, 0xBB, 0xBF, '<', '?', 'x', 'm', 'l'};
  EXPECT_EQ(PAGImporter::DetectFormat(bomXml.data(), bomXml.size()), PAGFormat::PAGX);
}

PAGX_TEST(PAGImporterTest, DecodeResultStability) {
  // Decoding the same file twice produces structurally identical documents: same layer count,
  // same canvas size, same feature presence (masks, compositions, sub-documents, glyph runs).
  // This catches nondeterministic decoding (hash-order dependencies, uninitialized fields).
  auto doc = PAGXImporter::FromFile(
      pag::ProjectPath::Absolute("resources/pagx_to_pag/text_font/embedded_vector_glyphs.pagx"));
  ASSERT_TRUE(doc != nullptr);
  ASSERT_TRUE(PAGExporter::ExportToPAG(doc, "/tmp/stability.pag").success);

  auto first = PAGImporter::LoadFromPAG("/tmp/stability.pag");
  auto second = PAGImporter::LoadFromPAG("/tmp/stability.pag");
  ASSERT_TRUE(first != nullptr);
  ASSERT_TRUE(second != nullptr);

  EXPECT_EQ(first->width, second->width);
  EXPECT_EQ(first->height, second->height);
  ASSERT_EQ(first->layers.size(), second->layers.size());
  for (size_t i = 0; i < first->layers.size(); i++) {
    auto* la = first->layers[i];
    auto* lb = second->layers[i];
    EXPECT_EQ(la->contents.size(), lb->contents.size());
    EXPECT_EQ(la->children.size(), lb->children.size());
    EXPECT_EQ(la->composition != nullptr, lb->composition != nullptr);
    EXPECT_EQ(la->mask != nullptr, lb->mask != nullptr);
    EXPECT_EQ(la->x, lb->x);
    EXPECT_EQ(la->y, lb->y);
    EXPECT_EQ(la->alpha, lb->alpha);
  }

  // Both exports of the two independently decoded docs produce the same bytes.
  ASSERT_TRUE(PAGExporter::ExportToPAG(first, "/tmp/stability_a.pag").success);
  ASSERT_TRUE(PAGExporter::ExportToPAG(second, "/tmp/stability_b.pag").success);
  auto bytesA = ReadFileBytes("/tmp/stability_a.pag");
  auto bytesB = ReadFileBytes("/tmp/stability_b.pag");
  ASSERT_EQ(bytesA.size(), bytesB.size());
  EXPECT_EQ(memcmp(bytesA.data(), bytesB.data(), bytesA.size()), 0);
}

PAGX_TEST(PAGImporterTest, DecodeErrorRecoveryIdempotency) {
  // Malformed inputs consistently return nullptr + a non-empty error, never a partial
  // document, and repeated attempts give the same result (no state leakage between calls).
  auto doc = PAGXImporter::FromFile(
      pag::ProjectPath::Absolute("resources/pagx_to_pag/static_animated/pure_static.pagx"));
  ASSERT_TRUE(doc != nullptr);
  ASSERT_TRUE(PAGExporter::ExportToPAG(doc, "/tmp/error_idem.pag").success);
  auto bytes = ReadFileBytes("/tmp/error_idem.pag");
  ASSERT_FALSE(bytes.empty());

  // Truncated at three different points: header, mid-body, last byte.
  for (auto cut : {static_cast<size_t>(16), bytes.size() / 2, bytes.size() - 1}) {
    FILE* f = fopen("/tmp/error_idem_trunc.pag", "wb");
    ASSERT_TRUE(f != nullptr);
    fwrite(bytes.data(), 1, cut, f);
    fclose(f);
    // Decode twice — both attempts must fail identically.
    PAGImportResult result1 = {};
    PAGImportResult result2 = {};
    auto first = PAGImporter::LoadFromPAG("/tmp/error_idem_trunc.pag", &result1);
    auto second = PAGImporter::LoadFromPAG("/tmp/error_idem_trunc.pag", &result2);
    EXPECT_EQ(first, nullptr);
    EXPECT_EQ(second, nullptr);
    EXPECT_FALSE(result1.success);
    EXPECT_FALSE(result2.success);
    EXPECT_FALSE(result1.error.empty());
    EXPECT_FALSE(result2.error.empty());
  }

  // Garbage file: repeated decode is consistently null.
  FILE* f = fopen("/tmp/error_idem_garbage.pag", "wb");
  ASSERT_TRUE(f != nullptr);
  fwrite("this is not a pag file", 1, 22, f);
  fclose(f);
  PAGImportResult garbageResult = {};
  EXPECT_EQ(PAGImporter::LoadFromPAG("/tmp/error_idem_garbage.pag", &garbageResult), nullptr);
  EXPECT_FALSE(garbageResult.success);

  // After all failures, a valid file still decodes correctly (no state pollution).
  PAGImportResult validResult = {};
  auto valid = PAGImporter::LoadFromPAG("/tmp/error_idem.pag", &validResult);
  EXPECT_TRUE(valid != nullptr);
  EXPECT_TRUE(validResult.success);
}

PAGX_TEST(PAGImporterTest, LayerStyleRoundTrip) {
  // All five layer styles (spec §9.5 #355-359) survive export → decode with every field
  // intact, and the pipeline is byte-idempotent from the decoded state.
  auto doc = PAGXDocument::Make(200, 300);
  struct StyleCase {
    const char* name;
    LayerStyle* style;
  };
  std::vector<StyleCase> cases = {};

  auto* dropShadow = doc->makeNode<DropShadowStyle>();
  dropShadow->offsetX = 4.0f;
  dropShadow->offsetY = 6.0f;
  dropShadow->blurX = 8.0f;
  dropShadow->blurY = 8.0f;
  dropShadow->color = {0.5f, 0.0f, 0.0f, 0.6f, ColorSpace::SRGB};
  dropShadow->showBehindLayer = false;
  dropShadow->blendMode = BlendMode::Multiply;
  dropShadow->excludeChildEffects = true;
  cases.push_back({"dropShadow", dropShadow});

  auto* innerShadow = doc->makeNode<InnerShadowStyle>();
  innerShadow->offsetX = 2.0f;
  innerShadow->offsetY = 3.0f;
  innerShadow->blurX = 5.0f;
  innerShadow->color = {0.0f, 0.0f, 0.5f, 0.8f, ColorSpace::SRGB};
  cases.push_back({"innerShadow", innerShadow});

  auto* backgroundBlur = doc->makeNode<BackgroundBlurStyle>();
  backgroundBlur->blurX = 10.0f;
  backgroundBlur->blurY = 12.0f;
  backgroundBlur->tileMode = TileMode::Repeat;
  cases.push_back({"backgroundBlur", backgroundBlur});

  auto* noise = doc->makeNode<NoiseStyle>();
  noise->mode = NoiseMode::Duo;
  noise->size = 6.0f;
  noise->density = 0.7f;
  noise->seed = 42.0f;
  noise->color = {0.1f, 0.2f, 0.3f, 1.0f, ColorSpace::SRGB};
  noise->firstColor = {0.0f, 0.0f, 0.0f, 1.0f, ColorSpace::SRGB};
  noise->secondColor = {1.0f, 1.0f, 1.0f, 1.0f, ColorSpace::SRGB};
  noise->opacity = 0.3f;
  cases.push_back({"noise", noise});

  auto* glass = doc->makeNode<GlassStyle>();
  glass->refraction = 60.0f;
  glass->depth = 15.0f;
  glass->frost = 8.0f;
  glass->dispersion = 40.0f;
  glass->splay = 10.0f;
  glass->lightAngle = 90.0f;
  glass->lightIntensity = 70.0f;
  cases.push_back({"glass", glass});

  auto* layer = doc->makeNode<Layer>();
  layer->width = 200;
  layer->height = 300;
  for (auto& styleCase : cases) {
    layer->styles.push_back(styleCase.style);
  }
  doc->layers.push_back(layer);

  ASSERT_TRUE(PAGExporter::ExportToPAG(doc, "/tmp/style_rt1.pag").success);
  auto decoded = PAGImporter::LoadFromPAG("/tmp/style_rt1.pag");
  ASSERT_TRUE(decoded != nullptr);
  ASSERT_EQ(decoded->layers.size(), 1u);
  ASSERT_EQ(decoded->layers[0]->styles.size(), 5u);

  auto* ds = static_cast<DropShadowStyle*>(decoded->layers[0]->styles[0]);
  EXPECT_EQ(ds->offsetX, 4.0f);
  EXPECT_EQ(ds->offsetY, 6.0f);
  EXPECT_EQ(ds->blurX, 8.0f);
  EXPECT_NEAR(ds->color.red, 0.5f, 0.01f);
  EXPECT_FALSE(ds->showBehindLayer);
  EXPECT_EQ(ds->blendMode, BlendMode::Multiply);
  EXPECT_TRUE(ds->excludeChildEffects);

  auto* is = static_cast<InnerShadowStyle*>(decoded->layers[0]->styles[1]);
  EXPECT_EQ(is->offsetX, 2.0f);
  EXPECT_EQ(is->blurX, 5.0f);
  EXPECT_NEAR(is->color.blue, 0.5f, 0.01f);

  auto* bb = static_cast<BackgroundBlurStyle*>(decoded->layers[0]->styles[2]);
  EXPECT_EQ(bb->blurX, 10.0f);
  EXPECT_EQ(bb->tileMode, TileMode::Repeat);

  auto* ns = static_cast<NoiseStyle*>(decoded->layers[0]->styles[3]);
  EXPECT_EQ(ns->mode, NoiseMode::Duo);
  EXPECT_EQ(ns->size, 6.0f);
  EXPECT_EQ(ns->seed, 42.0f);
  EXPECT_NEAR(ns->opacity, 0.3f, 0.01f);

  auto* gs = static_cast<GlassStyle*>(decoded->layers[0]->styles[4]);
  EXPECT_EQ(gs->refraction, 60.0f);
  EXPECT_EQ(gs->lightAngle, 90.0f);

  // Steady-state idempotency.
  int byteCount = 0;
  EXPECT_TRUE(AssertSteadyStateIdempotency(decoded, "layer-styles", &byteCount));
  EXPECT_GT(byteCount, 0);
}

PAGX_TEST(PAGImporterTest, LayerFilterRoundTrip) {
  // All six layer filters (spec §9.5 #360-365) survive export → decode with fields intact.
  auto doc = PAGXDocument::Make(200, 200);
  auto* layer = doc->makeNode<Layer>();
  layer->width = 200;
  layer->height = 200;
  doc->layers.push_back(layer);

  auto* blur = doc->makeNode<BlurFilter>();
  blur->blurX = 5.0f;
  blur->blurY = 7.0f;
  blur->tileMode = TileMode::Repeat;
  layer->filters.push_back(blur);

  auto* dropShadow = doc->makeNode<DropShadowFilter>();
  dropShadow->offsetX = 3.0f;
  dropShadow->blurX = 4.0f;
  dropShadow->color = {0.2f, 0.4f, 0.6f, 0.8f, ColorSpace::SRGB};
  dropShadow->shadowOnly = true;
  layer->filters.push_back(dropShadow);

  auto* innerShadow = doc->makeNode<InnerShadowFilter>();
  innerShadow->offsetY = 5.0f;
  innerShadow->blurY = 6.0f;
  layer->filters.push_back(innerShadow);

  auto* blend = doc->makeNode<BlendFilter>();
  blend->color = {0.9f, 0.1f, 0.1f, 1.0f, ColorSpace::SRGB};
  blend->blendMode = BlendMode::Screen;
  layer->filters.push_back(blend);

  auto* colorMatrix = doc->makeNode<ColorMatrixFilter>();
  colorMatrix->matrix = {1.5f, 0, 0, 0, 0.1f, 0, 0.8f, 0, 0, 0, 0, 0, 1.2f, 0, 0, 0, 0, 0, 1, 0};
  layer->filters.push_back(colorMatrix);

  auto* noise = doc->makeNode<NoiseFilter>();
  noise->mode = NoiseMode::Multi;
  noise->size = 8.0f;
  noise->density = 0.9f;
  noise->seed = 123.0f;
  noise->blendMode = BlendMode::Overlay;
  noise->opacity = 0.4f;
  layer->filters.push_back(noise);

  ASSERT_TRUE(PAGExporter::ExportToPAG(doc, "/tmp/filter_rt1.pag").success);
  auto decoded = PAGImporter::LoadFromPAG("/tmp/filter_rt1.pag");
  ASSERT_TRUE(decoded != nullptr);
  ASSERT_EQ(decoded->layers[0]->filters.size(), 6u);

  auto* dBlur = static_cast<BlurFilter*>(decoded->layers[0]->filters[0]);
  EXPECT_EQ(dBlur->blurX, 5.0f);
  EXPECT_EQ(dBlur->blurY, 7.0f);
  EXPECT_EQ(dBlur->tileMode, TileMode::Repeat);

  auto* dDrop = static_cast<DropShadowFilter*>(decoded->layers[0]->filters[1]);
  EXPECT_EQ(dDrop->offsetX, 3.0f);
  EXPECT_EQ(dDrop->blurX, 4.0f);
  EXPECT_NEAR(dDrop->color.alpha, 0.8f, 0.01f);
  EXPECT_TRUE(dDrop->shadowOnly);

  auto* dInner = static_cast<InnerShadowFilter*>(decoded->layers[0]->filters[2]);
  EXPECT_EQ(dInner->offsetY, 5.0f);
  EXPECT_EQ(dInner->blurY, 6.0f);

  auto* dBlend = static_cast<BlendFilter*>(decoded->layers[0]->filters[3]);
  EXPECT_NEAR(dBlend->color.red, 0.9f, 0.01f);
  EXPECT_EQ(dBlend->blendMode, BlendMode::Screen);

  auto* dMatrix = static_cast<ColorMatrixFilter*>(decoded->layers[0]->filters[4]);
  EXPECT_EQ(dMatrix->matrix[0], 1.5f);
  EXPECT_EQ(dMatrix->matrix[2 * 5 + 2], 1.2f);

  auto* dNoise = static_cast<NoiseFilter*>(decoded->layers[0]->filters[5]);
  EXPECT_EQ(dNoise->mode, NoiseMode::Multi);
  EXPECT_EQ(dNoise->size, 8.0f);
  EXPECT_EQ(dNoise->seed, 123.0f);
  EXPECT_NEAR(dNoise->opacity, 0.4f, 0.01f);

  int byteCount = 0;
  EXPECT_TRUE(AssertSteadyStateIdempotency(decoded, "layer-filters", &byteCount));
}

PAGX_TEST(PAGImporterTest, TextBoxRoundTrip) {
  // TextBox (spec §9.2 #431/432) survives export → decode with all layout fields and child
  // Text elements intact.
  auto doc = PAGXDocument::Make(300, 200);
  auto* layer = doc->makeNode<Layer>();
  layer->width = 300;
  layer->height = 200;
  doc->layers.push_back(layer);

  auto* textBox = doc->makeNode<TextBox>();
  textBox->position.x = 20.0f;
  textBox->position.y = 30.0f;
  textBox->width = 200.0f;
  textBox->height = 100.0f;
  textBox->padding = {5.0f, 10.0f, 15.0f, 20.0f};
  textBox->textAlign = TextAlign::Center;
  textBox->paragraphAlign = ParagraphAlign::Middle;
  textBox->writingMode = WritingMode::Vertical;
  textBox->lineHeight = 24.0f;
  textBox->wordWrap = false;
  textBox->overflow = Overflow::Hidden;
  textBox->alpha = 0.8f;

  auto* text = doc->makeNode<Text>();
  text->text = "Hello";
  text->fontSize = 16.0f;
  auto* fill = doc->makeNode<Fill>();
  auto* solid = doc->makeNode<SolidColor>();
  solid->color = {0.1f, 0.5f, 0.9f, 1.0f, ColorSpace::SRGB};
  fill->color = solid;
  textBox->elements.push_back(text);
  textBox->elements.push_back(fill);
  layer->contents.push_back(textBox);

  ASSERT_TRUE(PAGExporter::ExportToPAG(doc, "/tmp/textbox_rt1.pag").success);
  auto decoded = PAGImporter::LoadFromPAG("/tmp/textbox_rt1.pag");
  ASSERT_TRUE(decoded != nullptr);
  ASSERT_EQ(decoded->layers.size(), 1u);
  ASSERT_EQ(decoded->layers[0]->contents.size(), 1u);

  auto* dTextBox = static_cast<TextBox*>(decoded->layers[0]->contents[0]);
  ASSERT_EQ(dTextBox->nodeType(), NodeType::TextBox);
  EXPECT_EQ(dTextBox->position.x, 20.0f);
  EXPECT_EQ(dTextBox->width, 200.0f);
  EXPECT_EQ(dTextBox->padding.top, 5.0f);
  EXPECT_EQ(dTextBox->padding.left, 20.0f);
  EXPECT_EQ(dTextBox->textAlign, TextAlign::Center);
  EXPECT_EQ(dTextBox->paragraphAlign, ParagraphAlign::Middle);
  EXPECT_EQ(dTextBox->writingMode, WritingMode::Vertical);
  EXPECT_EQ(dTextBox->lineHeight, 24.0f);
  EXPECT_FALSE(dTextBox->wordWrap);
  EXPECT_EQ(dTextBox->overflow, Overflow::Hidden);
  EXPECT_NEAR(dTextBox->alpha, 0.8f, 0.01f);
  ASSERT_EQ(dTextBox->elements.size(), 2u);
  auto* dText = static_cast<Text*>(dTextBox->elements[0]);
  EXPECT_EQ(dText->text, "Hello");
  EXPECT_EQ(dText->fontSize, 16.0f);
  auto* dFill = static_cast<Fill*>(dTextBox->elements[1]);
  ASSERT_TRUE(dFill->color != nullptr);

  // Render equivalence: same machine, same fonts.
  auto sceneA = PAGScene::Make(doc);
  auto sceneB = PAGScene::Make(decoded);
  ASSERT_TRUE(sceneA != nullptr);
  ASSERT_TRUE(sceneB != nullptr);
  auto surfaceA = PAGSurface::MakeOffscreen(300, 200);
  auto surfaceB = PAGSurface::MakeOffscreen(300, 200);
  ASSERT_TRUE(surfaceA != nullptr && surfaceB != nullptr);
  ASSERT_TRUE(sceneA->draw(surfaceA));
  ASSERT_TRUE(sceneB->draw(surfaceB));
  std::vector<uint8_t> pixelsA(300 * 200 * 4);
  std::vector<uint8_t> pixelsB(300 * 200 * 4);
  ASSERT_TRUE(surfaceA->readPixels(pixelsA.data(), 300 * 4));
  ASSERT_TRUE(surfaceB->readPixels(pixelsB.data(), 300 * 4));
  // TextBox re-measures text at render time; allow quantization-level tolerance.
  {
    auto total = static_cast<size_t>(300) * 200;
    size_t diffPixels = 0;
    uint8_t maxChannelDiff = 0;
    for (size_t i = 0; i < total; i++) {
      const uint8_t* pa = pixelsA.data() + i * 4;
      const uint8_t* pb = pixelsB.data() + i * 4;
      for (int c = 0; c < 4; c++) {
        auto delta = static_cast<uint8_t>(std::abs(static_cast<int>(pa[c]) - pb[c]));
        if (delta > maxChannelDiff) {
          maxChannelDiff = delta;
        }
      }
      if (pa[0] != pb[0] || pa[1] != pb[1] || pa[2] != pb[2] || pa[3] != pb[3]) {
        diffPixels++;
      }
    }
    EXPECT_LE(maxChannelDiff, 5);
    EXPECT_LE(diffPixels, total / 100);
  }

  // Steady-state idempotency.
  int byteCount = 0;
  EXPECT_TRUE(AssertSteadyStateIdempotency(decoded, "textbox", &byteCount));
}

PAGX_TEST(PAGImporterTest, RepeaterRoundTrip) {
  // Repeater (spec §9.3 #435) survives export → decode with all fields intact.
  auto doc = PAGXDocument::Make(300, 300);
  auto* layer = doc->makeNode<Layer>();
  layer->width = 300;
  layer->height = 300;
  doc->layers.push_back(layer);

  auto* rect = doc->makeNode<Rectangle>();
  rect->size.width = 50;
  rect->size.height = 50;
  layer->contents.push_back(rect);
  auto* fill = doc->makeNode<Fill>();
  auto* solid = doc->makeNode<SolidColor>();
  solid->color = {0.8f, 0.2f, 0.2f, 1.0f, ColorSpace::SRGB};
  fill->color = solid;
  layer->contents.push_back(fill);

  auto* repeater = doc->makeNode<Repeater>();
  repeater->copies = 5.0f;
  repeater->offset = 0.5f;
  repeater->order = RepeaterOrder::AboveOriginal;
  repeater->anchor = {25.0f, 25.0f};
  repeater->position = {30.0f, 20.0f};
  repeater->rotation = 15.0f;
  repeater->scale = {0.9f, 0.95f};
  repeater->startAlpha = 1.0f;
  repeater->endAlpha = 0.3f;
  layer->contents.push_back(repeater);

  ASSERT_TRUE(PAGExporter::ExportToPAG(doc, "/tmp/repeater_rt1.pag").success);
  auto decoded = PAGImporter::LoadFromPAG("/tmp/repeater_rt1.pag");
  ASSERT_TRUE(decoded != nullptr);
  ASSERT_EQ(decoded->layers[0]->contents.size(), 3u);

  auto* dRepeater = static_cast<Repeater*>(decoded->layers[0]->contents[2]);
  ASSERT_EQ(dRepeater->nodeType(), NodeType::Repeater);
  EXPECT_EQ(dRepeater->copies, 5.0f);
  EXPECT_EQ(dRepeater->offset, 0.5f);
  EXPECT_EQ(dRepeater->order, RepeaterOrder::AboveOriginal);
  EXPECT_EQ(dRepeater->anchor.x, 25.0f);
  EXPECT_EQ(dRepeater->position.x, 30.0f);
  EXPECT_EQ(dRepeater->rotation, 15.0f);
  EXPECT_EQ(dRepeater->scale.x, 0.9f);
  EXPECT_NEAR(dRepeater->endAlpha, 0.3f, 0.01f);

  // Render equivalence: repeater copies render identically.
  auto sceneA = PAGScene::Make(doc);
  auto sceneB = PAGScene::Make(decoded);
  ASSERT_TRUE(sceneA != nullptr && sceneB != nullptr);
  auto surfaceA = PAGSurface::MakeOffscreen(300, 300);
  auto surfaceB = PAGSurface::MakeOffscreen(300, 300);
  ASSERT_TRUE(surfaceA != nullptr && surfaceB != nullptr);
  ASSERT_TRUE(sceneA->draw(surfaceA));
  ASSERT_TRUE(sceneB->draw(surfaceB));
  std::vector<uint8_t> pixelsA(300 * 300 * 4);
  std::vector<uint8_t> pixelsB(300 * 300 * 4);
  ASSERT_TRUE(surfaceA->readPixels(pixelsA.data(), 300 * 4));
  ASSERT_TRUE(surfaceB->readPixels(pixelsB.data(), 300 * 4));
  // Repeater alpha goes through u8 quantization; allow quantization-level tolerance.
  {
    auto total = static_cast<size_t>(300) * 300;
    size_t diffPixels = 0;
    uint8_t maxChannelDiff = 0;
    for (size_t i = 0; i < total; i++) {
      const uint8_t* pa = pixelsA.data() + i * 4;
      const uint8_t* pb = pixelsB.data() + i * 4;
      for (int c = 0; c < 4; c++) {
        auto delta = static_cast<uint8_t>(std::abs(static_cast<int>(pa[c]) - pb[c]));
        if (delta > maxChannelDiff) {
          maxChannelDiff = delta;
        }
      }
      if (pa[0] != pb[0] || pa[1] != pb[1] || pa[2] != pb[2] || pa[3] != pb[3]) {
        diffPixels++;
      }
    }
    EXPECT_LE(maxChannelDiff, 8);
    EXPECT_LE(diffPixels, total / 50);
  }

  int byteCount = 0;
  EXPECT_TRUE(AssertSteadyStateIdempotency(decoded, "repeater", &byteCount));
}

PAGX_TEST(PAGImporterTest, TextModifierRoundTrip) {
  // TextModifier + RangeSelector (spec §9.4 #428/#429/#436) round-trip with full fields.
  auto doc = PAGXDocument::Make(400, 200);
  auto* layer = doc->makeNode<Layer>();
  layer->width = 400;
  layer->height = 200;
  doc->layers.push_back(layer);

  auto* group = doc->makeNode<Group>();
  auto* text = doc->makeNode<Text>();
  text->text = "Animate";
  text->fontSize = 24.0f;
  group->elements.push_back(text);
  auto* fill = doc->makeNode<Fill>();
  auto* solid = doc->makeNode<SolidColor>();
  solid->color = {0.2f, 0.6f, 0.4f, 1.0f, ColorSpace::SRGB};
  fill->color = solid;
  group->elements.push_back(fill);

  auto* modifier = doc->makeNode<TextModifier>();
  modifier->position = {5.0f, -10.0f};
  modifier->rotation = 30.0f;
  modifier->scale = {1.2f, 0.8f};
  modifier->alpha = 0.6f;
  modifier->fillColor = Color{0.9f, 0.1f, 0.5f, 1.0f, ColorSpace::SRGB};
  modifier->strokeWidth = 2.5f;

  auto* selector = doc->makeNode<RangeSelector>();
  selector->start = 0.2f;
  selector->end = 0.8f;
  selector->offset = 0.1f;
  selector->unit = SelectorUnit::Index;
  selector->shape = SelectorShape::RampUp;
  selector->easeIn = 0.3f;
  selector->easeOut = 0.4f;
  selector->mode = SelectorMode::Intersect;
  selector->weight = 0.7f;
  selector->randomOrder = true;
  selector->randomSeed = 42;
  modifier->selectors.push_back(selector);
  group->elements.push_back(modifier);
  layer->contents.push_back(group);

  ASSERT_TRUE(PAGExporter::ExportToPAG(doc, "/tmp/textmod_rt1.pag").success);
  auto decoded = PAGImporter::LoadFromPAG("/tmp/textmod_rt1.pag");
  ASSERT_TRUE(decoded != nullptr);
  ASSERT_EQ(decoded->layers[0]->contents.size(), 1u);
  auto* dGroup = static_cast<Group*>(decoded->layers[0]->contents[0]);
  ASSERT_EQ(dGroup->elements.size(), 3u);

  auto* dModifier = static_cast<TextModifier*>(dGroup->elements[2]);
  ASSERT_EQ(dModifier->nodeType(), NodeType::TextModifier);
  EXPECT_EQ(dModifier->position.x, 5.0f);
  EXPECT_EQ(dModifier->rotation, 30.0f);
  EXPECT_EQ(dModifier->scale.x, 1.2f);
  EXPECT_NEAR(dModifier->alpha, 0.6f, 0.01f);
  ASSERT_TRUE(dModifier->fillColor.has_value());
  EXPECT_NEAR(dModifier->fillColor->red, 0.9f, 0.01f);
  ASSERT_TRUE(dModifier->strokeWidth.has_value());
  EXPECT_EQ(dModifier->strokeWidth.value(), 2.5f);

  ASSERT_EQ(dModifier->selectors.size(), 1u);
  auto* dSelector = static_cast<RangeSelector*>(dModifier->selectors[0]);
  EXPECT_EQ(dSelector->start, 0.2f);
  EXPECT_EQ(dSelector->end, 0.8f);
  EXPECT_EQ(dSelector->unit, SelectorUnit::Index);
  EXPECT_EQ(dSelector->shape, SelectorShape::RampUp);
  EXPECT_EQ(dSelector->mode, SelectorMode::Intersect);
  EXPECT_NEAR(dSelector->weight, 0.7f, 0.001f);
  EXPECT_TRUE(dSelector->randomOrder);
  EXPECT_EQ(dSelector->randomSeed, 42);

  int byteCount = 0;
  EXPECT_TRUE(AssertSteadyStateIdempotency(decoded, "text-modifier", &byteCount));
}

// Collects every layer that carries a mask, pre-order over the children tree.
static void CollectMaskedLayers(const std::vector<Layer*>& layers, std::vector<Layer*>* out) {
  for (auto* layer : layers) {
    if (layer->mask != nullptr) {
      out->push_back(layer);
    }
    CollectMaskedLayers(layer->children, out);
  }
}

struct MaskAudit {
  int maskedCount = 0;
  int selfMasked = 0;
  int ancestorMasked = 0;
};

// A decoded mask that points at the masked layer itself or at one of its ancestors is the
// structural condition that sends tgfx Layer::computeBounds into infinite recursion, so the
// audit counts those separately from well-formed sibling masks.
static void AuditLayerMasks(const std::vector<Layer*>& layers, const std::vector<Layer*>& ancestry,
                            MaskAudit* audit) {
  for (auto* layer : layers) {
    if (layer->mask != nullptr) {
      audit->maskedCount++;
      if (layer->mask == layer) {
        audit->selfMasked++;
      }
      for (auto* ancestor : ancestry) {
        if (layer->mask == ancestor) {
          audit->ancestorMasked++;
        }
      }
    }
    auto extendedAncestry = ancestry;
    extendedAncestry.push_back(layer);
    AuditLayerMasks(layer->children, extendedAncestry, audit);
  }
}

PAGX_TEST(PAGImporterTest, ClipAndMaskSampleMaskIdentity) {
  // Regression for the clip_and_mask SIGSEGV: NodeRef indices that ignored style, gradient
  // stop and selector nodes shifted decoded masks onto ancestor layers, and tgfx
  // computeBounds recursed forever walking down children and jumping back up the mask edge.
  auto doc = PAGXImporter::FromFile(
      pag::ProjectPath::Absolute("resources/pagx_to_html/clip_and_mask.pagx"));
  ASSERT_TRUE(doc != nullptr);
  ASSERT_TRUE(PAGExporter::ExportToPAG(doc, "/tmp/cam_identity.pag").success);
  auto decoded = PAGImporter::LoadFromPAG("/tmp/cam_identity.pag");
  ASSERT_TRUE(decoded != nullptr);

  MaskAudit audit = {};
  AuditLayerMasks(decoded->layers, {}, &audit);
  EXPECT_EQ(audit.maskedCount, 3);
  EXPECT_EQ(audit.selfMasked, 0);
  EXPECT_EQ(audit.ancestorMasked, 0);

  // Every mask target is the column's hidden shape layer: alpha and luminance columns use an
  // Ellipse, the contour column a five-point Polystar, while each masked layer draws a Rectangle.
  // The invisible shape content pins the NodeRef to the exact intended node instead of a
  // type-only check that a shifted index could still satisfy.
  std::vector<Layer*> maskedLayers = {};
  CollectMaskedLayers(decoded->layers, &maskedLayers);
  ASSERT_EQ(maskedLayers.size(), 3u);
  int alphaCount = 0;
  int luminanceCount = 0;
  int contourCount = 0;
  for (auto* masked : maskedLayers) {
    ASSERT_TRUE(masked->mask != nullptr);
    EXPECT_FALSE(masked->mask->visible);
    ASSERT_FALSE(masked->mask->contents.empty());
    auto maskShape = masked->mask->contents[0]->nodeType();
    if (masked->maskType == MaskType::Alpha) {
      alphaCount++;
      EXPECT_EQ(maskShape, NodeType::Ellipse);
    } else if (masked->maskType == MaskType::Luminance) {
      luminanceCount++;
      EXPECT_EQ(maskShape, NodeType::Ellipse);
    } else if (masked->maskType == MaskType::Contour) {
      contourCount++;
      EXPECT_EQ(maskShape, NodeType::Polystar);
    }
  }
  EXPECT_EQ(alphaCount, 1);
  EXPECT_EQ(luminanceCount, 1);
  EXPECT_EQ(contourCount, 1);

  // Pixel equivalence with the XML source (same machine, same font resolution).
  auto sceneA = PAGScene::Make(doc);
  auto sceneB = PAGScene::Make(decoded);
  ASSERT_TRUE(sceneA != nullptr);
  ASSERT_TRUE(sceneB != nullptr);
  int width = static_cast<int>(doc->width);
  int height = static_cast<int>(doc->height);
  auto surfaceA = PAGSurface::MakeOffscreen(width, height);
  auto surfaceB = PAGSurface::MakeOffscreen(width, height);
  ASSERT_TRUE(surfaceA != nullptr);
  ASSERT_TRUE(surfaceB != nullptr);
  ASSERT_TRUE(sceneA->draw(surfaceA));
  ASSERT_TRUE(sceneB->draw(surfaceB));
  std::vector<uint8_t> pixelsA(static_cast<size_t>(width) * height * 4);
  std::vector<uint8_t> pixelsB(static_cast<size_t>(width) * height * 4);
  ASSERT_TRUE(surfaceA->readPixels(pixelsA.data(), static_cast<size_t>(width) * 4));
  ASSERT_TRUE(surfaceB->readPixels(pixelsB.data(), static_cast<size_t>(width) * 4));
  EXPECT_EQ(memcmp(pixelsA.data(), pixelsB.data(), pixelsA.size()), 0);
}

// Returns the layer whose children list contains the target, or null when the target is a
// root layer or absent from the tree.
static Layer* FindParentOf(const std::vector<Layer*>& layers, const Layer* target) {
  for (auto* layer : layers) {
    for (auto* child : layer->children) {
      if (child == target) {
        return layer;
      }
    }
    auto* found = FindParentOf(layer->children, target);
    if (found != nullptr) {
      return found;
    }
  }
  return nullptr;
}

PAGX_TEST(PAGImporterTest, MaskNodeRefIndexShiftSources) {
  // The node index walk must count every node the decoder creates in emission order: layer
  // styles and filters (between contents and children), gradient stops, and TextModifier
  // selectors. This document stacks all of them before the mask pair so any missed category
  // shifts the mask NodeRef onto the wrong node.
  auto doc = PAGXDocument::Make(200, 200);
  auto* root = doc->makeNode<Layer>();
  doc->layers.push_back(root);

  auto* shadow = doc->makeNode<DropShadowStyle>();
  shadow->offsetX = 2.0f;
  shadow->offsetY = 3.0f;
  shadow->blurX = 4.0f;
  shadow->blurY = 4.0f;
  shadow->color = {0.0f, 0.0f, 0.0f, 0.25f, ColorSpace::SRGB};
  root->styles.push_back(shadow);

  auto* gradientHolder = doc->makeNode<Layer>();
  root->children.push_back(gradientHolder);
  auto* rect = doc->makeNode<Rectangle>();
  rect->size = {100.0f, 100.0f};
  gradientHolder->contents.push_back(rect);
  auto* fill = doc->makeNode<Fill>();
  auto* gradient = doc->makeNode<LinearGradient>();
  gradient->startPoint = {0.0f, 0.0f};
  gradient->endPoint = {1.0f, 1.0f};
  for (int i = 0; i < 3; i++) {
    auto* stop = doc->makeNode<ColorStop>();
    stop->offset = 0.5f * static_cast<float>(i);
    stop->color = {0.5f * static_cast<float>(i), 0.2f, 0.8f, 1.0f, ColorSpace::SRGB};
    gradient->colorStops.push_back(stop);
  }
  fill->color = gradient;
  gradientHolder->contents.push_back(fill);
  auto* group = doc->makeNode<Group>();
  auto* text = doc->makeNode<Text>();
  text->text = "AB";
  text->fontSize = 20.0f;
  group->elements.push_back(text);
  auto* modifier = doc->makeNode<TextModifier>();
  modifier->rotation = 10.0f;
  for (int i = 0; i < 2; i++) {
    auto* selector = doc->makeNode<RangeSelector>();
    selector->start = 0.1f * static_cast<float>(i);
    selector->end = 0.9f;
    modifier->selectors.push_back(selector);
  }
  group->elements.push_back(modifier);
  gradientHolder->contents.push_back(group);

  auto* holder = doc->makeNode<Layer>();
  root->children.push_back(holder);
  auto* maskLayer = doc->makeNode<Layer>();
  maskLayer->name = "theMask";
  maskLayer->visible = false;
  auto* ellipse = doc->makeNode<Ellipse>();
  ellipse->position = {50.0f, 50.0f};
  ellipse->size = {80.0f, 80.0f};
  maskLayer->contents.push_back(ellipse);
  auto* maskFill = doc->makeNode<Fill>();
  auto* white = doc->makeNode<SolidColor>();
  white->color = {1.0f, 1.0f, 1.0f, 1.0f, ColorSpace::SRGB};
  maskFill->color = white;
  maskLayer->contents.push_back(maskFill);
  holder->children.push_back(maskLayer);
  auto* maskedLayer = doc->makeNode<Layer>();
  maskedLayer->mask = maskLayer;
  maskedLayer->maskType = MaskType::Alpha;
  auto* maskedRect = doc->makeNode<Rectangle>();
  maskedRect->position = {50.0f, 50.0f};
  maskedRect->size = {80.0f, 80.0f};
  maskedLayer->contents.push_back(maskedRect);
  auto* maskedFill = doc->makeNode<Fill>();
  auto* cyan = doc->makeNode<SolidColor>();
  cyan->color = {0.2f, 0.7f, 0.9f, 1.0f, ColorSpace::SRGB};
  maskedFill->color = cyan;
  maskedLayer->contents.push_back(maskedFill);
  holder->children.push_back(maskedLayer);

  ASSERT_TRUE(PAGExporter::ExportToPAG(doc, "/tmp/mask_shift.pag").success);
  auto decoded = PAGImporter::LoadFromPAG("/tmp/mask_shift.pag");
  ASSERT_TRUE(decoded != nullptr);

  std::vector<Layer*> maskedLayers = {};
  CollectMaskedLayers(decoded->layers, &maskedLayers);
  ASSERT_EQ(maskedLayers.size(), 1u);
  auto* decodedMasked = maskedLayers[0];
  ASSERT_TRUE(decodedMasked->mask != nullptr);
  // The mask target is identified by its name, invisibility and Ellipse content; a shifted
  // NodeRef would land on one of the ancestor or sibling layers instead.
  EXPECT_EQ(decodedMasked->mask->name, "theMask");
  EXPECT_FALSE(decodedMasked->mask->visible);
  ASSERT_FALSE(decodedMasked->mask->contents.empty());
  EXPECT_EQ(decodedMasked->mask->contents[0]->nodeType(), NodeType::Ellipse);
  EXPECT_EQ(decodedMasked->maskType, MaskType::Alpha);
  // The mask is the first child of the same parent, and no mask is self- or ancestor-referencing.
  ASSERT_EQ(decodedMasked->children.size(), 0u);
  auto* maskParent = FindParentOf(decoded->layers, decodedMasked->mask);
  ASSERT_TRUE(maskParent != nullptr);
  EXPECT_EQ(maskParent->children.size(), 2u);
  EXPECT_EQ(maskParent->children[0], decodedMasked->mask);
  EXPECT_EQ(maskParent->children[1], decodedMasked);
  EXPECT_EQ(FindParentOf(decoded->layers, decodedMasked), maskParent);
  MaskAudit audit = {};
  AuditLayerMasks(decoded->layers, {}, &audit);
  EXPECT_EQ(audit.maskedCount, 1);
  EXPECT_EQ(audit.selfMasked, 0);
  EXPECT_EQ(audit.ancestorMasked, 0);
}

PAGX_TEST(PAGImporterTest, SubDocumentMaskNamespaceRoundTrip) {
  // Each sub-document has its own node numbering space (spec 5.2): a mask inside the external
  // document must resolve against the instance's own emission order, never the root's.
  const std::string dir = "resources/pagx_to_pag/sub_document/mask_in_child";
  auto doc = PAGXImporter::FromFile(pag::ProjectPath::Absolute(dir + "/main.pagx"));
  ASSERT_TRUE(doc != nullptr);
  ASSERT_TRUE(PAGXExternalFiles::Load(doc.get(), pag::ProjectPath::Absolute(dir + "/")));
  auto result = PAGExporter::ExportToPAG(doc, "/tmp/subdoc_mask.pag");
  ASSERT_TRUE(result.success);
  auto decoded = PAGImporter::LoadFromPAG("/tmp/subdoc_mask.pag");
  ASSERT_TRUE(decoded != nullptr);
  ASSERT_EQ(decoded->layers.size(), 1u);
  ASSERT_TRUE(decoded->layers[0]->externalDoc != nullptr);

  std::vector<Layer*> maskedLayers = {};
  CollectMaskedLayers(decoded->layers[0]->externalDoc->layers, &maskedLayers);
  ASSERT_EQ(maskedLayers.size(), 1u);
  auto* masked = maskedLayers[0];
  ASSERT_TRUE(masked->mask != nullptr);
  EXPECT_EQ(masked->mask->name, "childMask");
  EXPECT_FALSE(masked->mask->visible);
  ASSERT_FALSE(masked->mask->contents.empty());
  EXPECT_EQ(masked->mask->contents[0]->nodeType(), NodeType::Ellipse);
  EXPECT_EQ(masked->maskType, MaskType::Alpha);
  MaskAudit audit = {};
  AuditLayerMasks(decoded->layers[0]->externalDoc->layers, {}, &audit);
  EXPECT_EQ(audit.maskedCount, 1);
  EXPECT_EQ(audit.selfMasked, 0);
  EXPECT_EQ(audit.ancestorMasked, 0);
}

PAGX_TEST(PAGImporterTest, SharedReferenceMaskNodeRefRoundTrip) {
  // A color source or path data shared by several fillers (resource @id in XML) is written as
  // one inline sub-tag per referencing tag, and the decoder creates a fresh node per tag, so
  // the node index walk must count shared nodes once per reference. Counting them once per
  // node shifted later mask NodeRefs onto non-Layer nodes and silently dropped the mask.
  auto doc = PAGXDocument::Make(200, 200);
  auto* solid = doc->makeNode<SolidColor>();
  solid->color = {0.9f, 0.2f, 0.2f, 1.0f, ColorSpace::SRGB};
  auto* gradient = doc->makeNode<LinearGradient>();
  gradient->startPoint = {0.0f, 0.0f};
  gradient->endPoint = {1.0f, 0.0f};
  for (int i = 0; i < 2; i++) {
    auto* stop = doc->makeNode<ColorStop>();
    stop->offset = static_cast<float>(i);
    stop->color = {0.1f, 0.5f * static_cast<float>(i), 0.9f, 1.0f, ColorSpace::SRGB};
    gradient->colorStops.push_back(stop);
  }
  for (int i = 0; i < 2; i++) {
    auto* filler = doc->makeNode<Layer>();
    auto* rect = doc->makeNode<Rectangle>();
    rect->size = {40.0f, 40.0f};
    rect->position = {20.0f + 60.0f * i, 20.0f};
    filler->contents.push_back(rect);
    auto* fill = doc->makeNode<Fill>();
    fill->color = i == 0 ? static_cast<ColorSource*>(solid) : static_cast<ColorSource*>(gradient);
    filler->contents.push_back(fill);
    auto* fillAgain = doc->makeNode<Fill>();
    fillAgain->color = fill->color;
    filler->contents.push_back(fillAgain);
    doc->layers.push_back(filler);
  }
  auto* maskLayer = doc->makeNode<Layer>();
  maskLayer->name = "theMask";
  maskLayer->visible = false;
  auto* ellipse = doc->makeNode<Ellipse>();
  ellipse->position = {100.0f, 140.0f};
  ellipse->size = {60.0f, 60.0f};
  maskLayer->contents.push_back(ellipse);
  auto* maskFill = doc->makeNode<Fill>();
  maskFill->color = solid;
  maskLayer->contents.push_back(maskFill);
  doc->layers.push_back(maskLayer);
  auto* maskedLayer = doc->makeNode<Layer>();
  maskedLayer->mask = maskLayer;
  maskedLayer->maskType = MaskType::Alpha;
  auto* maskedRect = doc->makeNode<Rectangle>();
  maskedRect->position = {100.0f, 140.0f};
  maskedRect->size = {60.0f, 60.0f};
  maskedLayer->contents.push_back(maskedRect);
  auto* maskedFill = doc->makeNode<Fill>();
  maskedFill->color = gradient;
  maskedLayer->contents.push_back(maskedFill);
  doc->layers.push_back(maskedLayer);

  ASSERT_TRUE(PAGExporter::ExportToPAG(doc, "/tmp/shared_ref_mask.pag").success);
  auto decoded = PAGImporter::LoadFromPAG("/tmp/shared_ref_mask.pag");
  ASSERT_TRUE(decoded != nullptr);
  std::vector<Layer*> maskedLayers = {};
  CollectMaskedLayers(decoded->layers, &maskedLayers);
  ASSERT_EQ(maskedLayers.size(), 1u);
  auto* decodedMasked = maskedLayers[0];
  ASSERT_TRUE(decodedMasked->mask != nullptr);
  EXPECT_EQ(decodedMasked->mask->name, "theMask");
  EXPECT_FALSE(decodedMasked->mask->visible);
  ASSERT_FALSE(decodedMasked->mask->contents.empty());
  EXPECT_EQ(decodedMasked->mask->contents[0]->nodeType(), NodeType::Ellipse);
  MaskAudit audit = {};
  AuditLayerMasks(decoded->layers, {}, &audit);
  EXPECT_EQ(audit.maskedCount, 1);
  EXPECT_EQ(audit.selfMasked, 0);
  EXPECT_EQ(audit.ancestorMasked, 0);

  // The sample with shared @cyan/@purple/@emerald color sources, a shared @starPath and a
  // mask: its mask must survive the round trip as a Layer, not shift onto a ColorStop.
  auto sample =
      PAGXImporter::FromFile(pag::ProjectPath::Absolute("spec/samples/complete_example.pagx"));
  ASSERT_TRUE(sample != nullptr);
  ASSERT_TRUE(PAGExporter::ExportToPAG(sample, "/tmp/ce_shared_ref.pag").success);
  auto decodedSample = PAGImporter::LoadFromPAG("/tmp/ce_shared_ref.pag");
  ASSERT_TRUE(decodedSample != nullptr);
  std::vector<Layer*> sampleMasked = {};
  CollectMaskedLayers(decodedSample->layers, &sampleMasked);
  ASSERT_EQ(sampleMasked.size(), 1u);
  auto* sampleMaskedLayer = sampleMasked[0];
  ASSERT_TRUE(sampleMaskedLayer->mask != nullptr);
  EXPECT_EQ(sampleMaskedLayer->mask->nodeType(), NodeType::Layer);
  EXPECT_FALSE(sampleMaskedLayer->mask->visible);
  ASSERT_FALSE(sampleMaskedLayer->mask->contents.empty());
  EXPECT_EQ(sampleMaskedLayer->mask->contents[0]->nodeType(), NodeType::Ellipse);
}

PAGX_TEST(PAGImporterTest, StateMachineRoundTrip) {
  // State machine round trip (spec §9.7): every sample must survive with its full graph —
  // inputs (name/type/defaults), regions (name/initialState), states (name/animationId),
  // transitions (from/to/duration/frameRate/interpolation/beziers/flags) and conditions
  // (inputName/op/valueBool/valueNumber) — and the machine id must resolve through findNode.
  // Rendering equivalence lands with P2-6's default-timeline unification.
  const std::vector<std::string> samples = {
      "any_state_and_empty",
      "input_types_and_conditions",
      "multi_region_parallel",
      "transition_params",
  };
  for (auto& name : samples) {
    auto source = PAGXImporter::FromFile(
        pag::ProjectPath::Absolute("resources/pagx_to_pag/state_machine/" + name + ".pagx"));
    ASSERT_TRUE(source != nullptr) << name;
    auto result = PAGExporter::ExportToPAG(source, "/tmp/sm_diag.pag");
    ASSERT_TRUE(result.success) << name;
    auto decoded = PAGImporter::LoadFromPAG("/tmp/sm_diag.pag");
    ASSERT_TRUE(decoded != nullptr) << name;
    ASSERT_EQ(decoded->animations.size(), source->animations.size()) << name;
    for (size_t i = 0; i < source->animations.size(); i++) {
      auto* srcNode = source->animations[i];
      auto* dstNode = decoded->animations[i];
      ASSERT_EQ(dstNode->nodeType(), srcNode->nodeType()) << name;
      EXPECT_EQ(dstNode->id, srcNode->id) << name;
      if (srcNode->nodeType() != NodeType::StateMachine) {
        continue;
      }
      auto* src = static_cast<StateMachine*>(srcNode);
      auto* dst = static_cast<StateMachine*>(dstNode);
      ASSERT_EQ(dst->inputs.size(), src->inputs.size()) << name;
      for (size_t j = 0; j < src->inputs.size(); j++) {
        EXPECT_EQ(dst->inputs[j]->name, src->inputs[j]->name) << name;
        EXPECT_EQ(dst->inputs[j]->type, src->inputs[j]->type) << name;
        EXPECT_EQ(dst->inputs[j]->defaultBool, src->inputs[j]->defaultBool) << name;
        EXPECT_EQ(dst->inputs[j]->defaultNumber, src->inputs[j]->defaultNumber) << name;
      }
      ASSERT_EQ(dst->regions.size(), src->regions.size()) << name;
      for (size_t j = 0; j < src->regions.size(); j++) {
        auto* srcRegion = src->regions[j];
        auto* dstRegion = dst->regions[j];
        EXPECT_EQ(dstRegion->name, srcRegion->name) << name;
        EXPECT_EQ(dstRegion->initialState, srcRegion->initialState) << name;
        ASSERT_EQ(dstRegion->states.size(), srcRegion->states.size()) << name;
        for (size_t k = 0; k < srcRegion->states.size(); k++) {
          EXPECT_EQ(dstRegion->states[k]->name, srcRegion->states[k]->name) << name;
          EXPECT_EQ(static_cast<AnimationState*>(dstRegion->states[k])->animationId,
                    static_cast<AnimationState*>(srcRegion->states[k])->animationId)
              << name;
        }
        ASSERT_EQ(dstRegion->transitions.size(), srcRegion->transitions.size()) << name;
        for (size_t k = 0; k < srcRegion->transitions.size(); k++) {
          auto* srcT = srcRegion->transitions[k];
          auto* dstT = dstRegion->transitions[k];
          EXPECT_EQ(dstT->from, srcT->from) << name;
          EXPECT_EQ(dstT->to, srcT->to) << name;
          EXPECT_EQ(dstT->duration, srcT->duration) << name;
          EXPECT_EQ(dstT->frameRate, srcT->frameRate) << name;
          EXPECT_EQ(dstT->interpolation, srcT->interpolation) << name;
          EXPECT_EQ(dstT->bezierOut.x, srcT->bezierOut.x) << name;
          EXPECT_EQ(dstT->bezierIn.y, srcT->bezierIn.y) << name;
          EXPECT_EQ(dstT->enableEarlyExit, srcT->enableEarlyExit) << name;
          EXPECT_EQ(dstT->pauseOnExit, srcT->pauseOnExit) << name;
          ASSERT_EQ(dstT->conditions.size(), srcT->conditions.size()) << name;
          for (size_t m = 0; m < srcT->conditions.size(); m++) {
            EXPECT_EQ(dstT->conditions[m]->inputName, srcT->conditions[m]->inputName) << name;
            EXPECT_EQ(dstT->conditions[m]->op, srcT->conditions[m]->op) << name;
            EXPECT_EQ(dstT->conditions[m]->valueBool, srcT->conditions[m]->valueBool) << name;
            EXPECT_EQ(dstT->conditions[m]->valueNumber, srcT->conditions[m]->valueNumber) << name;
          }
        }
      }
      EXPECT_TRUE(decoded->findNode(srcNode->id) != nullptr) << name;
    }
    int byteCount = 0;
    EXPECT_TRUE(AssertSteadyStateIdempotency(decoded, "state-machine", &byteCount)) << name;
  }
}

PAGX_TEST(PAGImporterTest, TimelineMountRoundTrip) {
  // Layer timelines (spec §9.6): AnimationTimeline carries animationId / playing /
  // evaluationOffset (signed varint, negative skips ahead); StateMachineTimeline carries only
  // stateMachineId. Both mount between filters and children and are owned by the layer, so they
  // take no NodeRef indices.
  auto source = PAGXImporter::FromFile(
      pag::ProjectPath::Absolute("resources/pagx_to_pag/animation/layer_timeline_mount.pagx"));
  ASSERT_TRUE(source != nullptr);
  ASSERT_TRUE(PAGExporter::ExportToPAG(source, "/tmp/timeline_rt.pag").success);
  auto decoded = PAGImporter::LoadFromPAG("/tmp/timeline_rt.pag");
  ASSERT_TRUE(decoded != nullptr);
  ASSERT_EQ(source->layers.size(), decoded->layers.size());
  int animationTimelines = 0;
  for (size_t i = 0; i < source->layers.size(); i++) {
    ASSERT_EQ(decoded->layers[i]->timelines.size(), source->layers[i]->timelines.size());
    for (size_t j = 0; j < source->layers[i]->timelines.size(); j++) {
      auto* src = source->layers[i]->timelines[j].get();
      auto* dst = decoded->layers[i]->timelines[j].get();
      ASSERT_EQ(dst->timelineType(), src->timelineType());
      if (src->timelineType() == TimelineType::Animation) {
        auto* srcAnim = static_cast<AnimationTimeline*>(src);
        auto* dstAnim = static_cast<AnimationTimeline*>(dst);
        EXPECT_EQ(dstAnim->animationId, srcAnim->animationId);
        EXPECT_EQ(dstAnim->playing, srcAnim->playing);
        EXPECT_EQ(dstAnim->evaluationOffset, srcAnim->evaluationOffset);
        animationTimelines++;
      }
    }
  }
  EXPECT_GT(animationTimelines, 0);
  // Render equivalence through the runtime animation path at several playback times.
  for (auto& timeText : std::vector<std::string>{"0", "0.25", "0.5"}) {
    auto sceneA = PAGScene::Make(source);
    auto sceneB = PAGScene::Make(decoded);
    ASSERT_TRUE(sceneA != nullptr);
    ASSERT_TRUE(sceneB != nullptr);
    auto timeUs = static_cast<int64_t>(llround(std::stod(timeText) * 1e6));
    for (auto& id : sceneA->getAnimationIds()) {
      auto animation = sceneA->getAnimation(id);
      animation->setCurrentTime(timeUs);
      animation->apply(1.0f);
    }
    for (auto& id : sceneB->getAnimationIds()) {
      auto animation = sceneB->getAnimation(id);
      animation->setCurrentTime(timeUs);
      animation->apply(1.0f);
    }
    int width = static_cast<int>(source->width);
    int height = static_cast<int>(source->height);
    auto surfaceA = PAGSurface::MakeOffscreen(width, height);
    auto surfaceB = PAGSurface::MakeOffscreen(width, height);
    ASSERT_TRUE(sceneA->draw(surfaceA));
    ASSERT_TRUE(sceneB->draw(surfaceB));
    std::vector<uint8_t> pixelsA(static_cast<size_t>(width) * height * 4);
    std::vector<uint8_t> pixelsB(static_cast<size_t>(width) * height * 4);
    ASSERT_TRUE(surfaceA->readPixels(pixelsA.data(), static_cast<size_t>(width) * 4));
    ASSERT_TRUE(surfaceB->readPixels(pixelsB.data(), static_cast<size_t>(width) * 4));
    EXPECT_EQ(memcmp(pixelsA.data(), pixelsB.data(), pixelsA.size()), 0) << " t=" << timeText;
  }
}

PAGX_TEST(PAGImporterTest, TimelineNegativeOffsetAndStateMachineRoundTrip) {
  // Hand-built document exercising the signed evaluationOffset varint (negative skips ahead)
  // and a StateMachineTimeline mount (structural round trip only: state machines themselves
  // encode in P2-4, so the referenced id is allowed to dangle for now).
  auto doc = PAGXDocument::Make(200, 100);
  auto* layer = doc->makeNode<Layer>();
  doc->layers.push_back(layer);
  auto animationDriver = std::make_unique<AnimationTimeline>();
  animationDriver->animationId = "spin";
  animationDriver->playing = false;
  animationDriver->evaluationOffset = -25;
  layer->timelines.push_back(std::move(animationDriver));
  auto stateMachineDriver = std::make_unique<StateMachineTimeline>();
  stateMachineDriver->stateMachineId = "machine";
  layer->timelines.push_back(std::move(stateMachineDriver));

  ASSERT_TRUE(PAGExporter::ExportToPAG(doc, "/tmp/timeline_flags.pag").success);
  auto decoded = PAGImporter::LoadFromPAG("/tmp/timeline_flags.pag");
  ASSERT_TRUE(decoded != nullptr);
  ASSERT_EQ(decoded->layers.size(), 1u);
  ASSERT_EQ(decoded->layers[0]->timelines.size(), 2u);
  auto* first = decoded->layers[0]->timelines[0].get();
  ASSERT_EQ(first->timelineType(), TimelineType::Animation);
  auto* decodedAnimation = static_cast<AnimationTimeline*>(first);
  EXPECT_EQ(decodedAnimation->animationId, "spin");
  EXPECT_FALSE(decodedAnimation->playing);
  EXPECT_EQ(decodedAnimation->evaluationOffset, -25);
  auto* second = decoded->layers[0]->timelines[1].get();
  ASSERT_EQ(second->timelineType(), TimelineType::StateMachine);
  EXPECT_EQ(static_cast<StateMachineTimeline*>(second)->stateMachineId, "machine");

  int byteCount = 0;
  EXPECT_TRUE(AssertSteadyStateIdempotency(decoded, "timeline-mount", &byteCount));
}

PAGX_TEST(PAGImporterTest, AnimationRoundTrip) {
  // Animation core round trip (spec §9.7): every animation sample must export, decode and
  // render bit-identically to the XML source at multiple playback times. The structure
  // assertions pin the decoded tree (ids via RetainedIdTable, targets, channels, keyframes)
  // and the render loop exercises every value type plus bezier interpolation and all loop
  // modes through the runtime animation path.
  const std::vector<std::string> samples = {
      "channel_float_int_bool",
      "channel_string_color_matrix",
      "composition_scope_autoplay",
      "interpolation_modes",
      "layer_timeline_mount",
      "loop_loop",
      "loop_once",
      "loop_pingpong",
      "multiple_root_animations",
      "path_point_channels",
  };
  for (auto& name : samples) {
    auto source = PAGXImporter::FromFile(
        pag::ProjectPath::Absolute("resources/pagx_to_pag/animation/" + name + ".pagx"));
    ASSERT_TRUE(source != nullptr) << name;
    ASSERT_TRUE(PAGExporter::ExportToPAG(source, "/tmp/anim_rt.pag").success) << name;
    auto decoded = PAGImporter::LoadFromPAG("/tmp/anim_rt.pag");
    ASSERT_TRUE(decoded != nullptr) << name;
    // Every source animation survives with its id, objects and channels.
    ASSERT_EQ(decoded->animations.size(), source->animations.size()) << name;
    for (size_t i = 0; i < source->animations.size(); i++) {
      auto* srcNode = source->animations[i];
      auto* dstNode = decoded->animations[i];
      ASSERT_EQ(dstNode->nodeType(), NodeType::Animation) << name;
      ASSERT_EQ(dstNode->id, srcNode->id) << name;
      auto* src = static_cast<Animation*>(srcNode);
      auto* dst = static_cast<Animation*>(dstNode);
      EXPECT_EQ(dst->duration, src->duration) << name;
      EXPECT_EQ(dst->frameRate, src->frameRate) << name;
      EXPECT_EQ(dst->loop, src->loop) << name;
      ASSERT_EQ(dst->objects.size(), src->objects.size()) << name;
      for (size_t j = 0; j < src->objects.size(); j++) {
        EXPECT_EQ(dst->objects[j]->target, src->objects[j]->target) << name;
        ASSERT_EQ(dst->objects[j]->channels.size(), src->objects[j]->channels.size()) << name;
        for (size_t k = 0; k < src->objects[j]->channels.size(); k++) {
          auto* srcChannel = src->objects[j]->channels[k];
          auto* dstChannel = dst->objects[j]->channels[k];
          EXPECT_EQ(dstChannel->name, srcChannel->name) << name;
          EXPECT_EQ(dstChannel->valueType(), srcChannel->valueType()) << name;
        }
      }
    }
    // Animation targets resolve through findNode (RetainedIdTable restored the ids).
    for (auto* node : decoded->animations) {
      for (auto* object : static_cast<Animation*>(node)->objects) {
        auto target = object->target;
        if (!target.empty() && target[0] == '@') {
          target = target.substr(1);
        }
        if (!target.empty()) {
          EXPECT_TRUE(decoded->findNode(target) != nullptr) << name << " target " << target;
        }
      }
    }
    // Pixel equivalence at several playback times through the runtime animation path.
    for (auto& timeText : std::vector<std::string>{"0", "0.25", "0.5", "0.9"}) {
      auto sceneA = PAGScene::Make(source);
      auto sceneB = PAGScene::Make(decoded);
      ASSERT_TRUE(sceneA != nullptr) << name;
      ASSERT_TRUE(sceneB != nullptr) << name;
      auto timeUs = static_cast<int64_t>(llround(std::stod(timeText) * 1e6));
      for (auto& id : sceneA->getAnimationIds()) {
        auto animation = sceneA->getAnimation(id);
        animation->setCurrentTime(timeUs);
        animation->apply(1.0f);
      }
      for (auto& id : sceneB->getAnimationIds()) {
        auto animation = sceneB->getAnimation(id);
        animation->setCurrentTime(timeUs);
        animation->apply(1.0f);
      }
      int width = static_cast<int>(source->width);
      int height = static_cast<int>(source->height);
      auto surfaceA = PAGSurface::MakeOffscreen(width, height);
      auto surfaceB = PAGSurface::MakeOffscreen(width, height);
      ASSERT_TRUE(surfaceA != nullptr) << name;
      ASSERT_TRUE(surfaceB != nullptr) << name;
      ASSERT_TRUE(sceneA->draw(surfaceA)) << name;
      ASSERT_TRUE(sceneB->draw(surfaceB)) << name;
      std::vector<uint8_t> pixelsA(static_cast<size_t>(width) * height * 4);
      std::vector<uint8_t> pixelsB(static_cast<size_t>(width) * height * 4);
      ASSERT_TRUE(surfaceA->readPixels(pixelsA.data(), static_cast<size_t>(width) * 4)) << name;
      ASSERT_TRUE(surfaceB->readPixels(pixelsB.data(), static_cast<size_t>(width) * 4)) << name;
      EXPECT_EQ(memcmp(pixelsA.data(), pixelsB.data(), pixelsA.size()), 0)
          << name << " t=" << timeText;
    }
  }
}

PAGX_TEST(PAGImporterTest, AllExportPathsIdempotency) {
  // Every fully-supported export path must be byte-idempotent from the decoded state.
  // Each sample exercises a distinct encoding feature; together they cover the full
  // phase-1 tag surface.
  struct Case {
    const char* path;
    const char* feature;
  };
  const std::vector<Case> cases = {
      // Shapes + fills + gradients
      {"resources/pagx_to_pag/static_animated/pure_static.pagx", "layer/ellipse/radial-gradient"},
      {"resources/pagx_to_pag/layout_flatten/clip_to_bounds.pagx",
       "nested-layer/scroll-rect/rectangle"},
      // Images: shared resource + inline data URI
      {"resources/pagx_to_pag/resource/image_external_shared.pagx",
       "shared-image/image-pattern/resource-block"},
      {"resources/pagx_to_pag/resource/image_data_uri.pagx", "inline-image/image-pattern"},
      // Embedded glyphs: vector outlines + bitmap images
      {"resources/pagx_to_pag/text_font/embedded_vector_glyphs.pagx",
       "inline-font/vector-glyph-run"},
      {"resources/pagx_to_pag/text_font/embedded_bitmap_glyphs.pagx", "bitmap-glyph/image-table"},
      // Sub-documents: single, multi-reference, nested
      {"resources/pagx_to_pag/sub_document/subdoc_with_layout/main.pagx",
       "sub-document/image-in-subdoc"},
      // Internal composition
      {"resources/cli/optimizer_all_rules.pagx", "internal-composition/shared-resources"},
  };
  for (auto& testCase : cases) {
    auto doc = PAGXImporter::FromFile(pag::ProjectPath::Absolute(testCase.path));
    ASSERT_TRUE(doc != nullptr) << testCase.path;
    // Sub-document samples need external file loading first.
    if (testCase.path ==
        std::string("resources/pagx_to_pag/sub_document/subdoc_with_layout/main.pagx")) {
      PAGXExternalFiles::Load(
          doc.get(),
          pag::ProjectPath::Absolute("resources/pagx_to_pag/sub_document/subdoc_with_layout/"));
    }
    int byteCount = 0;
    EXPECT_TRUE(AssertSteadyStateIdempotency(doc, testCase.feature, &byteCount)) << testCase.path;
    EXPECT_GT(byteCount, 0) << testCase.path;
  }
}

PAGX_TEST(PAGImporterTest, FontRoundTripIdempotency) {
  // Regression: fonts decoded from the v4 binary carry no ids, but the exporter's internal
  // XML round trip links GlyphRun -> Font by @id. The id synthesis (pag-v4-font-N) must keep
  // the linkage so the decoded document re-exports with the same font content — and the
  // pipeline must be byte-idempotent from the decoded state (export2 == export3).
  auto doc = PAGXImporter::FromFile(
      pag::ProjectPath::Absolute("resources/pagx_to_pag/text_font/embedded_vector_glyphs.pagx"));
  ASSERT_TRUE(doc != nullptr);
  ASSERT_TRUE(PAGExporter::ExportToPAG(doc, "/tmp/font_rt1.pag").success);
  auto decoded1 = PAGImporter::LoadFromPAG("/tmp/font_rt1.pag");
  ASSERT_TRUE(decoded1 != nullptr);

  // The decoded document's glyph runs must still reference their fonts.
  Text* text = nullptr;
  for (auto* layer : decoded1->layers) {
    for (auto* element : layer->contents) {
      if (element->nodeType() == NodeType::Text) {
        text = static_cast<Text*>(element);
        break;
      }
    }
    if (text != nullptr) break;
  }
  ASSERT_TRUE(text != nullptr);
  ASSERT_EQ(text->glyphRuns.size(), 1u);
  auto* run = text->glyphRuns[0];
  ASSERT_TRUE(run->font != nullptr);
  EXPECT_EQ(run->font->unitsPerEm, 1000);
  ASSERT_EQ(run->font->glyphs.size(), 2u);
  ASSERT_TRUE(run->font->glyphs[0]->path != nullptr);

  // Byte identity from the decoded state: export1 == export2 for this sample (no orphan
  // resources to normalize), and export2 == export3 for steady-state idempotency.
  auto r2 = PAGExporter::ExportToPAG(decoded1, "/tmp/font_rt2.pag");
  ASSERT_TRUE(r2.success);
  auto bytes1 = ReadFileBytes("/tmp/font_rt1.pag");
  auto bytes2 = ReadFileBytes("/tmp/font_rt2.pag");
  ASSERT_EQ(bytes1.size(), bytes2.size());
  EXPECT_EQ(memcmp(bytes1.data(), bytes2.data(), bytes1.size()), 0);

  auto decoded2 = PAGImporter::LoadFromPAG("/tmp/font_rt2.pag");
  ASSERT_TRUE(decoded2 != nullptr);
  auto r3 = PAGExporter::ExportToPAG(decoded2, "/tmp/font_rt3.pag");
  ASSERT_TRUE(r3.success);
  auto bytes3 = ReadFileBytes("/tmp/font_rt3.pag");
  ASSERT_EQ(bytes2.size(), bytes3.size());
  EXPECT_EQ(memcmp(bytes2.data(), bytes3.data(), bytes2.size()), 0);
}

PAGX_TEST(PAGImporterTest, SharedFontRoundTripIdempotency) {
  // A font referenced by two glyph runs goes through the ResourceBlock (spec §5.3). The
  // decoded document gets two independent references to the same font node; re-export must
  // still share it and produce identical bytes.
  auto doc = PAGXDocument::Make(200, 200);
  auto* layer = doc->makeNode<Layer>();
  layer->width = 200;
  layer->height = 200;
  doc->layers.push_back(layer);

  auto* font = doc->makeNode<Font>();
  font->unitsPerEm = 2048;
  auto* glyph = doc->makeNode<Glyph>();
  glyph->advance = 500.0f;
  font->glyphs.push_back(glyph);

  for (int i = 0; i < 2; i++) {
    auto* text = doc->makeNode<Text>();
    text->text = i == 0 ? "A" : "B";
    text->fontSize = 24.0f;
    text->position.x = 20.0f + i * 100.0f;
    text->position.y = 100.0f;
    auto* run = doc->makeNode<GlyphRun>();
    run->font = font;
    run->fontSize = 24.0f;
    run->glyphs = {1};
    run->x = 20.0f + i * 100.0f;
    run->y = 100.0f;
    text->glyphRuns.push_back(run);
    layer->contents.push_back(text);
  }
  auto* fill = doc->makeNode<Fill>();
  auto* solid = doc->makeNode<SolidColor>();
  solid->color = {0.1f, 0.2f, 0.3f, 1.0f, ColorSpace::SRGB};
  fill->color = solid;
  layer->contents.push_back(fill);

  auto r1 = PAGExporter::ExportToPAG(doc, "/tmp/shared_font_rt1.pag");
  ASSERT_TRUE(r1.success);
  auto decoded1 = PAGImporter::LoadFromPAG("/tmp/shared_font_rt1.pag");
  ASSERT_TRUE(decoded1 != nullptr);

  // Both texts' glyph runs reference the same font (shared via ResourceBlock).
  Font* firstFont = nullptr;
  Font* secondFont = nullptr;
  int runIndex = 0;
  for (auto* layerNode : decoded1->layers) {
    for (auto* element : layerNode->contents) {
      if (element->nodeType() != NodeType::Text) {
        continue;
      }
      auto* text = static_cast<Text*>(element);
      for (auto* glyphRun : text->glyphRuns) {
        ASSERT_TRUE(glyphRun->font != nullptr);
        if (runIndex == 0) {
          firstFont = glyphRun->font;
        } else if (runIndex == 1) {
          secondFont = glyphRun->font;
        }
        runIndex++;
      }
    }
  }
  ASSERT_EQ(runIndex, 2);
  ASSERT_TRUE(firstFont != nullptr);
  ASSERT_TRUE(secondFont != nullptr);
  EXPECT_EQ(firstFont, secondFont);
  EXPECT_EQ(firstFont->unitsPerEm, 2048);
  ASSERT_EQ(firstFont->glyphs.size(), 1u);

  // Steady-state idempotency: export2 == export3.
  auto r2 = PAGExporter::ExportToPAG(decoded1, "/tmp/shared_font_rt2.pag");
  ASSERT_TRUE(r2.success);
  auto decoded2 = PAGImporter::LoadFromPAG("/tmp/shared_font_rt2.pag");
  ASSERT_TRUE(decoded2 != nullptr);
  auto r3 = PAGExporter::ExportToPAG(decoded2, "/tmp/shared_font_rt3.pag");
  ASSERT_TRUE(r3.success);
  auto bytes2 = ReadFileBytes("/tmp/shared_font_rt2.pag");
  auto bytes3 = ReadFileBytes("/tmp/shared_font_rt3.pag");
  ASSERT_EQ(bytes2.size(), bytes3.size());
  EXPECT_EQ(memcmp(bytes2.data(), bytes3.data(), bytes2.size()), 0);
}

PAGX_TEST(PAGImporterTest, CompositionRoundTrip) {
  // Regression: internal Composition blocks (spec §9.1 #297) survive export → decode with
  // their layers, sizes, and referencing layer links intact.
  auto doc =
      PAGXImporter::FromFile(pag::ProjectPath::Absolute("resources/cli/optimizer_all_rules.pagx"));
  ASSERT_TRUE(doc != nullptr);
  ASSERT_TRUE(PAGExporter::ExportToPAG(doc, "/tmp/composition_rt1.pag").success);
  auto decoded = PAGImporter::LoadFromPAG("/tmp/composition_rt1.pag");
  ASSERT_TRUE(decoded != nullptr);

  // Find the composition-referencing layer in both trees and compare structure.
  Layer* sourceCompLayer = nullptr;
  for (auto* layer : doc->layers) {
    if (layer->composition != nullptr) {
      sourceCompLayer = layer;
      break;
    }
  }
  ASSERT_TRUE(sourceCompLayer != nullptr);
  auto* sourceComposition = sourceCompLayer->composition;
  EXPECT_GT(sourceComposition->width, 0.0f);
  EXPECT_GT(sourceComposition->height, 0.0f);
  auto sourceLayerCount = sourceComposition->layers.size();
  EXPECT_GT(sourceLayerCount, 0u);

  Layer* decodedCompLayer = nullptr;
  for (auto* layer : decoded->layers) {
    if (layer->composition != nullptr) {
      decodedCompLayer = layer;
      break;
    }
  }
  ASSERT_TRUE(decodedCompLayer != nullptr);
  auto* decodedComposition = decodedCompLayer->composition;
  ASSERT_TRUE(decodedComposition != nullptr);
  EXPECT_EQ(decodedComposition->width, sourceComposition->width);
  EXPECT_EQ(decodedComposition->height, sourceComposition->height);
  ASSERT_EQ(decodedComposition->layers.size(), sourceLayerCount);

  // The composition's layers carry their own geometry and fills: compare non-empty contents.
  int contentsChecked = 0;
  for (size_t i = 0; i < sourceLayerCount; i++) {
    auto* sourceLayer = sourceComposition->layers[i];
    auto* decodedLayer = decodedComposition->layers[i];
    if (sourceLayer->contents.empty()) {
      continue;
    }
    ASSERT_FALSE(decodedLayer->contents.empty());
    ASSERT_EQ(sourceLayer->contents.size(), decodedLayer->contents.size());
    contentsChecked++;
  }
  EXPECT_GT(contentsChecked, 0);

  // Byte identity from the decoded state: the optimizer fixture's first export normalizes
  // its structures (orphan resources, shared PathData references, empty composition layers),
  // so export1 legitimately differs from export2. From the decoded state the pipeline is
  // idempotent: export2 == export3.
  auto r2 = PAGExporter::ExportToPAG(decoded, "/tmp/composition_rt2.pag");
  ASSERT_TRUE(r2.success);
  auto decoded2 = PAGImporter::LoadFromPAG("/tmp/composition_rt2.pag");
  ASSERT_TRUE(decoded2 != nullptr);
  auto r3 = PAGExporter::ExportToPAG(decoded2, "/tmp/composition_rt3.pag");
  ASSERT_TRUE(r3.success);
  auto bytes2 = ReadFileBytes("/tmp/composition_rt2.pag");
  auto bytes3 = ReadFileBytes("/tmp/composition_rt3.pag");
  ASSERT_EQ(bytes2.size(), bytes3.size());
  EXPECT_EQ(memcmp(bytes2.data(), bytes3.data(), bytes2.size()), 0);
}

PAGX_TEST(PAGImporterTest, CompositionRenderEquivalence) {
  // T7-d scope extension: the optimizer fixture with internal compositions renders within
  // quantization noise (u8 gradient stop colors inside the composition produce ≤ 4/255 shifts).
  auto doc =
      PAGXImporter::FromFile(pag::ProjectPath::Absolute("resources/cli/optimizer_all_rules.pagx"));
  ASSERT_TRUE(doc != nullptr);
  ASSERT_TRUE(PAGExporter::ExportToPAG(doc, "/tmp/composition_render.pag").success);
  auto decoded = PAGImporter::LoadFromPAG("/tmp/composition_render.pag");
  ASSERT_TRUE(decoded != nullptr);

  auto sceneA = PAGScene::Make(doc);
  auto sceneB = PAGScene::Make(decoded);
  ASSERT_TRUE(sceneA != nullptr);
  ASSERT_TRUE(sceneB != nullptr);
  int width = static_cast<int>(doc->width);
  int height = static_cast<int>(doc->height);
  auto surfaceA = PAGSurface::MakeOffscreen(width, height);
  auto surfaceB = PAGSurface::MakeOffscreen(width, height);
  ASSERT_TRUE(surfaceA != nullptr);
  ASSERT_TRUE(surfaceB != nullptr);
  ASSERT_TRUE(sceneA->draw(surfaceA));
  ASSERT_TRUE(sceneB->draw(surfaceB));
  std::vector<uint8_t> pixelsA(static_cast<size_t>(width) * height * 4);
  std::vector<uint8_t> pixelsB(static_cast<size_t>(width) * height * 4);
  ASSERT_TRUE(surfaceA->readPixels(pixelsA.data(), static_cast<size_t>(width) * 4));
  ASSERT_TRUE(surfaceB->readPixels(pixelsB.data(), static_cast<size_t>(width) * 4));

  // Allow only the quantization residual: every channel within 5/255 and at most 0.1% of
  // pixels differing at all (measured residual: 406 px of 2.56M = 0.016%, max 4/255).
  auto total = static_cast<size_t>(width) * height;
  size_t diffPixels = 0;
  uint8_t maxChannelDiff = 0;
  for (size_t i = 0; i < total; i++) {
    const uint8_t* pa = pixelsA.data() + i * 4;
    const uint8_t* pb = pixelsB.data() + i * 4;
    for (int c = 0; c < 4; c++) {
      auto delta = static_cast<uint8_t>(std::abs(static_cast<int>(pa[c]) - pb[c]));
      if (delta > 0) {
        if (delta > maxChannelDiff) {
          maxChannelDiff = delta;
        }
      }
    }
    if (pa[0] != pb[0] || pa[1] != pb[1] || pa[2] != pb[2] || pa[3] != pb[3]) {
      diffPixels++;
    }
  }
  EXPECT_LE(maxChannelDiff, 5);
  EXPECT_LE(diffPixels, total / 1000);
}

}  // namespace pagx
