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

#include <cstring>
#include "pagx/types/Data.h"
#include "renderer/ToTGFX.h"
#include "utils/TestUtils.h"

namespace pag {

/**
 * Test case: ToTGFXData forwards a null input straight through as a null result.
 */
PAGX_TEST(RendererToTGFXTest, NullInputReturnsNull) {
  EXPECT_EQ(pagx::ToTGFXData(nullptr), nullptr);
}

/**
 * Test case: a zero-length pagx Data yields a valid empty tgfx Data rather than crashing or leaking.
 * The public Data factories reject empty input, so the size-0 buffer is built through the private
 * constructor (the test target compiles with -fno-access-control).
 */
PAGX_TEST(RendererToTGFXTest, EmptyDataReturnsValidEmpty) {
  auto emptyData = std::shared_ptr<pagx::Data>(new pagx::Data(nullptr, 0));
  ASSERT_TRUE(emptyData != nullptr);
  ASSERT_EQ(emptyData->size(), 0u);

  auto result = pagx::ToTGFXData(emptyData);
  ASSERT_TRUE(result != nullptr);
  EXPECT_EQ(result->size(), 0u);
  EXPECT_TRUE(result->empty());
}

/**
 * Test case: a non-empty pagx Data is adopted by the tgfx Data with matching size and bytes.
 */
PAGX_TEST(RendererToTGFXTest, NonEmptyDataMatchesBytes) {
  const uint8_t bytes[] = {0x11, 0x22, 0x33, 0x44, 0x55};
  auto data = pagx::Data::MakeWithCopy(bytes, sizeof(bytes));
  ASSERT_TRUE(data != nullptr);

  auto result = pagx::ToTGFXData(data);
  ASSERT_TRUE(result != nullptr);
  ASSERT_EQ(result->size(), sizeof(bytes));
  EXPECT_EQ(std::memcmp(result->bytes(), bytes, sizeof(bytes)), 0);
}

}  // namespace pag
