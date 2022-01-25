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

#include <thread>
#include "framework/pag_test.h"
#include "framework/utils/PAGTestUtils.h"
#include "gpu/Surface.h"
#include "image/Image.h"
#include "nlohmann/json.hpp"
#include "pag/pag.h"

namespace pag {
using nlohmann::json;

PAG_TEST_SUIT(PAGImageTest)

/**
 * 用例描述: PAGImage contentDuration 接口
 */
PAG_TEST_F(PAGImageTest, contentDuration) {
    int target = 0;
    std::shared_ptr<PAGImageLayer> imageLayer =
        std::static_pointer_cast<PAGImageLayer>(GetLayer(TestPAGFile, LayerType::Image, target));
    ASSERT_EQ(imageLayer->contentDuration(), 2 * 1000000);
}

/**
 * 用例描述: PAGImage解码等功能
 */
PAG_TEST_F(PAGImageTest, image) {
    int target = 0;
    std::shared_ptr<PAGImageLayer> imageLayer =
        std::static_pointer_cast<PAGImageLayer>(GetLayer(TestPAGFile, LayerType::Image, target));
    ASSERT_NE(imageLayer, nullptr);
    ASSERT_EQ(imageLayer->editableIndex(), 1);
    int width = 110;
    int height = 110;
    size_t rowBytes = 110 * 4;
    auto fileData = ByteData::FromPath("../resources/apitest/data.rgba");
    Bitmap bitmap = {};
    auto result = bitmap.allocPixels(0, height);
    ASSERT_EQ(result, false);
    result = bitmap.allocPixels(width, height, true);
    ASSERT_EQ(result, true);
    EXPECT_EQ(bitmap.colorType(), ColorType::ALPHA_8);
    result = bitmap.allocPixels(width, height);
    ASSERT_EQ(result, true);
    auto info =
        ImageInfo::Make(width, height, ColorType::RGBA_8888, AlphaType::Premultiplied, rowBytes);
    bitmap.writePixels(info, fileData->data());
    auto newFileData = ByteData::Make(fileData->length());
    bitmap.readPixels(info, newFileData->data());
    auto compare = memcmp(fileData->data(), newFileData->data(), fileData->length());
    ASSERT_EQ(compare, 0);
    auto image = PAGImage::FromPixels(fileData->data(), width, height, rowBytes, ColorType::RGBA_8888,
                                      AlphaType::Premultiplied);
    TestPAGFile->setCurrentTime(3000000);
    imageLayer->replaceImage(image);
    TestPAGPlayer->flush();
    auto md5 = getMd5FromSnap();
    json imageLayerJson = {{"image", md5}};
    PAGTestEnvironment::DumpJson["PAGImageTest"] = imageLayerJson;
#ifdef COMPARE_JSON_PATH
    auto cJson = PAGTestEnvironment::CompareJson["PAGImageTest"]["image"];
    ASSERT_EQ(cJson.get<std::string>(), md5);
#endif
}

/**
 * 用例描述: PAGImage解码等功能
 */
PAG_TEST_F(PAGImageTest, image2) {
    auto image = Image::MakeFrom("../resources/apitest/imageReplacement.png");
    ASSERT_TRUE(image != nullptr);
    ASSERT_EQ(image->height(), 110);
    ASSERT_EQ(image->width(), 110);
    ASSERT_EQ(image->height(), 110);
    ASSERT_EQ(image->orientation(), Orientation::TopLeft);
    Bitmap bitmap = {};
    auto result = bitmap.allocPixels(image->width(), image->height());
    ASSERT_TRUE(result);
    auto pixels = bitmap.lockPixels();
    result = image->readPixels(bitmap.info(), pixels);
    bitmap.unlockPixels();
    ASSERT_TRUE(result);

    auto md5 = DumpMD5(bitmap);
    json imageLayerJson = {{"image", md5}};
    PAGTestEnvironment::DumpJson["PAGImageTester"] = imageLayerJson;
#ifdef COMPARE_JSON_PATH
    auto cJson = PAGTestEnvironment::CompareJson["PAGImageTester"]["image"];
    TraceIf(bitmap, "../test/out/PAGImageTester_image.png", md5 != cJson.get<std::string>());
    ASSERT_EQ(cJson.get<std::string>(), md5);
#endif
}

/**
 * 用例描述: 带旋转图片PAGImage解码等功能
 */
PAG_TEST_F(PAGImageTest, image3) {
    auto pagImage = PAGImage::FromPath("../resources/apitest/rotation.jpg");
    ASSERT_TRUE(pagImage != nullptr);
    EXPECT_EQ(pagImage->width(), 3024);
    EXPECT_EQ(pagImage->height(), 4032);
    auto pagFile = PAGFile::Load("../resources/apitest/replace2.pag");
    ASSERT_TRUE(pagFile != nullptr);
    pagFile->replaceImage(0, pagImage);
    auto surface = PAGSurface::MakeOffscreen(720, 720);
    ASSERT_TRUE(surface != nullptr);
    auto player = new PAGPlayer();
    player->setComposition(pagFile);
    player->setSurface(surface);
    auto result = player->flush();
    EXPECT_TRUE(result);
    auto md5 = DumpMD5(surface);
    PAGTestEnvironment::DumpJson["PAGImageDecodeTest"]["image_rotation"] = md5;
#ifdef COMPARE_JSON_PATH
    auto cJson = PAGTestEnvironment::CompareJson["PAGImageDecodeTest"]["image_rotation"];
    TraceIf(surface, "../test/out/test_image_rotation.png", md5 != cJson.get<std::string>());
    ASSERT_EQ(cJson.get<std::string>(), md5);
#endif
}
}  // namespace pag
