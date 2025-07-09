/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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

#include <map>
#include <thread>
#include <vector>
#include "nlohmann/json.hpp"
#include "pag/pag.h"
#include "tgfx/core/Clock.h"
#include "tgfx/core/Task.h"
#include "utils/TestUtils.h"

namespace pag {
using namespace tgfx;
using nlohmann::json;

/**
 * 用例描述: 校验加载混淆过的错误 PAG 文件是否会崩溃。
 */
PAG_TEST(PAGFuzzTest, DecodeFiles) {
  auto files = GetAllPAGFiles("resources/fuzz");
  for (auto& file : files) {
    auto pagFile = PAGFile::Load(file);
    ASSERT_EQ(pagFile, nullptr);
  }
}
}  // namespace pag
