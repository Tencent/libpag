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

#include "framework/pag_test.h"
#include "framework/utils/PAGTestUtils.h"
#include "pag/pag.h"
#include "platform/swiftshader/NativePlatform.h"
#include "rendering/caches/RenderCache.h"

namespace pag {

PAG_TEST_SUIT(PAGSequenceTest)
void pagSequenceTest() {
    auto pagFile = PAGFile::Load("../resources/apitest/wz_mvp.pag");
    EXPECT_NE(pagFile, nullptr);
    auto pagSurface = PAGSurface::MakeOffscreen(750, 1334);
    auto pagPlayer = std::make_shared<PAGPlayer>();
    pagPlayer->setSurface(pagSurface);
    pagPlayer->setComposition(pagFile);
    pagPlayer->setMatrix(Matrix::I());
    pagPlayer->setProgress(0.5);
    pagPlayer->flush();
    EXPECT_EQ(static_cast<int>(pagPlayer->renderCache->sequenceCaches.size()), 1);
    auto md5 = DumpMD5(pagSurface);
    PAGTestEnvironment::DumpJson["PAGSequenceTest"]["RenderOnScreen_ID80475733"] = md5;
#ifdef COMPARE_JSON_PATH
    auto compareMD5 = PAGTestEnvironment::CompareJson["PAGSequenceTest"]["RenderOnScreen_ID80475733"];
    TraceIf(pagSurface, "../test/out/RenderOnScreen_ID80475733.png",
            compareMD5.get<std::string>() != md5);
    EXPECT_EQ(compareMD5.get<std::string>(), md5);
#endif
}

/**
 * 用例描述: 测试直接上屏
 */
PAG_TEST_F(PAGSequenceTest, RenderOnScreen_ID80475733) {
    pagSequenceTest();
}

/**
 * 用例描述: bitmapSequence关键帧不是全屏的时候要清屏
 */
PAG_TEST_F(PAGSequenceTest, BitmapSequenceReader_ID82681127) {
    auto pagFile = PAGFile::Load("../resources/apitest/ZC_mg_seky2_landscape.pag");
    auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
    auto pagPlayer = std::make_shared<PAGPlayer>();
    pagPlayer->setSurface(pagSurface);
    pagPlayer->setComposition(pagFile);
    pagPlayer->setProgress(0.5);
    pagPlayer->flush();
    pagPlayer->setProgress(0.75);
    pagPlayer->flush();
    auto md5 = DumpMD5(pagSurface);
    PAGTestEnvironment::DumpJson["PAGSequenceTest"]["BitmapSequenceReader_ID82681127"] = md5;
#ifdef COMPARE_JSON_PATH
    auto compareMD5 =
        PAGTestEnvironment::CompareJson["PAGSequenceTest"]["BitmapSequenceReader_ID82681127"];
    TraceIf(pagSurface, "../test/out/BitmapSequenceReader_ID82681127.png",
            compareMD5.get<std::string>() != md5);
    EXPECT_EQ(compareMD5.get<std::string>(), md5);
#endif
}

/**
 * 用例描述: 视频序列帧作为遮罩
 */
PAG_TEST_F(PAGSequenceTest, VideoSequenceAsMask_ID83191791) {
    auto pagFile = PAGFile::Load("../resources/apitest/video_sequence_as_mask.pag");
    auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
    auto pagPlayer = std::make_shared<PAGPlayer>();
    pagPlayer->setSurface(pagSurface);
    pagPlayer->setComposition(pagFile);
    pagPlayer->setProgress(0.2);
    pagPlayer->flush();
    auto md5 = DumpMD5(pagSurface);
    PAGTestEnvironment::DumpJson["PAGSequenceTest"]["VideoSequenceAsMask_ID83191791"] = md5;
#ifdef COMPARE_JSON_PATH
    auto compareMD5 =
        PAGTestEnvironment::CompareJson["PAGSequenceTest"]["VideoSequenceAsMask_ID83191791"];
    TraceIf(pagSurface, "../test/out/VideoSequenceAsMask_ID83191791.png",
            compareMD5.get<std::string>() != md5);
    EXPECT_EQ(compareMD5.get<std::string>(), md5);
#endif
}
}  // namespace pag
