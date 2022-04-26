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

#include <vector>
#include "framework/pag_test.h"
#include "framework/utils/PAGTestUtils.h"
#include "gpu/opengl/GLUtil.h"
#include "tgfx/core/Bitmap.h"
#include "tgfx/core/Buffer.h"
#include "tgfx/core/Image.h"
#include "tgfx/gpu/Surface.h"
#include "tgfx/gpu/opengl/GLDevice.h"
#include "tgfx/gpu/opengl/GLTexture.h"

namespace pag {
using namespace tgfx;
using nlohmann::json;

#define CHECK_PIXELS(info, pixels, key)                                          \
  {                                                                              \
    Bitmap bm(info, pixels);                                                     \
    EXPECT_TRUE(Baseline::Compare(bm, "PAGReadPixelsTest/" + std::string(key))); \
  }

/**
 * 用例描述: 像素格式转换相关功能测试-PixelMap
 */
PAG_TEST(PAGReadPixelsTest, TestPixelMap) {
  auto image = Image::MakeFrom("../resources/apitest/test_timestretch.png");
  EXPECT_TRUE(image != nullptr);
  auto width = image->width();
  auto height = image->height();
  auto RGBAInfo =
      ImageInfo::Make(width, height, tgfx::ColorType::RGBA_8888, tgfx::AlphaType::Unpremultiplied);
  auto byteSize = RGBAInfo.byteSize();
  Buffer pixelsA(byteSize);
  Buffer pixelsB(byteSize);
  auto result = image->readPixels(RGBAInfo, pixelsA.data());
  EXPECT_TRUE(result);

  Bitmap RGBAMap(RGBAInfo, pixelsA.data());
  CHECK_PIXELS(RGBAInfo, pixelsA.data(), "PixelMap_RGBA_Original");

  result = RGBAMap.readPixels(RGBAInfo, pixelsB.data());
  EXPECT_TRUE(result);
  CHECK_PIXELS(RGBAInfo, pixelsB.data(), "PixelMap_RGBA_to_RGBA");

  pixelsB.clear();
  result = RGBAMap.readPixels(RGBAInfo, pixelsB.data(), 100, 100);
  ASSERT_TRUE(result);
  CHECK_PIXELS(RGBAInfo, pixelsB.data(), "PixelMap_RGBA_to_RGBA_100_100");

  auto RGBARectInfo =
      ImageInfo::Make(500, 500, tgfx::ColorType::RGBA_8888, tgfx::AlphaType::Premultiplied);
  pixelsB.clear();
  result = RGBAMap.readPixels(RGBARectInfo, pixelsB.data(), -100, -100);
  ASSERT_TRUE(result);
  CHECK_PIXELS(RGBARectInfo, pixelsB.data(), "PixelMap_RGBA_to_RGBA_-100_-100");

  pixelsB.clear();
  result = RGBAMap.readPixels(RGBARectInfo, pixelsB.data(), 100, -100);
  ASSERT_TRUE(result);
  CHECK_PIXELS(RGBARectInfo, pixelsB.data(), "PixelMap_RGBA_to_RGBA_100_-100");

  auto rgb_AInfo = RGBAInfo.makeAlphaType(tgfx::AlphaType::Premultiplied);
  result = RGBAMap.readPixels(rgb_AInfo, pixelsB.data());
  EXPECT_TRUE(result);
  CHECK_PIXELS(rgb_AInfo, pixelsB.data(), "PixelMap_RGBA_to_rgb_A");

  auto bgr_AInfo = rgb_AInfo.makeColorType(tgfx::ColorType::BGRA_8888);
  result = RGBAMap.readPixels(bgr_AInfo, pixelsB.data());
  EXPECT_TRUE(result);
  CHECK_PIXELS(bgr_AInfo, pixelsB.data(), "PixelMap_RGBA_to_bgr_A");

  auto BGRAInfo = bgr_AInfo.makeAlphaType(tgfx::AlphaType::Unpremultiplied);
  result = RGBAMap.readPixels(BGRAInfo, pixelsB.data());
  EXPECT_TRUE(result);
  CHECK_PIXELS(BGRAInfo, pixelsB.data(), "PixelMap_RGBA_to_BGRA");

  Bitmap BGRAMap(BGRAInfo, pixelsB.data());

  result = BGRAMap.readPixels(BGRAInfo, pixelsA.data());
  EXPECT_TRUE(result);
  CHECK_PIXELS(BGRAInfo, pixelsA.data(), "PixelMap_BGRA_to_BGRA");

  result = BGRAMap.readPixels(RGBAInfo, pixelsA.data());
  EXPECT_TRUE(result);
  CHECK_PIXELS(RGBAInfo, pixelsA.data(), "PixelMap_BGRA_to_RGBA");

  result = BGRAMap.readPixels(rgb_AInfo, pixelsA.data());
  EXPECT_TRUE(result);
  CHECK_PIXELS(rgb_AInfo, pixelsA.data(), "PixelMap_BGRA_to_rgb_A");

  Bitmap rgb_AMap(rgb_AInfo, pixelsA.data());

  result = rgb_AMap.readPixels(RGBAInfo, pixelsB.data());
  EXPECT_TRUE(result);
  CHECK_PIXELS(RGBAInfo, pixelsB.data(), "PixelMap_rgb_A_to_RGBA");

  result = rgb_AMap.readPixels(BGRAInfo, pixelsB.data());
  EXPECT_TRUE(result);
  CHECK_PIXELS(BGRAInfo, pixelsB.data(), "PixelMap_rgb_A_to_BGRA");

  result = rgb_AMap.readPixels(bgr_AInfo, pixelsB.data());
  EXPECT_TRUE(result);
  CHECK_PIXELS(bgr_AInfo, pixelsB.data(), "PixelMap_rgb_A_to_bgr_A");

  auto A8Info =
      ImageInfo::Make(width, height, tgfx::ColorType::ALPHA_8, tgfx::AlphaType::Unpremultiplied);
  EXPECT_EQ(A8Info.alphaType(), tgfx::AlphaType::Premultiplied);
  auto alphaByteSize = A8Info.byteSize();
  Buffer pixelsC(alphaByteSize);

  result = rgb_AMap.readPixels(A8Info, pixelsC.data());
  EXPECT_TRUE(result);
  CHECK_PIXELS(A8Info, pixelsC.data(), "PixelMap_rgb_A_to_alpha");

  Bitmap A8Map(A8Info, pixelsC.data());

  result = A8Map.readPixels(rgb_AInfo, pixelsB.data());
  EXPECT_TRUE(result);
  CHECK_PIXELS(rgb_AInfo, pixelsB.data(), "PixelMap_alpha_to_rgb_A");

  result = A8Map.readPixels(BGRAInfo, pixelsB.data());
  EXPECT_TRUE(result);
  CHECK_PIXELS(BGRAInfo, pixelsB.data(), "PixelMap_alpha_to_BGRA");
}

/**
 * 用例描述: 像素格式转换相关功能测试-SurfaceReadPixels
 */
PAG_TEST(PAGReadPixelsTest, TestSurfaceReadPixels) {
  auto image = Image::MakeFrom("../resources/apitest/test_timestretch.png");
  ASSERT_TRUE(image != nullptr);
  auto pixelBuffer = PixelBuffer::Make(image->width(), image->height(), false, false);
  ASSERT_TRUE(pixelBuffer != nullptr);
  auto pixels = pixelBuffer->lockPixels();
  auto result = image->readPixels(pixelBuffer->info(), pixels);
  pixelBuffer->unlockPixels();
  ASSERT_TRUE(result);

  auto device = GLDevice::Make();
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);
  auto texture = pixelBuffer->makeTexture(context);
  ASSERT_TRUE(texture != nullptr);
  auto surface = Surface::Make(context, pixelBuffer->width(), pixelBuffer->height());
  ASSERT_TRUE(surface != nullptr);
  auto canvas = surface->getCanvas();
  canvas->drawTexture(texture.get());

