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
#include "tgfx/core/Image.h"
#include "tgfx/gpu/Surface.h"
#include "tgfx/gpu/opengl/GLDevice.h"

namespace tgfx {
static bool Compare(Surface* surface, const std::string& key) {
  auto pixelBuffer = PixelBuffer::Make(surface->width(), surface->height());
  if (pixelBuffer == nullptr) {
    return false;
  }
  Bitmap bitmap(pixelBuffer);
  bitmap.eraseAll();
  auto result = surface->readPixels(bitmap.info(), bitmap.writablePixels());
  if (!result) {
    return false;
  }
  return pag::Baseline::Compare(bitmap, key);
}

/**
 * 用例描述: 测试 ColorMatrixFilter
 */
PAG_TEST(CanvasTest, ColorMatrixFilter) {
  auto device = GLDevice::Make();
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);
  auto image = Image::MakeFrom("../resources/apitest/test_timestretch.png")
                   ->makeBuffer()
                   ->makeTexture(context);
  ASSERT_TRUE(image != nullptr);
  auto surface = Surface::Make(context, image->width(), image->height());
  auto canvas = surface->getCanvas();
  tgfx::Paint paint;
  std::array<float, 20> matrix = {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0};
  paint.setColorFilter(ColorFilter::Matrix(matrix));
  canvas->drawTexture(image.get(), &paint);
  EXPECT_TRUE(Compare(surface.get(), "CanvasTest/identityMatrix"));
  canvas->clear();
  std::array<float, 20> greyColorMatrix = {0.21f, 0.72f, 0.07f, 0.41f, 0,  // red
                                           0.21f, 0.72f, 0.07f, 0.41f, 0,  // green
                                           0.21f, 0.72f, 0.07f, 0.41f, 0,  // blue
                                           0,     0,     0,     1.0f,  0};
  paint.setColorFilter(ColorFilter::Matrix(greyColorMatrix));
  canvas->drawTexture(image.get(), &paint);
  EXPECT_TRUE(Compare(surface.get(), "CanvasTest/greyColorMatrix"));
  device->unlock();
}
}  // namespace tgfx
