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
#include "gpu/DrawingManager.h"
#include "gpu/ops/FillRectOp.h"
#include "gpu/ops/RRectOp.h"
#include "gpu/ops/TriangulatingPathOp.h"
#include "rendering/utils/shaper/TextShaper.h"
#include "tgfx/core/ImageCodec.h"
#include "tgfx/core/Mask.h"
#include "tgfx/core/PathEffect.h"
#include "tgfx/gpu/Canvas.h"
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
  auto image = ImageCodec::MakeFrom("../resources/apitest/test_timestretch.png")
                   ->makeBuffer()
                   ->makeTexture(context);
  ASSERT_TRUE(image != nullptr);
  auto surface = Surface::Make(context, image->width(), image->height());
  auto canvas = surface->getCanvas();
  tgfx::Paint paint;
  std::array<float, 20> matrix = {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0};
  paint.setColorFilter(ColorFilter::Matrix(matrix));
  canvas->drawTexture(image, &paint);
  EXPECT_TRUE(Compare(surface.get(), "CanvasTest/identityMatrix"));
  canvas->clear();
  std::array<float, 20> greyColorMatrix = {0.21f, 0.72f, 0.07f, 0.41f, 0,  // red
                                           0.21f, 0.72f, 0.07f, 0.41f, 0,  // green
                                           0.21f, 0.72f, 0.07f, 0.41f, 0,  // blue
                                           0,     0,     0,     1.0f,  0};
  paint.setColorFilter(ColorFilter::Matrix(greyColorMatrix));
  canvas->drawTexture(image, &paint);
  EXPECT_TRUE(Compare(surface.get(), "CanvasTest/greyColorMatrix"));
  device->unlock();
}

PAG_TEST(CanvasTest, Blur) {
  auto device = GLDevice::Make();
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);
  auto image = ImageCodec::MakeFrom("../resources/apitest/rotation.jpg");
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
  canvas->drawTexture(texture, &paint);
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
  canvas->drawTexture(texture, &paint);
  canvas->restore();
  paint.setImageFilter(nullptr);
  canvas->drawPath(path, paint);

  canvas->concat(tgfx::Matrix::MakeTrans(-imageWidth - padding, imageHeight + padding));
  canvas->save();
  canvas->concat(imageMatrix);
  paint.setImageFilter(ImageFilter::Blur(130, 130, TileMode::Clamp,
                                         tgfx::Rect::MakeWH(image->width(), image->height())));
  canvas->drawTexture(texture, &paint);
  canvas->restore();
  paint.setImageFilter(nullptr);
  canvas->drawPath(path, paint);

  canvas->concat(tgfx::Matrix::MakeTrans(imageWidth + padding, 0));
  canvas->save();
  canvas->concat(imageMatrix);
  paint.setImageFilter(
      ImageFilter::Blur(130, 130, TileMode::Clamp, tgfx::Rect::MakeLTRB(-100, -100, 2000, 1000)));
  canvas->drawTexture(texture, &paint);
  paint.setImageFilter(
      ImageFilter::Blur(130, 130, TileMode::Clamp, tgfx::Rect::MakeXYWH(1000, 1000, 1000, 1000)));
  canvas->drawTexture(texture, &paint);
  paint.setImageFilter(
      ImageFilter::Blur(130, 130, TileMode::Clamp, tgfx::Rect::MakeXYWH(2000, 1000, 1000, 1000)));
  canvas->drawTexture(texture, &paint);
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
  auto image = ImageCodec::MakeFrom("../resources/apitest/image_as_mask.png");
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
  canvas->drawTexture(texture, &paint);

  canvas->concat(tgfx::Matrix::MakeTrans(imageWidth + padding, 0));
  paint.setImageFilter(ImageFilter::DropShadowOnly(0, 0, 15, 15, tgfx::Color::White()));
  canvas->drawTexture(texture, &paint);

  canvas->concat(tgfx::Matrix::MakeTrans(-imageWidth - padding, imageWidth + padding));
  paint.setImageFilter(ImageFilter::DropShadow(0, 0, 15, 15, tgfx::Color::White()));
  canvas->drawTexture(texture, &paint);

  canvas->concat(tgfx::Matrix::MakeTrans(imageWidth + padding, 0));
  auto filter = ImageFilter::DropShadow(3, 3, 0, 0, tgfx::Color::White());
  paint.setImageFilter(filter);
  canvas->drawTexture(texture, &paint);

  EXPECT_TRUE(Compare(surface.get(), "CanvasTest/dropShadow"));
  device->unlock();

  auto src = Rect::MakeXYWH(10, 10, 10, 10);
  auto bounds = filter->filterBounds(src);
  EXPECT_EQ(bounds, Rect::MakeXYWH(10, 10, 13, 13));
  bounds = ImageFilter::DropShadowOnly(3, 3, 0, 0, Color::White())->filterBounds(src);
  EXPECT_EQ(bounds, Rect::MakeXYWH(13, 13, 10, 10));
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
  auto codec = ImageCodec::MakeFrom("../resources/apitest/rotation.jpg");
  ASSERT_TRUE(codec != nullptr);
  auto texture = codec->makeBuffer()->makeTexture(context);
  ASSERT_TRUE(texture != nullptr);
  auto surface = Surface::Make(context, codec->width() / 2, codec->height() / 2);
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

