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

#include <algorithm>
#include <optional>
#include "pagx/PAGAnimation.h"
#include "pagx/PAGScene.h"
#include "pagx/PAGSurface.h"
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXImporter.h"
#include "pagx/nodes/Animation.h"
#include "pagx/nodes/AnimationObject.h"
#include "pagx/nodes/AnimationTimeline.h"
#include "pagx/nodes/Channel.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/runtime/Drawable.h"
#include "pagx/runtime/KeyframeEvaluator.h"
#include "pagx/runtime/KeyframeEvaluatorImpl.h"
#include "pagx/runtime/MixUtils.h"
#include "pagx/tgfx.h"
#include "renderer/LayerBuilder.h"
#include "tgfx/core/Bitmap.h"
#include "tgfx/core/Data.h"
#include "tgfx/core/Pixmap.h"
#include "tgfx/gpu/Context.h"
#include "utils/Baseline.h"
#include "utils/TestUtils.h"

#ifdef TGFX_USE_OPENGL
#if defined(PAG_USE_SWIFTSHADER) || defined(__EMSCRIPTEN__)
#include <GLES3/gl3.h>
#else
#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION
#endif
#include <OpenGL/gl3.h>
#endif
#endif  // TGFX_USE_OPENGL

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
PAGX_TEST(PAGXRuntimeTest, PAGSceneRejectsInvalidOutputView) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  doc->targetLayerId = "missing";
  doc->outputBounds = pagx::Rect::MakeXYWH(0, 0, 10, 10);
  EXPECT_EQ(pagx::PAGScene::Make(doc), nullptr);

  auto incomplete = pagx::PAGXDocument::Make(100, 100);
  incomplete->targetLayerId = "missing";
  EXPECT_EQ(pagx::PAGScene::Make(incomplete), nullptr);
}

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

  auto ids = file->getAnimationIds();
  ASSERT_EQ(ids.size(), 2u);
  EXPECT_EQ(ids[0], "main");
  EXPECT_EQ(ids[1], "hint");

  auto t1 = file->getAnimation("main");
  auto t1Again = file->getAnimation("main");
  ASSERT_TRUE(t1 != nullptr);
  EXPECT_EQ(t1.get(), t1Again.get());
  EXPECT_EQ(t1->getId(), "main");
  EXPECT_EQ(t1->duration(), 1'000'000);

  auto t2 = file->getAnimation("hint");
  ASSERT_TRUE(t2 != nullptr);
  EXPECT_NE(t1.get(), t2.get());

  EXPECT_EQ(file->getAnimation("missing"), nullptr);

  auto def = std::static_pointer_cast<pagx::PAGAnimation>(file->getDefaultTimeline());
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
 * Test case: wrapping a surface while its device is locked preserves ownership and supports Record.
 */
PAGX_TEST(PAGXRuntimeTest, PAGSurfaceWrapsMatchingDeviceWhileLocked) {
  auto device = Devices::MakeDefault();
  ASSERT_TRUE(device != nullptr);
  std::weak_ptr<tgfx::Device> weakDevice = device;
  std::weak_ptr<tgfx::Surface> weakSurface;
  std::shared_ptr<pagx::PAGSurface> wrapped;
  {
    DeviceLockScope scope(device);
    ASSERT_TRUE(static_cast<bool>(scope));
    auto surface = tgfx::Surface::Make(scope.context(), 64, 48);
    ASSERT_TRUE(surface != nullptr);
    weakSurface = surface;
    auto drawable = pagx::Drawable::MakeFrom(device, surface);
    ASSERT_TRUE(drawable != nullptr);
    EXPECT_EQ(drawable->getDevice(), device);
    EXPECT_EQ(drawable->getSurface(scope.context()), surface);
    wrapped = pagx::MakeFrom(device, surface);
    ASSERT_TRUE(wrapped != nullptr);
    EXPECT_EQ(wrapped->width(), 64);
    EXPECT_EQ(wrapped->height(), 48);
    auto scene = pagx::PAGScene::Make(pagx::PAGXDocument::Make(64, 48));
    ASSERT_TRUE(scene != nullptr);
    auto recording = pagx::Record(scope.context(), scene, wrapped);
    ASSERT_TRUE(recording != nullptr);
    scope.context()->submit(std::move(recording));
  }
  device.reset();
  EXPECT_FALSE(weakDevice.expired());
  EXPECT_FALSE(weakSurface.expired());
  wrapped.reset();
  EXPECT_TRUE(weakSurface.expired());
}

/**
 * Test case: both public and internal wrapping factories reject null arguments.
 */
PAGX_TEST(PAGXRuntimeTest, PAGSurfaceWrapRejectsNullInputs) {
  EXPECT_EQ(pagx::MakeFrom(nullptr, nullptr), nullptr);
  EXPECT_EQ(pagx::Drawable::MakeFrom(nullptr, nullptr), nullptr);
  auto device = Devices::MakeDefault();
  ASSERT_TRUE(device != nullptr);
  EXPECT_EQ(pagx::MakeFrom(device, nullptr), nullptr);
  EXPECT_EQ(pagx::Drawable::MakeFrom(device, nullptr), nullptr);
  DeviceLockScope scope(device);
  ASSERT_TRUE(static_cast<bool>(scope));
  auto surface = tgfx::Surface::Make(scope.context(), 64, 48);
  ASSERT_TRUE(surface != nullptr);
  EXPECT_EQ(pagx::MakeFrom(nullptr, surface), nullptr);
  EXPECT_EQ(pagx::Drawable::MakeFrom(nullptr, surface), nullptr);
}

// Excluded from the webgl lane via PAG_WEBGL_ONLY: Devices::MakeDefault() returns the same
// singleton on Emscripten's single global GL context, so a "mismatched device" scenario cannot
// be constructed there. Native GL builds keep this case. (WebGPU builds do not compile it:
// TGFX_USE_OPENGL is undefined there.)
#if defined(TGFX_USE_OPENGL) && !defined(PAG_WEBGL_ONLY)
/**
 * Test case: a surface created on one GL device is rejected when paired with another device.
 */
PAGX_TEST(PAGXRuntimeTest, PAGSurfaceWrapRejectsMismatchedDevice) {
  auto ownerDevice = Devices::MakeDefault();
  auto otherDevice = Devices::MakeDefault();
  ASSERT_TRUE(ownerDevice != nullptr);
  ASSERT_TRUE(otherDevice != nullptr);
  ASSERT_NE(ownerDevice.get(), otherDevice.get());
  DeviceLockScope scope(ownerDevice);
  ASSERT_TRUE(static_cast<bool>(scope));
  auto surface = tgfx::Surface::Make(scope.context(), 64, 48);
  ASSERT_TRUE(surface != nullptr);
  EXPECT_EQ(pagx::MakeFrom(otherDevice, surface), nullptr);
  EXPECT_EQ(pagx::Drawable::MakeFrom(otherDevice, surface), nullptr);
  EXPECT_TRUE(pagx::MakeFrom(ownerDevice, surface) != nullptr);
}
#endif

/**
 * Test case: advancing a top-level alpha animation produces distinct rendered frames over time.
 */
PAGX_PIXEL_TEST(PAGXRuntimeTest, AdvanceRendersDistinctFrames) {
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
  auto timeline = std::static_pointer_cast<pagx::PAGAnimation>(file->getDefaultTimeline());
  ASSERT_TRUE(timeline != nullptr);

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
PAGX_PIXEL_TEST(PAGXRuntimeTest, PAGSceneDrawAutoClearOverlay) {
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
  obj->target = "L";
  anim->objects.push_back(obj);
  auto* alphaProp = doc->makeNode<pagx::TypedChannel<float>>();
  alphaProp->name = "alpha";
  alphaProp->keyframes.push_back({0, 0.5f, pagx::KeyframeInterpolationType::Hold, {}, {}});
  obj->channels.push_back(alphaProp);

  auto* colorObj = doc->makeNode<pagx::AnimationObject>();
  colorObj->target = "S";
  anim->objects.push_back(colorObj);
  auto* colorProp = doc->makeNode<pagx::TypedChannel<pagx::Color>>();
  colorProp->name = "color";
  pagx::Color red{1.0f, 0.0f, 0.0f, 1.0f, pagx::ColorSpace::SRGB};
  colorProp->keyframes.push_back({0, red, pagx::KeyframeInterpolationType::Hold, {}, {}});
  colorObj->channels.push_back(colorProp);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);
  auto timeline = std::static_pointer_cast<pagx::PAGAnimation>(scene->getDefaultTimeline());
  ASSERT_TRUE(timeline != nullptr);
  timeline->apply(1.0f);

  auto surface = pagx::PAGSurface::MakeOffscreen(100, 100);
  ASSERT_TRUE(surface != nullptr);

  // Draw with autoClear=true: semi-transparent red square on cleared (black) surface.
  ASSERT_TRUE(scene->draw(surface));
  EXPECT_TRUE(Baseline::Compare(surface, "PAGXRuntimeTest/PAGSceneDrawAutoClearOverlay_red"));

  // Change animation to semi-transparent green and draw with autoClear=true: green replaces red.
  pagx::Color green{0.0f, 1.0f, 0.0f, 1.0f, pagx::ColorSpace::SRGB};
  colorProp->keyframes[0].value = green;
  timeline->apply(1.0f);
  ASSERT_TRUE(scene->draw(surface, true));
  EXPECT_TRUE(Baseline::Compare(surface, "PAGXRuntimeTest/PAGSceneDrawAutoClearOverlay_green"));

  // Reset to semi-transparent red, draw with autoClear=false: red is composited (SrcOver) on top
  // of the green, producing a blended color (yellowish) that differs from both pure red and green.
  colorProp->keyframes[0].value = red;
  timeline->apply(1.0f);
  ASSERT_TRUE(scene->draw(surface, false));
  EXPECT_TRUE(Baseline::Compare(surface, "PAGXRuntimeTest/PAGSceneDrawAutoClearOverlay_overlay"));
}

/**
 * Test case: PAGSurface::MakeFrom(BackendTexture) creates a surface that can render PAGScene
 * content and produce a correct screenshot.
 *
 * GL-specific: this exercises the GL-backed BackendTexture path (external GLTextureInfo + share-
 * context adoption). No equivalent public API exists yet on non-GL backends. The following two
 * test cases are gated on TGFX_USE_OPENGL until Metal-friendly BackendTexture APIs are
 * introduced.
 */
#ifdef TGFX_USE_OPENGL
PAGX_PIXEL_TEST(PAGXRuntimeTest, PAGSurfaceFromBackendTexture) {
  const int width = 100;
  const int height = 100;

  // Hold the device lock across texture + surface creation (GL needs a current context there),
  // then release before PAGScene::draw() below, which acquires the device on its own. The device
  // is created once and reused by the cleanup block below: on the GL backend every
  // Devices::MakeDefault() builds a new, non-sharing GL context, so deleting the texture under a
  // different device would be a silent no-op on the creating context.
  tgfx::GLTextureInfo textureInfo = {};
  std::shared_ptr<pagx::PAGSurface> surface = nullptr;
  auto device = pag::Devices::MakeDefault();
  ASSERT_TRUE(device != nullptr);
  {
    pag::DeviceLockScope deviceLock(device);
    ASSERT_TRUE(static_cast<bool>(deviceLock));
    CreateGLTexture(deviceLock.context(), width, height, &textureInfo);
    auto backendTexture = ToBackendTexture(textureInfo, width, height);
    surface = pagx::PAGSurface::MakeFrom(backendTexture, pag::ImageOrigin::TopLeft);
  }
  ASSERT_TRUE(surface != nullptr);
  EXPECT_EQ(surface->width(), width);
  EXPECT_EQ(surface->height(), height);

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
  auto timeline = std::static_pointer_cast<pagx::PAGAnimation>(scene->getDefaultTimeline());
  ASSERT_TRUE(timeline != nullptr);
  timeline->apply(1.0f);

  ASSERT_TRUE(scene->draw(surface));
  EXPECT_TRUE(Baseline::Compare(surface, "PAGXRuntimeTest/PAGSurfaceFromBackendTexture"));

  {
    // Lock the same device that created the texture (see the comment at the top of this test).
    pag::DeviceLockScope lock(device);
    ASSERT_TRUE(static_cast<bool>(lock));
    glDeleteTextures(1, &textureInfo.id);
  }
}

/**
 * Test case: PAGSurface::MakeFrom(BackendRenderTarget) creates a surface that can render PAGScene
 * content and produce a correct screenshot.
 */
