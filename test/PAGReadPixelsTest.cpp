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
#include <vector>
#include "framework/pag_test.h"
#include "framework/utils/PAGTestUtils.h"
#include "gpu/Surface.h"
#include "gpu/opengl/GLUtil.h"
#include "image/Image.h"
#include "image/PixelMap.h"
#include "platform/NativeGLDevice.h"

namespace pag {
using nlohmann::json;

#define CHECK_PIXELS(info, pixels, key)                                      \
  {                                                                          \
    PixelMap pm(info, pixels);                                               \
    Baseline::Compare(pm, "PAGReadPixelsTest/" + std::string(key) + ".webp"); \
  }

/**
 * 用例描述: 像素格式转换相关功能测试-PixelMap
 */
PAG_TEST(PAGReadPixelsTest, TestPixelMap) {
  auto image = Image::MakeFrom("../resources/apitest/test_timestretch.png");
  EXPECT_TRUE(image != nullptr);
  auto width = image->width();
  auto height = image->height();
  auto RGBAInfo = ImageInfo::Make(width, height, ColorType::RGBA_8888, AlphaType::Unpremultiplied);
  auto byteSize = RGBAInfo.byteSize();
  auto pixelsA = new uint8_t[byteSize];
  auto pixelsB = new uint8_t[byteSize];
  auto result = image->readPixels(RGBAInfo, pixelsA);
  EXPECT_TRUE(result);

  PixelMap RGBAMap(RGBAInfo, pixelsA);
  CHECK_PIXELS(RGBAInfo, pixelsA, "PixelMap_RGBA_Original");

  result = RGBAMap.readPixels(RGBAInfo, pixelsB);
  EXPECT_TRUE(result);
  CHECK_PIXELS(RGBAInfo, pixelsB, "PixelMap_RGBA_to_RGBA");

  memset(pixelsB, 0, RGBAInfo.byteSize());
  result = RGBAMap.readPixels(RGBAInfo, pixelsB, 100, 100);
  ASSERT_TRUE(result);
  CHECK_PIXELS(RGBAInfo, pixelsB, "PixelMap_RGBA_to_RGBA_100_100");

  auto RGBARectInfo = ImageInfo::Make(500, 500, ColorType::RGBA_8888, AlphaType::Premultiplied);
  memset(pixelsB, 0, RGBARectInfo.byteSize());
  result = RGBAMap.readPixels(RGBARectInfo, pixelsB, -100, -100);
  ASSERT_TRUE(result);
  CHECK_PIXELS(RGBARectInfo, pixelsB, "PixelMap_RGBA_to_RGBA_-100_-100");

  memset(pixelsB, 0, RGBARectInfo.byteSize());
  result = RGBAMap.readPixels(RGBARectInfo, pixelsB, 100, -100);
  ASSERT_TRUE(result);
  CHECK_PIXELS(RGBARectInfo, pixelsB, "PixelMap_RGBA_to_RGBA_100_-100");

  auto rgbAInfo = RGBAInfo.makeAlphaType(AlphaType::Premultiplied);
  result = RGBAMap.readPixels(rgbAInfo, pixelsB);
  EXPECT_TRUE(result);
  CHECK_PIXELS(rgbAInfo, pixelsB, "PixelMap_RGBA_to_rgbA");

  auto bgrAInfo = rgbAInfo.makeColorType(ColorType::BGRA_8888);
  result = RGBAMap.readPixels(bgrAInfo, pixelsB);
  EXPECT_TRUE(result);
  CHECK_PIXELS(bgrAInfo, pixelsB, "PixelMap_RGBA_to_bgrA");

  auto BGRAInfo = bgrAInfo.makeAlphaType(AlphaType::Unpremultiplied);
  result = RGBAMap.readPixels(BGRAInfo, pixelsB);
  EXPECT_TRUE(result);
  CHECK_PIXELS(BGRAInfo, pixelsB, "PixelMap_RGBA_to_BGRA");

  PixelMap BGRAMap(BGRAInfo, pixelsB);

  result = BGRAMap.readPixels(BGRAInfo, pixelsA);
  EXPECT_TRUE(result);
  CHECK_PIXELS(BGRAInfo, pixelsA, "PixelMap_BGRA_to_BGRA");

  result = BGRAMap.readPixels(RGBAInfo, pixelsA);
  EXPECT_TRUE(result);
  CHECK_PIXELS(RGBAInfo, pixelsA, "PixelMap_BGRA_to_RGBA");

  result = BGRAMap.readPixels(rgbAInfo, pixelsA);
  EXPECT_TRUE(result);
  CHECK_PIXELS(rgbAInfo, pixelsA, "PixelMap_BGRA_to_rgbA");

  PixelMap rgbAMap(rgbAInfo, pixelsA);

  result = rgbAMap.readPixels(RGBAInfo, pixelsB);
  EXPECT_TRUE(result);
  CHECK_PIXELS(RGBAInfo, pixelsB, "PixelMap_rgbA_to_RGBA");

  result = rgbAMap.readPixels(BGRAInfo, pixelsB);
  EXPECT_TRUE(result);
  CHECK_PIXELS(BGRAInfo, pixelsB, "PixelMap_rgbA_to_BGRA");

  result = rgbAMap.readPixels(bgrAInfo, pixelsB);
  EXPECT_TRUE(result);
  CHECK_PIXELS(bgrAInfo, pixelsB, "PixelMap_rgbA_to_bgrA");

  auto A8Info = ImageInfo::Make(width, height, ColorType::ALPHA_8, AlphaType::Unpremultiplied);
  EXPECT_EQ(A8Info.alphaType(), AlphaType::Premultiplied);
  auto alphaByteSize = A8Info.byteSize();
  auto pixelsC = new uint8_t[alphaByteSize];

  result = rgbAMap.readPixels(A8Info, pixelsC);
  EXPECT_TRUE(result);
  CHECK_PIXELS(A8Info, pixelsC, "PixelMap_rgbA_to_alpha");

  PixelMap A8Map(A8Info, pixelsC);

  result = A8Map.readPixels(rgbAInfo, pixelsB);
  EXPECT_TRUE(result);
  CHECK_PIXELS(rgbAInfo, pixelsB, "PixelMap_alpha_to_rgbA");

  result = A8Map.readPixels(BGRAInfo, pixelsB);
  EXPECT_TRUE(result);
  CHECK_PIXELS(BGRAInfo, pixelsB, "PixelMap_alpha_to_BGRA");

  delete[] pixelsA;
  delete[] pixelsB;
  delete[] pixelsC;
}

/**
 * 用例描述: 像素格式转换相关功能测试-SurfaceReadPixels
 */
PAG_TEST(PAGReadPixelsTest, TestSurfaceReadPixels) {
  auto image = Image::MakeFrom("../resources/apitest/test_timestretch.png");
  ASSERT_TRUE(image != nullptr);
  Bitmap bitmap = {};
  auto result = bitmap.allocPixels(image->width(), image->height());
  ASSERT_TRUE(result);
  auto pixels = bitmap.lockPixels();
  result = image->readPixels(bitmap.info(), pixels);
  bitmap.unlockPixels();
  ASSERT_TRUE(result);

  auto device = NativeGLDevice::Make();
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);
  auto texture = bitmap.makeTexture(context);
  ASSERT_TRUE(texture != nullptr);
  auto surface = Surface::Make(context, bitmap.width(), bitmap.height());
  ASSERT_TRUE(surface != nullptr);
  auto canvas = surface->getCanvas();
  canvas->drawTexture(texture.get());