/**
 * 用例描述: 测试 rect 合并绘制
 */
PAG_TEST(CanvasTest, merge_draw_call_rect) {
  auto device = GLDevice::Make();
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);
  int width = 72;
  int height = 72;
  auto surface = Surface::Make(context, width, height);
  auto canvas = surface->getCanvas();
  canvas->clear(Color::White());
  Paint paint;
  paint.setColor(Color{0.8f, 0.8f, 0.8f, 1.f});
  paint.setColorFilter(ColorFilter::MakeLumaColorFilter());
  int tileSize = 8;
  size_t drawCallCount = 0;
  for (int y = 0; y < height; y += tileSize) {
    bool draw = (y / tileSize) % 2 == 1;
    for (int x = 0; x < width; x += tileSize) {
      if (draw) {
        auto rect = Rect::MakeXYWH(static_cast<float>(x), static_cast<float>(y),
                                   static_cast<float>(tileSize), static_cast<float>(tileSize));
        canvas->drawRect(rect, paint);
        drawCallCount++;
      }
      draw = !draw;
    }
  }
  auto* drawingManager = context->drawingManager();
  EXPECT_TRUE(drawingManager->tasks.size() == 1);
  auto task = std::static_pointer_cast<OpsTask>(drawingManager->tasks[0]);
  EXPECT_TRUE(task->ops.size() == 2);
  EXPECT_EQ(static_cast<FillRectOp*>(task->ops[1].get())->rects.size(), drawCallCount);
  canvas->flush();
  EXPECT_TRUE(Compare(surface.get(), "CanvasTest/merge_draw_call_rect"));
  device->unlock();
}

/**
 * 用例描述: 测试 path 合并绘制
 */
