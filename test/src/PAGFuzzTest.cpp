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
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-warning-option"
#pragma clang diagnostic ignored "-Wdeprecated-literal-operator"
#include "nlohmann/json.hpp"
#pragma clang diagnostic pop
#include "pag/file.h"
#include "pag/pag.h"
#include "tgfx/core/Clock.h"
#include "tgfx/core/Task.h"
#include "utils/TestUtils.h"

namespace pag {
using namespace tgfx;
using nlohmann::json;

static VectorComposition* MakeVectorComposition(ID id) {
  auto composition = new VectorComposition();
  composition->id = id;
  composition->width = 100;
  composition->height = 100;
  composition->duration = 60;
  composition->frameRate = 30;
  return composition;
}

static PreComposeLayer* MakePreComposeLayer(ID compositionID) {
  auto reference = new Composition();
  reference->id = compositionID;
  reference->duration = 60;
  auto layer = PreComposeLayer::Wrap(reference).release();
  layer->id = compositionID;
  return layer;
}

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

/**
 * 用例描述: 校验预合成循环引用的文件不会导致栈溢出。
 */
PAG_TEST(PAGFuzzTest, PreComposeReferenceCycle) {
  auto leafComposition = MakeVectorComposition(1);
  auto mainComposition = MakeVectorComposition(2);
  mainComposition->layers.push_back(MakePreComposeLayer(leafComposition->id));

  std::vector<Composition*> compositions = {leafComposition, mainComposition};
  Codec::InstallReferences(compositions);
  auto file = Codec::VerifyAndMake(compositions, {});
  ASSERT_NE(file, nullptr);

  auto validBytes = Codec::Encode(file);
  ASSERT_NE(validBytes, nullptr);
  ASSERT_NE(File::Load(validBytes->data(), validBytes->length()), nullptr);

  // 让预合成图层指回主合成自身，编码后即得到循环引用的文件。
  static_cast<PreComposeLayer*>(mainComposition->layers[0])->composition = mainComposition;
  auto cycleBytes = Codec::Encode(file);
  ASSERT_NE(cycleBytes, nullptr);
  ASSERT_EQ(File::Load(cycleBytes->data(), cycleBytes->length()), nullptr);
}

/**
 * 用例描述: 校验预合成嵌套过深的文件不会导致栈溢出。
 */
PAG_TEST(PAGFuzzTest, PreComposeNestingDepth) {
  const int compositionCount = 200;
  std::vector<Composition*> compositions = {};
  for (int i = 0; i < compositionCount; i++) {
    auto composition = MakeVectorComposition(static_cast<ID>(i + 1));
    if (i + 1 < compositionCount) {
      composition->layers.push_back(MakePreComposeLayer(static_cast<ID>(i + 2)));
    }
    compositions.push_back(composition);
  }
  Codec::InstallReferences(compositions);
  ASSERT_EQ(Codec::VerifyAndMake(compositions, {}), nullptr);
}

/**
 * 用例描述: 校验图层父级链成环的文件不会导致死循环。
 */
PAG_TEST(PAGFuzzTest, LayerParentCycle) {
  auto mainComposition = MakeVectorComposition(1);
  auto layer = new Layer();
  layer->id = 2;
  layer->duration = 60;
  layer->transform = Transform2D::MakeDefault().release();
  mainComposition->layers.push_back(layer);

  std::vector<Composition*> compositions = {mainComposition};
  Codec::InstallReferences(compositions);
  auto file = Codec::VerifyAndMake(compositions, {});
  ASSERT_NE(file, nullptr);

  layer->parent = layer;
  auto bytes = Codec::Encode(file);
  ASSERT_NE(bytes, nullptr);
  ASSERT_EQ(File::Load(bytes->data(), bytes->length()), nullptr);
}
}  // namespace pag
