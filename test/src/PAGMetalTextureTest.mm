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
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#import <Metal/Metal.h>
#include "pagx/PAGAnimation.h"
#include "pagx/PAGScene.h"
#include "pagx/PAGSurface.h"
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Animation.h"
#include "pagx/nodes/AnimationObject.h"
#include "pagx/nodes/Channel.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/SolidColor.h"
#include "tgfx/gpu/Context.h"
#include "tgfx/gpu/metal/MetalDevice.h"
#include "utils/TestUtils.h"

namespace pag {
namespace {

class MetalTestTexture {
 public:
  /**
   * Creates an externally owned Metal texture on libpag's default device. The context lock is
   * released before returning so surface factories, rendering, and readback can lock the device.
   */
  static std::shared_ptr<MetalTestTexture> Make(int width, int height,
                                                MTLPixelFormat format = MTLPixelFormatRGBA8Unorm) {
    if (width <= 0 || height <= 0) {
      return nullptr;
    }
    DeviceLockScope lock;
    if (!lock) {
      return nullptr;
    }
    auto device = static_cast<tgfx::MetalDevice*>(lock.context()->device());
    auto nativeDevice = (__bridge id<MTLDevice>)device->metalDevice();
    @autoreleasepool {
      auto descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format
                                                                           width:width
                                                                          height:height
                                                                       mipmapped:NO];
      descriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
      auto texture = [nativeDevice newTextureWithDescriptor:descriptor];
      if (texture == nil) {
        return nullptr;
      }
      return std::shared_ptr<MetalTestTexture>(new MetalTestTexture(texture));
    }
  }

  ~MetalTestTexture() {
    [texture release];
  }

  /**
   * Returns an external texture description with the requested logical dimensions. The native
   * texture remains owned by this helper until all surface and image users have been destroyed.
   */
  BackendTexture backendTexture(int width, int height) const {
    return {info(), width, height};
  }

  /**
   * Returns an external render target description backed by the owned native Metal texture.
   */
  BackendRenderTarget backendRenderTarget(int width, int height) const {
    return {info(), width, height};
  }

 private:
  explicit MetalTestTexture(id<MTLTexture> texture) : texture(texture) {
  }

  MtlTextureInfo info() const {
    MtlTextureInfo value = {};
    value.texture = (__bridge void*)texture;
    return value;
  }

  id<MTLTexture> texture = nil;
};

static std::shared_ptr<MetalTestTexture> GetBottomLeftMetalImage(int width, int height) {
  auto texture = MetalTestTexture::Make(width, height);
  if (texture == nullptr) {
    return nullptr;
  }
  auto surface =
      PAGSurface::MakeFrom(texture->backendTexture(width, height), ImageOrigin::BottomLeft);
  if (surface == nullptr) {
    return nullptr;
  }
  auto image = MakePAGImage("assets/mountain.jpg");
  if (image == nullptr) {
    return nullptr;
  }
  auto composition = PAGComposition::Make(1080, 1920);
  auto imageLayer = PAGImageLayer::Make(1080, 1920, 1000000);
  imageLayer->replaceImage(image);
  composition->addLayer(imageLayer);
  auto player = std::make_shared<PAGPlayer>();
  player->setSurface(surface);
  player->setComposition(composition);
  if (!player->flush()) {
    return nullptr;
  }
  return texture;
}

static void CheckMetalPAGXSurface(bool renderTarget) {
  const int width = 100;
  const int height = 100;
  auto texture = MetalTestTexture::Make(width, height);
  ASSERT_TRUE(texture != nullptr);
  auto surface = renderTarget
                     ? pagx::PAGSurface::MakeFrom(texture->backendRenderTarget(width, height),
                                                  ImageOrigin::TopLeft)
                     : pagx::PAGSurface::MakeFrom(texture->backendTexture(width, height),
                                                  ImageOrigin::TopLeft);
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
  solid->color = {0, 0, 0, 1};
  fill->color = solid;
  layer->contents.push_back(fill);

  auto animation = doc->makeNode<pagx::Animation>("main");
  animation->duration = 60;
  animation->frameRate = 60;
  doc->animations.push_back(animation);
  auto object = doc->makeNode<pagx::AnimationObject>();
  object->target = "S";
  animation->objects.push_back(object);
  auto channel = doc->makeNode<pagx::TypedChannel<pagx::Color>>();
  channel->name = "color";
  pagx::Color color = renderTarget ? pagx::Color{0, 1, 0, 1, pagx::ColorSpace::SRGB}
                                   : pagx::Color{0, 0, 1, 1, pagx::ColorSpace::SRGB};
  channel->keyframes.push_back({0, color, pagx::KeyframeInterpolationType::Hold, {}, {}});
  object->channels.push_back(channel);
  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);
  auto timeline = std::static_pointer_cast<pagx::PAGAnimation>(scene->getDefaultTimeline());
  ASSERT_TRUE(timeline != nullptr);
  timeline->apply(1);
  ASSERT_TRUE(scene->draw(surface));

