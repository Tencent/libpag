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
#include "tgfx/core/Buffer.h"
#include "tgfx/core/ImageCodec.h"
#include "tgfx/core/Pixmap.h"
#include "tgfx/gpu/Surface.h"
#include "tgfx/gpu/opengl/GLDevice.h"
#include "tgfx/gpu/opengl/GLTexture.h"

namespace pag {
using namespace tgfx;
using nlohmann::json;

#define CHECK_PIXELS(info, pixels, key)                                          \
  {                                                                              \
    Pixmap bm(info, pixels);                                                     \
    EXPECT_TRUE(Baseline::Compare(bm, "PAGReadPixelsTest/" + std::string(key))); \
  }

/**
 * 用例描述: 像素格式转换相关功能测试-PixelMap
 */
PAG_TEST(PAGReadPixelsTest, TestPixelMap) {
  auto codec = MakeImageCodec("resources/apitest/test_timestretch.png");
  EXPECT_TRUE(codec != nullptr);
  auto width = codec->width();
  auto height = codec->height();
  auto RGBAInfo =
      ImageInfo::Make(width, height, tgfx::ColorType::RGBA_8888, tgfx::AlphaType::Unpremultiplied);
  auto byteSize = RGBAInfo.byteSize();
  Buffer pixelsA(byteSize);
  Buffer pixelsB(byteSize);
  auto result = codec->readPixels(RGBAInfo, pixelsA.data());
  EXPECT_TRUE(result);

  Pixmap RGBAMap(RGBAInfo, pixelsA.data());
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

  Pixmap BGRAMap(BGRAInfo, pixelsB.data());

  result = BGRAMap.readPixels(BGRAInfo, pixelsA.data());
  EXPECT_TRUE(result);
  CHECK_PIXELS(BGRAInfo, pixelsA.data(), "PixelMap_BGRA_to_BGRA");

  result = BGRAMap.readPixels(RGBAInfo, pixelsA.data());
  EXPECT_TRUE(result);
  CHECK_PIXELS(RGBAInfo, pixelsA.data(), "PixelMap_BGRA_to_RGBA");

  result = BGRAMap.readPixels(rgb_AInfo, pixelsA.data());
  EXPECT_TRUE(result);
  CHECK_PIXELS(rgb_AInfo, pixelsA.data(), "PixelMap_BGRA_to_rgb_A");

  Pixmap rgb_AMap(rgb_AInfo, pixelsA.data());

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

  Pixmap A8Map(A8Info, pixelsC.data());

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
  auto codec = MakeImageCodec("resources/apitest/test_timestretch.png");
  ASSERT_TRUE(codec != nullptr);
  Bitmap bitmap(codec->width(), codec->height(), false, false);
  ASSERT_FALSE(bitmap.isEmpty());
  auto pixels = bitmap.lockPixels();
  auto result = codec->readPixels(bitmap.info(), pixels);
  bitmap.unlockPixels();
  ASSERT_TRUE(result);

  auto device = GLDevice::Make();
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);
  auto pixelBuffer = bitmap.makeBuffer();
  auto texture = pixelBuffer->makeTexture(context);
  ASSERT_TRUE(texture != nullptr);
  auto surface = Surface::Make(context, pixelBuffer->width(), pixelBuffer->height());
  ASSERT_TRUE(surface != nullptr);
  auto canvas = surface->getCanvas();
  canvas->drawTexture(texture);

  auto width = bitmap.width();
  auto height = bitmap.height();
  pixels = bitmap.lockPixels();

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

  surface = Surface::Make(context, width, height, true);
  ASSERT_TRUE(surface != nullptr);
  canvas = surface->getCanvas();
  canvas->drawTexture(texture);

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
      GLTexture::MakeFrom(context, textureInfo, width, height, tgfx::SurfaceOrigin::BottomLeft);
  surface = Surface::MakeFrom(glTexture);
  ASSERT_TRUE(surface != nullptr);
  canvas = surface->getCanvas();
  canvas->clear();
  canvas->drawTexture(texture);

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
  bitmap.unlockPixels();
}

/**
 * 用例描述: PNG 解码器测试
 */
