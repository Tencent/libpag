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
#include "tgfx/core/PathEffect.h"
#include "tgfx/gpu/Surface.h"
#include "tgfx/gpu/opengl/GLDevice.h"
#include "tgfx/gpu/opengl/GLFunctions.h"
#include "tgfx/gpu/opengl/GLTexture.h"

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

PAG_TEST(CanvasTest, Blur) {
  auto device = GLDevice::Make();
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);
  auto image = Image::MakeFrom("../resources/apitest/rotation.jpg");
  ASSERT_TRUE(image != nullptr);
  auto texture = image->makeBuffer()->makeTexture(context);
  ASSERT_TRUE(texture != nullptr);
  auto imageMatrix = OrientationToMatrix(image->orientation(), image->width(), image->height());
  imageMatrix.postScale(0.2, 0.2);
  auto width = image->width();
  auto height = image->height();
  ApplyOrientation(image->orientation(), &width, &height);
  auto imageWidth = static_cast<float>(width) * 0.2f;
  auto imageHeight = static_cast<float>(height) * 0.2f;
  auto padding = 30.f;
  Paint paint;
  auto surface = Surface::Make(context, static_cast<int>(imageWidth * 2.f + padding * 3.f),
                               static_cast<int>(imageHeight * 2.f + padding * 3.f));
  auto canvas = surface->getCanvas();
  canvas->concat(tgfx::Matrix::MakeTrans(padding, padding));
  canvas->save();
  canvas->concat(imageMatrix);
  canvas->drawTexture(texture.get(), &paint);
  canvas->restore();
  Path path;
  path.addRect(Rect::MakeWH(imageWidth, imageHeight));
  PathEffect::MakeStroke(Stroke(1.f))->applyTo(&path);
  paint.setImageFilter(nullptr);
  paint.setColor(Color{1.f, 0.f, 0.f, 1.f});
  canvas->drawPath(path, paint);

  canvas->concat(tgfx::Matrix::MakeTrans(imageWidth + padding, 0));
  canvas->save();
  canvas->concat(imageMatrix);
  paint.setImageFilter(ImageFilter::Blur(130, 130, TileMode::Decal));
  canvas->drawTexture(texture.get(), &paint);
  canvas->restore();
  paint.setImageFilter(nullptr);
  canvas->drawPath(path, paint);

  canvas->concat(tgfx::Matrix::MakeTrans(-imageWidth - padding, imageHeight + padding));
  canvas->save();
  canvas->concat(imageMatrix);
  paint.setImageFilter(ImageFilter::Blur(
      130, 130, TileMode::Clamp,
      tgfx::Rect::MakeWH(static_cast<float>(image->width()), static_cast<float>(image->height()))));
  canvas->drawTexture(texture.get(), &paint);
  canvas->restore();
  paint.setImageFilter(nullptr);
  canvas->drawPath(path, paint);

  canvas->concat(tgfx::Matrix::MakeTrans(imageWidth + padding, 0));
  canvas->save();
  canvas->concat(imageMatrix);
  paint.setImageFilter(
      ImageFilter::Blur(130, 130, TileMode::Clamp, tgfx::Rect::MakeLTRB(-100, -100, 2000, 1000)));
  canvas->drawTexture(texture.get(), &paint);
  paint.setImageFilter(
      ImageFilter::Blur(130, 130, TileMode::Clamp, tgfx::Rect::MakeXYWH(1000, 1000, 1000, 1000)));
  canvas->drawTexture(texture.get(), &paint);
  paint.setImageFilter(
      ImageFilter::Blur(130, 130, TileMode::Clamp, tgfx::Rect::MakeXYWH(2000, 1000, 1000, 1000)));
  canvas->drawTexture(texture.get(), &paint);
  canvas->restore();
  paint.setImageFilter(nullptr);
  canvas->drawPath(path, paint);

  EXPECT_TRUE(Compare(surface.get(), "CanvasTest/blur"));
  device->unlock();
}

