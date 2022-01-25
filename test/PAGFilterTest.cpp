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

#include <fstream>
#include "framework/pag_test.h"
#include "framework/utils/PAGTestUtils.h"
#include "nlohmann/json.hpp"

namespace pag {
using nlohmann::json;

PAG_TEST_CASE(PAGFilterTest)

/**
 * 用例描述: CornerPin用例
 */
PAG_TEST(PAGFilterTest, CornerPin_ID79157693) {
    json compareJson;
    std::ifstream inputFile("../test/res/compare_filter_md5.json");
    if (inputFile) {
        inputFile >> compareJson;
        inputFile.close();
    }
    auto pagFile = PAGFile::Load("../resources/filter/cornerpin.pag");
    ASSERT_NE(pagFile, nullptr);
    auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
    ASSERT_NE(pagSurface, nullptr);
    auto pagPlayer = std::make_shared<PAGPlayer>();
    pagPlayer->setSurface(pagSurface);
    pagPlayer->setComposition(pagFile);

    pagFile->setCurrentTime(1000000);
    pagPlayer->flush();
    auto snapshot = MakeSnapshot(pagSurface);
    std::string md5 = DumpMD5(snapshot);
#ifdef COMPARE_JSON_PATH
    auto cJson = compareJson["PAGFilterTest"]["cornerpin"];
    if (cJson != nullptr) {
        TraceIf(snapshot, "../test/out/pag_cornerpin_test_1000000.png",
                cJson.get<std::string>() != md5);
        EXPECT_EQ(cJson.get<std::string>(), md5);
    }
#endif
    json dumpJson;
    std::ifstream dumpInputFile("../test/out/compare_filter_md5.json");
    if (dumpInputFile) {
        dumpInputFile >> dumpJson;
        dumpInputFile.close();
    }
    dumpJson["PAGFilterTest"]["cornerpin"] = md5;
    std::ofstream outFile("../test/out/compare_filter_md5.json");
    outFile << std::setw(4) << dumpJson << std::endl;
    outFile.close();
}

/**
 * 用例描述: Bulge效果测试
 */
PAG_TEST(PAGFilterTest, Bulge_ID79159683) {
    json compareJson;
    std::ifstream inputFile("../test/res/compare_filter_md5.json");
    if (inputFile) {
        inputFile >> compareJson;
        inputFile.close();
    }
    auto pagFile = PAGFile::Load("../resources/filter/bulge.pag");
    ASSERT_NE(pagFile, nullptr);
    auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
    ASSERT_NE(pagSurface, nullptr);
    auto pagPlayer = std::make_shared<PAGPlayer>();
    pagPlayer->setSurface(pagSurface);
    pagPlayer->setComposition(pagFile);

    pagFile->setCurrentTime(300000);
    pagPlayer->flush();
    auto snapshot = MakeSnapshot(pagSurface);
    std::string md5 = DumpMD5(snapshot);
#ifdef COMPARE_JSON_PATH
    auto cJson = compareJson["PAGFilterTest"]["bulge"];
    if (cJson != nullptr) {
        TraceIf(snapshot, "../test/out/pag_bulge_test_300000.png", cJson.get<std::string>() != md5);
        EXPECT_EQ(cJson.get<std::string>(), md5);
    }
#endif

    json dumpJson;
    std::ifstream dumpInputFile("../test/out/compare_filter_md5.json");
    if (dumpInputFile) {
        dumpInputFile >> dumpJson;
        dumpInputFile.close();
    }
    dumpJson["PAGFilterTest"]["bulge"] = md5;
    std::ofstream outFile("../test/out/compare_filter_md5.json");
    outFile << std::setw(4) << dumpJson << std::endl;
    outFile.close();
}

/**
 * 用例描述: MotionTile效果测试
 */
PAG_TEST(PAGFilterTest, MotionTile_ID79162339) {
    json compareJson;
    std::ifstream inputFile("../test/res/compare_filter_md5.json");
    if (inputFile) {
        inputFile >> compareJson;
        inputFile.close();
    }
    auto pagFile = PAGFile::Load("../resources/filter/motiontile.pag");
    ASSERT_NE(pagFile, nullptr);
    auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
    ASSERT_NE(pagSurface, nullptr);
    auto pagPlayer = std::make_shared<PAGPlayer>();
    pagPlayer->setSurface(pagSurface);
    pagPlayer->setComposition(pagFile);

    pagFile->setCurrentTime(1000000);
    pagPlayer->flush();
    auto snapshot = MakeSnapshot(pagSurface);
    auto md5 = DumpMD5(snapshot);
#ifdef COMPARE_JSON_PATH
    auto cJson = compareJson["PAGFilterTest"]["motiontile"];
    if (cJson != nullptr) {
        TraceIf(snapshot, "../test/out/pag_motiontile_test_1000000.png",
                cJson.get<std::string>() != md5);
        EXPECT_EQ(cJson.get<std::string>(), md5);
    }
#endif
    json dumpJson;
    std::ifstream dumpInputFile("../test/out/compare_filter_md5.json");
    if (dumpInputFile) {
        dumpInputFile >> dumpJson;
        dumpInputFile.close();
    }
    dumpJson["PAGFilterTest"]["motiontile"] = md5;
    std::ofstream outFile("../test/out/compare_filter_md5.json");
    outFile << std::setw(4) << dumpJson << std::endl;
    outFile.close();
}

/**
 * 用例描述: MotionBlur效果测试
 */
PAG_TEST(PAGFilterTest, MotionBlur_ID79162447) {
    json compareJson;
    std::ifstream inputFile("../test/res/compare_filter_md5.json");
    if (inputFile) {
        inputFile >> compareJson;
        inputFile.close();
    }
    auto pagFile = PAGFile::Load("../resources/filter/MotionBlur.pag");
    ASSERT_NE(pagFile, nullptr);
    auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
    ASSERT_NE(pagSurface, nullptr);
    auto pagPlayer = std::make_shared<PAGPlayer>();
    pagPlayer->setSurface(pagSurface);
    pagPlayer->setComposition(pagFile);

    pagFile->setCurrentTime(200000);
    pagPlayer->flush();
    auto snapshot = MakeSnapshot(pagSurface);
    auto md5 = DumpMD5(snapshot);
#ifdef COMPARE_JSON_PATH
    auto cJson = compareJson["PAGFilterTest"]["motionblur_scale"];
    if (cJson != nullptr) {
        TraceIf(snapshot, "../test/out/pag_motionblur_test_200000.png",
                cJson.get<std::string>() != md5);
        EXPECT_EQ(cJson.get<std::string>(), md5);
    }
#endif
    json dumpJson;
    std::ifstream dumpInputFile("../test/out/compare_filter_md5.json");
    if (dumpInputFile) {
        dumpInputFile >> dumpJson;
        dumpInputFile.close();
    }
    dumpJson["PAGFilterTest"]["motionblur_scale"] = md5;

    pagFile->setCurrentTime(600000);
    pagPlayer->flush();
    snapshot = MakeSnapshot(pagSurface);
    md5 = DumpMD5(snapshot);

#ifdef COMPARE_JSON_PATH
    cJson = compareJson["PAGFilterTest"]["motionblur_translate"];
    if (cJson != nullptr) {
        TraceIf(snapshot, "../test/out/pag_motionblur_test_600000.png",
                cJson.get<std::string>() != md5);
        EXPECT_EQ(cJson.get<std::string>(), md5);
    }
#endif
    dumpJson["PAGFilterTest"]["motionblur_translate"] = md5;
    std::ofstream outFile("../test/out/compare_filter_md5.json");
    outFile << std::setw(4) << dumpJson << std::endl;
    outFile.close();
}

/**
 * 用例描述: GaussBlur效果测试
 */
PAG_TEST(PAGFilterTest, GaussBlur_ID79162977) {
    json compareJson;
    std::ifstream inputFile("../test/res/compare_filter_md5.json");
    if (inputFile) {
        inputFile >> compareJson;
        inputFile.close();
    }
    auto pagFile = PAGFile::Load("../resources/filter/fastblur.pag");
    ASSERT_NE(pagFile, nullptr);
    auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
    ASSERT_NE(pagSurface, nullptr);
    auto pagPlayer = std::make_shared<PAGPlayer>();
    pagPlayer->setSurface(pagSurface);
    pagPlayer->setComposition(pagFile);

    pagFile->setCurrentTime(1000000);
    pagPlayer->flush();
    auto snapshot = MakeSnapshot(pagSurface);
    std::string md5 = DumpMD5(snapshot);
#ifdef COMPARE_JSON_PATH
    auto cJson = compareJson["PAGFilterTest"]["gaussblur"];
    if (cJson != nullptr) {
        TraceIf(snapshot, "../test/out/pag_gaussblur_test_1000000.png",
                cJson.get<std::string>() != md5);
        EXPECT_EQ(cJson.get<std::string>(), md5);
    }
#endif
    json dumpJson;
    std::ifstream dumpInputFile("../test/out/compare_filter_md5.json");
    if (dumpInputFile) {
        dumpInputFile >> dumpJson;
        dumpInputFile.close();
    }
    dumpJson["PAGFilterTest"]["gaussblur"] = md5;

    pagFile = PAGFile::Load("../resources/filter/fastblur_norepeat.pag");
    ASSERT_NE(pagFile, nullptr);
    pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
    ASSERT_NE(pagSurface, nullptr);
    pagPlayer = std::make_shared<PAGPlayer>();
    pagPlayer->setSurface(pagSurface);
    pagPlayer->setComposition(pagFile);

    pagFile->setCurrentTime(1000000);
    pagPlayer->flush();
    snapshot = MakeSnapshot(pagSurface);
    md5 = DumpMD5(snapshot);
#ifdef COMPARE_JSON_PATH
    cJson = compareJson["PAGFilterTest"]["gaussblur_norepeat"];
    if (cJson != nullptr) {
        TraceIf(snapshot, "../test/out/pag_gaussblur_norepeat_test_1000000.png",
                cJson.get<std::string>() != md5);
        EXPECT_EQ(cJson.get<std::string>(), md5);
    }
#endif
    dumpJson["PAGFilterTest"]["gaussblur_norepeat"] = md5;
    std::ofstream outFile("../test/out/compare_filter_md5.json");
    outFile << std::setw(4) << dumpJson << std::endl;
    outFile.close();
}

/**
 * 用例描述: Glow效果测试
 */
PAG_TEST(PAGFilterTest, Glow_ID79163671) {
    json compareJson;
    std::ifstream inputFile("../test/res/compare_filter_md5.json");
    if (inputFile) {
        inputFile >> compareJson;
        inputFile.close();
    }
    auto pagFile = PAGFile::Load("../resources/filter/Glow.pag");
    ASSERT_NE(pagFile, nullptr);
    auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
    ASSERT_NE(pagSurface, nullptr);
    auto pagPlayer = std::make_shared<PAGPlayer>();
    pagPlayer->setSurface(pagSurface);
    pagPlayer->setComposition(pagFile);

    pagFile->setCurrentTime(200000);
    pagPlayer->flush();
    auto snapshot = MakeSnapshot(pagSurface);
    std::string md5 = DumpMD5(snapshot);
#ifdef COMPARE_JSON_PATH
    auto cJson = compareJson["PAGFilterTest"]["glow"];
    if (cJson != nullptr) {
        TraceIf(snapshot, "../test/out/pag_glow_test_200000.png", cJson.get<std::string>() != md5);
        EXPECT_EQ(cJson.get<std::string>(), md5);
    }
#endif
    json dumpJson;
    std::ifstream dumpInputFile("../test/out/compare_filter_md5.json");
    if (dumpInputFile) {
        dumpInputFile >> dumpJson;
        dumpInputFile.close();
    }
    dumpJson["PAGFilterTest"]["glow"] = md5;
    std::ofstream outFile("../test/out/compare_filter_md5.json");
    outFile << std::setw(4) << dumpJson << std::endl;
    outFile.close();
}

/**
 * 用例描述: DropShadow效果测试
 */
PAG_TEST(PAGFilterTest, DropShadow_ID79164133) {
    json compareJson;
    std::ifstream inputFile("../test/res/compare_filter_md5.json");
    if (inputFile) {
        inputFile >> compareJson;
        inputFile.close();
    }
    auto pagFile = PAGFile::Load("../resources/filter/DropShadow.pag");
    ASSERT_NE(pagFile, nullptr);
    auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
    ASSERT_NE(pagSurface, nullptr);
    auto pagPlayer = std::make_shared<PAGPlayer>();
    pagPlayer->setSurface(pagSurface);
    pagPlayer->setComposition(pagFile);

    pagFile->setCurrentTime(1000000);
    pagPlayer->flush();
    auto snapshot = MakeSnapshot(pagSurface);
    std::string md5 = DumpMD5(snapshot);
#ifdef COMPARE_JSON_PATH
    auto cJson = compareJson["PAGFilterTest"]["dropshadow"];
    if (cJson != nullptr) {
        TraceIf(snapshot, "../test/out/pag_dropshadow_test_200000.png",
                cJson.get<std::string>() != md5);
        EXPECT_EQ(cJson.get<std::string>(), md5);
    }
#endif
    json dumpJson;
    std::ifstream dumpInputFile("../test/out/compare_filter_md5.json");
    if (dumpInputFile) {
        dumpInputFile >> dumpJson;
        dumpInputFile.close();
    }
    dumpJson["PAGFilterTest"]["dropshadow"] = md5;
    std::ofstream outFile("../test/out/compare_filter_md5.json");
    outFile << std::setw(4) << dumpJson << std::endl;
    outFile.close();
}

/**
 * 用例描述: DisplacementMap
 */
PAG_TEST(PAGFilterTest, DisplacementMap_ID79234919) {
    json compareJson;
    std::ifstream inputFile("../test/res/compare_filter_md5.json");
    if (inputFile) {
        inputFile >> compareJson;
        inputFile.close();
    }
    auto pagFile = PAGFile::Load("../resources/filter/DisplacementMap.pag");
    ASSERT_NE(pagFile, nullptr);
    auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
    ASSERT_NE(pagSurface, nullptr);
    auto pagPlayer = std::make_shared<PAGPlayer>();
    pagPlayer->setSurface(pagSurface);
    pagPlayer->setComposition(pagFile);

    pagFile->setCurrentTime(600000);
    pagPlayer->flush();
    auto snapshot = MakeSnapshot(pagSurface);
    std::string md5 = DumpMD5(snapshot);
#ifdef COMPARE_JSON_PATH
    auto cJson = compareJson["PAGFilterTest"]["displacementmap"];
    if (cJson != nullptr) {
        TraceIf(snapshot, "../test/out/pag_displacementmap_test_600000.png",
                cJson.get<std::string>() != md5);
        EXPECT_EQ(cJson.get<std::string>(), md5);
    }
#endif
    json dumpJson;
    std::ifstream dumpInputFile("../test/out/compare_filter_md5.json");
    if (dumpInputFile) {
        dumpInputFile >> dumpJson;
        dumpInputFile.close();
    }
    dumpJson["PAGFilterTest"]["displacementmap"] = md5;
    std::ofstream outFile("../test/out/compare_filter_md5.json");
    outFile << std::setw(4) << dumpJson << std::endl;
    outFile.close();
}

/**
 * 用例描述: DisplacementMap特殊用例测试，包含：缩放，模糊
 */
PAG_TEST(PAGFilterTest, DisplacementMap_ID82637265) {
    json compareJson;
    std::ifstream inputFile("../test/res/compare_filter_md5.json");
    if (inputFile) {
        inputFile >> compareJson;
        inputFile.close();
    }
    auto pagFile = PAGFile::Load("../resources/filter/displement_map_video_scale.pag");
    ASSERT_NE(pagFile, nullptr);
    auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
    ASSERT_NE(pagSurface, nullptr);
    auto pagPlayer = std::make_shared<PAGPlayer>();
    pagPlayer->setSurface(pagSurface);
    pagPlayer->setComposition(pagFile);

    pagFile->setCurrentTime(3632520);
    pagPlayer->flush();
    auto snapshot = MakeSnapshot(pagSurface);
    std::string md5 = DumpMD5(snapshot);
#ifdef COMPARE_JSON_PATH
    auto cJson = compareJson["PAGFilterTest"]["displacementmap_video_scale"];
    if (cJson != nullptr) {
        TraceIf(snapshot, "../test/out/displacementmap_video_scale_3632520.png",
                cJson.get<std::string>() != md5);
        EXPECT_EQ(cJson.get<std::string>(), md5);
    }
#endif
    json dumpJson;
    std::ifstream dumpInputFile("../test/out/compare_filter_md5.json");
    if (dumpInputFile) {
        dumpInputFile >> dumpJson;
        dumpInputFile.close();
    }
    dumpJson["PAGFilterTest"]["displacementmap_video_scale"] = md5;
    std::ofstream outFile("../test/out/compare_filter_md5.json");
    outFile << std::setw(4) << dumpJson << std::endl;
    outFile.close();
}

/**
 * 用例描述: GaussBlur_Static
 */
PAG_TEST(PAGFilterTest, GaussBlur_Static) {
    json compareJson;
    std::ifstream inputFile("../test/res/compare_filter_md5.json");
    if (inputFile) {
        inputFile >> compareJson;
        inputFile.close();
    }
    auto pagFile = PAGFile::Load("../resources/filter/GaussBlur_Static.pag");
    ASSERT_NE(pagFile, nullptr);
    auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
    ASSERT_NE(pagSurface, nullptr);
    auto pagPlayer = std::make_shared<PAGPlayer>();
    pagPlayer->setSurface(pagSurface);
    pagPlayer->setComposition(pagFile);

    pagFile->setCurrentTime(0);
    pagPlayer->flush();
    auto snapshot = MakeSnapshot(pagSurface);
    std::string md5 = DumpMD5(snapshot);
#ifdef COMPARE_JSON_PATH
    auto cJson = compareJson["PAGFilterTest"]["gaussblur_static"];
    if (cJson != nullptr) {
        TraceIf(snapshot, "../test/out/gaussblur_static.png", cJson.get<std::string>() != md5);
        EXPECT_EQ(cJson.get<std::string>(), md5);
    }
#endif
    json dumpJson;
    std::ifstream dumpInputFile("../test/out/compare_filter_md5.json");
    if (dumpInputFile) {
        dumpInputFile >> dumpJson;
        dumpInputFile.close();
    }
    dumpJson["PAGFilterTest"]["gaussblur_static"] = md5;
    std::ofstream outFile("../test/out/compare_filter_md5.json");
    outFile << std::setw(4) << dumpJson << std::endl;
    outFile.close();
}

/**
 * 用例描述: RadialBlur
 */
PAG_TEST(PAGFilterTest, RadialBlur) {
    json compareJson;
    std::ifstream inputFile("../test/res/compare_filter_md5.json");
    if (inputFile) {
        inputFile >> compareJson;
        inputFile.close();
    }
    auto pagFile = PAGFile::Load("../resources/filter/RadialBlur.pag");
    ASSERT_NE(pagFile, nullptr);
    auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
    ASSERT_NE(pagSurface, nullptr);
    auto pagPlayer = std::make_shared<PAGPlayer>();
    pagPlayer->setSurface(pagSurface);
    pagPlayer->setComposition(pagFile);

    pagFile->setCurrentTime(1000000);
    pagPlayer->flush();
    auto snapshot = MakeSnapshot(pagSurface);
    std::string md5 = DumpMD5(snapshot);
#ifdef COMPARE_JSON_PATH
    auto cJson = compareJson["PAGFilterTest"]["radialblur"];
    if (cJson != nullptr) {
        TraceIf(snapshot, "../test/out/pag_radialblur_test_1000000.png",
                cJson.get<std::string>() != md5);
        EXPECT_EQ(cJson.get<std::string>(), md5);
    }
#endif
    json dumpJson;
    std::ifstream dumpInputFile("../test/out/compare_filter_md5.json");
    if (dumpInputFile) {
        dumpInputFile >> dumpJson;
        dumpInputFile.close();
    }
    dumpJson["PAGFilterTest"]["radialblur"] = md5;
    std::ofstream outFile("../test/out/compare_filter_md5.json");
    outFile << std::setw(4) << dumpJson << std::endl;
    outFile.close();
}

/**
 * 用例描述: Mosaic
 */
PAG_TEST(PAGFilterTest, Mosaic) {
    json compareJson;
    std::ifstream inputFile("../test/res/compare_filter_md5.json");
    if (inputFile) {
        inputFile >> compareJson;
        inputFile.close();
    }
    auto pagFile = PAGFile::Load("../resources/filter/MosaicChange.pag");
    ASSERT_NE(pagFile, nullptr);
    auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
    ASSERT_NE(pagSurface, nullptr);
    auto pagPlayer = std::make_shared<PAGPlayer>();
    pagPlayer->setSurface(pagSurface);
    pagPlayer->setComposition(pagFile);

    pagFile->setCurrentTime(0);
    pagPlayer->flush();
    auto snapshot = MakeSnapshot(pagSurface);
    std::string md5 = DumpMD5(snapshot);
#ifdef COMPARE_JSON_PATH
    auto cJson = compareJson["PAGFilterTest"]["mosaic"];
    if (cJson != nullptr) {
        TraceIf(snapshot, "../test/out/pag_mosaic_test_1000000.png", cJson.get<std::string>() != md5);
        EXPECT_EQ(cJson.get<std::string>(), md5);
    }
#endif
    json dumpJson;
    std::ifstream dumpInputFile("../test/out/compare_filter_md5.json");
    if (dumpInputFile) {
        dumpInputFile >> dumpJson;
        dumpInputFile.close();
    }
    dumpJson["PAGFilterTest"]["mosaic"] = md5;
    std::ofstream outFile("../test/out/compare_filter_md5.json");
    outFile << std::setw(4) << dumpJson << std::endl;
    outFile.close();
}

/**
 * 用例描述: 多滤镜混合效果测试
 */
PAG_TEST(PAGFilterTest, MultiFilter_ID79164477) {
    json compareJson;
    std::ifstream inputFile("../test/res/compare_filter_md5.json");
    if (inputFile) {
        inputFile >> compareJson;
        inputFile.close();
    }
    auto pagFile = PAGFile::Load("../resources/filter/cornerpin-bulge.pag");
    ASSERT_NE(pagFile, nullptr);
    auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
    ASSERT_NE(pagSurface, nullptr);
    auto pagPlayer = std::make_shared<PAGPlayer>();
    pagPlayer->setSurface(pagSurface);
    pagPlayer->setComposition(pagFile);

    pagFile->setCurrentTime(1000000);
    pagPlayer->flush();
    auto snapshot = MakeSnapshot(pagSurface);
    std::string md5 = DumpMD5(snapshot);
#ifdef COMPARE_JSON_PATH
    auto cJson = compareJson["PAGFilterTest"]["cornerpin-bulge"];
    if (cJson != nullptr) {
        TraceIf(snapshot, "../test/out/pag_cornerpin_bulge_test_1000000.png",
                cJson.get<std::string>() != md5);
        EXPECT_EQ(cJson.get<std::string>(), md5);
    }
#endif
    json dumpJson;
    std::ifstream dumpInputFile("../test/out/compare_filter_md5.json");
    if (dumpInputFile) {
        dumpInputFile >> dumpJson;
        dumpInputFile.close();
    }
    dumpJson["PAGFilterTest"]["cornerpin-bulge"] = md5;

    pagFile = PAGFile::Load("../resources/filter/motiontile_blur.pag");
    ASSERT_NE(pagFile, nullptr);
    pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
    ASSERT_NE(pagSurface, nullptr);
    pagPlayer = std::make_shared<PAGPlayer>();
    pagPlayer->setSurface(pagSurface);
    pagPlayer->setComposition(pagFile);

    pagFile->setCurrentTime(400000);
    pagPlayer->flush();
    snapshot = MakeSnapshot(pagSurface);
    md5 = DumpMD5(snapshot);
#ifdef COMPARE_JSON_PATH
    cJson = compareJson["PAGFilterTest"]["motiontile_blur"];
    if (cJson != nullptr) {
        TraceIf(snapshot, "../test/out/pag_motiontile_blur_test_400000.png",
                cJson.get<std::string>() == md5);
        EXPECT_EQ(cJson.get<std::string>(), md5);
    }
#endif
    dumpJson["PAGFilterTest"]["motiontile_blur"] = md5;
    std::ofstream outFile("../test/out/compare_filter_md5.json");
    outFile << std::setw(4) << dumpJson << std::endl;
    outFile.close();
}
}  // namespace pag