PAG_TEST(PAGReadPixelsTest, PngCodec) {
  auto codec = MakeImageCodec("resources/apitest/test_timestretch.png");
  ASSERT_TRUE(codec != nullptr);
  ASSERT_EQ(codec->width(), 1280);
  ASSERT_EQ(codec->height(), 720);
  ASSERT_EQ(codec->origin(), tgfx::ImageOrigin::TopLeft);
  auto rowBytes = codec->width() * 4;
  auto pixels = new (std::nothrow) uint8_t[rowBytes * codec->height()];
  ASSERT_TRUE(pixels);
  auto info = ImageInfo::Make(codec->width(), codec->height(), tgfx::ColorType::RGBA_8888,
                              tgfx::AlphaType::Premultiplied);
  ASSERT_TRUE(codec->readPixels(info, pixels));
  CHECK_PIXELS(info, pixels, "PngCodec_Decode");
  Pixmap pixmap(info, pixels);
  auto bytes = ImageCodec::Encode(pixmap, EncodedFormat::PNG, 100);
  codec = ImageCodec::MakeFrom(bytes);
  ASSERT_TRUE(codec != nullptr);
  ASSERT_EQ(codec->width(), 1280);
  ASSERT_EQ(codec->height(), 720);
  ASSERT_EQ(codec->origin(), tgfx::ImageOrigin::TopLeft);
  delete[] pixels;
}

/**
 * 用例描述: Webp 解码器测试
 */
PAG_TEST(PAGReadPixelsTest, WebpCodec) {
  auto codec = MakeImageCodec("resources/apitest/imageReplacement.webp");
  ASSERT_TRUE(codec != nullptr);
  ASSERT_EQ(codec->width(), 110);
  ASSERT_EQ(codec->height(), 110);
  ASSERT_EQ(codec->origin(), tgfx::ImageOrigin::TopLeft);
  auto info = ImageInfo::Make(codec->width(), codec->height(), tgfx::ColorType::RGBA_8888,
                              tgfx::AlphaType::Premultiplied);
  auto pixels = new (std::nothrow) uint8_t[info.byteSize()];
  ASSERT_TRUE(pixels);
  ASSERT_TRUE(codec->readPixels(info, pixels));
  CHECK_PIXELS(info, pixels, "WebpCodec_Decode");
  Pixmap pixmap(info, pixels);
  auto bytes = ImageCodec::Encode(pixmap, EncodedFormat::WEBP, 100);
  codec = ImageCodec::MakeFrom(bytes);
  ASSERT_TRUE(codec != nullptr);
  ASSERT_EQ(codec->width(), 110);
  ASSERT_EQ(codec->height(), 110);
  ASSERT_EQ(codec->origin(), tgfx::ImageOrigin::TopLeft);

  auto a8Info = ImageInfo::Make(codec->width(), codec->height(), tgfx::ColorType::ALPHA_8,
                                tgfx::AlphaType::Premultiplied);
  auto a8Pixels = new (std::nothrow) uint8_t[a8Info.byteSize()];
  ASSERT_TRUE(codec->readPixels(a8Info, a8Pixels));
  auto rgbaFromA8Data = ImageCodec::Encode(Pixmap(a8Info, a8Pixels), EncodedFormat::WEBP, 100);
  auto rgbaFromA8Image = ImageCodec::MakeFrom(rgbaFromA8Data);
  rgbaFromA8Image->readPixels(info, pixels);
  CHECK_PIXELS(info, pixels, "WebpCodec_EncodeA8");
  delete[] pixels;
  delete[] a8Pixels;
}

/**
 * 用例描述: JPEG 解码器测试
 */
PAG_TEST(PAGReadPixelsTest, JpegCodec) {
  auto codec = MakeImageCodec("resources/apitest/rotation.jpg");
  ASSERT_TRUE(codec != nullptr);
  ASSERT_EQ(codec->width(), 4032);
  ASSERT_EQ(codec->height(), 3024);
  ASSERT_EQ(codec->origin(), tgfx::ImageOrigin::RightTop);
  auto outputColorType = tgfx::ColorType::RGBA_8888;
  auto pixels = new (std::nothrow)
      uint8_t[codec->height() * codec->width() * ImageInfo::GetBytesPerPixel(outputColorType)];
  ASSERT_TRUE(pixels);
  auto info = ImageInfo::Make(codec->width(), codec->height(), outputColorType,
                              tgfx::AlphaType::Premultiplied);
  bool res = codec->readPixels(info, pixels);
  CHECK_PIXELS(info, pixels, "JpegCodec_Decode");
  Pixmap pixmap(info, pixels);

  auto bytes = ImageCodec::Encode(pixmap, EncodedFormat::JPEG, 20);
  codec = ImageCodec::MakeFrom(bytes);
  ASSERT_TRUE(codec != nullptr);
  ASSERT_EQ(codec->width(), 4032);
  ASSERT_EQ(codec->height(), 3024);
  ASSERT_EQ(codec->origin(), tgfx::ImageOrigin::TopLeft);
  delete[] pixels;
  ASSERT_TRUE(res);
}

}  // namespace pag