PAG_TEST(CanvasTest, merge_draw_call_triangle) {
  auto device = GLDevice::Make();
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);
  auto codec = ImageCodec::MakeFrom("../resources/apitest/imageReplacement.png");
  ASSERT_TRUE(codec != nullptr);
  auto texture = codec->makeBuffer()->makeTexture(context);
  ASSERT_TRUE(texture != nullptr);
  int width = 72;
  int height = 72;
  auto surface = Surface::Make(context, width, height);
  auto canvas = surface->getCanvas();
  canvas->clear(Color::White());
  Paint paint;
  paint.setShader(Shader::MakeTextureShader(texture)->makeWithPreLocalMatrix(
      Matrix::MakeScale(static_cast<float>(width) / static_cast<float>(codec->width()),
                        static_cast<float>(height) / static_cast<float>(codec->height()))));
  int tileSize = 8;
  int drawCallCount = 0;
  for (int y = 0; y < height; y += tileSize) {
    bool draw = (y / tileSize) % 2 == 1;
    for (int x = 0; x < width; x += tileSize) {
      if (draw) {
        auto rect = Rect::MakeXYWH(static_cast<float>(x), static_cast<float>(y),
                                   static_cast<float>(tileSize), static_cast<float>(tileSize));
        auto centerX = rect.x() + rect.width() / 2.f;
        auto centerY = rect.y() + rect.height() / 2.f;
        Path path;
        path.addRect(rect);
        Matrix matrix = Matrix::I();
        matrix.postRotate(45, centerX, centerY);
        path.transform(matrix);
        canvas->drawPath(path, paint);
        drawCallCount += 1;
      }
      draw = !draw;
    }
  }
  auto* drawingManager = context->drawingManager();
  EXPECT_TRUE(drawingManager->tasks.size() == 1);
  auto task = std::static_pointer_cast<OpsTask>(drawingManager->tasks[0]);
  EXPECT_TRUE(task->ops.size() == 2);
  EXPECT_EQ(static_cast<TriangulatingPathOp*>(task->ops[1].get())->vertexCount, drawCallCount * 30);
  canvas->flush();
  EXPECT_TRUE(Compare(surface.get(), "CanvasTest/merge_draw_call_triangle"));
  device->unlock();
}

/**
 * 用例描述: 测试 rrect 合并绘制
 */
PAG_TEST(CanvasTest, merge_draw_call_rrect) {
  auto device = GLDevice::Make();
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);
  int width = 72;
  int height = 72;
  auto surface = Surface::Make(context, width, height);
  auto canvas = surface->getCanvas();
  canvas->clear(Color::White());
  Paint paint;
  paint.setShader(Shader::MakeLinearGradient(
      Point{0.f, 0.f}, Point{static_cast<float>(width), static_cast<float>(height)},
      {Color{0.f, 1.f, 0.f, 1.f}, Color{0.f, 0.f, 0.f, 1.f}}, {}));
  int tileSize = 8;
  size_t drawCallCount = 0;
  for (int y = 0; y < height; y += tileSize) {
    bool draw = (y / tileSize) % 2 == 1;
    for (int x = 0; x < width; x += tileSize) {
      if (draw) {
        auto rect = Rect::MakeXYWH(static_cast<float>(x), static_cast<float>(y),
                                   static_cast<float>(tileSize), static_cast<float>(tileSize));
        Path path;
        auto radius = static_cast<float>(tileSize) / 4.f;
        path.addRoundRect(rect, radius, radius);
        canvas->drawPath(path, paint);
        drawCallCount++;
      }
      draw = !draw;
    }
  }
  auto* drawingManager = context->drawingManager();
  EXPECT_TRUE(drawingManager->tasks.size() == 1);
  auto task = std::static_pointer_cast<OpsTask>(drawingManager->tasks[0]);
  EXPECT_TRUE(task->ops.size() == 2);
  EXPECT_EQ(static_cast<RRectOp*>(task->ops[1].get())->rRects.size(), drawCallCount);
  canvas->flush();
  EXPECT_TRUE(Compare(surface.get(), "CanvasTest/merge_draw_call_rrect"));
  device->unlock();
}

/**
 * 用例描述: 测试 ClearOp
 */
