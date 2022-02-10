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
#include "core/Image.h"
#include "framework/pag_test.h"
#include "framework/utils/PAGTestUtils.h"
#include "gpu/Surface.h"
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
  auto pixelBuffer = PixelBuffer::Make(0, height);
  ASSERT_TRUE(pixelBuffer == nullptr);
  pixelBuffer = PixelBuffer::Make(width, height, true);
  ASSERT_TRUE(pixelBuffer != nullptr);
  EXPECT_EQ(pixelBuffer->colorType(), ColorType::ALPHA_8);
  pixelBuffer = PixelBuffer::Make(width, height);
  ASSERT_TRUE(pixelBuffer != nullptr);
  Bitmap bitmap(pixelBuffer);
  auto info =
      ImageInfo::Make(width, height, ColorType::RGBA_8888, AlphaType::Premultiplied, rowBytes);
  auto result = bitmap.writePixels(info, fileData->data());
  ASSERT_TRUE(result);
  auto newFileData = ByteData::Make(fileData->length());
  bitmap.readPixels(info, newFileData->data());
  auto compare = memcmp(fileData->data(), newFileData->data(), fileData->length());
  ASSERT_EQ(compare, 0);
  auto emptyData = ByteData::Make(fileData->length());
  memset(emptyData->data(), 0, emptyData->length());
  bitmap.eraseAll();
  compare = memcmp(bitmap.pixels(), emptyData->data(), emptyData->length());
  ASSERT_EQ(compare, 0);
  result = bitmap.writePixels(info, fileData->data(), 20, -10);
  ASSERT_TRUE(result);
  result = bitmap.readPixels(info, newFileData->data(), 20, -10);
  ASSERT_TRUE(result);
  compare = memcmp(fileData->data(), newFileData->data(), fileData->length());
  ASSERT_EQ(compare, 0);
  memset(emptyData->data(), 1, emptyData->length());
  auto emptyInfo = info.makeWH(100, height);
  Bitmap bitmap2(emptyInfo, emptyData->data());
  bitmap2.eraseAll();
  auto bytes = reinterpret_cast<uint8_t*>(emptyData->data());
  EXPECT_TRUE(bytes[399] == 0);
  EXPECT_TRUE(bytes[400] == 1);
  auto image = PAGImage::FromPixels(fileData->data(), width, height, rowBytes, ColorType::RGBA_8888,
                                    AlphaType::Premultiplied);
  TestPAGFile->setCurrentTime(3000000);
  imageLayer->replaceImage(image);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGImageTest/image"));
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
  auto pixelBuffer = PixelBuffer::Make(image->width(), image->height());
  ASSERT_TRUE(pixelBuffer != nullptr);
  Bitmap bitmap(pixelBuffer);
  auto result = image->readPixels(bitmap.info(), bitmap.writablePixels());
  ASSERT_TRUE(result);
  EXPECT_TRUE(Baseline::Compare(bitmap, "PAGImageTest/image2"));
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
  EXPECT_TRUE(Baseline::Compare(surface, "PAGImageTest/image3"));
}
}  // namespace pag