PAG_TEST(CanvasTest, DropShadow) {
  auto device = GLDevice::Make();
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);
  auto image = Image::MakeFrom("../resources/apitest/image_as_mask.png");
  ASSERT_TRUE(image != nullptr);
  auto texture = image->makeBuffer()->makeTexture(context);
  ASSERT_TRUE(texture != nullptr);
  auto imageWidth = static_cast<float>(image->width());
  auto imageHeight = static_cast<float>(image->height());
  auto padding = 30.f;
  Paint paint;
  auto surface = Surface::Make(context, static_cast<int>(imageWidth * 2.f + padding * 3.f),
                               static_cast<int>(imageHeight * 2.f + padding * 3.f));
  auto canvas = surface->getCanvas();
  canvas->concat(tgfx::Matrix::MakeTrans(padding, padding));
  paint.setImageFilter(ImageFilter::Blur(15, 15));
  canvas->drawTexture(texture.get(), &paint);

  canvas->concat(tgfx::Matrix::MakeTrans(imageWidth + padding, 0));
  paint.setImageFilter(ImageFilter::DropShadowOnly(0, 0, 15, 15, tgfx::Color::White()));
  canvas->drawTexture(texture.get(), &paint);

  canvas->concat(tgfx::Matrix::MakeTrans(-imageWidth - padding, imageWidth + padding));
  paint.setImageFilter(ImageFilter::DropShadow(0, 0, 15, 15, tgfx::Color::White()));
  canvas->drawTexture(texture.get(), &paint);

  EXPECT_TRUE(Compare(surface.get(), "CanvasTest/dropShadow"));
  device->unlock();
}

PAG_TEST(CanvasTest, clip) {
  auto device = GLDevice::Make();
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);
  auto width = 1080;
  auto height = 1776;
  tgfx::GLSampler textureInfo;
  pag::CreateGLTexture(context, width, height, &textureInfo);
  auto glTexture =
      GLTexture::MakeFrom(context, textureInfo, width, height, ImageOrigin::BottomLeft);
  auto surface = Surface::MakeFrom(glTexture);
  auto canvas = surface->getCanvas();
  canvas->clear();
  canvas->setMatrix(tgfx::Matrix::MakeScale(3));
  auto clipPath = Path();
  clipPath.addRect(tgfx::Rect::MakeLTRB(0, 0, 200, 300));
  auto paint = Paint();
  paint.setColor(tgfx::Color::FromRGBA(0, 0, 0));
  paint.setStyle(PaintStyle::Stroke);
  paint.setStroke(Stroke(1));
  canvas->drawPath(clipPath, paint);
  canvas->clipPath(clipPath);
  auto drawPath = Path();
  drawPath.addRect(tgfx::Rect::MakeLTRB(50, 295, 150, 590));
  paint.setColor(tgfx::Color::FromRGBA(255, 0, 0));
  paint.setStyle(PaintStyle::Fill);
  canvas->drawPath(drawPath, paint);
  EXPECT_TRUE(Compare(surface.get(), "CanvasTest/Clip"));
  auto gl = GLFunctions::Get(context);
  gl->deleteTextures(1, &textureInfo.id);
  device->unlock();
}

/**
 * 用例描述: 测试绘制 Rectangle 纹理时使用 TileMode::Repeat 和 TileMode::Mirror。
 */
PAG_TEST(CanvasTest, TileMode) {
  auto device = GLDevice::Make();
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);
  auto image = Image::MakeFrom("../resources/apitest/rotation.jpg");
  ASSERT_TRUE(image != nullptr);
  auto texture = image->makeBuffer()->makeTexture(context);
  ASSERT_TRUE(texture != nullptr);
  auto surface = Surface::Make(context, image->width() / 2, image->height() / 2);
  auto canvas = surface->getCanvas();
  Paint paint;
  paint.setShader(Shader::MakeTextureShader(texture, TileMode::Repeat, TileMode::Mirror)
                      ->makeWithPreLocalMatrix(Matrix::MakeScale(0.125f)));
  canvas->drawRect(Rect::MakeWH(static_cast<float>(surface->width()),
                                static_cast<float>(surface->height()) * 0.9f),
                   paint);
  EXPECT_TRUE(Compare(surface.get(), "CanvasTest/tileMode"));
  device->unlock();
}
}  // namespace tgfx
