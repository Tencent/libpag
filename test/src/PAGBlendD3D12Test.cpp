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
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "base/utils/TGFXCast.h"

#if defined(TGFX_USE_D3D12)

#include <memory>
#include "pag/pag.h"
#include "rendering/gpu/Devices.h"
#include "tgfx/gpu/Context.h"
#include "tgfx/gpu/GPU.h"
#include "tgfx/gpu/Texture.h"
#include "utils/D3D12TestUtils.h"
#include "utils/TestUtils.h"

namespace pag {
namespace {

std::shared_ptr<tgfx::Texture> MakeBottomLeftImageTexture(int width, int height) {
  auto texture = MakeD3D12RenderableTexture(width, height);
  if (texture == nullptr) {
    return nullptr;
  }
  auto surface = PAGSurface::MakeFrom(ToPAG(texture->getBackendTexture()), ImageOrigin::BottomLeft);
  if (surface == nullptr) {
    return nullptr;
  }
  auto composition = PAGComposition::Make(width, height);
  auto imageLayer = PAGImageLayer::Make(width, height, 1000000);
  imageLayer->replaceImage(MakePAGImage("assets/mountain.jpg"));
  composition->addLayer(imageLayer);
  auto player = std::make_shared<PAGPlayer>();
  player->setSurface(surface);
  player->setComposition(composition);
  player->setMatrix(Matrix::I());
  if (!player->flush()) {
    return nullptr;
  }
  if (!surface->waitUntilCompleted()) {
    return nullptr;
  }
  return texture;
}

}  // namespace

/**
 * Covers copy-destination blending into an external BottomLeft D3D12 texture.
 */
PAG_PIXEL_TEST(PAGBlendD3D12Test, CopyDstTexture) {
  const int width = 400;
  const int height = 400;
  auto texture = MakeD3D12RenderableTexture(width, height);
  ASSERT_TRUE(texture != nullptr);
  auto surface = PAGSurface::MakeFrom(ToPAG(texture->getBackendTexture()), ImageOrigin::BottomLeft);
  ASSERT_TRUE(surface != nullptr);
  auto file = LoadPAGFile("resources/blend/Multiply.pag");
  ASSERT_TRUE(file != nullptr);
  auto player = std::make_shared<PAGPlayer>();
  player->setSurface(surface);
  player->setComposition(file);
  player->setMatrix(Matrix::I());
  player->setProgress(0.5);
  ASSERT_TRUE(player->flush());
  EXPECT_TRUE(Baseline::Compare(surface, "PAGBlendD3D12Test/CopyDstTexture"));
}

/**
 * Covers a BottomLeft texture-backed replacement image rendered into a TopLeft D3D12 surface.
 */
PAG_PIXEL_TEST(PAGBlendD3D12Test, TextureBottomLeft) {
  const int width = 720;
  const int height = 1280;
  auto replacement = MakeBottomLeftImageTexture(width, height);
  ASSERT_TRUE(replacement != nullptr);
  auto image =
      PAGImage::FromTexture(ToPAG(replacement->getBackendTexture()), ImageOrigin::BottomLeft);
  ASSERT_TRUE(image != nullptr);
  auto texture = MakeD3D12RenderableTexture(width, height);
  ASSERT_TRUE(texture != nullptr);
  auto surface = PAGSurface::MakeFrom(ToPAG(texture->getBackendTexture()), ImageOrigin::TopLeft);
  ASSERT_TRUE(surface != nullptr);
  auto file = LoadPAGFile("resources/apitest/texture_bottom_left.pag");
  ASSERT_TRUE(file != nullptr);
  file->replaceImage(3, image);
  auto player = std::make_shared<PAGPlayer>();
  player->setSurface(surface);
  player->setComposition(file);
  player->setMatrix(Matrix::I());
  player->setProgress(0.34);
  ASSERT_TRUE(player->flush());
  EXPECT_TRUE(Baseline::Compare(surface, "PAGBlendD3D12Test/TextureBottomLeft"));
}

/**
 * Covers BottomLeft origin handling for both the external render target and replacement image.
 */
PAG_PIXEL_TEST(PAGBlendD3D12Test, BothBottomLeft) {
  const int width = 720;
  const int height = 1280;
  auto replacement = MakeBottomLeftImageTexture(width, height);
  ASSERT_TRUE(replacement != nullptr);
  tgfx::D3D12TextureInfo textureInfo = {};
  ASSERT_TRUE(replacement->getBackendTexture().getD3D12TextureInfo(&textureInfo));
  D3D12TextureInfo info = {};
  info.resource = const_cast<void*>(textureInfo.resource);
  auto image =
      PAGImage::FromTexture(BackendTexture(info, width / 2, height / 2), ImageOrigin::BottomLeft);
  ASSERT_TRUE(image != nullptr);
  image->setMatrix(Matrix::MakeTrans(width / 10, height / 5));
  auto texture = MakeD3D12RenderableTexture(width, height);
  ASSERT_TRUE(texture != nullptr);
  auto surface = PAGSurface::MakeFrom(ToPAG(texture->getBackendTexture()), ImageOrigin::BottomLeft);
  ASSERT_TRUE(surface != nullptr);

  auto composition = PAGComposition::Make(width, height);
  auto imageLayer = PAGImageLayer::Make(width, height, 1000000);
  imageLayer->replaceImage(image);
  composition->addLayer(imageLayer);

  auto player = std::make_shared<PAGPlayer>();
  player->setSurface(surface);
  player->setComposition(composition);
  player->setMatrix(Matrix::I());
  player->setProgress(0.3);
  ASSERT_TRUE(player->flush());
  EXPECT_TRUE(Baseline::Compare(surface, "PAGBlendD3D12Test/BothBottomLeft"));
}

}  // namespace pag

#endif  // TGFX_USE_D3D12
