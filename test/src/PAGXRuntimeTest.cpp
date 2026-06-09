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

#include "pagx/PAGScene.h"
#include "pagx/PAGSurface.h"
#include "pagx/PAGTimeline.h"
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Animation.h"
#include "pagx/nodes/AnimationObject.h"
#include "pagx/nodes/Channel.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/SolidColor.h"
#include "utils/Baseline.h"
#include "utils/TestUtils.h"

#ifdef PAG_USE_SWIFTSHADER
#include <GLES3/gl3.h>
#else
#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION
#endif
#include <OpenGL/gl3.h>
#endif

namespace pag {

/**
 * Test case: PAGScene registers itself with the source document on creation and unregisters on
 * destruction.
 */
PAGX_TEST(PAGXRuntimeTest, PAGSceneLiveSceneRegistration) {
  auto doc = pagx::PAGXDocument::Make(100, 100);

  EXPECT_TRUE(doc->liveScenes.empty());

  auto file = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(file != nullptr);
  ASSERT_EQ(doc->liveScenes.size(), 1u);
  EXPECT_EQ(doc->liveScenes[0].lock().get(), file.get());

  file.reset();
  EXPECT_TRUE(doc->liveScenes.empty());

  auto file2 = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(file2 != nullptr);
  EXPECT_EQ(file2->width(), 100.0f);
  EXPECT_EQ(file2->height(), 100.0f);
}

/**
 * Test case: PAGScene timeline lookup by name and identity sharing.
 */
PAGX_TEST(PAGXRuntimeTest, PAGSceneTimelineLookup) {
  auto doc = pagx::PAGXDocument::Make(100, 100);

  auto main = doc->makeNode<pagx::Animation>("main");
  main->duration = 60;
  main->frameRate = 60;
  doc->animations.push_back(main);

  auto hint = doc->makeNode<pagx::Animation>("hint");
  hint->duration = 30;
  hint->frameRate = 60;
  doc->animations.push_back(hint);

  auto file = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(file != nullptr);

  auto ids = file->getTimelineIds();
  ASSERT_EQ(ids.size(), 2u);
  EXPECT_EQ(ids[0], "main");
  EXPECT_EQ(ids[1], "hint");

  auto t1 = file->getTimeline("main");
  auto t1Again = file->getTimeline("main");
  ASSERT_TRUE(t1 != nullptr);
  EXPECT_EQ(t1.get(), t1Again.get());
  EXPECT_EQ(t1->getId(), "main");
  EXPECT_EQ(t1->duration(), 1'000'000);

  auto t2 = file->getTimeline("hint");
  ASSERT_TRUE(t2 != nullptr);
  EXPECT_NE(t1.get(), t2.get());

  EXPECT_EQ(file->getTimeline("missing"), nullptr);

  auto def = file->getDefaultTimeline();
  EXPECT_EQ(def.get(), t1.get());
}

/**
 * Test case: PAGScene::Make refuses to build when applyLayout reported a cyclic external
 * composition reference, since the partially laid-out tree is inconsistent.
 */
PAGX_TEST(PAGXRuntimeTest, PAGSceneMakeNullOnLayoutCycle) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto* layer = doc->makeNode<pagx::Layer>("comp");
  layer->width = 50;
  layer->height = 50;
  layer->externalDoc = doc;
  doc->layers.push_back(layer);

  auto scene = pagx::PAGScene::Make(doc);
  EXPECT_EQ(scene, nullptr);
  bool reportedCycle = false;
  for (const auto& error : doc->errors) {
    if (error.rfind("Cyclic external composition reference detected", 0) == 0) {
      reportedCycle = true;
    }
  }
  EXPECT_TRUE(reportedCycle);
  layer->externalDoc = nullptr;
}

/**
 * Test case: PAGSurface::MakeOffscreen creates a real GPU-backed surface with the requested size.
 */
