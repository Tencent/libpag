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

#include <array>
#include "base/utils/UniqueID.h"
#include "rendering/caches/RenderCache.h"
#include "rendering/filters/Transform3DSceneFilter.h"
#include "rendering/gpu/Devices.h"
#include "rendering/graphics/Picture.h"
#include "rendering/graphics/Shape.h"
#include "rendering/graphics/Snapshot.h"
#include "rendering/layers/PAGStage.h"
#include "tgfx/gpu/CommandQueue.h"
#include "utils/TestUtils.h"

#ifdef TGFX_USE_OPENGL
#include "tgfx/gpu/opengl/GLTypes.h"
#ifdef PAG_USE_SWIFTSHADER
#include <GLES3/gl3.h>
#else
#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION
#endif
#include <OpenGL/gl3.h>
#endif
#endif

namespace pag {

/** 验证场景纹理在复用、改尺寸、空闲释放和整体释放时与快照独立对称计账。 */
PAG_TEST(PAGTransform3DResourceTest, SceneMemoryUsageTracksOwnedTextures) {
  DeviceLockScope lock;
  ASSERT_TRUE(static_cast<bool>(lock));
  auto gpu = lock.context()->gpu();
  auto stage = PAGStage::Make(64, 64);
  RenderCache cache(stage.get());
  cache.attachToContext(lock.context());
  tgfx::Path path;
  path.addRect(tgfx::Rect::MakeWH(16, 16));
  auto picture = std::static_pointer_cast<Picture>(
      Picture::MakeFrom(UniqueID::Next(), Shape::MakeFrom(0, path, tgfx::Color::White())));
  ASSERT_NE(picture, nullptr);
  auto snapshot = cache.getSnapshot(picture.get(), 1, 1);
  ASSERT_NE(snapshot, nullptr);
  auto snapshotMemory = snapshot->memoryUsage();
  ASSERT_GT(snapshotMemory, 0u);
  EXPECT_EQ(cache.memoryUsage(), snapshotMemory);
  auto ownedResources = std::make_unique<Transform3DSceneFilter::Resources>();
  auto resources = ownedResources.get();
  cache.addFilterResources(UniqueID::Next(), std::move(ownedResources));
  cache.addFilterResources(UniqueID::Next(), std::make_unique<FilterResources>());
  EXPECT_EQ(cache.memoryUsage(), snapshotMemory);

  Transform3DSceneFilter filter(&cache, {}, {}, {}, 64, 64);
  ASSERT_TRUE(filter.prepareTextures(resources, gpu));
  size_t sceneMemory = 64 * 64 * 32;
  EXPECT_EQ(resources->memoryUsage(), sceneMemory);
  EXPECT_EQ(cache.memoryUsage(), snapshotMemory + sceneMemory);
  ASSERT_TRUE(filter.prepareTextures(resources, gpu));
  EXPECT_EQ(cache.memoryUsage(), snapshotMemory + sceneMemory);

  Transform3DSceneFilter reshaped(&cache, {}, {}, {}, 128, 32);
  ASSERT_TRUE(reshaped.prepareTextures(resources, gpu));
  EXPECT_EQ(cache.memoryUsage(), snapshotMemory + sceneMemory);
  Transform3DSceneFilter enlarged(&cache, {}, {}, {}, 128, 64);
  ASSERT_TRUE(enlarged.prepareTextures(resources, gpu));
  sceneMemory = 128 * 64 * 32;
  EXPECT_EQ(cache.memoryUsage(), snapshotMemory + sceneMemory);
  EXPECT_EQ(cache.graphicsMemory, snapshotMemory);

  resources->onFrameEnd();
  EXPECT_EQ(cache.memoryUsage(), snapshotMemory + sceneMemory);
  resources->onFrameEnd();
  EXPECT_EQ(cache.memoryUsage(), snapshotMemory + sceneMemory);
  resources->onFrameEnd();
  EXPECT_EQ(cache.memoryUsage(), snapshotMemory);
  EXPECT_EQ(cache.graphicsMemory, snapshotMemory);
  ASSERT_TRUE(filter.prepareTextures(resources, gpu));
  EXPECT_EQ(cache.memoryUsage(), snapshotMemory + 64 * 64 * 32);
  cache.releaseAll();
  EXPECT_EQ(cache.memoryUsage(), 0u);
  cache.detachFromContext();
}

/** 验证尺寸元数据尚未提交的部分分配资源按实际纹理尺寸计账。 */
PAG_TEST(PAGTransform3DResourceTest, IncompleteSceneResourcesCountActualTextureSizes) {
  DeviceLockScope lock;
  ASSERT_TRUE(static_cast<bool>(lock));
  auto gpu = lock.context()->gpu();
  auto stage = PAGStage::Make(64, 64);
  RenderCache cache(stage.get());
  auto ownedResources = std::make_unique<Transform3DSceneFilter::Resources>();
  auto resources = ownedResources.get();
  cache.addFilterResources(UniqueID::Next(), std::move(ownedResources));
  tgfx::TextureDescriptor depth(64, 64, tgfx::PixelFormat::DEPTH24_STENCIL8, false, 1,
                                tgfx::TextureUsage::RENDER_ATTACHMENT);
  resources->depth = gpu->createTexture(depth);
  ASSERT_NE(resources->depth, nullptr);
  tgfx::TextureDescriptor color(
      128, 32, tgfx::PixelFormat::RGBA_8888, false, 1,
      tgfx::TextureUsage::RENDER_ATTACHMENT | tgfx::TextureUsage::TEXTURE_BINDING);
  resources->color = gpu->createTexture(color);
  ASSERT_NE(resources->color, nullptr);
  color.width = 64;
  resources->encodedDepth[0] = gpu->createTexture(color);
  ASSERT_NE(resources->encodedDepth[0], nullptr);
  EXPECT_EQ(resources->width, 0);
  EXPECT_EQ(resources->height, 0);
  EXPECT_EQ(cache.memoryUsage(), 64u * 64 * 4 + 128u * 32 * 4 + 64u * 32 * 4);
  resources->color = nullptr;
  EXPECT_EQ(cache.memoryUsage(), 64u * 64 * 4 + 64u * 32 * 4);
  resources->onFrameEnd();
  EXPECT_EQ(cache.memoryUsage(), 64u * 64 * 4 + 64u * 32 * 4);
  resources->onFrameEnd();
  EXPECT_EQ(cache.memoryUsage(), 0u);
  cache.releaseAll();
  EXPECT_EQ(cache.memoryUsage(), 0u);
}

namespace {

struct QueryLifecycleResources : FilterResources {
  int frameEndCount = 0;
  int queryEndCount = 0;