  std::vector<uint8_t> pixels(width * height * 4);
  ASSERT_TRUE(surface->readPixels(pixels.data(), width * 4));
  for (size_t i = 0; i < pixels.size(); i += 4) {
    EXPECT_EQ(pixels[i], 0);
    EXPECT_EQ(pixels[i + 1], renderTarget ? 255 : 0);
    EXPECT_EQ(pixels[i + 2], renderTarget ? 0 : 255);
    EXPECT_EQ(pixels[i + 3], 255);
  }
  auto key = renderTarget ? "PAGXRuntimeTest/MetalPAGSurfaceFromBackendRenderTarget"
                          : "PAGXRuntimeTest/MetalPAGSurfaceFromBackendTexture";
  EXPECT_TRUE(Baseline::Compare(surface, key));
}

}  // namespace

/**
 * Covers copy-destination blending into an external BottomLeft Metal texture.
 */
PAG_TEST(PAGBlendMetalTest, CopyDstTexture) {
  const int width = 400;
  const int height = 400;
  auto texture = MetalTestTexture::Make(width, height);
  ASSERT_TRUE(texture != nullptr);
  auto surface =
      PAGSurface::MakeFrom(texture->backendTexture(width, height), ImageOrigin::BottomLeft);
  ASSERT_TRUE(surface != nullptr);
  auto file = LoadPAGFile("resources/blend/Multiply.pag");
  ASSERT_TRUE(file != nullptr);
  auto player = std::make_shared<PAGPlayer>();
  player->setSurface(surface);
  player->setComposition(file);
  player->setMatrix(Matrix::I());
  player->setProgress(0.5);
  ASSERT_TRUE(player->flush());
  EXPECT_TRUE(Baseline::Compare(surface, "PAGBlendMetalTest/CopyDstTexture"));
}

/**
 * Covers a BottomLeft texture-backed replacement image rendered into a TopLeft Metal surface.
 */
PAG_TEST(PAGBlendMetalTest, TextureBottomLeft) {
  const int width = 720;
  const int height = 1280;
  auto replacement = GetBottomLeftMetalImage(width, height);
  ASSERT_TRUE(replacement != nullptr);
  auto texture = MetalTestTexture::Make(width, height);
  ASSERT_TRUE(texture != nullptr);
  auto image =
      PAGImage::FromTexture(replacement->backendTexture(width, height), ImageOrigin::BottomLeft);
  ASSERT_TRUE(image != nullptr);
  auto surface = PAGSurface::MakeFrom(texture->backendTexture(width, height), ImageOrigin::TopLeft);
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
  EXPECT_TRUE(Baseline::Compare(surface, "PAGBlendMetalTest/TextureBottomLeft"));
}

/**
 * Covers BottomLeft origin handling for both the external render target and replacement image.
 */