PAGX_TEST(PAGXRuntimeTest, PAGSurfaceMakeOffscreen) {
  auto surface = pagx::PAGSurface::MakeOffscreen(64, 48);
  ASSERT_TRUE(surface != nullptr);
  EXPECT_EQ(surface->width(), 64);
  EXPECT_EQ(surface->height(), 48);

  EXPECT_EQ(pagx::PAGSurface::MakeOffscreen(0, 48), nullptr);
  EXPECT_EQ(pagx::PAGSurface::MakeOffscreen(64, -1), nullptr);
}

/**
 * Test case: end-to-end PAGScene::draw renders a SolidColor layer into a PAGSurface and the
 * resulting pixels match the channel-applied color.
 */
PAGX_TEST(PAGXRuntimeTest, PAGSceneDrawAndReadPixels) {
  auto doc = pagx::PAGXDocument::Make(8, 8);
  auto layer = doc->makeNode<pagx::Layer>("L");
  layer->width = 8;
  layer->height = 8;
  doc->layers.push_back(layer);

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size.width = 8;
  rect->size.height = 8;
  layer->contents.push_back(rect);

  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>("S");
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  fill->color = solid;
  layer->contents.push_back(fill);

  auto anim = doc->makeNode<pagx::Animation>("main");
  anim->duration = 60;
  anim->frameRate = 60;
  doc->animations.push_back(anim);
  auto* obj = doc->makeNode<pagx::AnimationObject>();
  obj->target = "S";
  anim->objects.push_back(obj);
  auto* prop = doc->makeNode<pagx::TypedChannel<pagx::Color>>();
  prop->name = "color";
  pagx::Color red{1.0f, 0.0f, 0.0f, 1.0f, pagx::ColorSpace::SRGB};
  prop->keyframes.push_back({0, red, pagx::KeyframeInterpolationType::Hold, {}, {}});
  obj->channels.push_back(prop);

  auto file = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(file != nullptr);

  auto timeline = file->getDefaultTimeline();
  ASSERT_TRUE(timeline != nullptr);
  timeline->apply(1.0f);

  auto surface = pagx::PAGSurface::MakeOffscreen(8, 8);
  ASSERT_TRUE(surface != nullptr);

  ASSERT_TRUE(file->draw(surface));
  EXPECT_TRUE(Baseline::Compare(surface, "PAGXRuntimeTest/PAGSceneDrawAndReadPixels"));
}

/**
 * Test case: advancing a top-level alpha animation produces distinct rendered frames over time.
 */
