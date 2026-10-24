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
#include "base/PAGTest.h"
#include "pagx/PAGExporter.h"
#include "pagx/PAGImporter.h"
#include "pagx/PAGXImporter.h"
#include "utils/ProjectPath.h"

namespace pagx {

PAGX_TEST(PAGExporterTest, ExportSimpleDocument) {
  auto doc = PAGXImporter::FromFile(
      pag::ProjectPath::Absolute("resources/pagx_to_pag/static_animated/pure_static.pagx"));
  ASSERT_TRUE(doc != nullptr);
  PAGExportResult result = PAGExporter::ExportToPAG(doc, "/tmp/test_export.pag");
  EXPECT_TRUE(result.success);
  EXPECT_GT(result.byteSize, 0u);
  FILE* f = fopen("/tmp/test_export.pag", "rb");
  ASSERT_TRUE(f != nullptr);
  uint8_t header[16] = {};
  fread(header, 1, 16, f);
  fclose(f);
  EXPECT_EQ(header[0], 'P');
  EXPECT_EQ(header[1], 'A');
  EXPECT_EQ(header[2], 'G');
  EXPECT_EQ(header[3], 4);
  EXPECT_EQ(header[8], 'U');
}

PAGX_TEST(PAGExporterTest, DetectFormatOfExportedFile) {
  auto doc = PAGXImporter::FromFile(
      pag::ProjectPath::Absolute("resources/pagx_to_pag/static_animated/pure_static.pagx"));
  ASSERT_TRUE(doc != nullptr);
  auto result = PAGExporter::ExportToPAG(doc, "/tmp/test_detect.pag");
  ASSERT_TRUE(result.success);
  EXPECT_EQ(PAGImporter::DetectFormat("/tmp/test_detect.pag"), PAGFormat::PAGv4);
}

PAGX_TEST(PAGExporterTest, ExportMultipleSamples) {
  std::vector<std::string> paths = {
      "resources/pagx_to_pag/static_animated/pure_static.pagx",
      "resources/pagx_to_pag/layout_flatten/clip_to_bounds.pagx",
      "resources/pagx_to_pag/layout_flatten/text_anchor_modes.pagx",
  };
  for (auto& rel : paths) {
    auto doc = PAGXImporter::FromFile(pag::ProjectPath::Absolute(rel));
    ASSERT_TRUE(doc != nullptr) << rel;
    auto result = PAGExporter::ExportToPAG(doc, "/tmp/test_multi.pag");
    EXPECT_TRUE(result.success) << rel;
    EXPECT_GT(result.byteSize, 0u) << rel;
    EXPECT_EQ(PAGImporter::DetectFormat("/tmp/test_multi.pag"), PAGFormat::PAGv4) << rel;
  }
}

PAGX_TEST(PAGExporterTest, AnimatedContentWarning) {
  // Phase 2 encodes animations (spec §9.7), so an animated sample no longer produces the
  // AnimatedContentSkipped warning; the warning now only covers viewmodel/data binds and
  // state machines, which still have no binary encoding.
  auto doc = PAGXImporter::FromFile(pag::ProjectPath::Absolute(
      "resources/pagx_to_pag/static_animated/mixed_static_animated.pagx"));
  ASSERT_TRUE(doc != nullptr);
  auto result = PAGExporter::ExportToPAG(doc, "/tmp/test_anim.pag");
  EXPECT_TRUE(result.success);
  for (auto& w : result.warnings) {
    EXPECT_NE(w.code, PAGExportWarning::AnimatedContentSkipped)
        << "animations are encoded in phase 2";
  }
}

}  // namespace pagx
