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

#include "gpu/DrawingManager.h"
#include "gpu/ops/FillRectOp.h"
#include "tgfx/core/ImageReader.h"
#include "tgfx/core/Mask.h"
#include "tgfx/core/PathEffect.h"
#include "tgfx/gpu/Surface.h"
#include "tgfx/opengl/GLDevice.h"
#include "utils/TestUtils.h"

using namespace pag;

namespace tgfx {
/**
 * 用例描述: 测试使用 ImageReader 读取更新后的 Mask
 */
PAG_TEST(ImageReaderTest, updateMask) {
  auto device = GLDevice::Make();
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);
  auto mask = Mask::Make(100, 50, false);
  auto surface = Surface::Make(context, mask->width(), mask->height());
  auto canvas = surface->getCanvas();
  Path path;
  path.addRect(Rect::MakeXYWH(10, 10, 10, 10));
  mask->fillPath(path);
  auto reader = ImageReader::MakeFrom(mask);
  auto buffer = reader->acquireNextBuffer();
  EXPECT_TRUE(buffer != nullptr);
  auto maskImage = Image::MakeFrom(buffer);
  auto newBuffer = reader->acquireNextBuffer();
  EXPECT_FALSE(newBuffer != nullptr);
  canvas->drawImage(maskImage);
  canvas->flush();

  path.reset();
  path.addRoundRect(Rect::MakeXYWH(22, 22, 10, 10), 3, 3);
  mask->fillPath(path);
  newBuffer = reader->acquireNextBuffer();
  EXPECT_TRUE(newBuffer != nullptr);
  maskImage = Image::MakeFrom(newBuffer);
  canvas->setMatrix(Matrix::MakeTrans(50, 0));
  canvas->drawImage(maskImage);
  EXPECT_FALSE(buffer->expired());
  canvas->flush();
  EXPECT_TRUE(buffer->expired());

  EXPECT_TRUE(Baseline::Compare(surface, "ImageReaderTest/update_mask"));
  device->unlock();
}

/**
 * 用例描述: 测试使用 ImageReader 读取更新后的 Bitmap
 */
PAG_TEST(ImageReaderTest, updateBitmap) {
  auto device = GLDevice::Make();
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);
  Bitmap bitmap(300, 150);
  bitmap.clear();
  auto surface = Surface::Make(context, bitmap.width(), bitmap.height());
  auto canvas = surface->getCanvas();
  auto codec = MakeImageCodec("resources/apitest/imageReplacement.png");
  ASSERT_TRUE(codec != nullptr);
  auto pixels = bitmap.lockPixels();
  ASSERT_TRUE(pixels != nullptr);
  codec->readPixels(bitmap.info(), pixels);
  bitmap.unlockPixels();
  auto reader = ImageReader::MakeFrom(bitmap);
  auto buffer = reader->acquireNextBuffer();
  EXPECT_TRUE(buffer != nullptr);
  auto image = Image::MakeFrom(buffer);
  auto newBuffer = reader->acquireNextBuffer();
  EXPECT_FALSE(newBuffer != nullptr);
  canvas->drawImage(image);
  canvas->flush();
  if (tgfx::HardwareBufferAvailable()) {
    context->submit(true);
  }

  auto codec2 = MakeImageCodec("resources/apitest/image_as_mask.png");
  ASSERT_TRUE(codec != nullptr);
  pixels = bitmap.lockPixels();
  ASSERT_TRUE(pixels != nullptr);
  pixels = bitmap.info().computeOffset(pixels, 100, 0);
  codec2->readPixels(bitmap.info(), pixels);
  bitmap.unlockPixels();
  if (tgfx::HardwareBufferAvailable()) {
    EXPECT_TRUE(buffer->expired());
  }
  newBuffer = reader->acquireNextBuffer();
  EXPECT_TRUE(newBuffer != nullptr);
  image = Image::MakeFrom(newBuffer);
  canvas->setMatrix(Matrix::MakeTrans(120, 0));
  canvas->drawImage(image);
  if (!tgfx::HardwareBufferAvailable()) {
    EXPECT_FALSE(buffer->expired());
  }
  canvas->flush();
  EXPECT_TRUE(buffer->expired());

  EXPECT_TRUE(Baseline::Compare(surface, "ImageReaderTest/update_bitmap"));
  device->unlock();
}

}  // namespace tgfx