PAGX_TEST(PAGXRuntimeTest, AdvanceRendersDistinctFrames) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>("L");
  layer->width = 200;
  layer->height = 200;
  doc->layers.push_back(layer);

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size.width = 200;
  rect->size.height = 200;
  layer->contents.push_back(rect);

  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1.0f, 0.0f, 0.0f, 1.0f};
  fill->color = solid;
  layer->contents.push_back(fill);

  auto anim = doc->makeNode<pagx::Animation>("fade");
  anim->duration = 60;
  anim->frameRate = 60;
  doc->animations.push_back(anim);
  auto* obj = doc->makeNode<pagx::AnimationObject>();
  obj->target = "L";
  anim->objects.push_back(obj);
  auto* alphaProp = doc->makeNode<pagx::TypedChannel<float>>();
  alphaProp->name = "alpha";
  alphaProp->keyframes.push_back({0, 0.0f, pagx::KeyframeInterpolationType::Linear, {}, {}});
  alphaProp->keyframes.push_back({60, 1.0f, pagx::KeyframeInterpolationType::Linear, {}, {}});
  obj->channels.push_back(alphaProp);

  auto file = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(file != nullptr);
  auto timeline = file->getDefaultTimeline();
  ASSERT_TRUE(timeline != nullptr);
  timeline->play();

  auto surface = pagx::PAGSurface::MakeOffscreen(200, 200);
  ASSERT_TRUE(surface != nullptr);

  // Frame 0: t=0, alpha=0.
  timeline->apply(1.0f);
  ASSERT_TRUE(file->draw(surface));
  EXPECT_TRUE(Baseline::Compare(surface, "PAGXRuntimeTest/AdvanceRendersDistinctFrames_0"));

  // Frame 1: advance 0.5s -> alpha=0.5.
  timeline->advanceAndApply(500'000);
  ASSERT_TRUE(file->draw(surface));
  EXPECT_TRUE(Baseline::Compare(surface, "PAGXRuntimeTest/AdvanceRendersDistinctFrames_1"));

  // Frame 2: advance another 0.5s -> alpha=1.0.
  timeline->advanceAndApply(500'000);
  ASSERT_TRUE(file->draw(surface));
  EXPECT_TRUE(Baseline::Compare(surface, "PAGXRuntimeTest/AdvanceRendersDistinctFrames_2"));
}

/**
 * Test case: PAGScene::draw() with autoClear=false overlays the scene content onto existing
 * surface pixels instead of clearing the surface first.
 */
PAGX_TEST(PAGXRuntimeTest, PAGSceneDrawAutoClearOverlay) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>("L");
  layer->width = 100;
  layer->height = 100;
  doc->layers.push_back(layer);

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size.width = 100;
  rect->size.height = 100;
  layer->contents.push_back(rect);

  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>("S");
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  fill->color = solid;
  layer->contents.push_back(fill);

  auto anim = doc->makeNode<pagx::Animation>("main");
  anim->duration = 60;
  anim->frameRate = 60;
  doc->animations.push_back(anim);
  auto* obj = doc->makeNode<pagx::AnimationObject>();
  obj->target = "S";
  anim->objects.push_back(obj);
  auto* prop = doc->makeNode<pagx::TypedChannel<pagx::Color>>();
  prop->name = "color";
  pagx::Color red{1.0f, 0.0f, 0.0f, 1.0f, pagx::ColorSpace::SRGB};
  prop->keyframes.push_back({0, red, pagx::KeyframeInterpolationType::Hold, {}, {}});
  obj->channels.push_back(prop);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);
  auto timeline = scene->getDefaultTimeline();
  ASSERT_TRUE(timeline != nullptr);
  timeline->apply(1.0f);

  auto surface = pagx::PAGSurface::MakeOffscreen(100, 100);
  ASSERT_TRUE(surface != nullptr);

  // Draw with autoClear=true (default): red square.
  ASSERT_TRUE(scene->draw(surface));
  EXPECT_TRUE(Baseline::Compare(surface, "PAGXRuntimeTest/PAGSceneDrawAutoClearOverlay_red"));

  // Change animation to green and draw with autoClear=true: green replaces red.
  pagx::Color green{0.0f, 1.0f, 0.0f, 1.0f, pagx::ColorSpace::SRGB};
  prop->keyframes[0].value = green;
  timeline->apply(1.0f);
  ASSERT_TRUE(scene->draw(surface, true));
  EXPECT_TRUE(Baseline::Compare(surface, "PAGXRuntimeTest/PAGSceneDrawAutoClearOverlay_green"));

  // Reset to red, draw with autoClear=false: red is composited (SrcOver) on top of green.
  prop->keyframes[0].value = red;
  timeline->apply(1.0f);
  ASSERT_TRUE(scene->draw(surface, false));
  EXPECT_TRUE(Baseline::Compare(surface, "PAGXRuntimeTest/PAGSceneDrawAutoClearOverlay_overlay"));
}

/**
 * Test case: PAGSurface::MakeFrom(BackendTexture) creates a surface that can render PAGScene
 * content and produce a correct screenshot.
 */