PAGX_PIXEL_TEST(PAGXRuntimeTest, PAGSurfaceFromBackendRenderTarget) {
  const int width = 100;
  const int height = 100;

  // Hold the device lock across GL object + surface creation (GL needs a current context there),
  // then release before PAGScene::draw() below, which acquires the device on its own. The device
  // is created once and reused by the cleanup block below: on the GL backend every
  // Devices::MakeDefault() builds a new, non-sharing GL context, so deleting the GL objects under
  // a different device would be a silent no-op on the creating context.
  tgfx::GLTextureInfo textureInfo = {};
  GLuint fbo = 0;
  std::shared_ptr<pagx::PAGSurface> surface = nullptr;
  auto device = pag::Devices::MakeDefault();
  ASSERT_TRUE(device != nullptr);
  {
    pag::DeviceLockScope deviceLock(device);
    ASSERT_TRUE(static_cast<bool>(deviceLock));
    CreateGLTexture(deviceLock.context(), width, height, &textureInfo);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureInfo.id, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    pag::GLFrameBufferInfo fbInfo = {};
    fbInfo.id = fbo;
    fbInfo.format = GL_RGBA8;
    pag::BackendRenderTarget backendRT(fbInfo, width, height);
    surface = pagx::PAGSurface::MakeFrom(backendRT, pag::ImageOrigin::TopLeft);
  }
  ASSERT_TRUE(surface != nullptr);
  EXPECT_EQ(surface->width(), width);
  EXPECT_EQ(surface->height(), height);

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
  auto timeline = std::static_pointer_cast<pagx::PAGAnimation>(scene->getDefaultTimeline());
  ASSERT_TRUE(timeline != nullptr);
  timeline->apply(1.0f);

  ASSERT_TRUE(scene->draw(surface));
  EXPECT_TRUE(Baseline::Compare(surface, "PAGXRuntimeTest/PAGSurfaceFromBackendRenderTarget"));

  {
    // Lock the same device that created the GL objects (see the comment at the top of this test).
    pag::DeviceLockScope lock(device);
    ASSERT_TRUE(static_cast<bool>(lock));
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &textureInfo.id);
  }
}
#endif  // TGFX_USE_OPENGL

/**
 * Test case: EvaluateKeyframeSequence treats KeyframeInterpolationType::None identically to Hold,
 * holding the left keyframe value across the segment instead of interpolating linearly toward the
 * right keyframe. This locks the None == Hold semantics so it cannot silently drift back to linear.
 */
PAGX_TEST(PAGXRuntimeTest, NoneInterpolationHoldsValue) {
  std::vector<pagx::Keyframe<float>> keyframes;
  keyframes.push_back({0, 10.0f, pagx::KeyframeInterpolationType::None, {}, {}});
  keyframes.push_back({60, 20.0f, pagx::KeyframeInterpolationType::None, {}, {}});

  EXPECT_FLOAT_EQ(pagx::EvaluateKeyframeSequence(keyframes, 0.0), 10.0f);
  EXPECT_FLOAT_EQ(pagx::EvaluateKeyframeSequence(keyframes, 30.0), 10.0f);
  EXPECT_FLOAT_EQ(pagx::EvaluateKeyframeSequence(keyframes, 59.0), 10.0f);
  EXPECT_FLOAT_EQ(pagx::EvaluateKeyframeSequence(keyframes, 60.0), 20.0f);

  std::vector<pagx::Keyframe<float>> linear;
  linear.push_back({0, 10.0f, pagx::KeyframeInterpolationType::Linear, {}, {}});
  linear.push_back({60, 20.0f, pagx::KeyframeInterpolationType::Linear, {}, {}});
  EXPECT_FLOAT_EQ(pagx::EvaluateKeyframeSequence(linear, 30.0), 15.0f);
}

/**
 * Test case: matrix interpolation has two spellings and the channel picks one. With
 * pivotInterpolation the transient translation is spelled from the target pose's fixed point, so a
 * rotation happens in place (SVG transform animations). Without it the decomposed translation
 * interpolates linearly, which is what CSS transforms expect.
 */
PAGX_TEST(PAGXRuntimeTest, MatrixInterpolationRespectsPivotFlag) {
  // rotate(90) about (50,50): fixed point is the shape centre.
  auto rotateAboutCenter = pagx::Matrix::Translate(50.0f, 50.0f) * pagx::Matrix::Rotate(90.0f) *
                           pagx::Matrix::Translate(-50.0f, -50.0f);
  std::vector<pagx::Keyframe<pagx::Matrix>> centerTrack;
  centerTrack.push_back(
      {0, pagx::Matrix::Identity(), pagx::KeyframeInterpolationType::Linear, {}, {}});
  centerTrack.push_back({60, rotateAboutCenter, pagx::KeyframeInterpolationType::Linear, {}, {}});

  auto pivotMid = pagx::EvaluateKeyframeSequence(centerTrack, 30.0, true);
  EXPECT_NEAR(pivotMid.a, 0.7071f, 1e-2f);
  EXPECT_NEAR(pivotMid.tx, 50.0f, 0.5f);
  EXPECT_NEAR(pivotMid.ty, -20.71f, 0.5f);

  auto linearMid = pagx::EvaluateKeyframeSequence(centerTrack, 30.0, false);
  EXPECT_NEAR(linearMid.a, 0.7071f, 1e-2f);
  EXPECT_NEAR(linearMid.tx, 50.0f, 0.5f);
  EXPECT_NEAR(linearMid.ty, 0.0f, 0.5f);

  // none -> translate(50,50) rotate(90): the CSS spelling moves the pose to (25,25) halfway, while
  // the fixed-point spelling (that pose's fixed point is (0,50)) swings it along an arc.
  auto cssTarget = pagx::Matrix::Translate(50.0f, 50.0f) * pagx::Matrix::Rotate(90.0f);
  std::vector<pagx::Keyframe<pagx::Matrix>> cssTrack;
  cssTrack.push_back(
      {0, pagx::Matrix::Identity(), pagx::KeyframeInterpolationType::Linear, {}, {}});
  cssTrack.push_back({60, cssTarget, pagx::KeyframeInterpolationType::Linear, {}, {}});

  auto cssMid = pagx::EvaluateKeyframeSequence(cssTrack, 30.0, false);
  EXPECT_NEAR(cssMid.tx, 25.0f, 0.5f);
  EXPECT_NEAR(cssMid.ty, 25.0f, 0.5f);

  auto pivotCssMid = pagx::EvaluateKeyframeSequence(cssTrack, 30.0, true);
  EXPECT_NEAR(pivotCssMid.tx, 35.36f, 0.5f);
  EXPECT_NEAR(pivotCssMid.ty, 14.64f, 0.5f);
}

/**
 * Test case: a perspective distance mixes its projection coefficient -1/d instead of the distance,
 * so a distance going from 1000 to 200 passes through 333.33 at the midpoint instead of 600. This is
 * how a browser interpolates the perspective() transform function, and it is the only channel that
 * blends this way.
 */
PAGX_TEST(PAGXRuntimeTest, PerspectiveKeyframesBlendTheProjectionCoefficient) {
  std::vector<pagx::Keyframe<float>> keyframes;
  keyframes.push_back({0, 1000.0f, pagx::KeyframeInterpolationType::Linear, {}, {}});
  keyframes.push_back({60, 200.0f, pagx::KeyframeInterpolationType::Linear, {}, {}});

  EXPECT_NEAR(pagx::EvaluatePerspectiveSequence(keyframes, 30.0), 333.3333f, 0.001f);
  // The same keys read by a channel that does not carry a projection blend the distances.
  EXPECT_FLOAT_EQ(pagx::EvaluateKeyframeSequence(keyframes, 30.0), 600.0f);

  // Bezier easing shapes the same fraction, so an identity curve reproduces the linear result.
  std::vector<pagx::Keyframe<float>> eased;
  eased.push_back({0, 1000.0f, pagx::KeyframeInterpolationType::Bezier, {0.5f, 0.5f}, {}});
  eased.push_back({60, 200.0f, pagx::KeyframeInterpolationType::Linear, {}, {0.5f, 0.5f}});
  EXPECT_NEAR(pagx::EvaluatePerspectiveSequence(eased, 30.0), 333.3333f, 0.001f);

  // The projection mix is shared with the component writer, so a weighted write agrees with a
  // keyframe blend at the same fraction.
  EXPECT_NEAR(pagx::MixPerspectiveDistance(1000.0f, 200.0f, 0.5f), 333.3333f, 0.001f);
  EXPECT_FLOAT_EQ(pagx::MixPerspectiveDistance(500.0f, 500.0f, 0.25f), 500.0f);
  // An easing that overshoots past the no-projection end has no coefficient to continue into, so it
  // stops there instead of returning a negative, behind-the-camera distance.
  EXPECT_FLOAT_EQ(pagx::MixPerspectiveDistance(0.0f, 200.0f, -1.0f), 0.0f);
}

/**
 * Test case: perspective 0 means no projection, which is the coefficient 0, i.e. the infinite
 * distance end of the coefficient space. A segment touching it stays continuous there instead of
 * snapping to a near camera: blending 0 with 200 halfway yields 400, and one frame in yields a
 * distance far beyond the endpoint.
 */
PAGX_TEST(PAGXRuntimeTest, PerspectiveZeroIsTheInfiniteDistanceEnd) {
  std::vector<pagx::Keyframe<float>> keyframes;
  keyframes.push_back({0, 0.0f, pagx::KeyframeInterpolationType::Linear, {}, {}});
  keyframes.push_back({60, 200.0f, pagx::KeyframeInterpolationType::Linear, {}, {}});

  EXPECT_NEAR(pagx::EvaluatePerspectiveSequence(keyframes, 30.0), 400.0f, 0.001f);
  // Frame 1 of 60 holds a sixtieth of the target coefficient, i.e. d = 12000: still almost no
  // projection, where a linear distance blend would already sit at d = 3.33.
  EXPECT_NEAR(pagx::EvaluatePerspectiveSequence(keyframes, 1.0), 12000.0f, 1.0f);
  EXPECT_FLOAT_EQ(pagx::EvaluatePerspectiveSequence(keyframes, 0.0), 0.0f);
  EXPECT_FLOAT_EQ(pagx::EvaluatePerspectiveSequence(keyframes, 60.0), 200.0f);
}

/**
 * Test case: a negative distance is not a projection, so a keyframe holding one reads as "no
 * projection" at an endpoint and in a hold segment too, not only inside a blended segment. Without
 * that, the renderer would reject the raw negative value and freeze the pose for that frame.
 */
PAGX_TEST(PAGXRuntimeTest, PerspectiveFoldsANegativeKeyIntoNoProjection) {
  std::vector<pagx::Keyframe<float>> leading;
  leading.push_back({0, -45.0f, pagx::KeyframeInterpolationType::Linear, {}, {}});
  leading.push_back({60, 45.0f, pagx::KeyframeInterpolationType::Linear, {}, {}});
  // The first frame reads as no projection, continuous with the segment that follows rather than a
  // value the renderer drops.
  EXPECT_FLOAT_EQ(pagx::EvaluatePerspectiveSequence(leading, 0.0), 0.0f);
  EXPECT_NEAR(pagx::EvaluatePerspectiveSequence(leading, 1.0), 2700.0f, 1.0f);
  EXPECT_FLOAT_EQ(pagx::EvaluatePerspectiveSequence(leading, 60.0), 45.0f);

  std::vector<pagx::Keyframe<float>> trailing;
  trailing.push_back({0, 45.0f, pagx::KeyframeInterpolationType::Linear, {}, {}});
  trailing.push_back({60, -45.0f, pagx::KeyframeInterpolationType::Linear, {}, {}});
  EXPECT_FLOAT_EQ(pagx::EvaluatePerspectiveSequence(trailing, 60.0), 0.0f);

  // A hold segment returns the held value through the same normalization.
  std::vector<pagx::Keyframe<float>> hold;
  hold.push_back({0, -45.0f, pagx::KeyframeInterpolationType::Hold, {}, {}});
  hold.push_back({60, 45.0f, pagx::KeyframeInterpolationType::Hold, {}, {}});
  EXPECT_FLOAT_EQ(pagx::EvaluatePerspectiveSequence(hold, 30.0), 0.0f);
}

/**
 * Test case: a channel named "perspective" blends its keyframes through the projection coefficient
 * end to end, while the other channels of the same object keep blending their values.
 */
PAGX_TEST(PAGXRuntimeTest, PerspectiveChannelBlendsThroughEvaluation) {
  std::string xml =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<pagx width=\"320\" height=\"200\">\n"
      "  <Layer id=\"box\" x=\"120\" y=\"60\" width=\"80\" height=\"80\" rotationY=\"45\">\n"
      "    <Rectangle left=\"0\" right=\"0\" top=\"0\" bottom=\"0\"/>\n"
      "  </Layer>\n"
      "  <Animations>\n"
      "    <Animation id=\"pull\" duration=\"60\" frameRate=\"60\" loop=\"once\">\n"
      "      <Object target=\"box\">\n"
      "        <Channel name=\"perspective\" type=\"float\">\n"
      "          <Key time=\"0\" value=\"1000\" interpolation=\"linear\"/>\n"
      "          <Key time=\"60\" value=\"200\"/>\n"
      "        </Channel>\n"
      "        <Channel name=\"alpha\" type=\"float\">\n"
      "          <Key time=\"0\" value=\"0\" interpolation=\"linear\"/>\n"
      "          <Key time=\"60\" value=\"1\"/>\n"
      "        </Channel>\n"
      "      </Object>\n"
      "    </Animation>\n"
      "  </Animations>\n"
      "</pagx>\n";
  auto doc = pagx::PAGXImporter::FromXML(xml);
  ASSERT_NE(doc, nullptr);
  ASSERT_EQ(doc->animations.size(), 1u);
  auto* animation = static_cast<pagx::Animation*>(doc->animations.front());
  ASSERT_EQ(animation->objects.size(), 1u);
  pagx::Channel* perspective = nullptr;
  pagx::Channel* alpha = nullptr;
  for (auto* channel : animation->objects.front()->channels) {
    if (channel->name == "perspective") {
      perspective = channel;
    } else if (channel->name == "alpha") {
      alpha = channel;
    }
  }
  ASSERT_NE(perspective, nullptr);
  ASSERT_NE(alpha, nullptr);
  EXPECT_NEAR(std::get<float>(perspective->evaluateAt(pagx::Frame{30})), 333.3333f, 0.001f);
  EXPECT_FLOAT_EQ(std::get<float>(alpha->evaluateAt(pagx::Frame{30})), 0.5f);
}

