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

//#include "pag_test.h"
#include <fstream>
#include "framework/pag_test.h"
#include "framework/utils/PAGTestUtils.h"
#include "nlohmann/json.hpp"

namespace pag {

PAG_TEST_SUIT(PAGSolidLayerTest)

/**
 * 用例描述: PAGSolidLayerTest基础功能
 */
PAG_TEST_F(PAGSolidLayerTest, SolidColor) {
    int targetIndex = 0;
    TestPAGFile->setCurrentTime(1.5 * 1000);
    auto layer = GetLayer(TestPAGFile, LayerType::Solid, targetIndex);
    ASSERT_NE(layer, nullptr) << "don't find solidLayer" << std::endl;
    auto solidLayer = std::static_pointer_cast<PAGSolidLayer>(layer);
    ASSERT_TRUE(solidLayer->solidColor() == Green);
    solidLayer->setSolidColor(Red);
    ASSERT_EQ(Red, solidLayer->solidColor());
    TestPAGPlayer->flush();
    auto md5 = getMd5FromSnap();
    PAGTestEnvironment::DumpJson["PAGSolidLayerTest"] = {{"SolidColor", md5}};
#ifdef COMPARE_JSON_PATH
    auto colorJson = PAGTestEnvironment::CompareJson["PAGSolidLayerTest"]["SolidColor"];
    ASSERT_EQ(colorJson.get<std::string>(), md5);
#endif
}

}  // namespace pag