  Bitmap bitmap(pixelBuffer);
  auto width = bitmap.width();
  auto height = bitmap.height();
  pixels = bitmap.writablePixels();

  auto RGBAInfo =
      ImageInfo::Make(width, height, tgfx::ColorType::RGBA_8888, tgfx::AlphaType::Premultiplied);
  result = surface->readPixels(RGBAInfo, pixels);
  ASSERT_TRUE(result);
  CHECK_PIXELS(RGBAInfo, pixels, "Surface_rgb_A_to_rgb_A");

  auto BGRAInfo =
      ImageInfo::Make(width, height, tgfx::ColorType::BGRA_8888, tgfx::AlphaType::Premultiplied);
  result = surface->readPixels(BGRAInfo, pixels);
  ASSERT_TRUE(result);
  CHECK_PIXELS(BGRAInfo, pixels, "Surface_rgb_A_to_bgr_A");

  memset(pixels, 0, RGBAInfo.byteSize());
  result = surface->readPixels(RGBAInfo, pixels, 100, 100);
  ASSERT_TRUE(result);
  CHECK_PIXELS(RGBAInfo, pixels, "Surface_rgb_A_to_rgb_A_100_100");

  auto RGBARectInfo =
      ImageInfo::Make(500, 500, tgfx::ColorType::RGBA_8888, tgfx::AlphaType::Premultiplied);
  memset(pixels, 0, RGBARectInfo.byteSize());
  result = surface->readPixels(RGBARectInfo, pixels, -100, -100);
  ASSERT_TRUE(result);
  CHECK_PIXELS(RGBARectInfo, pixels, "Surface_rgb_A_to_rgb_A_-100_-100");

