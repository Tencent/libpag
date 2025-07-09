/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include <fstream>
#include <vector>
#include "base/utils/TimeUtil.h"
#include "nlohmann/json.hpp"
#include "utils/TestUtils.h"

namespace pag {
using nlohmann::json;

/**
 * 用例描述: 字体相关功能测试
 */
PAG_TEST(PAGFontTest, TestFont) {
  PAGFont::RegisterFont(ProjectPath::Absolute("resources/font/NotoSerifSC-Regular.otf"), 0,
                        "TTTGBMedium", "Regular");
  auto TestPAGFile = LoadPAGFile("resources/apitest/test_font.pag");
  ASSERT_NE(TestPAGFile, nullptr);
  auto pagSurface = OffscreenSurface::Make(TestPAGFile->width(), TestPAGFile->height());
  ASSERT_NE(pagSurface, nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(TestPAGFile);

  Frame totalFrames = TimeToFrame(TestPAGFile->duration(), TestPAGFile->frameRate());
  Frame currentFrame = 0;

  std::string errorMsg;
  while (currentFrame < totalFrames) {
    //添加0.1帧目的是保证progress不会由于精度问题帧数计算错误，frame应该使用totalFrames作为总体帧数。因为对于file来说总时长为[0,totalFrames],对应于[0,1]，因此归一化时，分母应该为totalFrames
    pagPlayer->setProgress((currentFrame + 0.1) * 1.0 / totalFrames);
    auto changed = pagPlayer->flush();
    if (changed) {
      bool same = Baseline::Compare(pagSurface, "PAGFontTest/" + ToString(currentFrame));
      if (!same) {
        errorMsg += (std::to_string(currentFrame) + ";");
      }
    }
    currentFrame++;
  }
  EXPECT_EQ(errorMsg, "") << "test_font frame fail";
}

}  // namespace pag