/**
 * Test case: a weighted write to the perspective channel — the path a timeline crossfade and a
 * weighted data bind take — mixes the projection coefficient like a keyframe blend does. A write
 * that starts at no perspective lands on twice the target distance halfway, and one that starts at
 * 1000 lands on 333.33. A negative distance is rejected and leaves the pose untouched.
 */
PAGX_TEST(PAGXRuntimeTest, PerspectiveComponentWriteMixesTheProjectionCoefficient) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* layer = doc->makeNode<pagx::Layer>("L");
  doc->layers.push_back(layer);
  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_NE(scene, nullptr);
  auto* binding = scene->mutableBinding();
  pagx::KeyValue value;

  // Half of the target coefficient from no perspective: 1 / (0.5 / 200) = 400.
  ASSERT_TRUE(binding->apply(layer, "perspective", pagx::KeyValue{200.0f}, 0.5f));
  ASSERT_TRUE(binding->read(layer, "perspective", &value));
  EXPECT_NEAR(std::get<float>(value), 400.0f, 0.01f);

  // A full write replaces the distance, then half of the new coefficient reads back as 333.33.
  ASSERT_TRUE(binding->apply(layer, "perspective", pagx::KeyValue{1000.0f}, 1.0f));
  ASSERT_TRUE(binding->apply(layer, "perspective", pagx::KeyValue{200.0f}, 0.5f));
  ASSERT_TRUE(binding->read(layer, "perspective", &value));
  EXPECT_NEAR(std::get<float>(value), 333.3333f, 0.01f);

  // A negative distance is not a projection, so the write is rejected and the distance stays.
  ASSERT_TRUE(binding->read(layer, "perspective", &value));
  auto unchanged = std::get<float>(value);
  EXPECT_FALSE(binding->apply(layer, "perspective", pagx::KeyValue{-1.0f}, 1.0f));
  ASSERT_TRUE(binding->read(layer, "perspective", &value));
  EXPECT_FLOAT_EQ(std::get<float>(value), unchanged);
}

/**
 * Test case: EvaluateKeyframeSegment evaluates a single keyframe pair at the boundary and mid
 * point for each interpolation mode. Hold/None returns the left value, Linear returns exact lerp,
 * and Bezier returns the eased lerp (verified against a known control curve).
 */
PAGX_TEST(PAGXRuntimeTest, EvaluateKeyframeSegmentBasic) {
  pagx::Keyframe<float> left{0, 10.0f};
  pagx::Keyframe<float> right{60, 40.0f};

  // Midpoint t = 0.5 between frame 0 and frame 60.
  double mid = 30.0;
  double progress = (mid - static_cast<double>(left.time)) /
                    (static_cast<double>(right.time) - static_cast<double>(left.time));

  auto eval = [&]() {
    return std::get<float>(pagx::EvaluateKeyframeSegment(
        left.value, right.value, progress, left.interpolation, &left.bezierOut, &right.bezierIn));
  };

  // Hold / None: left value throughout the segment.
  left.interpolation = pagx::KeyframeInterpolationType::Hold;
  EXPECT_FLOAT_EQ(eval(), 10.0f);
  left.interpolation = pagx::KeyframeInterpolationType::None;
  EXPECT_FLOAT_EQ(eval(), 10.0f);

  // Linear: exact lerp — midpoint yields 25.0.
  left.interpolation = pagx::KeyframeInterpolationType::Linear;
  EXPECT_FLOAT_EQ(eval(), 25.0f);

  // Bezier with identity curve (control points on diagonal) behaves like linear.
  left.interpolation = pagx::KeyframeInterpolationType::Bezier;
  left.bezierOut = {0.5f, 0.5f};
  right.bezierIn = {0.5f, 0.5f};
  EXPECT_FLOAT_EQ(eval(), 25.0f);
}

/**
 * Test case: EvaluateKeyframeSegment handles Hold semantics for discrete value types — bool, int,
 * string, and ImageRef — always returning the left value regardless of interp or rawT.
 */
PAGX_TEST(PAGXRuntimeTest, EvaluateKeyframeSegmentDiscreteTypes) {
  pagx::KeyValue leftBool = true;
  pagx::KeyValue rightBool = false;
  pagx::KeyValue result = pagx::EvaluateKeyframeSegment(leftBool, rightBool, 0.5,
                                                        pagx::KeyframeInterpolationType::Linear);
  EXPECT_EQ(std::get<bool>(result), true);

  pagx::KeyValue leftInt = 42;
  pagx::KeyValue rightInt = 99;
  result = pagx::EvaluateKeyframeSegment(leftInt, rightInt, 0.5,
                                         pagx::KeyframeInterpolationType::Linear);
  EXPECT_EQ(std::get<int>(result), 42);

  pagx::KeyValue leftStr = std::string("hello");
  pagx::KeyValue rightStr = std::string("world");
  result = pagx::EvaluateKeyframeSegment(leftStr, rightStr, 0.5,
                                         pagx::KeyframeInterpolationType::Linear);
  EXPECT_EQ(std::get<std::string>(result), "hello");

  pagx::KeyValue leftImg = pagx::ImageRef{"img_a"};
  pagx::KeyValue rightImg = pagx::ImageRef{"img_b"};
  result = pagx::EvaluateKeyframeSegment(leftImg, rightImg, 0.5,
                                         pagx::KeyframeInterpolationType::Linear);
  EXPECT_EQ(std::get<pagx::ImageRef>(result).id, "img_a");
}

/**
 * Test case: EvaluateKeyframeSegment interpolates Color between two SRGB values at midpoint.
 */
PAGX_TEST(PAGXRuntimeTest, EvaluateKeyframeSegmentColor) {
  pagx::KeyValue leftColor = pagx::Color{0.2f, 0.0f, 0.0f, 1.0f, pagx::ColorSpace::SRGB};
  pagx::KeyValue rightColor = pagx::Color{1.0f, 0.0f, 0.0f, 1.0f, pagx::ColorSpace::SRGB};
  pagx::KeyValue result = pagx::EvaluateKeyframeSegment(leftColor, rightColor, 0.5,
                                                        pagx::KeyframeInterpolationType::Linear);
  auto color = std::get<pagx::Color>(result);
  EXPECT_GT(color.red, 0.2f);
  EXPECT_LT(color.red, 1.0f);
}

/**
 * Test case: EvaluateKeyframeSegment interpolates Matrix via decomposition at midpoint.
 */
PAGX_TEST(PAGXRuntimeTest, EvaluateKeyframeSegmentMatrix) {
  pagx::KeyValue leftMat = pagx::Matrix{2.0f, 0.0f, 0.0f, 2.0f, 0.0f, 0.0f};
  pagx::KeyValue rightMat = pagx::Matrix{4.0f, 0.0f, 0.0f, 4.0f, 0.0f, 0.0f};
  pagx::KeyValue result = pagx::EvaluateKeyframeSegment(leftMat, rightMat, 0.5,
                                                        pagx::KeyframeInterpolationType::Linear);
  auto mat = std::get<pagx::Matrix>(result);
  EXPECT_GT(mat.a, 2.0f);
  EXPECT_LT(mat.a, 4.0f);
}

/**
 * Test case: EvaluateKeyframeSegment returns the left value when the two KeyValue types differ.
 */
PAGX_TEST(PAGXRuntimeTest, EvaluateKeyframeSegmentTypeMismatch) {
  pagx::KeyValue leftFloat = 10.0f;
  pagx::KeyValue leftInt = 42;
  pagx::KeyValue result = pagx::EvaluateKeyframeSegment(leftFloat, leftInt, 0.5,
                                                        pagx::KeyframeInterpolationType::Linear);
  EXPECT_EQ(result, leftFloat);
}

/**
 * Test case: AnimationTimeline.evaluationOffset shifts the evaluation point of an animation
 * so its content is delayed relative to the timeline's own clock. The animation uses two Hold
 * keyframes (green at frame 0, red at frame 10). With evaluationOffset = 5, the timeline
 * clock reaches frame 10 at 167ms but the shifted evaluation is at frame 5 (still green); by
 * 250ms evaluation passes frame 10 (red). This confirms the offset affects content timing without
 * altering the timeline's own advance() clock.
 */
PAGX_PIXEL_TEST(PAGXRuntimeTest, AnimationTimelineEvaluationOffset) {
  auto doc = pagx::PAGXDocument::Make(100, 100);

  auto innerComp = doc->makeNode<pagx::Composition>("inner");
  innerComp->width = 100;
  innerComp->height = 100;

  auto childLayer = doc->makeNode<pagx::Layer>("child-layer");
  childLayer->width = 100;
  childLayer->height = 100;
  innerComp->layers.push_back(childLayer);

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size.width = 100;
  rect->size.height = 100;
  childLayer->contents.push_back(rect);

  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>("color");
  solid->color = {0, 0, 0, 1};
  fill->color = solid;
  childLayer->contents.push_back(fill);

  auto anim = doc->makeNode<pagx::Animation>("shift");
  anim->duration = 20;
  anim->frameRate = 60;
  anim->loop = pagx::LoopMode::Once;
  innerComp->animations.push_back(anim);

  auto* obj = doc->makeNode<pagx::AnimationObject>();
  obj->target = "color";
  anim->objects.push_back(obj);

  auto* colorProp = doc->makeNode<pagx::TypedChannel<pagx::Color>>();
  colorProp->name = "color";
  pagx::Color green{0, 1, 0, 1, pagx::ColorSpace::SRGB};
  pagx::Color red{1, 0, 0, 1, pagx::ColorSpace::SRGB};
  colorProp->keyframes.push_back({0, green, pagx::KeyframeInterpolationType::Hold, {}, {}});
  colorProp->keyframes.push_back({10, red, pagx::KeyframeInterpolationType::Hold, {}, {}});
  obj->channels.push_back(colorProp);

  auto rootLayer = doc->makeNode<pagx::Layer>("slot");
  rootLayer->width = 100;
  rootLayer->height = 100;
  rootLayer->composition = innerComp;

  auto driver = std::make_unique<pagx::AnimationTimeline>();
  driver->animationId = "shift";
  driver->evaluationOffset = 5;
  rootLayer->timelines.push_back(std::move(driver));

  doc->layers.push_back(rootLayer);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);

  auto surface = pagx::PAGSurface::MakeOffscreen(100, 100);
  ASSERT_TRUE(surface != nullptr);

  // At frame 10 (167ms), evalFrame = max(0, 10-5) = 5, still in the green (frame 0) Hold.
  // Without the offset, frame 10 would already be at the red (frame 10) keyframe.
  scene->advanceAndApply(167000);
  ASSERT_TRUE(scene->draw(surface));
  EXPECT_TRUE(
      Baseline::Compare(surface, "PAGXRuntimeTest/AnimationTimelineEvaluationOffset_green"));

  // At frame 15 (250ms), evalFrame = max(0, 15-5) = 10, reaching the red keyframe.
  scene->advanceAndApply(83000);
  ASSERT_TRUE(scene->draw(surface));
  EXPECT_TRUE(Baseline::Compare(surface, "PAGXRuntimeTest/AnimationTimelineEvaluationOffset_red"));
}

/**
 * Test case: a negative AnimationTimeline.evaluationOffset skips the animation ahead so its
 * content starts partway in, matching PAG's signed compositionStartTime (negative = trim from the
 * front). The animation is green at frame 0 and red at frame 10 (Hold). With
 * evaluationOffset = -10, the evaluation frame is currentFrame + 10, so even at timeline
 * frame 0 the content is already at the red keyframe.
 */