  memset(pixels, 0, RGBARectInfo.byteSize());
  result = surface->readPixels(RGBARectInfo, pixels, 100, -100);
  ASSERT_TRUE(result);
  CHECK_PIXELS(RGBARectInfo, pixels, "Surface_rgb_A_to_rgb_A_100_-100");

  surface = Surface::Make(context, bitmap.width(), bitmap.height(), true);
  ASSERT_TRUE(surface != nullptr);
  canvas = surface->getCanvas();
  canvas->drawTexture(texture.get());

  auto A8Info =
      ImageInfo::Make(width, height, tgfx::ColorType::ALPHA_8, tgfx::AlphaType::Premultiplied);
  result = surface->readPixels(A8Info, pixels);
  ASSERT_TRUE(result);
  CHECK_PIXELS(A8Info, pixels, "Surface_alpha_to_alpha");

  result = surface->readPixels(RGBAInfo, pixels);
  ASSERT_TRUE(result);
  CHECK_PIXELS(RGBAInfo, pixels, "Surface_alpha_to_rgba");

  auto AlphaRectInfo =
      ImageInfo::Make(500, 500, tgfx::ColorType::ALPHA_8, tgfx::AlphaType::Premultiplied);
  memset(pixels, 0, AlphaRectInfo.byteSize());
  result = surface->readPixels(AlphaRectInfo, pixels, 100, -100);
  ASSERT_TRUE(result);
  CHECK_PIXELS(AlphaRectInfo, pixels, "Surface_alpha_to_alpha_100_-100");

  tgfx::GLSampler textureInfo = {};
  result = CreateGLTexture(context, width, height, &textureInfo);
  ASSERT_TRUE(result);
  auto glTexture =
      GLTexture::MakeFrom(context, textureInfo, width, height, tgfx::ImageOrigin::BottomLeft);
  surface = Surface::MakeFrom(glTexture);
  ASSERT_TRUE(surface != nullptr);
  canvas = surface->getCanvas();
  canvas->clear();
  canvas->drawTexture(texture.get());

  result = surface->readPixels(RGBAInfo, pixels);
  ASSERT_TRUE(result);
  CHECK_PIXELS(RGBAInfo, pixels, "Surface_BL_rgb_A_to_rgb_A");

