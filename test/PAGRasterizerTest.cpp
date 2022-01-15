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
#include "framework/pag_test.h"
#include "framework/utils/PAGTestUtils.h"
#include "gpu/Surface.h"
#include "nlohmann/json.hpp"
#include "platform/NativeGLDevice.h"
#include "raster/Mask.h"
#include "raster/freetype/FTMask.h"

namespace pag {
using nlohmann::json;

/**
 * 用例描述: 矢量栅格化相关功能测试
 */
PAG_TEST(PAGRasterizerTest, TestRasterizer) {
  Path path = {};
  path.addRect(100, 100, 400, 400);
  path.addRoundRect(Rect::MakeLTRB(150, 150, 350, 350), 30, 20, true);
  path.addOval(Rect::MakeLTRB(200, 200, 300, 300));
  // 501*501 是为了测试 GL_UNPACK_ALIGNMENT
  auto mask = Mask::Make(501, 501);
  ASSERT_TRUE(mask != nullptr);
  auto matrix = Matrix::MakeTrans(50, 50);
  mask->setMatrix(matrix);
  mask->fillPath(path);
  auto pixelBuffer = std::static_pointer_cast<FTMask>(mask)->getBuffer();
  std::string pathMD5 = DumpMD5(pixelBuffer);
  json rasterizerJson = {{"rasterizer_path", pathMD5}};
  auto pathCompareMD5 = PAGTestEnvironment::CompareJson["PAGRasterizerTest"]["rasterizer_path"];
  std::string imagePath = "../test/out/rasterizer_path.png";
  TraceIf(pixelBuffer, imagePath, pathMD5 != pathCompareMD5);
  if (pathCompareMD5 != nullptr) {
    EXPECT_EQ(pathCompareMD5.get<std::string>(), pathMD5);
  }

  auto device = NativeGLDevice::Make();
  ASSERT_TRUE(device != nullptr);
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);
  auto texture = mask->makeTexture(context);
  ASSERT_TRUE(texture != nullptr);
  auto surface = Surface::Make(context, mask->width(), mask->height());
  ASSERT_TRUE(surface != nullptr);
  auto canvas = surface->getCanvas();
  canvas->drawTexture(texture.get());
  Bitmap bitmap = {};
  auto result = bitmap.allocPixels(mask->width(), mask->height(), true);
  ASSERT_TRUE(result);
  bitmap.eraseAll();
  auto pixels = bitmap.lockPixels();
  result = surface->readPixels(bitmap.info(), pixels);
  bitmap.unlockPixels();
  ASSERT_TRUE(result);
  device->unlock();
  std::string textureMD5 = DumpMD5(bitmap);
  rasterizerJson["rasterizer_path_texture"] = textureMD5;
  auto textureCompareMD5 =
      PAGTestEnvironment::CompareJson["PAGRasterizerTest"]["rasterizer_path_texture"];
  std::string texturePath = "../test/out/rasterizer_path_texture.png";
  TraceIf(bitmap, texturePath, textureMD5 != textureCompareMD5);
  if (textureCompareMD5 != nullptr) {
    EXPECT_EQ(textureCompareMD5.get<std::string>(), textureMD5);
  }

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
  bitmap = Bitmap(std::static_pointer_cast<PixelBuffer>(buffer));
  auto glyphMD5 = DumpMD5(bitmap);
  rasterizerJson["rasterizer_glyph"] = glyphMD5;
  auto glyphCompareMD5 = PAGTestEnvironment::CompareJson["PAGRasterizerTest"]["rasterizer_glyph"];
  imagePath = "../test/out/rasterizer_emoji.png";
  TraceIf(bitmap, imagePath, glyphMD5 != glyphCompareMD5);
  if (glyphCompareMD5 != nullptr) {
    EXPECT_EQ(glyphCompareMD5.get<std::string>(), glyphMD5);
  }
  PAGTestEnvironment::DumpJson["PAGRasterizerTest"] = rasterizerJson;
}
}  // namespace pag