PAGX_PIXEL_TEST(PAGXRuntimeTest, AnimationTimelineNegativeEvaluationOffset) {
  auto doc = pagx::PAGXDocument::Make(100, 100);

  auto innerComp = doc->makeNode<pagx::Composition>("inner");
  innerComp->width = 100;
  innerComp->height = 100;

  auto childLayer = doc->makeNode<pagx::Layer>("child-layer");
  childLayer->width = 100;
  childLayer->height = 100;
  innerComp->layers.push_back(childLayer);

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size.width = 100;
  rect->size.height = 100;
  childLayer->contents.push_back(rect);

  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>("color");
  solid->color = {0, 0, 0, 1};
  fill->color = solid;
  childLayer->contents.push_back(fill);

  auto anim = doc->makeNode<pagx::Animation>("shift");
  anim->duration = 20;
  anim->frameRate = 60;
  anim->loop = pagx::LoopMode::Once;
  innerComp->animations.push_back(anim);

  auto* obj = doc->makeNode<pagx::AnimationObject>();
  obj->target = "color";
  anim->objects.push_back(obj);

  auto* colorProp = doc->makeNode<pagx::TypedChannel<pagx::Color>>();
  colorProp->name = "color";
  pagx::Color green{0, 1, 0, 1, pagx::ColorSpace::SRGB};
  pagx::Color red{1, 0, 0, 1, pagx::ColorSpace::SRGB};
  colorProp->keyframes.push_back({0, green, pagx::KeyframeInterpolationType::Hold, {}, {}});
  colorProp->keyframes.push_back({10, red, pagx::KeyframeInterpolationType::Hold, {}, {}});
  obj->channels.push_back(colorProp);

  auto rootLayer = doc->makeNode<pagx::Layer>("slot");
  rootLayer->width = 100;
  rootLayer->height = 100;
  rootLayer->composition = innerComp;

  auto driver = std::make_unique<pagx::AnimationTimeline>();
  driver->animationId = "shift";
  driver->evaluationOffset = -10;
  rootLayer->timelines.push_back(std::move(driver));

  doc->layers.push_back(rootLayer);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);

  auto surface = pagx::PAGSurface::MakeOffscreen(100, 100);
  ASSERT_TRUE(surface != nullptr);

  // At frame 2 (33ms), evalFrame = max(0, 2-(-10)) = 12, already past the red (frame 10) keyframe.
  // Without the negative offset, frame 2 would still be green. The skip-ahead lands on red.
  scene->advanceAndApply(33000);
  ASSERT_TRUE(scene->draw(surface));
  EXPECT_TRUE(
      Baseline::Compare(surface, "PAGXRuntimeTest/AnimationTimelineNegativeEvaluationOffset_red"));
}

/**
 * Test case: visibility driven by a "visible" bool channel with Hold keyframes (0, false) ->
 * (30, true) -> (60, false). The target fills the frame red, so the three sampled instants
 * distinguish before-window (hidden), inside-window (visible red), and after-window (hidden).
 */
PAGX_PIXEL_TEST(PAGXRuntimeTest, AnimationVisibleChannelWindow) {
  auto doc = pagx::PAGXDocument::Make(100, 100);

  auto innerComp = doc->makeNode<pagx::Composition>("inner");
  innerComp->width = 100;
  innerComp->height = 100;

  auto childLayer = doc->makeNode<pagx::Layer>("color");
  childLayer->width = 100;
  childLayer->height = 100;
  innerComp->layers.push_back(childLayer);

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size.width = 100;
  rect->size.height = 100;
  childLayer->contents.push_back(rect);

  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1, 0, 0, 1};
  fill->color = solid;
  childLayer->contents.push_back(fill);

  auto anim = doc->makeNode<pagx::Animation>("window");
  anim->duration = 60;
  anim->frameRate = 60;
  anim->loop = pagx::LoopMode::Once;
  innerComp->animations.push_back(anim);

  auto* obj = doc->makeNode<pagx::AnimationObject>();
  obj->target = "color";
  anim->objects.push_back(obj);

  auto* visibleProp = doc->makeNode<pagx::TypedChannel<bool>>();
  visibleProp->name = "visible";
  visibleProp->keyframes.push_back({0, false, pagx::KeyframeInterpolationType::Hold, {}, {}});
  visibleProp->keyframes.push_back({30, true, pagx::KeyframeInterpolationType::Hold, {}, {}});
  visibleProp->keyframes.push_back({60, false, pagx::KeyframeInterpolationType::Hold, {}, {}});
  obj->channels.push_back(visibleProp);

  auto rootLayer = doc->makeNode<pagx::Layer>("slot");
  rootLayer->width = 100;
  rootLayer->height = 100;
  rootLayer->composition = innerComp;

  auto driver = std::make_unique<pagx::AnimationTimeline>();
  driver->animationId = "window";
  rootLayer->timelines.push_back(std::move(driver));

  doc->layers.push_back(rootLayer);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);

  auto surface = pagx::PAGSurface::MakeOffscreen(100, 100);
  ASSERT_TRUE(surface != nullptr);

  // At 0.25s the animation clock is before the window start (frame 30 = 0.5s): the target is hidden.
  scene->advanceAndApply(250000);
  ASSERT_TRUE(scene->draw(surface));
  EXPECT_TRUE(Baseline::Compare(surface, "PAGXRuntimeTest/AnimationVisibleChannelWindow_hidden"));

  // At 0.75s the clock is inside [0.5s, 1.0s): the target is visible (red).
  scene->advanceAndApply(500000);
  ASSERT_TRUE(scene->draw(surface));
  EXPECT_TRUE(Baseline::Compare(surface, "PAGXRuntimeTest/AnimationVisibleChannelWindow_visible"));

  // At 1.25s the clock is past the window end (frame 60 = 1.0s): the target is hidden again.
  scene->advanceAndApply(500000);
  ASSERT_TRUE(scene->draw(surface));
  EXPECT_TRUE(Baseline::Compare(surface, "PAGXRuntimeTest/AnimationVisibleChannelWindow_hidden"));
}

static bool ReadSurfacePixels(const std::shared_ptr<pagx::PAGSurface>& surface,
                              std::vector<uint8_t>* pixels) {
  tgfx::Bitmap bitmap(surface->width(), surface->height(), false, false);
  tgfx::Pixmap pixmap(bitmap);
  if (pixmap.isEmpty() || !surface->readPixels(pixmap.writablePixels(), pixmap.rowBytes())) {
    return false;
  }
  auto* bytes = static_cast<uint8_t*>(pixmap.writablePixels());
  pixels->assign(bytes, bytes + pixmap.rowBytes() * pixmap.height());
  return true;
}

// Builds a 100x100 document whose default output view is a 20x20 window over a red square, so a
// target-view render shows red in the top-left corner and nothing elsewhere.
static std::shared_ptr<pagx::PAGXDocument> MakeTargetViewDocument() {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto* layer = doc->makeNode<pagx::Layer>("target");
  layer->width = 100;
  layer->height = 100;
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->size = {10, 10};
  rect->position = {25, 25};
  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1.0f, 0.0f, 0.0f, 1.0f};
  fill->color = solid;
  layer->contents = {rect, fill};
  doc->layers.push_back(layer);
  doc->targetLayerId = "target";
  doc->outputBounds = pagx::Rect::MakeXYWH(20, 20, 20, 20);
  return doc;
}

// Returns the public handle registered for an authored layer node, or nullptr when the node has no
// runtime layer in this scene.
static std::shared_ptr<pagx::PAGLayer> LayerHandleFor(pagx::PAGScene* scene,
                                                      const pagx::Layer* node) {
  auto composition = scene != nullptr ? scene->rootComposition() : nullptr;
  auto* binding = composition != nullptr ? composition->binding.get() : nullptr;
  if (binding == nullptr || node == nullptr) {
    return nullptr;
  }
  auto runtimeLayer = binding->get<tgfx::Layer>(node);
  if (runtimeLayer == nullptr) {
    return nullptr;
  }
  auto it = scene->layerRegistry.find(runtimeLayer.get());
  return it != scene->layerRegistry.end() ? it->second->shared_from_this() : nullptr;
}

// Adds a layer whose only content is a 10x10 opaque rect centered at root (25,25), so the layer
// covers the same point as MakeTargetViewDocument() does.
static pagx::Layer* AddCenteredRectLayer(pagx::PAGXDocument* doc, const std::string& id) {
  auto* layer = doc->makeNode<pagx::Layer>(id);
  layer->width = 100;
  layer->height = 100;
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->size = {10, 10};
  rect->position = {25, 25};
  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1.0f, 1.0f, 1.0f, 1.0f};
  fill->color = solid;
  layer->contents = {rect, fill};
  return layer;
}

static bool ContainsLayer(const std::vector<std::shared_ptr<pagx::PAGLayer>>& layers,
                          const std::shared_ptr<pagx::PAGLayer>& layer) {
  return std::find(layers.begin(), layers.end(), layer) != layers.end();
}

/**
 * Test case: drawing a target-layer view repeatedly to the same surface must produce identical
 * frames. A PAGSurface keeps one Canvas for its lifetime, so the view transform applied by
 * PAGScene::draw() has to be scoped to the draw; otherwise every frame accumulates the translate /
 * scale and the content eventually leaves the output window.
 */
PAGX_TEST(PAGXRuntimeTest, RepeatedTargetViewDrawKeepsCanvasState) {
  auto doc = MakeTargetViewDocument();

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);
  auto surface = pagx::PAGSurface::MakeOffscreen(20, 20);
  ASSERT_TRUE(surface != nullptr);

  std::vector<uint8_t> firstFrame = {};
  EXPECT_TRUE(scene->hasContentChanged());
  ASSERT_TRUE(scene->draw(surface));
  EXPECT_FALSE(scene->hasContentChanged());
  ASSERT_TRUE(ReadSurfacePixels(surface, &firstFrame));
  EXPECT_EQ(firstFrame[3], 255u);

  std::vector<uint8_t> secondFrame = {};
  ASSERT_TRUE(scene->draw(surface));
  ASSERT_TRUE(ReadSurfacePixels(surface, &secondFrame));
  EXPECT_EQ(secondFrame, firstFrame);
  EXPECT_FALSE(scene->hasContentChanged());
}

PAGX_TEST(PAGXRuntimeTest, TargetViewClearedBoundsWithoutNotifyFailsDraw) {
  auto doc = MakeTargetViewDocument();
  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);
  auto surface = pagx::PAGSurface::MakeOffscreen(20, 20);
  ASSERT_TRUE(surface != nullptr);
  ASSERT_TRUE(scene->draw(surface));

  // outputBounds is public: clearing it without notifyChange() leaves the cached state Valid, so
  // draw() has to reject the frame instead of dereferencing the now-empty optional.
  doc->outputBounds = std::nullopt;
  EXPECT_FALSE(scene->draw(surface));
}

/**
 * Test case: surface-space queries must apply the same output-window shift as draw(), and hit
 * testing must follow the same transform. The red square is centered at root (25,25) and covers
 * (20,20)-(30,30); with an output window starting at (20,20) it appears at (0,0)-(10,10) on the
 * surface.
 */
PAGX_TEST(PAGXRuntimeTest, TargetViewHitTestOriginAndDisplayTransform) {
  auto doc = MakeTargetViewDocument();
  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);
  EXPECT_EQ(scene->getOutputViewStatus(), pagx::OutputViewStatus::Valid);
  auto* targetNode = doc->findNode<pagx::Layer>("target");
  ASSERT_NE(targetNode, nullptr);
  auto* binding = scene->rootComposition()->binding.get();
  ASSERT_NE(binding, nullptr);
  auto runtimeTarget = binding->get<tgfx::Layer>(targetNode);
  ASSERT_NE(runtimeTarget, nullptr);
  auto it = scene->layerRegistry.find(runtimeTarget.get());
  ASSERT_NE(it, scene->layerRegistry.end());
  auto target = it->second->shared_from_this();
  ASSERT_NE(target, nullptr);

  auto bounds = scene->getGlobalBounds(target);
  EXPECT_FLOAT_EQ(bounds.x, 0);
  EXPECT_FLOAT_EQ(bounds.y, 0);
  EXPECT_FLOAT_EQ(bounds.width, 10);
  EXPECT_FLOAT_EQ(bounds.height, 10);
  EXPECT_TRUE(target->hitTestPoint(5, 5));
  EXPECT_FALSE(target->hitTestPoint(15, 15));
  auto identityHits = scene->getLayersUnderPoint(5, 5);
  ASSERT_EQ(identityHits.size(), 1u);
  EXPECT_EQ(identityHits[0], target);
  // Local (20,20) maps to surface (0,0), so the root-to-surface matrix translates by -20.
  auto matrix = target->getGlobalMatrix();
  EXPECT_FLOAT_EQ(matrix.tx, -20);
  EXPECT_FLOAT_EQ(matrix.ty, -20);

  // The output-window shift composes with the display zoom and content offset exactly like draw().
  auto* options = scene->getDisplayOptions();
  options->setZoomScale(2);
  options->setContentOffset(10, 5);
  auto zoomed = scene->getGlobalBounds(target);
  EXPECT_FLOAT_EQ(zoomed.x, 10);
  EXPECT_FLOAT_EQ(zoomed.y, 5);
  EXPECT_FLOAT_EQ(zoomed.width, 20);
  EXPECT_FLOAT_EQ(zoomed.height, 20);
  EXPECT_TRUE(target->hitTestPoint(10, 5));
  EXPECT_FALSE(target->hitTestPoint(0, 0));
  auto zoomedHits = scene->getLayersUnderPoint(10, 5);
  ASSERT_EQ(zoomedHits.size(), 1u);
  EXPECT_EQ(zoomedHits[0], target);
  EXPECT_TRUE(scene->getLayersUnderPoint(0, 0).empty());
}

