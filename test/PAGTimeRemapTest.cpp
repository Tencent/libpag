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

#ifdef FILE_MOVIE
#include <fstream>
#include "base/utils/TimeUtil.h"
#include "framework/pag_test.h"
#include "framework/utils/PAGTestUtils.h"
#include "nlohmann/json.hpp"
#include "video/VideoDecoder.h"

namespace pag {
using nlohmann::json;

json dumpJson_timeremap;
json compareJson_timeremap;
bool dumpStatus_timeremap;

PAG_TEST_SUIT(PAGTimeRemapTest)

void InitTimeRemapData() {
  std::ifstream inputFile("../test/res/compare_pagtimeremap_md5.json");
  if (inputFile) {
    inputFile >> compareJson_timeremap;
    dumpJson_timeremap = compareJson_timeremap;
  }
}

void SaveTimeRemapFile() {
  if (dumpStatus_timeremap) {
    std::ofstream outFile("../test/out/pagtimeremap_md5_dump.json");
    outFile << std::setw(4) << dumpJson_timeremap << std::endl;
    outFile.close();
  }
}

void TimeRemapTest(const std::string& path, const std::string& name) {
  std::vector<std::string> compareVector;

  if (compareJson_timeremap.contains(name) && compareJson_timeremap[name].is_array()) {
    compareVector = compareJson_timeremap[name].get<std::vector<std::string>>();
  }

  auto pagFile = pag::PAGFile::Load(path);
  auto pagMovie = pag::PAGMovie::FromVideoPath("../resources/timeremap/yuanshi.mp4");
  pagFile->replaceImage(0, pagMovie);

  auto pagSurface = pag::PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
  auto pagPlayer = new pag::PAGPlayer();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);
  auto totalFrames = pag::TimeToFrame(pagFile->duration(), pagFile->frameRate());
  auto count = std::min(totalFrames, static_cast<Frame>(20));
  std::vector<std::string> md5Vector;
  std::string errorMsg;
  bool status = true;
  int size = static_cast<int>(compareVector.size());
  for (int i = 0; i < count; i++) {
    pagPlayer->setProgress((i + 0.1) * 1.0 / totalFrames);
    pagPlayer->flush();

    auto skImage = MakeSnapshot(pagSurface);
    std::string md5 = DumpMD5(skImage);
    md5Vector.push_back(md5);
    if (size <= i || compareVector[i] != md5) {
      errorMsg += (std::to_string(i) + ";");
      dumpStatus_timeremap = true;
      if (status) {
        std::string imagePath = "../test/out/" + name + "_" + std::to_string(i) + ".png";
        Trace(skImage, imagePath);
        status = false;
      }
    }
  }
  EXPECT_EQ(errorMsg, "") << name << " frame fail";
  dumpJson_timeremap[name] = md5Vector;
}

/*CaseAdditionInfo start
{
"FT":"内核FT",
"模块":"TimeRemap",
"功能":"线性变速0。5倍",
"测试分类":"功能",
"测试阶段":"全特性",
"被测函数":"",
"管理者":"kevingpqi",
"用例等级":"P0",
"用例类型":"5",
"className":"PAGImageLayer",
"methodName":"Linear_variable_speed_0_5_ID860883981",
"用例描述":"PAGTimeRemapTest"
}
CaseAdditionInfo end*/
PAG_TEST_F(PAGTimeRemapTest, Linear_variable_speed_0_5_ID860883981) {
  InitTimeRemapData();
  TimeRemapTest("../resources/timeremap/biansu_0_5.pag", "biansu_0_5");
}

/*CaseAdditionInfo start
{
"FT":"内核FT",
"模块":"TimeRemap",
"功能":"线性变速0.75倍",
"测试分类":"功能",
"测试阶段":"全特性",
"被测函数":"",
"管理者":"kevingpqi",
"用例等级":"P0",
"用例类型":"5",
"className":"PAGImageLayer",
"methodName":"Linear_variable_speed_0_75_ID860883981",
"用例描述":"PAGTimeRemapTest"
}
CaseAdditionInfo end*/
PAG_TEST_F(PAGTimeRemapTest, Linear_variable_speed_0_75_ID860883981) {
  TimeRemapTest("../resources/timeremap/biansu_0_75.pag", "biansu_0_75");
}

/*CaseAdditionInfo start
{
"FT":"内核FT",
"模块":"TimeRemap",
"功能":"贝塞尔曲线变速",
"测试分类":"功能",
"测试阶段":"全特性",
"被测函数":"",
"管理者":"kevingpqi",
"用例等级":"P0",
"用例类型":"5",
"className":"PAGImageLayer",
"methodName":"Linear_variable_speed_bezier_ID860883981",
"用例描述":"PAGTimeRemapTest"
}
CaseAdditionInfo end*/
PAG_TEST_F(PAGTimeRemapTest, Linear_variable_speed_bezier_ID860883981) {
  TimeRemapTest("../resources/timeremap/biansu_bezier.pag", "biansu_bezier");
  SaveTimeRemapFile();
}

}  // namespace pag
#endif