  BitmapLock lock(bitmap);
  auto width = bitmap.width();
  auto height = bitmap.height();
  pixels = lock.pixels();

  auto RGBAInfo = ImageInfo::Make(width, height, ColorType::RGBA_8888, AlphaType::Premultiplied);
  result = surface->readPixels(RGBAInfo, pixels);
  ASSERT_TRUE(result);
  CHECK_PIXELS(RGBAInfo, pixels, "Surface_rgbA_to_rgbA");

  auto BGRAInfo = ImageInfo::Make(width, height, ColorType::BGRA_8888, AlphaType::Premultiplied);
  result = surface->readPixels(BGRAInfo, pixels);
  ASSERT_TRUE(result);
  CHECK_PIXELS(BGRAInfo, pixels, "Surface_rgbA_to_bgrA");

  memset(pixels, 0, RGBAInfo.byteSize());
  result = surface->readPixels(RGBAInfo, pixels, 100, 100);
  ASSERT_TRUE(result);
  CHECK_PIXELS(RGBAInfo, pixels, "Surface_rgbA_to_rgbA_100_100");

  auto RGBARectInfo = ImageInfo::Make(500, 500, ColorType::RGBA_8888, AlphaType::Premultiplied);
  memset(pixels, 0, RGBARectInfo.byteSize());
  result = surface->readPixels(RGBARectInfo, pixels, -100, -100);
  ASSERT_TRUE(result);
  CHECK_PIXELS(RGBARectInfo, pixels, "Surface_rgbA_to_rgbA_-100_-100");

