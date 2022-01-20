/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#ifdef SMOKE_TEST
#include <fstream>
#include <vector>
#include "TestUtils.h"
#include "base/utils/TimeUtil.h"
#include "framework/pag_test.h"
#include "framework/utils/Baseline.h"
#include "framework/utils/PAGTestUtils.h"

namespace pag {
/**
 * 用例描述: smoke文件夹下文件是否渲染正常
 */
PAG_TEST(PAGSmokeTest, RenderFrames) {
  std::vector<std::string> files;
  GetAllPAGFiles("../resources/smoke", files);

  int size = static_cast<int>(files.size());
  for (int i = 0; i < size; i++) {
    auto fileName = files[i].substr(files[i].rfind("/") + 1, files[i].size());
    auto pagFile = PAGFile::Load(files[i]);
    auto width = pagFile->width();
    auto height = pagFile->height();
    if (std::min(width, height) > 360) {
      if (width > height) {
        width = static_cast<int>(
            ceilf(static_cast<float>(width) * 360.0f / static_cast<float>(height)));
        height = 360;
      } else {
        height = static_cast<int>(
            ceilf(static_cast<float>(height) * 360.0f / static_cast<float>(width)));
        width = 360;
      }
    }
    ASSERT_NE(pagFile, nullptr);
    auto pagSurface = PAGSurface::MakeOffscreen(width, height);
    ASSERT_NE(pagSurface, nullptr);
    auto pagPlayer = std::make_shared<PAGPlayer>();
    pagPlayer->setSurface(pagSurface);
    pagPlayer->setComposition(pagFile);
    Frame totalFrames = TimeToFrame(pagFile->duration(), pagFile->frameRate());
    Frame currentFrame = 0;

    std::string errorMsg = "";
    while (currentFrame < totalFrames) {
      // 添加0.1帧目的是保证progress不会由于精度问题帧数计算错误，frame应该使用totalFrames作为总体帧数。因为对于
      // file来说总时长为[0,totalFrames],对应于[0,1]，因此归一化时，分母应该为totalFrames
      pagPlayer->setProgress((static_cast<float>(currentFrame) + 0.1) * 1.0 /
                             static_cast<float>(totalFrames));
      pagPlayer->flush();

      auto snapshot = MakeSnapshot(pagSurface);
      auto result =
          Baseline::Compare(snapshot, "PAGSmokeTest/" + std::to_string(currentFrame) + "");
      if (!result) {
        errorMsg += (std::to_string(currentFrame) + ";");
      }
      currentFrame++;
    }
    EXPECT_EQ(errorMsg, "") << fileName << " frame fail";
  }
}

}  // namespace pag
#endif