PAG_TEST(CanvasTest, merge_draw_clear_op) {
  auto device = GLDevice::Make();
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);
  int width = 72;
  int height = 72;
  auto surface = Surface::Make(context, width, height);
  auto canvas = surface->getCanvas();
  canvas->clear(Color::White());
  canvas->save();
  Path path;
  path.addRect(Rect::MakeXYWH(0.f, 0.f, 10.f, 10.f));
  canvas->clipPath(path);
  canvas->clear(Color::White());
  canvas->restore();
  Paint paint;
  paint.setColor(Color{0.8f, 0.8f, 0.8f, 1.f});
  int tileSize = 8;
  size_t drawCallCount = 0;
  for (int y = 0; y < height; y += tileSize) {
    bool draw = (y / tileSize) % 2 == 1;
    for (int x = 0; x < width; x += tileSize) {
      if (draw) {
        auto rect = Rect::MakeXYWH(static_cast<float>(x), static_cast<float>(y),
                                   static_cast<float>(tileSize), static_cast<float>(tileSize));
        canvas->drawRect(rect, paint);
        drawCallCount++;
      }
      draw = !draw;
    }
  }

  auto* drawingManager = context->drawingManager();
  EXPECT_TRUE(drawingManager->tasks.size() == 1);
  auto task = std::static_pointer_cast<OpsTask>(drawingManager->tasks[0]);
  EXPECT_TRUE(task->ops.size() == drawCallCount + 1);
  canvas->flush();
  EXPECT_TRUE(Compare(surface.get(), "CanvasTest/merge_draw_clear_op"));
  device->unlock();
}

/**
 * 用例描述: 测试 shape
 */
PAG_TEST(CanvasTest, textShape) {
  auto serifTypeface = Typeface::MakeFromPath("../resources/font/NotoSerifSC-Regular.otf");
  ASSERT_TRUE(serifTypeface != nullptr);
  std::string text =
      "ffi fl\n"
      "x²-y²\n"
      "🤡👨🏼‍🦱👨‍👨‍👧‍👦\n"
      "🇨🇳🇫🇮\n"
      "#️⃣#*️⃣*\n"
      "1️⃣🔟";
  auto positionedGlyphs = pag::TextShaper::Shape(text, serifTypeface);

  float fontSize = 25.f;
  float lineHeight = fontSize * 1.2f;
  float height = 0;
  float width = 0;
  float x;
  struct TextRun {
    std::vector<GlyphID> ids;
    std::vector<Point> positions;
    Font font;
  };
  std::vector<TextRun> textRuns;
  Path path;
  TextRun* run = nullptr;
  auto count = positionedGlyphs.glyphCount();
  auto newline = [&]() {
    x = 0;
    height += lineHeight;
    path.moveTo(0, height);
  };
  newline();
  for (size_t i = 0; i < count; ++i) {
    auto typeface = positionedGlyphs.getTypeface(i);
    if (run == nullptr || run->font.getTypeface() != typeface) {
      run = &textRuns.emplace_back();
      run->font = Font(typeface, fontSize);
    }
    auto index = positionedGlyphs.getStringIndex(i);
    auto length = (i + 1 == count ? text.length() : positionedGlyphs.getStringIndex(i + 1)) - index;
    auto name = text.substr(index, length);
    if (name == "\n") {
      newline();
      continue;
    }
    auto glyphID = positionedGlyphs.getGlyphID(i);
    run->ids.emplace_back(glyphID);
    run->positions.push_back(Point{x, height});
    x += run->font.getGlyphAdvance(glyphID);
    path.lineTo(x, height);
    if (width < x) {
      width = x;
    }
  }
  height += lineHeight;

  auto device = GLDevice::Make();
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);
  auto surface =
      Surface::Make(context, static_cast<int>(ceil(width)), static_cast<int>(ceil(height)));
  ASSERT_TRUE(surface != nullptr);
  auto canvas = surface->getCanvas();
  canvas->clear(Color::White());

  Paint strokePaint;
  strokePaint.setColor(Color{1.f, 0.f, 0.f, 1.f});
  strokePaint.setStrokeWidth(2.f);
  strokePaint.setStyle(PaintStyle::Stroke);
  canvas->drawPath(path, strokePaint);

  Paint paint;
  paint.setColor(Color::Black());
  for (const auto& textRun : textRuns) {
    canvas->drawGlyphs(&(textRun.ids[0]), &(textRun.positions[0]), textRun.ids.size(), textRun.font,
                       paint);
  }
  canvas->flush();
  EXPECT_TRUE(Compare(surface.get(), "CanvasTest/text_shape"));
  device->unlock();
}