  memset(pixels, 0, RGBARectInfo.byteSize());
  result = surface->readPixels(RGBARectInfo, pixels, 100, -100);
  ASSERT_TRUE(result);
  CHECK_PIXELS(RGBARectInfo, pixels, "Surface_rgbA_to_rgbA_100_-100");

  surface = Surface::Make(context, bitmap.width(), bitmap.height(), true);
  ASSERT_TRUE(surface != nullptr);
  canvas = surface->getCanvas();
  canvas->drawTexture(texture.get());

  auto A8Info = ImageInfo::Make(width, height, ColorType::ALPHA_8, AlphaType::Premultiplied);
  result = surface->readPixels(A8Info, pixels);
  ASSERT_TRUE(result);
  CHECK_PIXELS(A8Info, pixels, "Surface_alpha_to_alpha");

  result = surface->readPixels(RGBAInfo, pixels);
  ASSERT_TRUE(result);
  CHECK_PIXELS(RGBAInfo, pixels, "Surface_alpha_to_rgba");

  auto AlphaRectInfo = ImageInfo::Make(500, 500, ColorType::ALPHA_8, AlphaType::Premultiplied);
  memset(pixels, 0, AlphaRectInfo.byteSize());
  result = surface->readPixels(AlphaRectInfo, pixels, 100, -100);
  ASSERT_TRUE(result);
  CHECK_PIXELS(AlphaRectInfo, pixels, "Surface_alpha_to_alpha_100_-100");

  auto gl = GLContext::Unwrap(context);
  GLTextureInfo textureInfo = {};
  result = CreateTexture(gl, width, height, &textureInfo);
  ASSERT_TRUE(result);
  auto backendTexture = BackendTexture(textureInfo, width, height);
  surface = Surface::MakeFrom(context, backendTexture, ImageOrigin::BottomLeft);
  ASSERT_TRUE(surface != nullptr);
  canvas = surface->getCanvas();
  canvas->clear();
  canvas->drawTexture(texture.get());

  result = surface->readPixels(RGBAInfo, pixels);
  ASSERT_TRUE(result);
  CHECK_PIXELS(RGBAInfo, pixels, "Surface_BL_rgbA_to_rgbA");

  memset(pixels, 0, RGBAInfo.byteSize());
  result = surface->readPixels(RGBAInfo, pixels, 100, 100);
  ASSERT_TRUE(result);
  CHECK_PIXELS(RGBAInfo, pixels, "Surface_BL_rgbA_to_rgbA_100_100");

  memset(pixels, 0, RGBARectInfo.byteSize());
  result = surface->readPixels(RGBARectInfo, pixels, -100, -100);
  ASSERT_TRUE(result);
  CHECK_PIXELS(RGBARectInfo, pixels, "Surface_BL_rgbA_to_rgbA_-100_-100");

  memset(pixels, 0, RGBARectInfo.byteSize());
  result = surface->readPixels(RGBARectInfo, pixels, 100, -100);
  ASSERT_TRUE(result);
  CHECK_PIXELS(RGBARectInfo, pixels, "Surface_BL_rgbA_to_rgbA_100_-100");

  gl->deleteTextures(1, &textureInfo.id);
  device->unlock();
}

/**
 * 用例描述: PNG 解码器测试
 */
