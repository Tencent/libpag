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

#include <thread>
#include "nlohmann/json.hpp"
#include "pag/pag.h"
#include "tgfx/core/ImageCodec.h"
#include "tgfx/core/Surface.h"
#include "tgfx/gpu/opengl/GLDevice.h"
#include "utils/TestUtils.h"

namespace pag {
using namespace tgfx;
using nlohmann::json;

/**
 * 用例描述: PAGImage contentDuration 接口
 */
PAG_TEST(PAGImageTest, contentDuration) {
  PAG_SETUP(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  int target = 0;
  std::shared_ptr<PAGImageLayer> imageLayer =
      std::static_pointer_cast<PAGImageLayer>(GetLayer(TestPAGFile, LayerType::Image, target));
  ASSERT_EQ(imageLayer->contentDuration(), 2 * 1000000);
}

/**
 * 用例描述: PAGImage解码等功能
 */
PAG_TEST(PAGImageTest, image) {
  PAG_SETUP(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  int target = 0;
  std::shared_ptr<PAGImageLayer> imageLayer =
      std::static_pointer_cast<PAGImageLayer>(GetLayer(TestPAGFile, LayerType::Image, target));
  ASSERT_NE(imageLayer, nullptr);
  ASSERT_EQ(imageLayer->editableIndex(), 1);
  int width = 110;
  int height = 110;
  size_t rowBytes = 110 * 4;
  auto fileData = ByteData::FromPath(ProjectPath::Absolute("resources/apitest/data.rgba"));
  Bitmap bitmap(0, height, false, false);
  ASSERT_TRUE(bitmap.isEmpty());
  bitmap.allocPixels(width, height, true, false);
  ASSERT_FALSE(bitmap.isEmpty());
  EXPECT_EQ(bitmap.colorType(), tgfx::ColorType::ALPHA_8);
  bitmap.allocPixels(width, height, false, false);
  ASSERT_FALSE(bitmap.isEmpty());
  Pixmap pixmap(bitmap);
  auto info = ImageInfo::Make(width, height, tgfx::ColorType::RGBA_8888,
                              tgfx::AlphaType::Premultiplied, rowBytes);
  auto result = pixmap.writePixels(info, fileData->data());
  ASSERT_TRUE(result);
  auto newFileData = ByteData::Make(fileData->length());
  pixmap.readPixels(info, newFileData->data());
  auto compare = memcmp(fileData->data(), newFileData->data(), fileData->length());
  ASSERT_EQ(compare, 0);
  auto emptyData = ByteData::Make(fileData->length());
  memset(emptyData->data(), 0, emptyData->length());
  pixmap.clear();
  compare = memcmp(pixmap.pixels(), emptyData->data(), emptyData->length());
  ASSERT_EQ(compare, 0);
  result = pixmap.writePixels(info, fileData->data(), 20, -10);
  ASSERT_TRUE(result);
  result = pixmap.readPixels(info, newFileData->data(), 20, -10);
  ASSERT_TRUE(result);
  compare = memcmp(fileData->data(), newFileData->data(), fileData->length());
  ASSERT_EQ(compare, 0);
  memset(emptyData->data(), 1, emptyData->length());
  auto emptyInfo = info.makeWH(100, height);
  Pixmap pixmap2(emptyInfo, emptyData->data());
  pixmap2.clear();
  auto bytes = reinterpret_cast<uint8_t*>(emptyData->data());
  EXPECT_TRUE(bytes[399] == 0);
  EXPECT_TRUE(bytes[400] == 1);
  EXPECT_TRUE(bytes[399 + info.rowBytes()] == 0);
  EXPECT_TRUE(bytes[400 + info.rowBytes()] == 1);
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
PAG_TEST(PAGImageTest, image2) {
  auto codec = MakeImageCodec("resources/apitest/imageReplacement.png");
  ASSERT_TRUE(codec != nullptr);
  ASSERT_EQ(codec->height(), 110);
  ASSERT_EQ(codec->width(), 110);
  ASSERT_EQ(codec->height(), 110);
  ASSERT_EQ(codec->orientation(), tgfx::Orientation::TopLeft);
  Bitmap bitmap(codec->width(), codec->height(), false, false);
  ASSERT_FALSE(bitmap.isEmpty());
  Pixmap pixmap(bitmap);
  auto result = codec->readPixels(pixmap.info(), pixmap.writablePixels());
  ASSERT_TRUE(result);
  EXPECT_TRUE(Baseline::Compare(pixmap, "PAGImageTest/image2"));
}

/**
 * 用例描述: 带旋转图片PAGImage解码等功能
 */
PAG_TEST(PAGImageTest, image3) {
  auto pagImage = MakePAGImage("resources/apitest/rotation.jpg");
  ASSERT_TRUE(pagImage != nullptr);
  EXPECT_EQ(pagImage->width(), 3024);
  EXPECT_EQ(pagImage->height(), 4032);
  auto pagFile = LoadPAGFile("resources/apitest/replace2.pag");
  ASSERT_TRUE(pagFile != nullptr);
  pagFile->replaceImage(0, pagImage);
  auto surface = OffscreenSurface::Make(720, 720);
  ASSERT_TRUE(surface != nullptr);
  auto player = std::make_unique<PAGPlayer>();
  player->setComposition(pagFile);
  player->setSurface(surface);
  auto result = player->flush();
  EXPECT_TRUE(result);
  EXPECT_TRUE(Baseline::Compare(surface, "PAGImageTest/image3"));
}

/**
 * 用例描述: texture 的 target 是 GL_TEXTURE_RECTANGLE，origin 是 BottomLeft，当作遮罩绘制。
 */
PAG_TEST(PAGImageTest, BottomLeftMask) {
  int width = 110;
  int height = 110;
  auto device = DevicePool::Make();
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);
  auto surface = Surface::Make(context, width, height);
  auto image1 = MakeImage("resources/apitest/imageReplacement.webp");
  auto imageAsMask = MakeImage("resources/apitest/image_as_mask.png");
  ASSERT_TRUE(imageAsMask != nullptr);
  auto image2 = imageAsMask->makeOriented(tgfx::Orientation::BottomLeft);
  ASSERT_TRUE(image2 != nullptr);
  auto canvas = surface->getCanvas();
  tgfx::Paint paint;
  paint.setMaskFilter(tgfx::MaskFilter::MakeShader(tgfx::Shader::MakeImageShader(image2)));
  canvas->drawImage(image1, &paint);
  Bitmap bitmap(width, height);
  ASSERT_FALSE(bitmap.isEmpty());
  Pixmap pixmap(bitmap);
  pixmap.clear();
  auto result = surface->readPixels(pixmap.info(), pixmap.writablePixels());
  ASSERT_TRUE(result);
  device->unlock();
  EXPECT_TRUE(Baseline::Compare(pixmap, "PAGImageTest/BottomLeftMask"));
}
}  // namespace pag