/**
 * 用例描述: 测试更新 Mask
 */
PAG_TEST(CanvasTest, updateMask) {
  auto device = GLDevice::Make();
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);
  auto mask = Mask::Make(100, 100);
  auto surface = Surface::Make(context, mask->width(), mask->height());
  auto canvas = surface->getCanvas();
  Path path;
  path.addRect(Rect::MakeXYWH(10, 10, 10, 10));
  mask->fillPath(path);
  canvas->drawTexture(mask->updateTexture(context));
  path.reset();
  path.addRoundRect(Rect::MakeXYWH(22, 22, 10, 10), 3, 3);
  mask->fillPath(path);
  canvas->drawTexture(mask->updateTexture(context));
  EXPECT_TRUE(Compare(surface.get(), "CanvasTest/update_mask"));
  device->unlock();
}

/**
 * 用例描述: 测试 filter mode
 */
PAG_TEST(CanvasTest, filterMode) {
  auto device = GLDevice::Make();
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);
  auto codec = ImageCodec::MakeFrom("../resources/apitest/imageReplacement.png");
  ASSERT_TRUE(codec != nullptr);
  auto texture = codec->makeBuffer()->makeTexture(context);
  ASSERT_TRUE(texture != nullptr);
  int width = texture->width() * 2;
  int height = texture->height() * 2;
  auto surface = Surface::Make(context, width, height);
  auto canvas = surface->getCanvas();
  canvas->setMatrix(Matrix::MakeScale(2.f));
  canvas->drawTexture(texture, SamplingOptions(FilterMode::Nearest));
  EXPECT_TRUE(Compare(surface.get(), "CanvasTest/filter_mode_nearest"));
  canvas->clear();
  canvas->drawTexture(texture, SamplingOptions(FilterMode::Linear));
  EXPECT_TRUE(Compare(surface.get(), "CanvasTest/filter_mode_linear"));
  device->unlock();
}

/**
 * 用例描述: 测试 mipmap
 */
PAG_TEST(CanvasTest, mipmap) {
  auto device = GLDevice::Make();
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);
  auto image = ImageCodec::MakeFrom("../resources/apitest/rotation.jpg");
  ASSERT_TRUE(image != nullptr);
  auto pixelBuffer = PixelBuffer::Make(image->width(), image->height(), false, false);
  ASSERT_TRUE(pixelBuffer != nullptr);
  Bitmap bitmap(pixelBuffer);
  auto result = image->readPixels(pixelBuffer->info(), bitmap.writablePixels());
  bitmap.reset();
  ASSERT_TRUE(result);
  auto texture = pixelBuffer->makeTexture(context);
  ASSERT_TRUE(texture != nullptr);
  auto textureMipMapped = pixelBuffer->makeMipMappedTexture(context);
  ASSERT_TRUE(textureMipMapped != nullptr);
  float scale = 0.03f;
  auto width = image->width();
  auto height = image->height();
  auto imageWidth = static_cast<float>(width) * scale;
  auto imageHeight = static_cast<float>(height) * scale;
  auto imageMatrix = Matrix::MakeScale(scale);
  auto surface =
      Surface::Make(context, static_cast<int>(imageWidth), static_cast<int>(imageHeight));
  auto canvas = surface->getCanvas();
  canvas->setMatrix(imageMatrix);
  // 绘制没有 mipmap 的 texture 时，使用 MipmapMode::Linear 会回退到 MipmapMode::None。
  canvas->drawTexture(texture, SamplingOptions(FilterMode::Linear, MipMapMode::Linear));
  EXPECT_TRUE(Compare(surface.get(), "CanvasTest/mipmap_none"));
  canvas->clear();
  canvas->drawTexture(textureMipMapped, SamplingOptions(FilterMode::Linear, MipMapMode::Nearest));
  EXPECT_TRUE(Compare(surface.get(), "CanvasTest/mipmap_nearest"));
  canvas->clear();
  canvas->drawTexture(textureMipMapped, SamplingOptions(FilterMode::Linear, MipMapMode::Linear));
  EXPECT_TRUE(Compare(surface.get(), "CanvasTest/mipmap_linear"));
  surface = Surface::Make(context, static_cast<int>(imageWidth * 4.f),
                          static_cast<int>(imageHeight * 4.f));
  canvas = surface->getCanvas();
  Paint paint;
  paint.setShader(Shader::MakeTextureShader(textureMipMapped, TileMode::Mirror, TileMode::Repeat,
                                            SamplingOptions(FilterMode::Linear, MipMapMode::Linear))
                      ->makeWithPreLocalMatrix(imageMatrix));
  canvas->drawRect(Rect::MakeWH(surface->width(), surface->height()), paint);
  EXPECT_TRUE(Compare(surface.get(), "CanvasTest/mipmap_linear_texture_effect"));
  device->unlock();
}