PAGX_TEST(PAGXRuntimeTest, TargetViewHitTestRestrictsSubtree) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto* outer = doc->makeNode<pagx::Layer>("outer");
  outer->width = 100;
  outer->height = 100;
  doc->layers.push_back(outer);
  auto* target = doc->makeNode<pagx::Layer>("target");
  target->width = 100;
  target->height = 100;
  outer->children.push_back(target);
  outer->children.push_back(AddCenteredRectLayer(doc.get(), "sibling"));
  target->children.push_back(AddCenteredRectLayer(doc.get(), "bottom"));
  target->children.push_back(AddCenteredRectLayer(doc.get(), "top"));
  doc->targetLayerId = "target";
  doc->outputBounds = pagx::Rect::MakeXYWH(20, 20, 20, 20);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_NE(scene, nullptr);
  auto top = LayerHandleFor(scene.get(), doc->findNode<pagx::Layer>("top"));
  auto bottom = LayerHandleFor(scene.get(), doc->findNode<pagx::Layer>("bottom"));
  auto targetHandle = LayerHandleFor(scene.get(), target);
  auto sibling = LayerHandleFor(scene.get(), doc->findNode<pagx::Layer>("sibling"));
  auto outerHandle = LayerHandleFor(scene.get(), outer);
  auto root = scene->rootComposition();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(bottom, nullptr);
  ASSERT_NE(targetHandle, nullptr);
  ASSERT_NE(sibling, nullptr);
  ASSERT_NE(outerHandle, nullptr);
  ASSERT_NE(root, nullptr);

  auto hits = scene->getLayersUnderPoint(5, 5);
  ASSERT_EQ(hits.size(), 3u);
  EXPECT_EQ(hits[0], top);
  EXPECT_EQ(hits[1], bottom);
  EXPECT_EQ(hits[2], targetHandle);
  EXPECT_FALSE(ContainsLayer(hits, sibling));
  EXPECT_FALSE(ContainsLayer(hits, outerHandle));

  EXPECT_TRUE(top->hitTestPoint(5, 5));
  EXPECT_TRUE(targetHandle->hitTestPoint(5, 5));
  // Layers the target view does not render are unreachable through either API.
  EXPECT_FALSE(sibling->hitTestPoint(5, 5));
  EXPECT_FALSE(outerHandle->hitTestPoint(5, 5));
  EXPECT_FALSE(root->hitTestPoint(5, 5));
}

PAGX_TEST(PAGXRuntimeTest, TargetViewHitTestClipsOutputBounds) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto* target = doc->makeNode<pagx::Layer>("target");
  target->width = 100;
  target->height = 100;
  auto* rect = doc->makeNode<pagx::Rectangle>();
  // The geometry covers the whole canvas, so only the output window can reject a point.
  rect->size = {200, 200};
  rect->position = {50, 50};
  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1.0f, 0.0f, 0.0f, 1.0f};
  fill->color = solid;
  target->contents = {rect, fill};
  doc->layers.push_back(target);
  doc->targetLayerId = "target";
  doc->outputBounds = pagx::Rect::MakeXYWH(20, 20, 20, 20);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_NE(scene, nullptr);
  auto targetHandle = LayerHandleFor(scene.get(), target);
  ASSERT_NE(targetHandle, nullptr);

  EXPECT_TRUE(targetHandle->hitTestPoint(0, 0));
  EXPECT_TRUE(targetHandle->hitTestPoint(19.5f, 19.5f));
  ASSERT_EQ(scene->getLayersUnderPoint(0, 0).size(), 1u);
  // The window bounds are top-left inclusive and bottom-right exclusive.
  EXPECT_FALSE(targetHandle->hitTestPoint(-0.5f, 5));
  EXPECT_FALSE(targetHandle->hitTestPoint(5, -0.5f));
  EXPECT_FALSE(targetHandle->hitTestPoint(20, 5));
  EXPECT_FALSE(targetHandle->hitTestPoint(5, 20));
  EXPECT_TRUE(scene->getLayersUnderPoint(-0.5f, 5).empty());
  EXPECT_TRUE(scene->getLayersUnderPoint(20, 5).empty());

  auto* options = scene->getDisplayOptions();
  options->setZoomScale(2);
  options->setContentOffset(10, 6);
  EXPECT_TRUE(targetHandle->hitTestPoint(20, 16));
  EXPECT_FALSE(targetHandle->hitTestPoint(9.5f, 16));
  EXPECT_FALSE(targetHandle->hitTestPoint(50, 16));
  EXPECT_FALSE(targetHandle->hitTestPoint(20, 5.5f));
  EXPECT_FALSE(targetHandle->hitTestPoint(20, 46));
  ASSERT_EQ(scene->getLayersUnderPoint(20, 16).size(), 1u);
  EXPECT_TRUE(scene->getLayersUnderPoint(50, 16).empty());
}

PAGX_TEST(PAGXRuntimeTest, TargetViewHitTestRejectsInvalidView) {
  auto doc = MakeTargetViewDocument();
  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_NE(scene, nullptr);
  auto target = LayerHandleFor(scene.get(), doc->findNode<pagx::Layer>("target"));
  ASSERT_NE(target, nullptr);

  // An incomplete declaration makes draw() and both hit test entry points reject the scene.
  doc->targetLayerId.clear();
  doc->notifyChange({doc.get()}, true);
  EXPECT_EQ(scene->getOutputViewStatus(), pagx::OutputViewStatus::Invalid);
  EXPECT_TRUE(scene->getLayersUnderPoint(5, 5).empty());
  target = LayerHandleFor(scene.get(), doc->findNode<pagx::Layer>("target"));
  ASSERT_NE(target, nullptr);
  EXPECT_FALSE(target->hitTestPoint(5, 5));

  doc->targetLayerId = "target";
  doc->notifyChange({doc.get()}, true);
  EXPECT_EQ(scene->getOutputViewStatus(), pagx::OutputViewStatus::Valid);
  target = LayerHandleFor(scene.get(), doc->findNode<pagx::Layer>("target"));
  ASSERT_NE(target, nullptr);
  EXPECT_TRUE(target->hitTestPoint(5, 5));

  // Clearing the public bounds field without notifyChange() leaves the cached state Valid.
  doc->outputBounds = std::nullopt;
  EXPECT_TRUE(scene->getLayersUnderPoint(5, 5).empty());
  EXPECT_FALSE(target->hitTestPoint(5, 5));

  doc->outputBounds = pagx::Rect::MakeXYWH(20, 20, 20, 20);
  doc->notifyChange({doc.get()}, true);
  target = LayerHandleFor(scene.get(), doc->findNode<pagx::Layer>("target"));
  ASSERT_NE(target, nullptr);
  scene->getDisplayOptions()->setZoomScale(0);
  EXPECT_TRUE(scene->getLayersUnderPoint(5, 5).empty());
  EXPECT_FALSE(target->hitTestPoint(5, 5));
}

PAGX_TEST(PAGXRuntimeTest, TargetViewHitTestAppliesLayerConstraints) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto* target = doc->makeNode<pagx::Layer>("target");
  target->width = 100;
  target->height = 100;
  doc->layers.push_back(target);
  auto* inner = AddCenteredRectLayer(doc.get(), "inner");
  target->children.push_back(inner);
  doc->targetLayerId = "target";
  doc->outputBounds = pagx::Rect::MakeXYWH(20, 20, 20, 20);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_NE(scene, nullptr);
  auto targetHandle = LayerHandleFor(scene.get(), target);
  auto innerHandle = LayerHandleFor(scene.get(), inner);
  ASSERT_NE(targetHandle, nullptr);
  ASSERT_NE(innerHandle, nullptr);
  EXPECT_TRUE(innerHandle->hitTestPoint(5, 5));

  inner->visible = false;
  doc->notifyChange({doc.get()}, true);
  innerHandle = LayerHandleFor(scene.get(), inner);
  ASSERT_NE(innerHandle, nullptr);
  EXPECT_TRUE(scene->getLayersUnderPoint(5, 5).empty());
  EXPECT_FALSE(innerHandle->hitTestPoint(5, 5));
  inner->visible = true;

  target->visible = false;
  doc->notifyChange({doc.get()}, true);
  targetHandle = LayerHandleFor(scene.get(), target);
  ASSERT_NE(targetHandle, nullptr);
  EXPECT_TRUE(scene->getLayersUnderPoint(5, 5).empty());
  EXPECT_FALSE(targetHandle->hitTestPoint(5, 5));
  target->visible = true;
  doc->notifyChange({doc.get()}, true);

  auto* mask = AddCenteredRectLayer(doc.get(), "mask");
  mask->visible = false;
  mask->x = 40;
  mask->y = 40;
  doc->layers.push_back(mask);
  inner->mask = mask;
  inner->maskType = pagx::MaskType::Alpha;
  doc->notifyChange({doc.get()}, true);
  innerHandle = LayerHandleFor(scene.get(), inner);
  ASSERT_NE(innerHandle, nullptr);
  EXPECT_TRUE(scene->getLayersUnderPoint(5, 5).empty());
  EXPECT_FALSE(innerHandle->hitTestPoint(5, 5));
  mask->x = 0;
  mask->y = 0;
  doc->notifyChange({doc.get()}, true);

  inner->scrollRect = pagx::Rect::MakeXYWH(30, 30, 10, 10);
  inner->hasScrollRect = true;
  doc->notifyChange({doc.get()}, true);
  innerHandle = LayerHandleFor(scene.get(), inner);
  ASSERT_NE(innerHandle, nullptr);
  EXPECT_TRUE(scene->getLayersUnderPoint(5, 5).empty());
  EXPECT_FALSE(innerHandle->hitTestPoint(5, 5));
  inner->scrollRect = pagx::Rect::MakeXYWH(0, 0, 30, 30);

  doc->notifyChange({doc.get()}, true);
  innerHandle = LayerHandleFor(scene.get(), inner);
  ASSERT_NE(innerHandle, nullptr);
  ASSERT_EQ(scene->getLayersUnderPoint(5, 5).size(), 2u);
  EXPECT_TRUE(innerHandle->hitTestPoint(5, 5));
}

PAGX_TEST(PAGXRuntimeTest, TargetViewHitTestIgnoresOuterConstraints) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto* outer = doc->makeNode<pagx::Layer>("outer");
  outer->width = 100;
  outer->height = 100;
  doc->layers.push_back(outer);
  auto* target = AddCenteredRectLayer(doc.get(), "target");
  outer->children.push_back(target);
  doc->targetLayerId = "target";
  doc->outputBounds = pagx::Rect::MakeXYWH(20, 20, 20, 20);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_NE(scene, nullptr);
  auto targetHandle = LayerHandleFor(scene.get(), target);
  ASSERT_NE(targetHandle, nullptr);

  // An ancestor that the target view does not render must not reject a hit inside the target.
  outer->visible = false;
  doc->notifyChange({outer}, false);
  targetHandle = LayerHandleFor(scene.get(), target);
  ASSERT_NE(targetHandle, nullptr);
  EXPECT_TRUE(targetHandle->hitTestPoint(5, 5));
  ASSERT_EQ(scene->getLayersUnderPoint(5, 5).size(), 1u);

  // scrollRect also translates the subtree, so the content moves to (15,15)-(25,25) and root
  // (20,20), the top-left corner of the output window, still lands on it. A scroll check applied
  // to the ancestor would reject (20,20) because its scrolled local point is (15,15).
  outer->visible = true;
  outer->scrollRect = pagx::Rect::MakeXYWH(5, 5, 10, 10);
  outer->hasScrollRect = true;
  doc->notifyChange({doc.get()}, true);
  targetHandle = LayerHandleFor(scene.get(), target);
  ASSERT_NE(targetHandle, nullptr);
  EXPECT_TRUE(targetHandle->hitTestPoint(0, 0));
  ASSERT_EQ(scene->getLayersUnderPoint(0, 0).size(), 1u);

  // An ancestor still contributes its transform: shifting the target by (10,10) moves the hit.
  outer->scrollRect = {};
  outer->hasScrollRect = false;
  outer->x = 10;
  outer->y = 10;
  doc->notifyChange({outer}, false);
  targetHandle = LayerHandleFor(scene.get(), target);
  ASSERT_NE(targetHandle, nullptr);
  EXPECT_TRUE(targetHandle->hitTestPoint(15, 15));
  EXPECT_FALSE(targetHandle->hitTestPoint(5, 5));
  ASSERT_EQ(scene->getLayersUnderPoint(15, 15).size(), 1u);
  EXPECT_TRUE(scene->getLayersUnderPoint(5, 5).empty());
}