PAG_TEST(PAGReadPixelsTest, PngCodec) {
  auto image = Image::MakeFrom("../resources/apitest/test_timestretch.png");
  ASSERT_TRUE(image != nullptr);
  ASSERT_EQ(image->width(), 1280);
  ASSERT_EQ(image->height(), 720);
  ASSERT_EQ(static_cast<int>(image->orientation()), static_cast<int>(Orientation::TopLeft));
  auto rowBytes = image->width() * 4;
  auto pixels = new (std::nothrow) uint8_t[rowBytes * image->height()];
  ASSERT_TRUE(pixels);
  auto info = ImageInfo::Make(1280, 720, ColorType::RGBA_8888, AlphaType::Premultiplied);
  ASSERT_TRUE(image->readPixels(info, pixels));
  PixelMap pixelMap(info, pixels);
  auto bytes = Image::Encode(pixelMap.info(), pixelMap.pixels(), EncodedFormat::PNG, 100);
  image = Image::MakeFrom(bytes);
  ASSERT_TRUE(image != nullptr);
  ASSERT_EQ(image->width(), 1280);
  ASSERT_EQ(image->height(), 720);
  ASSERT_EQ(static_cast<int>(image->orientation()), static_cast<int>(Orientation::TopLeft));
  delete[] pixels;
}

/**
 * 用例描述: Webp 解码器测试
 */
PAG_TEST(PAGReadPixelsTest, WebpCodec) {
  auto image = Image::MakeFrom("../resources/apitest/imageReplacement.webp");
  ASSERT_TRUE(image != nullptr);
  ASSERT_EQ(image->width(), 110);
  ASSERT_EQ(image->height(), 110);
  ASSERT_EQ(static_cast<int>(image->orientation()), static_cast<int>(Orientation::TopLeft));
  auto rowBytes = image->width() * 4;
  auto pixels = new (std::nothrow) uint8_t[rowBytes * image->height()];
  ASSERT_TRUE(pixels);
  auto info = ImageInfo::Make(110, 110, ColorType::RGBA_8888, AlphaType::Premultiplied);
  bool res = image->readPixels(info, pixels);
  ASSERT_TRUE(res);
  PixelMap pixelMap(info, pixels);
  auto bytes = Image::Encode(pixelMap.info(), pixelMap.pixels(), EncodedFormat::WEBP, 100);
  image = Image::MakeFrom(bytes);
  ASSERT_TRUE(image != nullptr);
  ASSERT_EQ(image->width(), 110);
  ASSERT_EQ(image->height(), 110);
  ASSERT_EQ(static_cast<int>(image->orientation()), static_cast<int>(Orientation::TopLeft));
  delete[] pixels;
}

/**
 * 用例描述: JPEG 解码器测试
 */
PAG_TEST(PAGReadPixelsTest, JpegCodec) {
  auto image = Image::MakeFrom("../resources/apitest/rotation.jpg");
  ASSERT_TRUE(image != nullptr);
  ASSERT_EQ(image->width(), 4032);
  ASSERT_EQ(image->height(), 3024);
  ASSERT_EQ(static_cast<int>(image->orientation()), static_cast<int>(Orientation::RightTop));
  ColorType outputColorType = ColorType::RGBA_8888;
  auto pixels = new (std::nothrow)
      uint8_t[image->height() * image->width() * ImageInfo::GetBytesPerPixel(outputColorType)];
  ASSERT_TRUE(pixels);
  auto info = ImageInfo::Make(4032, 3024, outputColorType, AlphaType::Premultiplied);
  bool res = image->readPixels(info, pixels);
  PixelMap pixelMap(info, pixels);

  auto bytes = Image::Encode(pixelMap.info(), pixelMap.pixels(), EncodedFormat::JPEG, 20);
  image = Image::MakeFrom(bytes);
  ASSERT_TRUE(image != nullptr);
  ASSERT_EQ(image->width(), 4032);
  ASSERT_EQ(image->height(), 3024);
  ASSERT_EQ(static_cast<int>(image->orientation()), static_cast<int>(Orientation::TopLeft));
  delete[] pixels;
  ASSERT_TRUE(res);
}

}  // namespace pag
