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
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/GlyphRun.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Node.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/Text.h"
#include "pagx/pag/PAGAttributeIO.h"
#include "pagx/pag/PAGTagHeader.h"
#include "pagx/pag/TagCode.h"
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

}  // namespace pagx
