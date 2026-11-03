/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
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

#include "utils/TestUtils.h"

namespace pag {

/**
 * 用例描述: 单图层 3D 变换，验证 3D 矩阵与投影。
 */
PAG_PIXEL_TEST(PAGTransform3DTest, Transform3D) {
  auto pagFile = LoadPAGFile("resources/filter/3Dtest.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
  ASSERT_NE(pagSurface, nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGTransform3DTest/Transform3D"));
}

/**
 * 用例描述: 多图层 3D 遮挡，验证共享深度缓冲的前后层级关系。
 */
PAG_PIXEL_TEST(PAGTransform3DTest, Transform3D_BOX) {
  auto pagFile = LoadPAGFile("resources/filter/3D_BOX.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
  ASSERT_NE(pagSurface, nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);
  pagPlayer->setProgress(1.0);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGTransform3DTest/Transform3D_BOX"));
}

/**
 * 用例描述: 相机图层，验证 view 矩阵与 zoom。
 */
PAG_PIXEL_TEST(PAGTransform3DTest, Camera) {
  auto pagFile = LoadPAGFile("resources/filter/CameraTest.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
  ASSERT_NE(pagSurface, nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);
  pagPlayer->setProgress(0.5);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGTransform3DTest/Camera"));
}

/**
 * 用例描述: 渲染尺寸与合成尺寸不同时，验证 3D 层的缩放换算与裁剪拷贝路径。
 */
PAG_PIXEL_TEST(PAGTransform3DTest, ScaledSurface) {
  auto pagFile = LoadPAGFile("resources/filter/3D_BOX.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = OffscreenSurface::Make(pagFile->width() / 2, pagFile->height() / 2);
  ASSERT_NE(pagSurface, nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);
  pagPlayer->setMatrix(Matrix::MakeScale(0.5f, 0.5f));
  pagPlayer->setProgress(1.0);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGTransform3DTest/ScaledSurface"));
}

/**
 * 用例描述: 同一个播放器连续绘制多帧，验证逐帧变化时的 3D 变换与深度状态。
 */
PAG_PIXEL_TEST(PAGTransform3DTest, Transform3D_Sequence) {
  auto pagFile = LoadPAGFile("resources/filter/3Dtest.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
  ASSERT_NE(pagSurface, nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);

  pagPlayer->setProgress(0.25);
  pagPlayer->flush();
  pagPlayer->setProgress(0.5);
  pagPlayer->flush();
  pagPlayer->setProgress(0.75);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGTransform3DTest/Transform3D_Sequence"));
}

}  // namespace pag
