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
#include "base/utils/TimeUtil.h"
#include "core/vectors/freetype/FTMask.h"
#include "framework/pag_test.h"
#include "framework/utils/PAGTestUtils.h"
#include "nlohmann/json.hpp"
#include "tgfx/core/Mask.h"
#include "tgfx/gpu/Surface.h"
#include "tgfx/gpu/opengl/GLDevice.h"

namespace pag {
using nlohmann::json;
using namespace tgfx;

/**
 * 用例描述: 矢量栅格化相关功能测试
 */
PAG_TEST(PAGRasterizerTest, TestRasterizer) {
  Path path = {};
  path.addRect(100, 100, 400, 400);
  path.addRoundRect(tgfx::Rect::MakeLTRB(150, 150, 350, 350), 30, 20, true);
  path.addOval(tgfx::Rect::MakeLTRB(200, 200, 300, 300));
  // 501*501 是为了测试 GL_UNPACK_ALIGNMENT
  auto mask = Mask::Make(501, 501);
  ASSERT_TRUE(mask != nullptr);
  auto matrix = tgfx::Matrix::MakeTrans(50, 50);
  mask->setMatrix(matrix);
  mask->fillPath(path);
  auto maskBuffer = std::static_pointer_cast<FTMask>(mask)->getBuffer();
  EXPECT_TRUE(Baseline::Compare(maskBuffer, "PAGRasterizerTest/rasterizer_path"));

  auto device = GLDevice::Make();
  ASSERT_TRUE(device != nullptr);
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);
  auto texture = mask->makeTexture(context);
  ASSERT_TRUE(texture != nullptr);
  auto surface = Surface::Make(context, mask->width(), mask->height());
  ASSERT_TRUE(surface != nullptr);
  auto canvas = surface->getCanvas();
  canvas->drawTexture(texture.get());
  auto pixelBuffer = PixelBuffer::Make(mask->width(), mask->height(), true, false);
  ASSERT_TRUE(pixelBuffer != nullptr);
  Bitmap bitmap(pixelBuffer);
  bitmap.eraseAll();
  auto result = surface->readPixels(bitmap.info(), bitmap.writablePixels());
  ASSERT_TRUE(result);
  device->unlock();
  EXPECT_TRUE(Baseline::Compare(bitmap, "PAGRasterizerTest/rasterizer_path_texture"));

  auto typeface = Typeface::MakeFromPath("../resources/font/NotoColorEmoji.ttf");
  ASSERT_TRUE(typeface != nullptr);
  ASSERT_TRUE(typeface->hasColor());
  auto glyphID = typeface->getGlyphID("👻");
  ASSERT_TRUE(glyphID != 0);
  Font font = {};
  font.setSize(300);
  font.setTypeface(typeface);
  font.setFauxItalic(true);
  font.setFauxBold(true);
  auto buffer = font.getGlyphImage(glyphID, &matrix);
  ASSERT_TRUE(buffer != nullptr);
  EXPECT_TRUE(fabsf(matrix.getScaleX() - 2.75229359f) < FLT_EPSILON);
  EXPECT_TRUE(fabsf(matrix.getSkewX() + 0.550458729f) < FLT_EPSILON);
  EXPECT_TRUE(Baseline::Compare(std::static_pointer_cast<PixelBuffer>(buffer),
                                "PAGRasterizerTest/rasterizer_emoji"));
}
}  // namespace pag