PAGX_TEST(PAGXRuntimeTest, TargetViewHitTestAllowsMaskTarget) {
  auto doc = MakeTargetViewDocument();
  auto* target = doc->findNode<pagx::Layer>("target");
  ASSERT_NE(target, nullptr);
  auto* masked = doc->makeNode<pagx::Layer>("masked");
  masked->width = 100;
  masked->height = 100;
  masked->mask = target;
  masked->maskType = pagx::MaskType::Alpha;
  doc->layers.push_back(masked);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_NE(scene, nullptr);
  auto targetHandle = LayerHandleFor(scene.get(), target);
  auto maskedHandle = LayerHandleFor(scene.get(), masked);
  ASSERT_NE(targetHandle, nullptr);
  ASSERT_NE(maskedHandle, nullptr);

  EXPECT_TRUE(targetHandle->hitTestPoint(5, 5));
  ASSERT_EQ(scene->getLayersUnderPoint(5, 5).size(), 1u);
  // Querying must not reparent the target or detach it from its mask owner.
  EXPECT_EQ(masked->mask, target);
  EXPECT_FALSE(maskedHandle->hitTestPoint(5, 5));
}

PAGX_TEST(PAGXRuntimeTest, TargetViewHitTestPreservesQueryState) {
  auto doc = MakeTargetViewDocument();
  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_NE(scene, nullptr);
  auto targetHandle = LayerHandleFor(scene.get(), doc->findNode<pagx::Layer>("target"));
  ASSERT_NE(targetHandle, nullptr);

  auto before = scene->getLayersUnderPoint(5, 5);
  ASSERT_EQ(before.size(), 1u);
  auto surface = pagx::PAGSurface::MakeOffscreen(20, 20);
  ASSERT_NE(surface, nullptr);
  ASSERT_TRUE(scene->draw(surface));
  EXPECT_FALSE(scene->hasContentChanged());

  auto after = scene->getLayersUnderPoint(5, 5);
  ASSERT_EQ(after.size(), before.size());
  EXPECT_EQ(after[0], before[0]);
  EXPECT_TRUE(targetHandle->hitTestPoint(5, 5));
  EXPECT_FALSE(scene->hasContentChanged());
}

PAGX_TEST(PAGXRuntimeTest, TargetViewHitTestShapePrecision) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto* target = doc->makeNode<pagx::Layer>("target");
  target->width = 100;
  target->height = 100;
  auto* ellipse = doc->makeNode<pagx::Ellipse>();
  ellipse->size = {20, 20};
  ellipse->position = {25, 25};
  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1.0f, 0.0f, 0.0f, 1.0f};
  fill->color = solid;
  target->contents = {ellipse, fill};
  doc->layers.push_back(target);
  doc->targetLayerId = "target";
  doc->outputBounds = pagx::Rect::MakeXYWH(0, 0, 50, 50);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_NE(scene, nullptr);
  auto targetHandle = LayerHandleFor(scene.get(), target);
  ASSERT_NE(targetHandle, nullptr);

  // (16,16) lies inside the 20x20 ellipse bounds but outside the circle itself.
  EXPECT_TRUE(targetHandle->hitTestPoint(16, 16, false));
  EXPECT_FALSE(targetHandle->hitTestPoint(16, 16, true));
  ASSERT_EQ(scene->getLayersUnderPoint(16, 16).size(), 1u);
}

/**
 * Test case: a target that another layer references as its mask must still be drawn. TGFX skips
 * child layers owned as masks, which used to drop the whole output when the target was such a mask.
 */
PAGX_TEST(PAGXRuntimeTest, TargetViewDrawsTargetReferencedAsMask) {
  auto doc = MakeTargetViewDocument();
  auto* target = doc->findNode<pagx::Layer>("target");
  ASSERT_NE(target, nullptr);
  auto* masked = doc->makeNode<pagx::Layer>("masked");
  masked->width = 100;
  masked->height = 100;
  masked->mask = target;
  doc->layers.push_back(masked);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);
  auto surface = pagx::PAGSurface::MakeOffscreen(20, 20);
  ASSERT_TRUE(surface != nullptr);
  ASSERT_TRUE(scene->draw(surface));
  std::vector<uint8_t> pixels = {};
  ASSERT_TRUE(ReadSurfacePixels(surface, &pixels));
  EXPECT_EQ(pixels[3], 255u);
  // The source document relationship is never changed by runtime parenting.
  EXPECT_EQ(masked->mask, target);
}

PAGX_TEST(PAGXRuntimeTest, TargetViewMaskOwnershipKeepsBoundsStable) {
  auto doc = pagx::PAGXImporter::FromXML(
      "<pagx width=\"100\" height=\"100\" targetLayer=\"@target\" outputBounds=\"0,0,100,100\">"
      "<Layer id=\"outer\"><Layer id=\"target\"><Rectangle position=\"25,25\" size=\"10,10\"/>"
      "<Fill color=\"#FF0000\"/></Layer></Layer>"
      "<Layer id=\"owner\" mask=\"@target\"><Rectangle position=\"50,50\" size=\"100,100\"/>"
      "<Fill color=\"#0000FF\"/></Layer></pagx>");
  ASSERT_NE(doc, nullptr);
  auto scene = pagx::PAGScene::Make(doc);
  auto otherScene = pagx::PAGScene::Make(doc);
  ASSERT_NE(scene, nullptr);
  ASSERT_NE(otherScene, nullptr);
  auto root = scene->rootComposition();
  auto outer = root->getChildren()[0];
  auto owner = root->getChildren()[1];
  auto target = outer->getChildren()[0];
  auto rootBounds = root->getBounds();
  auto outerBounds = outer->getBounds();
  auto ownerBounds = owner->getBounds();
  auto targetBounds = target->getBounds();
  auto otherBounds = otherScene->rootComposition()->getBounds();
  EXPECT_EQ(rootBounds, pagx::Rect::MakeXYWH(20, 20, 10, 10));
  EXPECT_TRUE(outerBounds.isEmpty());
  auto runtimeTarget = target->runtimeLayer;
  auto* originalParent = runtimeTarget->parent();
  ASSERT_EQ(owner->runtimeLayer->mask(), runtimeTarget);
  auto surface = pagx::PAGSurface::MakeOffscreen(200, 200);
  ASSERT_NE(surface, nullptr);
  for (int scale = 1; scale <= 3; scale++) {
    scene->getDisplayOptions()->setZoomScale(static_cast<float>(scale));
    scene->getDisplayOptions()->setContentOffset(10, 5);
    auto globalRoot = scene->getGlobalBounds(root);
    auto globalOuter = scene->getGlobalBounds(outer);
    auto globalOwner = scene->getGlobalBounds(owner);
    auto globalTarget = scene->getGlobalBounds(target);
    ASSERT_TRUE(scene->draw(surface));
    EXPECT_FALSE(scene->hasContentChanged());
    EXPECT_EQ(root->getBounds(), rootBounds);
    EXPECT_EQ(outer->getBounds(), outerBounds);
    EXPECT_EQ(owner->getBounds(), ownerBounds);
    EXPECT_EQ(target->getBounds(), targetBounds);
    EXPECT_EQ(scene->getGlobalBounds(root), globalRoot);
    EXPECT_EQ(scene->getGlobalBounds(outer), globalOuter);
    EXPECT_EQ(scene->getGlobalBounds(owner), globalOwner);
    EXPECT_EQ(scene->getGlobalBounds(target), globalTarget);
    EXPECT_EQ(runtimeTarget->parent(), originalParent);
    EXPECT_EQ(owner->runtimeLayer->mask(), runtimeTarget);
    EXPECT_EQ(otherScene->rootComposition()->getBounds(), otherBounds);
  }
  doc->clearOutputView();
  doc->notifyChange({doc.get()}, false);
  EXPECT_EQ(scene->getOutputViewStatus(), pagx::OutputViewStatus::None);
  EXPECT_EQ(scene->rootComposition()->getBounds(), rootBounds);
}

PAGX_TEST(PAGXRuntimeTest, OutputViewEditsInvalidateRenderedContent) {
  auto doc = MakeTargetViewDocument();
  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_NE(scene, nullptr);
  auto surface = pagx::PAGSurface::MakeOffscreen(20, 20);
  ASSERT_NE(surface, nullptr);
  ASSERT_TRUE(scene->draw(surface));
  EXPECT_FALSE(scene->hasContentChanged());

  auto* target = doc->findNode<pagx::Layer>("target");
  ASSERT_NE(target, nullptr);
  target->alpha = 0;
  doc->notifyChange({target}, false);
  EXPECT_TRUE(scene->hasContentChanged());
  ASSERT_TRUE(scene->draw(surface));
  EXPECT_FALSE(scene->hasContentChanged());
  std::vector<uint8_t> pixels = {};
  ASSERT_TRUE(ReadSurfacePixels(surface, &pixels));
  EXPECT_EQ(pixels[3], 0u);

  doc->outputBounds = pagx::Rect::MakeXYWH(10, 10, 20, 20);
  doc->notifyChange({doc.get()}, false);
  EXPECT_TRUE(scene->hasContentChanged());
  ASSERT_TRUE(scene->draw(surface));
  EXPECT_FALSE(scene->hasContentChanged());
}

PAGX_TEST(PAGXRuntimeTest, OutputViewDisplayChangesInvalidateRenderedContent) {
  auto scene = pagx::PAGScene::Make(MakeTargetViewDocument());
  ASSERT_NE(scene, nullptr);
  auto surface = pagx::PAGSurface::MakeOffscreen(20, 20);
  ASSERT_NE(surface, nullptr);
  ASSERT_TRUE(scene->draw(surface));
  auto* options = scene->getDisplayOptions();

  options->setZoomScale(2);
  EXPECT_TRUE(scene->hasContentChanged());
  ASSERT_TRUE(scene->draw(surface));
  EXPECT_FALSE(scene->hasContentChanged());
  options->setZoomScale(2);
  EXPECT_FALSE(scene->hasContentChanged());

  options->setContentOffset(5, 10);
  EXPECT_TRUE(scene->hasContentChanged());
  ASSERT_TRUE(scene->draw(surface));
  EXPECT_FALSE(scene->hasContentChanged());
  options->setContentOffset(5, 10);
  EXPECT_FALSE(scene->hasContentChanged());

  options->setBackgroundColor({0, 0, 1, 1});
  EXPECT_TRUE(scene->hasContentChanged());
  ASSERT_TRUE(scene->draw(surface));
  EXPECT_FALSE(scene->hasContentChanged());
  options->setBackgroundColor({0, 0, 1, 1});
  EXPECT_FALSE(scene->hasContentChanged());

  options->setZoomScale(1.5f);
  ASSERT_TRUE(scene->draw(surface));
  EXPECT_FALSE(scene->hasContentChanged());
  options->setZoomScalePrecision(1);
  EXPECT_TRUE(scene->hasContentChanged());
  ASSERT_TRUE(scene->draw(surface));
  EXPECT_FALSE(scene->hasContentChanged());
  options->setZoomScalePrecision(1);
  EXPECT_FALSE(scene->hasContentChanged());

  options->setZoomScale(0);
  EXPECT_TRUE(scene->hasContentChanged());
  EXPECT_FALSE(scene->draw(surface));
  EXPECT_TRUE(scene->hasContentChanged());
}

PAGX_TEST(PAGXRuntimeTest, OutputViewRecordConsumesContentChanges) {
  auto doc = MakeTargetViewDocument();
  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_NE(scene, nullptr);
  auto device = Devices::MakeDefault();
  ASSERT_NE(device, nullptr);
  DeviceLockScope scope(device);
  ASSERT_TRUE(static_cast<bool>(scope));
  auto surface = tgfx::Surface::Make(scope.context(), 20, 20);
  ASSERT_NE(surface, nullptr);
  auto wrapped = pagx::MakeFrom(device, surface);
  ASSERT_NE(wrapped, nullptr);

  EXPECT_TRUE(scene->hasContentChanged());
  auto recording = pagx::Record(scope.context(), scene, wrapped);
  ASSERT_NE(recording, nullptr);
  scope.context()->submit(std::move(recording));
  EXPECT_FALSE(scene->hasContentChanged());
  scene->getDisplayOptions()->setContentOffset(5, 5);
  EXPECT_TRUE(scene->hasContentChanged());
  recording = pagx::Record(scope.context(), scene, wrapped);
  ASSERT_NE(recording, nullptr);
  scope.context()->submit(std::move(recording));
  EXPECT_FALSE(scene->hasContentChanged());
}