/**
 * 用例描述: 测试 hardware buffer 的 mipmap
 */
PAG_TEST(CanvasTest, hardwareMipMap) {
  auto device = GLDevice::Make();
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);
  auto image = ImageCodec::MakeFrom("../resources/apitest/rotation.jpg");
  ASSERT_TRUE(image != nullptr);
  auto pixelBuffer = PixelBuffer::Make(image->width(), image->height(), false);
  ASSERT_TRUE(pixelBuffer != nullptr);
  Bitmap bitmap(pixelBuffer);
  auto result = image->readPixels(pixelBuffer->info(), bitmap.writablePixels());
  bitmap.reset();
  ASSERT_TRUE(result);
  auto textureMipMapped = pixelBuffer->makeMipMappedTexture(context);
  ASSERT_TRUE(textureMipMapped != nullptr);
  float scale = 0.03f;
  auto width = image->width();
  auto height = image->height();
  auto imageWidth = static_cast<float>(width) * scale;
  auto imageHeight = static_cast<float>(height) * scale;
  auto imageMatrix = Matrix::MakeScale(scale);
  auto surface =
      Surface::Make(context, static_cast<int>(imageWidth), static_cast<int>(imageHeight));
  auto canvas = surface->getCanvas();
  canvas->setMatrix(imageMatrix);
  canvas->drawTexture(textureMipMapped, SamplingOptions(FilterMode::Linear, MipMapMode::Linear));
  EXPECT_TRUE(Compare(surface.get(), "CanvasTest/mipmap_linear_hardware"));
  device->unlock();
}

PAG_TEST(CanvasTest, shape) {
  auto device = GLDevice::Make();
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);
  auto surface = Surface::Make(context, 500, 500);
  auto canvas = surface->getCanvas();
  Path path;
  path.addRect(Rect::MakeXYWH(10, 10, 100, 100));
  auto shape = Shape::MakeFromFill(path);
  Paint paint;
  paint.setColor(Color::White());
  canvas->drawShape(shape, paint);
  path.reset();
  path.addRoundRect(Rect::MakeXYWH(10, 120, 100, 100), 10, 10);
  shape = Shape::MakeFromFill(path);
  canvas->drawShape(shape, paint);
  path.reset();
  path.addRect(Rect::MakeXYWH(10, 250, 100, 100));
  auto matrix = Matrix::I();
  matrix.postRotate(30, 60, 300);
  path.transform(matrix);
  shape = Shape::MakeFromFill(path);
  canvas->drawShape(shape, paint);
  paint.setColor(Color::Black());
  paint.setAlpha(0.3);
  matrix.reset();
  matrix.postScale(0.5, 0.5, 60, 300);
  canvas->setMatrix(matrix);
  canvas->drawShape(shape, paint);
  EXPECT_TRUE(Compare(surface.get(), "CanvasTest/shape"));
  device->unlock();
}
}  // namespace tgfx