PAG_TEST(PAGBlendMetalTest, BothBottomLeft) {
  const int width = 720;
  const int height = 1280;
  auto replacement = GetBottomLeftMetalImage(width, height);
  ASSERT_TRUE(replacement != nullptr);
  auto texture = MetalTestTexture::Make(width, height);
  ASSERT_TRUE(texture != nullptr);
  auto image = PAGImage::FromTexture(replacement->backendTexture(width / 2, height / 2),
                                     ImageOrigin::BottomLeft);
  ASSERT_TRUE(image != nullptr);
  image->setMatrix(Matrix::MakeTrans(width / 10, height / 5));
  auto surface =
      PAGSurface::MakeFrom(texture->backendTexture(width, height), ImageOrigin::BottomLeft);
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
  EXPECT_TRUE(Baseline::Compare(surface, "PAGBlendMetalTest/BothBottomLeft"));
}

/**
 * Verifies that both texture and render target conversions read the real BGRA Metal pixel format.
 */
PAG_TEST(PAGBlendMetalTest, TextureFormatFromRealTexture) {
  const int size = 100;
  auto texture = MetalTestTexture::Make(size, size, MTLPixelFormatBGRA8Unorm);
  ASSERT_TRUE(texture != nullptr);
  tgfx::MetalTextureInfo info = {};
  auto backendTexture = ToTGFX(texture->backendTexture(size, size));
  ASSERT_TRUE(backendTexture.getMetalTextureInfo(&info));
  EXPECT_EQ(info.format, static_cast<unsigned>(MTLPixelFormatBGRA8Unorm));
  auto renderTarget = ToTGFX(texture->backendRenderTarget(size, size));
  ASSERT_TRUE(renderTarget.getMetalTextureInfo(&info));
  EXPECT_EQ(info.format, static_cast<unsigned>(MTLPixelFormatBGRA8Unorm));
}

/**
 * Covers masking when the external Metal render target uses BottomLeft origin.
 */
PAG_TEST(PAGSurfaceMetalTest, Mask) {
  auto file = LoadPAGFile("assets/test2.pag");
  ASSERT_TRUE(file != nullptr);
  auto texture = MetalTestTexture::Make(file->width(), file->height());
  ASSERT_TRUE(texture != nullptr);
  auto surface = PAGSurface::MakeFrom(texture->backendTexture(file->width(), file->height()),
                                      ImageOrigin::BottomLeft);
  ASSERT_TRUE(surface != nullptr);
  auto player = std::make_shared<PAGPlayer>();
  player->setSurface(surface);
  player->setComposition(file);
  player->setMatrix(Matrix::I());
  player->setProgress(0.9);
  ASSERT_TRUE(player->flush());
  EXPECT_TRUE(Baseline::Compare(surface, "PAGSurfaceMetalTest/Mask"));
}

/**
 * Verifies scissor orientation on an external BottomLeft Metal render target.
 */
PAG_TEST(PAGSurfaceMetalTest, BottomLeftScissor) {
  auto file = LoadPAGFile("assets/test.pag");
  ASSERT_TRUE(file != nullptr);
  const int width = file->width();
  const int height = file->height() * 2;
  auto texture = MetalTestTexture::Make(width, height);
  ASSERT_TRUE(texture != nullptr);
  auto surface =
      PAGSurface::MakeFrom(texture->backendTexture(width, height), ImageOrigin::BottomLeft);
  ASSERT_TRUE(surface != nullptr);
  auto player = std::make_shared<PAGPlayer>();
  player->setSurface(surface);
  player->setComposition(file);
  player->setMatrix(Matrix::MakeTrans(0, file->height()));
  player->setProgress(0.5);
  ASSERT_TRUE(player->flush());
  EXPECT_TRUE(Baseline::Compare(surface, "PAGSurfaceMetalTest/BottomLeftScissor"));
}

/**
 * Renders an animated PAGX scene into an external Metal texture and checks its pixels.
 */
PAGX_TEST(PAGXRuntimeTest, MetalPAGSurfaceFromBackendTexture) {
  CheckMetalPAGXSurface(false);
}

/**
 * Renders an animated PAGX scene into an external Metal render target and checks its pixels.
 */
PAGX_TEST(PAGXRuntimeTest, MetalPAGSurfaceFromBackendRenderTarget) {
  CheckMetalPAGXSurface(true);
}

}  // namespace pag