PAGX_TEST(PAGXRuntimeTest, OutputViewImageReplacementInvalidatesRenderedContent) {
  auto doc = MakeTargetViewDocument();
  auto* target = doc->findNode<pagx::Layer>("target");
  ASSERT_NE(target, nullptr);
  auto* imageNode = doc->makeNode<pagx::Image>();
  imageNode->filePath = "target.png";
  auto* pattern = doc->makeNode<pagx::ImagePattern>();
  pattern->image = imageNode;
  // A magnified 1x1 image with linear Decal sampling blends with transparent border texels.
  // Clamp keeps this solid-color fixture opaque so the assertions isolate resource replacement.
  pattern->tileModeX = pagx::TileMode::Clamp;
  pattern->tileModeY = pagx::TileMode::Clamp;
  auto* fill = static_cast<pagx::Fill*>(target->contents.back());
  fill->color = pattern;
  const uint8_t red[] = {255, 0, 0, 255};
  const uint8_t blue[] = {0, 0, 255, 255};
  auto info =
      tgfx::ImageInfo::Make(1, 1, tgfx::ColorType::RGBA_8888, tgfx::AlphaType::Premultiplied);
  auto redImage =
      pagx::MakeFrom(tgfx::Image::MakeFrom(info, tgfx::Data::MakeWithCopy(red, sizeof(red))));
  auto blueImage =
      pagx::MakeFrom(tgfx::Image::MakeFrom(info, tgfx::Data::MakeWithCopy(blue, sizeof(blue))));
  ASSERT_NE(redImage, nullptr);
  ASSERT_NE(blueImage, nullptr);
  ASSERT_TRUE(doc->loadFileData("target.png", redImage));
  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_NE(scene, nullptr);
  auto surface = pagx::PAGSurface::MakeOffscreen(20, 20);
  ASSERT_NE(surface, nullptr);
  ASSERT_TRUE(scene->draw(surface));
  EXPECT_FALSE(scene->hasContentChanged());
  std::vector<uint8_t> pixels = {};
  ASSERT_TRUE(ReadSurfacePixels(surface, &pixels));
  auto sampleOffset = (5 * surface->width() + 5) * 4;
  EXPECT_EQ(pixels[sampleOffset], 255u);
  EXPECT_EQ(pixels[sampleOffset + 2], 0u);
  EXPECT_EQ(pixels[sampleOffset + 3], 255u);

  auto runsBeforeImage = scene->outputViewValidationRuns;
  auto reusesBeforeImage = scene->outputViewValidationReuses;
  ASSERT_TRUE(doc->loadFileData("target.png", blueImage));
  EXPECT_TRUE(scene->hasContentChanged());
  // An Image resource notification is a render-only leaf edit, so the declaration is not rescanned.
  EXPECT_EQ(scene->outputViewValidationRuns, runsBeforeImage);
  EXPECT_EQ(scene->outputViewValidationReuses, reusesBeforeImage + 1);
  ASSERT_TRUE(scene->draw(surface));
  EXPECT_FALSE(scene->hasContentChanged());
  ASSERT_TRUE(ReadSurfacePixels(surface, &pixels));
  EXPECT_EQ(pixels[sampleOffset], 0u);
  EXPECT_EQ(pixels[sampleOffset + 2], 255u);
  EXPECT_EQ(pixels[sampleOffset + 3], 255u);
}

/**
 * Test case: an edit that breaks the target view declaration after the scene was built must make
 * draw() fail instead of silently switching to a whole-canvas render, consume the pending content
 * change, and report the invalid state. Clearing both fields returns the document to the
 * whole-canvas state so a host can recover without rebuilding the scene.
 */
PAGX_TEST(PAGXRuntimeTest, InvalidOutputViewAfterEditRejectsDraw) {
  auto doc = MakeTargetViewDocument();
  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);
  auto surface = pagx::PAGSurface::MakeOffscreen(20, 20);
  ASSERT_TRUE(surface != nullptr);
  ASSERT_TRUE(scene->draw(surface));

  doc->targetLayerId.clear();
  doc->notifyChange({doc.get()}, true);
  EXPECT_EQ(scene->getOutputViewStatus(), pagx::OutputViewStatus::Invalid);
  EXPECT_FALSE(scene->getOutputViewError().empty());
  EXPECT_FALSE(scene->getOutputBounds().has_value());
  auto info = scene->getOutputViewInfo();
  EXPECT_EQ(info.status, pagx::OutputViewStatus::Invalid);
  EXPECT_FALSE(info.error.empty());
  EXPECT_FALSE(info.bounds.has_value());
  EXPECT_TRUE(scene->hasContentChanged());
  EXPECT_FALSE(scene->draw(surface));
  EXPECT_FALSE(scene->hasContentChanged());

  doc->outputBounds.reset();
  doc->notifyChange({doc.get()}, true);
  EXPECT_EQ(scene->getOutputViewStatus(), pagx::OutputViewStatus::None);
  EXPECT_FALSE(scene->getOutputBounds().has_value());
  info = scene->getOutputViewInfo();
  EXPECT_EQ(info.status, pagx::OutputViewStatus::None);
  EXPECT_TRUE(info.error.empty());
  EXPECT_FALSE(info.bounds.has_value());
  EXPECT_TRUE(scene->draw(surface));
  EXPECT_FALSE(scene->hasContentChanged());

  doc->targetLayerId = "target";
  doc->outputBounds = pagx::Rect::MakeXYWH(20, 20, 20, 20);
  doc->notifyChange({doc.get()}, true);
  EXPECT_EQ(scene->getOutputViewStatus(), pagx::OutputViewStatus::Valid);
  ASSERT_TRUE(scene->getOutputBounds().has_value());
  info = scene->getOutputViewInfo();
  EXPECT_EQ(info.status, pagx::OutputViewStatus::Valid);
  EXPECT_TRUE(info.error.empty());
  ASSERT_TRUE(info.bounds.has_value());
  EXPECT_EQ(info.bounds->width, 20.0f);
  EXPECT_TRUE(scene->draw(surface));
  EXPECT_FALSE(scene->hasContentChanged());
}

// Returns the SolidColor node backing MakeTargetViewDocument()'s fill.
static pagx::SolidColor* SolidColorOfTarget(pagx::Layer* targetNode) {
  if (targetNode == nullptr || targetNode->contents.size() < 2) {
    return nullptr;
  }
  auto* fill = static_cast<pagx::Fill*>(targetNode->contents.back());
  return fill != nullptr ? static_cast<pagx::SolidColor*>(fill->color) : nullptr;
}

/**
 * Test case: render-only leaf edits (color, roundness) reuse the last Valid output view validation
 * instead of rescanning the document and the authored layer tree, while the frame is still marked
 * dirty and redrawn with the new content.
 */
PAGX_TEST(PAGXRuntimeTest, OutputViewValidationReusesForRenderOnlyLeafEdits) {
  auto doc = MakeTargetViewDocument();
  auto* targetNode = doc->findNode<pagx::Layer>("target");
  ASSERT_NE(targetNode, nullptr);
  auto* solid = SolidColorOfTarget(targetNode);
  ASSERT_NE(solid, nullptr);
  auto* rect = targetNode->contents.empty()
                   ? nullptr
                   : static_cast<pagx::Rectangle*>(targetNode->contents.front());
  ASSERT_NE(rect, nullptr);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_NE(scene, nullptr);
  auto surface = pagx::PAGSurface::MakeOffscreen(20, 20);
  ASSERT_NE(surface, nullptr);
  ASSERT_TRUE(scene->draw(surface));
  EXPECT_FALSE(scene->hasContentChanged());
  auto runs = scene->outputViewValidationRuns;
  auto reuses = scene->outputViewValidationReuses;

  solid->color = {0.0f, 1.0f, 0.0f, 1.0f};
  doc->notifyChange({solid}, false);
  EXPECT_EQ(scene->getOutputViewStatus(), pagx::OutputViewStatus::Valid);
  EXPECT_EQ(scene->outputViewValidationRuns, runs);
  EXPECT_EQ(scene->outputViewValidationReuses, reuses + 1);
  EXPECT_TRUE(scene->hasContentChanged());
  ASSERT_TRUE(scene->draw(surface));
  EXPECT_FALSE(scene->hasContentChanged());
  std::vector<uint8_t> pixels = {};
  ASSERT_TRUE(ReadSurfacePixels(surface, &pixels));
  auto offset = (5 * surface->width() + 5) * 4;
  EXPECT_EQ(pixels[offset], 0u);
  EXPECT_EQ(pixels[offset + 1], 255u);
  EXPECT_EQ(pixels[offset + 2], 0u);
  EXPECT_EQ(pixels[offset + 3], 255u);

  rect->roundness = 5;
  doc->notifyChange({rect}, false);
  EXPECT_EQ(scene->outputViewValidationRuns, runs);
  EXPECT_EQ(scene->outputViewValidationReuses, reuses + 2);
}

/**
 * Test case: edits that cannot be proven render-only fall back to the full resolver. A layout
 * change and an ambiguous Layer notification both take the full path.
 */
PAGX_TEST(PAGXRuntimeTest, OutputViewValidationFallsBackForLayoutAndLayerEdits) {
  auto doc = MakeTargetViewDocument();
  auto* targetNode = doc->findNode<pagx::Layer>("target");
  ASSERT_NE(targetNode, nullptr);
  auto* solid = SolidColorOfTarget(targetNode);
  ASSERT_NE(solid, nullptr);
  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_NE(scene, nullptr);
  auto surface = pagx::PAGSurface::MakeOffscreen(20, 20);
  ASSERT_NE(surface, nullptr);
  ASSERT_TRUE(scene->draw(surface));
  auto runs = scene->outputViewValidationRuns;

  doc->notifyChange({solid}, true);
  EXPECT_EQ(scene->outputViewValidationRuns, runs + 1);

  runs = scene->outputViewValidationRuns;
  targetNode->alpha = 0.5f;
  doc->notifyChange({targetNode}, false);
  EXPECT_EQ(scene->outputViewValidationRuns, runs + 1);
}

/**
 * Test case: a render leaf that directly carries the target id must invalidate the declaration even
 * though the id index was not re-registered. Fixing the id restores the Valid state.
 */
PAGX_TEST(PAGXRuntimeTest, OutputViewValidationDetectsRenderLeafIdConflict) {
  auto doc = MakeTargetViewDocument();
  auto* targetNode = doc->findNode<pagx::Layer>("target");
  ASSERT_NE(targetNode, nullptr);
  auto* solid = SolidColorOfTarget(targetNode);
  ASSERT_NE(solid, nullptr);
  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_NE(scene, nullptr);
  auto surface = pagx::PAGSurface::MakeOffscreen(20, 20);
  ASSERT_NE(surface, nullptr);
  ASSERT_TRUE(scene->draw(surface));
  auto runs = scene->outputViewValidationRuns;

  solid->id = "target";
  doc->notifyChange({solid}, false);
  EXPECT_EQ(scene->outputViewValidationRuns, runs + 1);
  EXPECT_EQ(scene->getOutputViewStatus(), pagx::OutputViewStatus::Invalid);
  EXPECT_FALSE(scene->getOutputViewError().empty());

  solid->id.clear();
  doc->notifyChange({solid}, false);
  EXPECT_EQ(scene->getOutputViewStatus(), pagx::OutputViewStatus::Valid);
  EXPECT_TRUE(scene->draw(surface));
}

/**
 * Test case: creating a node bumps the registry version and forces the full resolver even when the
 * new node has no owning Layer, so the collapsed dirty set is empty.
 */
PAGX_TEST(PAGXRuntimeTest, OutputViewValidationFallsBackForNewUnreferencedNode) {
  auto doc = MakeTargetViewDocument();
  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_NE(scene, nullptr);
  auto surface = pagx::PAGSurface::MakeOffscreen(20, 20);
  ASSERT_NE(surface, nullptr);
  ASSERT_TRUE(scene->draw(surface));
  auto runs = scene->outputViewValidationRuns;

  auto* orphan = doc->makeNode<pagx::SolidColor>();
  ASSERT_NE(orphan, nullptr);
  doc->notifyChange({orphan}, false);
  EXPECT_EQ(scene->getOutputViewStatus(), pagx::OutputViewStatus::Valid);
  EXPECT_EQ(scene->outputViewValidationRuns, runs + 1);
  EXPECT_TRUE(scene->hasContentChanged());
}

/**
 * Test case: each scene owns its validation snapshot, so one scene reusing a result does not starve
 * another scene of the same notification.
 */
PAGX_TEST(PAGXRuntimeTest, OutputViewValidationIsPerScene) {
  auto doc = MakeTargetViewDocument();
  auto* targetNode = doc->findNode<pagx::Layer>("target");
  ASSERT_NE(targetNode, nullptr);
  auto* solid = SolidColorOfTarget(targetNode);
  ASSERT_NE(solid, nullptr);
  auto sceneA = pagx::PAGScene::Make(doc);
  auto sceneB = pagx::PAGScene::Make(doc);
  ASSERT_NE(sceneA, nullptr);
  ASSERT_NE(sceneB, nullptr);
  auto surface = pagx::PAGSurface::MakeOffscreen(20, 20);
  ASSERT_NE(surface, nullptr);
  ASSERT_TRUE(sceneA->draw(surface));
  ASSERT_TRUE(sceneB->draw(surface));
  auto reusesA = sceneA->outputViewValidationReuses;
  auto reusesB = sceneB->outputViewValidationReuses;

  solid->color = {0.0f, 0.0f, 1.0f, 1.0f};
  doc->notifyChange({solid}, false);
  EXPECT_EQ(sceneA->outputViewValidationReuses, reusesA + 1);
  EXPECT_EQ(sceneB->outputViewValidationReuses, reusesB + 1);
}