  memset(pixels, 0, RGBAInfo.byteSize());
  result = surface->readPixels(RGBAInfo, pixels, 100, 100);
  ASSERT_TRUE(result);
  CHECK_PIXELS(RGBAInfo, pixels, "Surface_BL_rgb_A_to_rgb_A_100_100");

  memset(pixels, 0, RGBARectInfo.byteSize());
  result = surface->readPixels(RGBARectInfo, pixels, -100, -100);
  ASSERT_TRUE(result);
  CHECK_PIXELS(RGBARectInfo, pixels, "Surface_BL_rgb_A_to_rgb_A_-100_-100");

  memset(pixels, 0, RGBARectInfo.byteSize());
  result = surface->readPixels(RGBARectInfo, pixels, 100, -100);
  ASSERT_TRUE(result);
  CHECK_PIXELS(RGBARectInfo, pixels, "Surface_BL_rgb_A_to_rgb_A_100_-100");
  auto gl = GLFunctions::Get(context);
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
  auto info = ImageInfo::Make(image->width(), image->height(), tgfx::ColorType::RGBA_8888,
                              tgfx::AlphaType::Premultiplied);
  ASSERT_TRUE(image->readPixels(info, pixels));
  CHECK_PIXELS(info, pixels, "PngCodec_Decode");
  Bitmap bitmap(info, pixels);
  auto bytes = bitmap.encode(EncodedFormat::PNG, 100);
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
  auto info = ImageInfo::Make(image->width(), image->height(), tgfx::ColorType::RGBA_8888,
                              tgfx::AlphaType::Premultiplied);
  auto pixels = new (std::nothrow) uint8_t[info.byteSize()];
  ASSERT_TRUE(pixels);
  ASSERT_TRUE(image->readPixels(info, pixels));
  CHECK_PIXELS(info, pixels, "WebpCodec_Decode");
  Bitmap bitmap(info, pixels);
  auto bytes = bitmap.encode(EncodedFormat::WEBP, 100);
  image = Image::MakeFrom(bytes);
  ASSERT_TRUE(image != nullptr);
  ASSERT_EQ(image->width(), 110);
  ASSERT_EQ(image->height(), 110);
  ASSERT_EQ(static_cast<int>(image->orientation()), static_cast<int>(Orientation::TopLeft));

  auto a8Info = ImageInfo::Make(image->width(), image->height(), tgfx::ColorType::ALPHA_8,
                                tgfx::AlphaType::Premultiplied);
  auto a8Pixels = new (std::nothrow) uint8_t[a8Info.byteSize()];
  ASSERT_TRUE(image->readPixels(a8Info, a8Pixels));
  auto rgbaFromA8Data = Bitmap(a8Info, a8Pixels).encode(EncodedFormat::WEBP, 100);
  auto rgbaFromA8Image = Image::MakeFrom(rgbaFromA8Data);
  rgbaFromA8Image->readPixels(info, pixels);
  CHECK_PIXELS(info, pixels, "WebpCodec_EncodeA8");
  delete[] pixels;
  delete[] a8Pixels;
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
  auto outputColorType = tgfx::ColorType::RGBA_8888;
  auto pixels = new (std::nothrow)
      uint8_t[image->height() * image->width() * ImageInfo::GetBytesPerPixel(outputColorType)];
  ASSERT_TRUE(pixels);
  auto info = ImageInfo::Make(image->width(), image->height(), outputColorType,
                              tgfx::AlphaType::Premultiplied);
  bool res = image->readPixels(info, pixels);
  CHECK_PIXELS(info, pixels, "JpegCodec_Decode");
  Bitmap bitmap(info, pixels);

  auto bytes = bitmap.encode(EncodedFormat::JPEG, 20);
  image = Image::MakeFrom(bytes);
  ASSERT_TRUE(image != nullptr);
  ASSERT_EQ(image->width(), 4032);
  ASSERT_EQ(image->height(), 3024);
  ASSERT_EQ(static_cast<int>(image->orientation()), static_cast<int>(Orientation::TopLeft));
  delete[] pixels;
  ASSERT_TRUE(res);
}

}  // namespace pag