PAGX_TEST(PAGXRuntimeTest, PAGSurfaceFromBackendTexture) {
  const int width = 100;
  const int height = 100;

  tgfx::GLTextureInfo textureInfo = {};
  CreateGLTexture(context, width, height, &textureInfo);
  auto backendTexture = ToBackendTexture(textureInfo, width, height);

  auto surface = pagx::PAGSurface::MakeFrom(backendTexture, pag::ImageOrigin::TopLeft);
  ASSERT_TRUE(surface != nullptr);
  EXPECT_EQ(surface->width(), width);
  EXPECT_EQ(surface->height(), height);

  device->unlock();

  auto doc = pagx::PAGXDocument::Make(width, height);
  auto layer = doc->makeNode<pagx::Layer>("L");
  layer->width = width;
  layer->height = height;
  doc->layers.push_back(layer);

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size.width = width;
  rect->size.height = height;
  layer->contents.push_back(rect);

  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>("S");
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  fill->color = solid;
  layer->contents.push_back(fill);

  auto anim = doc->makeNode<pagx::Animation>("main");
  anim->duration = 60;
  anim->frameRate = 60;
  doc->animations.push_back(anim);
  auto* obj = doc->makeNode<pagx::AnimationObject>();
  obj->target = "S";
  anim->objects.push_back(obj);
  auto* prop = doc->makeNode<pagx::TypedChannel<pagx::Color>>();
  prop->name = "color";
  pagx::Color blue{0.0f, 0.0f, 1.0f, 1.0f, pagx::ColorSpace::SRGB};
  prop->keyframes.push_back({0, blue, pagx::KeyframeInterpolationType::Hold, {}, {}});
  obj->channels.push_back(prop);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);
  auto timeline = scene->getDefaultTimeline();
  ASSERT_TRUE(timeline != nullptr);
  timeline->apply(1.0f);

  ASSERT_TRUE(scene->draw(surface));
  EXPECT_TRUE(Baseline::Compare(surface, "PAGXRuntimeTest/PAGSurfaceFromBackendTexture"));

  context = device->lockContext();
  glDeleteTextures(1, &textureInfo.id);
}

/**
 * Test case: PAGSurface::MakeFrom(BackendRenderTarget) creates a surface that can render PAGScene
 * content and produce a correct screenshot.
 */
PAGX_TEST(PAGXRuntimeTest, PAGSurfaceFromBackendRenderTarget) {
  const int width = 100;
  const int height = 100;

  tgfx::GLTextureInfo textureInfo = {};
  CreateGLTexture(context, width, height, &textureInfo);

  GLuint fbo = 0;
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureInfo.id, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  pag::GLFrameBufferInfo fbInfo = {};
  fbInfo.id = fbo;
  fbInfo.format = GL_RGBA8;
  pag::BackendRenderTarget backendRT(fbInfo, width, height);

  auto surface = pagx::PAGSurface::MakeFrom(backendRT, pag::ImageOrigin::TopLeft);
  ASSERT_TRUE(surface != nullptr);
  EXPECT_EQ(surface->width(), width);
  EXPECT_EQ(surface->height(), height);

  device->unlock();

  auto doc = pagx::PAGXDocument::Make(width, height);
  auto layer = doc->makeNode<pagx::Layer>("L");
  layer->width = width;
  layer->height = height;
  doc->layers.push_back(layer);

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size.width = width;
  rect->size.height = height;
  layer->contents.push_back(rect);

  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>("S");
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  fill->color = solid;
  layer->contents.push_back(fill);

  auto anim = doc->makeNode<pagx::Animation>("main");
  anim->duration = 60;
  anim->frameRate = 60;
  doc->animations.push_back(anim);
  auto* obj = doc->makeNode<pagx::AnimationObject>();
  obj->target = "S";
  anim->objects.push_back(obj);
  auto* prop = doc->makeNode<pagx::TypedChannel<pagx::Color>>();
  prop->name = "color";
  pagx::Color green{0.0f, 1.0f, 0.0f, 1.0f, pagx::ColorSpace::SRGB};
  prop->keyframes.push_back({0, green, pagx::KeyframeInterpolationType::Hold, {}, {}});
  obj->channels.push_back(prop);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);
  auto timeline = scene->getDefaultTimeline();
  ASSERT_TRUE(timeline != nullptr);
  timeline->apply(1.0f);

  ASSERT_TRUE(scene->draw(surface));
  EXPECT_TRUE(Baseline::Compare(surface, "PAGXRuntimeTest/PAGSurfaceFromBackendRenderTarget"));

  context = device->lockContext();
  glDeleteFramebuffers(1, &fbo);
  glDeleteTextures(1, &textureInfo.id);
}

}  // namespace pag