// Hands back the runtime tgfx layer bound to the authored target, plus its parent pointer in the
// runtime tree. Used to simulate a broken hookup without changing the authored declaration.
static std::shared_ptr<tgfx::Layer> RuntimeTargetOf(pagx::PAGScene* scene,
                                                    const pagx::Layer* targetNode,
                                                    tgfx::Layer** outParent) {
  auto composition = scene != nullptr ? scene->rootComposition() : nullptr;
  auto* binding = composition != nullptr ? composition->binding.get() : nullptr;
  if (binding == nullptr || targetNode == nullptr) {
    return nullptr;
  }
  auto runtimeLayer = binding->get<tgfx::Layer>(targetNode);
  if (outParent != nullptr) {
    *outParent = runtimeLayer != nullptr ? runtimeLayer->parent() : nullptr;
  }
  return runtimeLayer;
}

// A prepared wrapper must be reused across frames: the target keeps the same parent layer between
// draws, and the original tree is restored after an edit invalidates the state.
PAGX_TEST(PAGXRuntimeTest, TargetViewReusesPreparedWrapper) {
  auto doc = MakeTargetViewDocument();
  auto* targetNode = doc->findNode<pagx::Layer>("target");
  ASSERT_NE(targetNode, nullptr);
  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_NE(scene, nullptr);
  auto surface = pagx::PAGSurface::MakeOffscreen(20, 20);
  ASSERT_NE(surface, nullptr);

  // Capture the untouched tree before drawing: the prepared state has not been created yet.
  tgfx::Layer* originalParent = nullptr;
  auto runtimeTarget = RuntimeTargetOf(scene.get(), targetNode, &originalParent);
  ASSERT_NE(runtimeTarget, nullptr);
  ASSERT_NE(originalParent, nullptr);

  ASSERT_TRUE(scene->draw(surface));
  auto* preparedParent = runtimeTarget->parent();
  ASSERT_NE(preparedParent, nullptr);
  ASSERT_NE(preparedParent, originalParent);

  // Repeating the draw reuses the same prepared wrapper instead of re-hooking the target.
  ASSERT_TRUE(scene->draw(surface));
  EXPECT_EQ(runtimeTarget->parent(), preparedParent);

  // An edit invalidates the state and restores the target to its original parent. Re-fetch the
  // runtime layer because an in-place refresh may replace it.
  targetNode->alpha = 0.5f;
  doc->notifyChange({targetNode}, false);
  auto refreshedTarget = RuntimeTargetOf(scene.get(), targetNode, nullptr);
  ASSERT_NE(refreshedTarget, nullptr);
  EXPECT_EQ(refreshedTarget->parent(), originalParent);

  // The next draw prepares a fresh wrapper and still renders correctly.
  EXPECT_TRUE(scene->draw(surface));
}

// Detaching the runtime target before the first draw makes preparation fail. draw() must return
// false without consuming the pending redraw, and re-attaching it must let a later draw succeed.
PAGX_TEST(PAGXRuntimeTest, TargetViewDetachedBeforePrepareFailsDraw) {
  auto doc = MakeTargetViewDocument();
  auto* targetNode = doc->findNode<pagx::Layer>("target");
  ASSERT_NE(targetNode, nullptr);
  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_NE(scene, nullptr);
  auto surface = pagx::PAGSurface::MakeOffscreen(20, 20);
  ASSERT_NE(surface, nullptr);

  tgfx::Layer* originalParent = nullptr;
  auto runtimeTarget = RuntimeTargetOf(scene.get(), targetNode, &originalParent);
  ASSERT_NE(runtimeTarget, nullptr);
  ASSERT_NE(originalParent, nullptr);
  runtimeTarget->removeFromParent();

  EXPECT_TRUE(scene->hasContentChanged());
  EXPECT_FALSE(scene->draw(surface));
  EXPECT_TRUE(scene->hasContentChanged());

  originalParent->addChild(runtimeTarget);
  EXPECT_TRUE(scene->draw(surface));
  EXPECT_FALSE(scene->hasContentChanged());
}

// Damaging the hookup after a successful preparation must fail without a silent re-hook, preserve
// the pending redraw, and make pagx::Record() fail the same way. Re-attaching into the prepared
// wrapper recovers the frame.
PAGX_TEST(PAGXRuntimeTest, TargetViewDamagedAfterPrepareFailsDrawAndRecord) {
  auto doc = MakeTargetViewDocument();
  auto* targetNode = doc->findNode<pagx::Layer>("target");
  ASSERT_NE(targetNode, nullptr);
  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_NE(scene, nullptr);
  auto surface = pagx::PAGSurface::MakeOffscreen(20, 20);
  ASSERT_NE(surface, nullptr);

  ASSERT_TRUE(scene->draw(surface));
  auto runtimeTarget = RuntimeTargetOf(scene.get(), targetNode, nullptr);
  ASSERT_NE(runtimeTarget, nullptr);
  auto* wrapper = runtimeTarget->parent();
  ASSERT_NE(wrapper, nullptr);
  runtimeTarget->removeFromParent();
  scene->getDisplayOptions()->setContentOffset(1, 1);
  EXPECT_TRUE(scene->hasContentChanged());

  EXPECT_FALSE(scene->draw(surface));
  EXPECT_TRUE(scene->hasContentChanged());

  {
    auto device = Devices::MakeDefault();
    ASSERT_NE(device, nullptr);
    DeviceLockScope scope(device);
    ASSERT_TRUE(static_cast<bool>(scope));
    auto recordingSurface = tgfx::Surface::Make(scope.context(), 20, 20);
    ASSERT_NE(recordingSurface, nullptr);
    auto wrapped = pagx::MakeFrom(device, recordingSurface);
    ASSERT_NE(wrapped, nullptr);
    EXPECT_EQ(pagx::Record(scope.context(), scene, wrapped), nullptr);
    EXPECT_TRUE(scene->hasContentChanged());
  }

  wrapper->addChild(runtimeTarget);
  EXPECT_TRUE(scene->draw(surface));
  EXPECT_FALSE(scene->hasContentChanged());
}

// Helper that builds a scene owning a single Animation with the given loop mode. The animation is
// 60 frames at 60 fps so DurationMicros == 1_000_000 exactly, keeping the boundary math free of
// rounding noise (an integer-only period is required for the period-boundary assertions below).
// The scene is returned alongside the animation because PAGAnimation only holds a weak reference
// to its owning scene: without keeping the scene alive in the caller, duration()/playbackPeriod()
// short-circuit to 0 via the owner.expired() guard and every assertion below would fire.
struct AnimationFixture {
  std::shared_ptr<pagx::PAGScene> scene;
  std::shared_ptr<pagx::PAGAnimation> animation;
};
static AnimationFixture MakeAnimationFixture(pagx::LoopMode loop) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto anim = doc->makeNode<pagx::Animation>("main");
  anim->duration = 60;
  anim->frameRate = 60;
  anim->loop = loop;
  doc->animations.push_back(anim);
  AnimationFixture fixture;
  fixture.scene = pagx::PAGScene::Make(doc);
  if (fixture.scene != nullptr) {
    fixture.animation =
        std::static_pointer_cast<pagx::PAGAnimation>(fixture.scene->getAnimation("main"));
  }
  return fixture;
}

/**
 * Test case: playbackPeriod reports one full loop period per loop mode. Once/Loop expose the
 * animation duration; PingPong doubles it because a full loop is forward + back.
 */
PAGX_TEST(PAGXRuntimeTest, PlaybackPeriodByLoopMode) {
  auto once = MakeAnimationFixture(pagx::LoopMode::Once);
  ASSERT_TRUE(once.animation != nullptr);
  EXPECT_EQ(once.animation->duration(), 1'000'000);
  EXPECT_EQ(once.animation->playbackPeriod(), 1'000'000);

  auto loop = MakeAnimationFixture(pagx::LoopMode::Loop);
  ASSERT_TRUE(loop.animation != nullptr);
  EXPECT_EQ(loop.animation->playbackPeriod(), 1'000'000);

  auto pingPong = MakeAnimationFixture(pagx::LoopMode::PingPong);
  ASSERT_TRUE(pingPong.animation != nullptr);
  EXPECT_EQ(pingPong.animation->playbackPeriod(), 2'000'000);
}

/**
 * Test case: playbackPosition traces a monotonic timeline across one full loop period per mode.
 * The key invariant guarded here is the period-boundary special case: when elapsedUs is a positive
 * integer multiple of the period, playbackPosition reports period (not 0), so a seek-to-end lands
 * the progress bar on the right edge instead of snapping back to the start. Once clamps to the
 * period end so post-duration reads stay at duration.
 */
PAGX_TEST(PAGXRuntimeTest, PlaybackPositionBoundaries) {
  const int64_t D = 1'000'000;

  auto once = MakeAnimationFixture(pagx::LoopMode::Once);
  ASSERT_TRUE(once.animation != nullptr);
  once.animation->setCurrentTime(0);
  EXPECT_EQ(once.animation->playbackPosition(), 0);
  once.animation->setCurrentTime(D / 2);
  EXPECT_EQ(once.animation->playbackPosition(), D / 2);
  once.animation->setCurrentTime(D);
  EXPECT_EQ(once.animation->playbackPosition(), D);
  // Once clamps past-the-end reads to the period end rather than wrapping.
  once.animation->setCurrentTime(2 * D);
  EXPECT_EQ(once.animation->playbackPosition(), D);

  auto loop = MakeAnimationFixture(pagx::LoopMode::Loop);
  ASSERT_TRUE(loop.animation != nullptr);
  loop.animation->setCurrentTime(0);
  EXPECT_EQ(loop.animation->playbackPosition(), 0);
  loop.animation->setCurrentTime(D / 2);
  EXPECT_EQ(loop.animation->playbackPosition(), D / 2);
  // Period boundary: elapsedUs % period is 0 but the position reports period so the progress bar
  // stays at the far right. Also holds at any positive integer multiple.
  loop.animation->setCurrentTime(D);
  EXPECT_EQ(loop.animation->playbackPosition(), D);
  loop.animation->setCurrentTime(2 * D);
  EXPECT_EQ(loop.animation->playbackPosition(), D);

  auto pingPong = MakeAnimationFixture(pagx::LoopMode::PingPong);
  ASSERT_TRUE(pingPong.animation != nullptr);
  const int64_t P = 2 * D;
  pingPong.animation->setCurrentTime(0);
  EXPECT_EQ(pingPong.animation->playbackPosition(), 0);
  pingPong.animation->setCurrentTime(D);
  EXPECT_EQ(pingPong.animation->playbackPosition(), D);
  // PingPong period boundary (elapsedUs == 2 * duration): same invariant as Loop, position holds
  // at period instead of folding to 0.
  pingPong.animation->setCurrentTime(P);
  EXPECT_EQ(pingPong.animation->playbackPosition(), P);
  pingPong.animation->setCurrentTime(2 * P);
  EXPECT_EQ(pingPong.animation->playbackPosition(), P);
}

/**
 * Test case: seek-to-end progress reading vs. currentTime (the value apply() feeds into channel
 * evaluation). At a period boundary Loop/PingPong intentionally report a non-zero playbackPosition
 * for the progress bar while currentTime folds to 0, because "end of a period" and "start of the
 * next period" are the same instant on the timeline. This asserts the intended divergence so a
 * future edit that unifies the two ends up caught here.
 */
PAGX_TEST(PAGXRuntimeTest, PlaybackPositionSeekEndConsistency) {
  const int64_t D = 1'000'000;

  auto once = MakeAnimationFixture(pagx::LoopMode::Once);
  ASSERT_TRUE(once.animation != nullptr);
  once.animation->setCurrentTime(D);
  // Once has no wrap: progress and evaluation both stop at duration.
  EXPECT_EQ(once.animation->playbackPosition(), D);
  EXPECT_EQ(once.animation->currentTime(), D);

  auto loop = MakeAnimationFixture(pagx::LoopMode::Loop);
  ASSERT_TRUE(loop.animation != nullptr);
  loop.animation->setCurrentTime(D);
  // Loop wraps: seek-to-end shows a full progress bar, but the evaluated frame is the start
  // (seamless loop t == duration equals t == 0).
  EXPECT_EQ(loop.animation->playbackPosition(), D);
  EXPECT_EQ(loop.animation->currentTime(), 0);

  auto pingPong = MakeAnimationFixture(pagx::LoopMode::PingPong);
  ASSERT_TRUE(pingPong.animation != nullptr);
  pingPong.animation->setCurrentTime(2 * D);
  // PingPong period boundary is the return to the start of the round trip, so evaluation is 0
  // while the progress bar sits at period.
  EXPECT_EQ(pingPong.animation->playbackPosition(), 2 * D);
  EXPECT_EQ(pingPong.animation->currentTime(), 0);
}

}  // namespace pag
