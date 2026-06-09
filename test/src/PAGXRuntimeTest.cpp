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

#include "pagx/nodes/Animation.h"
#include "pagx/nodes/AnimationObject.h"
#include "pagx/nodes/Channel.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/PAGScene.h"
#include "pagx/PAGSurface.h"
#include "pagx/PAGTimeline.h"
#include "pagx/PAGXDocument.h"
#include "tgfx/core/Bitmap.h"
#include "tgfx/core/Pixmap.h"
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

  tgfx::Bitmap frame(100, 100, false, false);
  tgfx::Pixmap framePixmap(frame);

  // Draw with autoClear=true (default): red square.
  ASSERT_TRUE(scene->draw(surface));
  ASSERT_TRUE(surface->readPixels(framePixmap.writablePixels(), framePixmap.rowBytes()));
  EXPECT_TRUE(Baseline::Compare(framePixmap, "PAGXRuntimeTest/PAGSceneDrawAutoClearOverlay_red"));

  // Change animation to green and draw with autoClear=true: green replaces red.
  pagx::Color green{0.0f, 1.0f, 0.0f, 1.0f, pagx::ColorSpace::SRGB};
  prop->keyframes[0].value = green;
  timeline->apply(1.0f);
  ASSERT_TRUE(scene->draw(surface, true));
  ASSERT_TRUE(surface->readPixels(framePixmap.writablePixels(), framePixmap.rowBytes()));
  EXPECT_TRUE(
      Baseline::Compare(framePixmap, "PAGXRuntimeTest/PAGSceneDrawAutoClearOverlay_green"));

  // Reset to red, draw with autoClear=false: red is composited (SrcOver) on top of green.
  // Since both are fully opaque, red completely covers green.
  prop->keyframes[0].value = red;
  timeline->apply(1.0f);
  ASSERT_TRUE(scene->draw(surface, false));
  ASSERT_TRUE(surface->readPixels(framePixmap.writablePixels(), framePixmap.rowBytes()));
  EXPECT_TRUE(
      Baseline::Compare(framePixmap, "PAGXRuntimeTest/PAGSceneDrawAutoClearOverlay_overlay"));
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
  tgfx::Bitmap frame(width, height, false, false);
  tgfx::Pixmap framePixmap(frame);
  ASSERT_TRUE(surface->readPixels(framePixmap.writablePixels(), framePixmap.rowBytes()));
  EXPECT_TRUE(Baseline::Compare(framePixmap, "PAGXRuntimeTest/PAGSurfaceFromBackendTexture"));

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

  // Create a texture + FBO for the render target.
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
  tgfx::Bitmap frame2(width, height, false, false);
  tgfx::Pixmap framePixmap2(frame2);
  ASSERT_TRUE(surface->readPixels(framePixmap2.writablePixels(), framePixmap2.rowBytes()));
  EXPECT_TRUE(
      Baseline::Compare(framePixmap2, "PAGXRuntimeTest/PAGSurfaceFromBackendRenderTarget"));

  context = device->lockContext();
  glDeleteFramebuffers(1, &fbo);
  glDeleteTextures(1, &textureInfo.id);
}

}  // namespace pag
