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
#include "pagx/PAGXImporter.h"
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

}  // namespace pagx