  void onFrameEnd() override {
    frameEndCount++;
  }

  void onQueryEnd() override {
    queryEndCount++;
  }
};

}  // namespace

/** 验证查询只读复用匹配快照，并立即释放场景纹理而不推进绘制帧生命周期。 */
PAG_TEST(PAGTransform3DResourceTest, QueriesReuseSnapshotsAndReleaseSceneTextures) {
  DeviceLockScope lock;
  ASSERT_TRUE(static_cast<bool>(lock));
  auto gpu = lock.context()->gpu();
  auto stage = PAGStage::Make(64, 64);
  RenderCache cache(stage.get());
  cache.attachToContext(lock.context());
  tgfx::Path path;
  path.addRect(tgfx::Rect::MakeWH(16, 16));
  auto shape = Shape::MakeFrom(0, path, tgfx::Color::White());
  auto assetID = UniqueID::Next();
  auto picture = std::static_pointer_cast<Picture>(Picture::MakeFrom(assetID, shape));
  ASSERT_NE(picture, nullptr);
  auto snapshot = cache.getSnapshot(picture.get(), 1, 1);
  ASSERT_NE(snapshot, nullptr);
  auto otherPicture = std::static_pointer_cast<Picture>(Picture::MakeFrom(UniqueID::Next(), shape));
  ASSERT_NE(otherPicture, nullptr);
  auto otherSnapshot = cache.getSnapshot(otherPicture.get(), 1, 1);
  ASSERT_NE(otherSnapshot, nullptr);
  cache.detachFromContext();
  cache.beginFrame();
  cache.attachToContext(lock.context());
  EXPECT_EQ(cache.getSnapshot(otherPicture.get(), 1, 1), otherSnapshot);
  cache.detachFromContext();
  ASSERT_EQ(snapshot->idleFrames, 1);
  auto snapshotLRU = cache.snapshotLRU;
  auto usedAssets = cache.usedAssets;
  auto snapshotMemory = cache.memoryUsage();
  auto snapshotCount = cache.snapshotCaches.size();
  auto timestampCount = cache.timestamps.size();
  auto ownedLifecycle = std::make_unique<QueryLifecycleResources>();
  auto lifecycle = ownedLifecycle.get();
  cache.addFilterResources(UniqueID::Next(), std::move(ownedLifecycle));

  cache.attachToContext(lock.context(), false);
  EXPECT_EQ(cache.getSnapshot(assetID), snapshot);
  EXPECT_EQ(cache.getSnapshot(picture.get(), 1, 1), snapshot);
  EXPECT_EQ(cache.getSnapshot(picture.get(), 2, 2), nullptr);
  auto changedPicture = std::static_pointer_cast<Picture>(Picture::MakeFrom(assetID, shape));
  ASSERT_NE(changedPicture, nullptr);
  EXPECT_EQ(cache.getSnapshot(changedPicture.get(), 1, 1), nullptr);
  EXPECT_EQ(cache.getSnapshot(assetID), snapshot);
  EXPECT_EQ(cache.snapshotLRU, snapshotLRU);
  EXPECT_EQ(cache.usedAssets, usedAssets);
  EXPECT_EQ(snapshot->idleFrames, 1);
  auto newPicture = std::static_pointer_cast<Picture>(Picture::MakeFrom(UniqueID::Next(), shape));
  ASSERT_NE(newPicture, nullptr);
  EXPECT_EQ(cache.getSnapshot(newPicture.get(), 1, 1), nullptr);
  EXPECT_EQ(cache.snapshotCaches.size(), snapshotCount);
  EXPECT_EQ(cache.memoryUsage(), snapshotMemory);
  Transform3DSceneFilter filter(&cache, {}, {}, {}, 64, 64);
  auto resources = static_cast<Transform3DSceneFilter::Resources*>(filter.getFilterResources(gpu));
  ASSERT_NE(resources, nullptr);
  auto pipeline = resources->pipeline;
  auto sampler = resources->sampler;
  ASSERT_NE(pipeline, nullptr);
  ASSERT_NE(sampler, nullptr);
  ASSERT_TRUE(filter.prepareTextures(resources, gpu));
  EXPECT_EQ(cache.memoryUsage(), snapshotMemory + 64 * 64 * 32);
#ifdef TGFX_USE_OPENGL
  std::array<const tgfx::Texture*, 7> queryTextures = {
      resources->color.get(),           resources->encodedDepth[0].get(),
      resources->encodedDepth[1].get(), resources->index[0].get(),
      resources->index[1].get(),        resources->accumulated[0].get(),
      resources->accumulated[1].get()};
  std::array<unsigned, 7> textureIDs = {};
  for (size_t index = 0; index < queryTextures.size(); index++) {
    tgfx::GLTextureInfo textureInfo = {};
    ASSERT_TRUE(queryTextures[index]->getBackendTexture().getGLTextureInfo(&textureInfo));
    textureIDs[index] = textureInfo.id;
    ASSERT_EQ(glIsTexture(textureIDs[index]), GL_TRUE);
  }
#endif
  auto frameTime = gpu->queue()->frameTime();
  cache.detachFromContext();
#ifdef TGFX_USE_OPENGL
  for (auto textureID : textureIDs) {
    EXPECT_EQ(glIsTexture(textureID), GL_FALSE);
  }
#endif
  EXPECT_EQ(gpu->queue()->frameTime(), frameTime);
  EXPECT_EQ(cache.getContext(), nullptr);
  EXPECT_EQ(cache.memoryUsage(), snapshotMemory);
  EXPECT_EQ(cache.snapshotCaches.size(), snapshotCount);
  EXPECT_EQ(cache.getSnapshot(assetID), snapshot);
  EXPECT_EQ(resources->width, 0);
  EXPECT_EQ(resources->height, 0);
  EXPECT_EQ(resources->depth, nullptr);
  EXPECT_EQ(resources->color, nullptr);
  for (size_t index = 0; index < 2; index++) {
    EXPECT_EQ(resources->encodedDepth[index], nullptr);
    EXPECT_EQ(resources->index[index], nullptr);
    EXPECT_EQ(resources->accumulated[index], nullptr);
  }
  EXPECT_EQ(resources->pipeline, pipeline);
  EXPECT_EQ(resources->sampler, sampler);
  EXPECT_EQ(lifecycle->frameEndCount, 0);
  EXPECT_EQ(lifecycle->queryEndCount, 1);
  EXPECT_EQ(cache.timestamps.size(), timestampCount);

  for (int query = 0; query < 3; query++) {
    cache.attachToContext(lock.context(), false);
    ASSERT_TRUE(filter.prepareTextures(resources, gpu));
    cache.detachFromContext();
    EXPECT_EQ(cache.memoryUsage(), snapshotMemory);
    EXPECT_EQ(cache.snapshotCaches.size(), snapshotCount);
  }
  EXPECT_EQ(resources->pipeline, pipeline);
  EXPECT_EQ(resources->sampler, sampler);
  EXPECT_EQ(lifecycle->frameEndCount, 0);
  EXPECT_EQ(lifecycle->queryEndCount, 4);
  EXPECT_EQ(cache.timestamps.size(), timestampCount);
  EXPECT_EQ(cache.snapshotLRU, snapshotLRU);
  EXPECT_EQ(cache.usedAssets, usedAssets);
  EXPECT_EQ(snapshot->idleFrames, 1);
}

}  // namespace pag
