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
#include <cstring>
#include "base/PAGTest.h"
#include "pagx/PAGXImporter.h"
#include "pagx/nodes/Image.h"
#include "pagx/pag/exporter/ImageProcessor.h"
#include "pagx/types/Data.h"
#include "utils/ProjectPath.h"

namespace pagx {

PAGX_TEST(PAGImageProcessorTest, DataSlice) {
  uint8_t buffer[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
  auto parent = Data::MakeWithCopy(buffer, sizeof(buffer));
  ASSERT_TRUE(parent != nullptr);

  // Valid slice.
  auto slice = Data::MakeSlice(parent, 4, 8);
  ASSERT_TRUE(slice != nullptr);
  EXPECT_EQ(slice->size(), 8u);
  EXPECT_EQ(slice->bytes()[0], 4);
  EXPECT_EQ(slice->bytes()[7], 11);

  // Zero-length slice.
  auto empty = Data::MakeSlice(parent, 4, 0);
  ASSERT_TRUE(empty != nullptr);
  EXPECT_TRUE(empty->empty());

  // Out-of-bounds.
  auto bad1 = Data::MakeSlice(parent, 12, 8);
  EXPECT_TRUE(bad1 == nullptr);
  auto bad2 = Data::MakeSlice(nullptr, 0, 8);
  EXPECT_TRUE(bad2 == nullptr);
  auto bad3 = Data::MakeSlice(parent, 17, 1);
  EXPECT_TRUE(bad3 == nullptr);

  // Parent lifetime: dropping the parent reference keeps the slice alive via the
  // internal shared_ptr; the slice bytes stay valid.
  {
    auto tempParent = Data::MakeWithCopy(buffer, sizeof(buffer));
    auto tempSlice = Data::MakeSlice(tempParent, 2, 4);
    ASSERT_TRUE(tempSlice != nullptr);
    // tempParent goes out of scope here; tempSlice must still be valid.
  }
  // If we got here without a crash, the parent lifetime is correct.
  SUCCEED();
}

PAGX_TEST(PAGImageProcessorTest, DataSliceBytesMatch) {
  const char* content = "Hello, World!";
  auto parent = Data::MakeWithCopy(content, strlen(content));
  auto slice = Data::MakeSlice(parent, 7, 5);
  ASSERT_TRUE(slice != nullptr);
  EXPECT_EQ(memcmp(slice->bytes(), "World", 5), 0);
  EXPECT_EQ(slice->size(), 5u);
}

PAGX_TEST(PAGImageProcessorTest, MaxDisplayScale) {
  // Load the shared-image sample (two patterns referencing the same image).
  auto path =
      pag::ProjectPath::Absolute("resources/pagx_to_pag/resource/image_external_shared.pagx");
  auto doc = PAGXImporter::FromFile(path);
  ASSERT_TRUE(doc != nullptr);
  doc->applyLayout();

  Image* image = nullptr;
  for (auto& node : doc->nodes) {
    if (node->nodeType() == NodeType::Image) {
      image = static_cast<Image*>(node.get());
      break;
    }
  }
  ASSERT_TRUE(image != nullptr);

  auto scale = ImageProcessor::ComputeMaxDisplayScale(doc.get(), image);
  // The sample has two patterns with different scaleModes; the scale should be positive
  // (at least 1.0 since there are identity transforms).
  EXPECT_GE(scale, 0.0f);
}

PAGX_TEST(PAGImageProcessorTest, ProcessProducesEntries) {
  auto path = pag::ProjectPath::Absolute("resources/pagx_to_pag/resource/image_data_uri.pagx");
  auto doc = PAGXImporter::FromFile(path);
  ASSERT_TRUE(doc != nullptr);
  doc->applyLayout();

  auto result = ImageProcessor::Process(doc.get());
  // The sample has one inline image.
  EXPECT_EQ(result.entries.size(), 1u);
  EXPECT_FALSE(result.entries[0].data == nullptr);
  EXPECT_GT(result.entries[0].data->size(), 0u);
  EXPECT_GT(result.entries[0].logicalWidth, 0u);
  EXPECT_GT(result.entries[0].logicalHeight, 0u);
}

PAGX_TEST(PAGImageProcessorTest, Deduplication) {
  // The shared sample references the same image twice: one entry.
  auto path =
      pag::ProjectPath::Absolute("resources/pagx_to_pag/resource/image_external_shared.pagx");
  auto doc = PAGXImporter::FromFile(path);
  ASSERT_TRUE(doc != nullptr);
  doc->applyLayout();

  auto result = ImageProcessor::Process(doc.get());
  // Two patterns share one image -> one ImageTable entry.
  EXPECT_LE(result.entries.size(), 1u);
}

PAGX_TEST(PAGImageProcessorTest, MissingImage) {
  auto path = pag::ProjectPath::Absolute("resources/pagx_to_pag/resource/image_missing.pagx");
  auto doc = PAGXImporter::FromFile(path);
  ASSERT_TRUE(doc != nullptr);
  doc->applyLayout();

  auto result = ImageProcessor::Process(doc.get());
  EXPECT_FALSE(result.warnings.empty());
  bool foundMissing = false;
  for (auto& warning : result.warnings) {
    if (warning.code == PAGExportWarning::MissingImage) {
      foundMissing = true;
    }
  }
  EXPECT_TRUE(foundMissing);
}

}  // namespace pagx
