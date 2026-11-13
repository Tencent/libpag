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
#include <climits>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include "rendering/caches/LayerCache.h"
#include "rendering/caches/RenderCache.h"
#include "rendering/filters/Transform3DFilter.h"
#include "rendering/filters/Transform3DSceneFilter.h"
#include "rendering/filters/utils/Filter3DFactory.h"
#include "rendering/graphics/Transform3DScene.h"
#include "rendering/layers/PAGStage.h"
#include "rendering/renderers/TextRenderer.h"
#include "tgfx/core/PictureRecorder.h"
#include "tgfx/core/Surface.h"
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-warning-option"
#pragma clang diagnostic ignored "-Wdeprecated-literal-operator"
#include "nlohmann/json.hpp"
#pragma clang diagnostic pop
#include "utils/TestUtils.h"

namespace pag {
namespace {

class Test3DGPU : public tgfx::GPU {
 public:
  explicit Test3DGPU(tgfx::GPU* gpu = nullptr) : delegate(gpu) {
    featureData.stencilAttachmentSupported = true;
    limitData.maxTextureDimension2D = 1024;
    limitData.maxSamplersPerShaderStage = 3;
    limitData.maxUniformBufferBindingSize = 64;
    if (delegate != nullptr) {
      featureData = *delegate->features();
      limitData = *delegate->limits();
    }
  }

  const tgfx::GPUInfo* info() const override {
    return &infoData;
  }
  const tgfx::GPUFeatures* features() const override {
    return &featureData;
  }
  const tgfx::GPULimits* limits() const override {
    return &limitData;
  }
  tgfx::CommandQueue* queue() const override {
    return nullptr;
  }
  bool isFormatRenderable(tgfx::PixelFormat format) const override {
    return format == tgfx::PixelFormat::DEPTH24_STENCIL8 ? depthRenderable : colorRenderable;
  }
  int getSampleCount(int, tgfx::PixelFormat) const override {
    return 1;
  }
  std::shared_ptr<tgfx::Texture> createTexture(const tgfx::TextureDescriptor& descriptor) override {
    textureCount++;
    if (textureCount == failTexture || delegate == nullptr) {
      return nullptr;
    }
    return delegate->createTexture(descriptor);
  }
  std::shared_ptr<tgfx::GPUBuffer> createBuffer(size_t, uint32_t) override {
    return nullptr;
  }
  std::vector<std::shared_ptr<tgfx::Texture>> importHardwareTextures(tgfx::HardwareBufferRef,
                                                                     uint32_t) override {
    return {};
  }
  std::shared_ptr<tgfx::Texture> importBackendTexture(const tgfx::BackendTexture&, uint32_t,
                                                      bool) override {
    return nullptr;
  }
  std::shared_ptr<tgfx::Texture> importBackendRenderTarget(
      const tgfx::BackendRenderTarget&) override {
    return nullptr;
  }
  std::shared_ptr<tgfx::Semaphore> importBackendSemaphore(const tgfx::BackendSemaphore&) override {
    return nullptr;
  }
  tgfx::BackendSemaphore stealBackendSemaphore(std::shared_ptr<tgfx::Semaphore>) override {
    return {};
  }
  std::shared_ptr<tgfx::Sampler> createSampler(const tgfx::SamplerDescriptor&) override {
    return nullptr;
  }
  std::shared_ptr<tgfx::ShaderModule> createShaderModule(
      const tgfx::ShaderModuleDescriptor&) override {
    return nullptr;
  }
  std::shared_ptr<tgfx::RenderPipeline> createRenderPipeline(
      const tgfx::RenderPipelineDescriptor&) override {
    return nullptr;
  }
  std::shared_ptr<tgfx::CommandEncoder> createCommandEncoder() override {
    return nullptr;
  }

  tgfx::GPUInfo infoData = {};
  tgfx::GPUFeatures featureData = {};
  tgfx::GPULimits limitData = {};
  bool depthRenderable = true;
  bool colorRenderable = true;
  int textureCount = 0;
  int failTexture = -1;

 private:
  tgfx::GPU* delegate = nullptr;
};

class DepthEncodingFilter : public Transform3DSceneFilter {
 public:
  using Transform3DSceneFilter::Transform3DSceneFilter;

 protected:
  DEFINE_RUNTIME_FILTER_TYPE

  std::string onBuildFragmentShader() const override {
    auto shader = Transform3DSceneFilter::onBuildFragmentShader();
    auto output = std::string("uOutput == 0 ? color");
    shader.replace(shader.find(output), output.size(), "uOutput == 0 ? vec4(depthColor.rgb, 1.0)");
    return shader;
  }
};

// Packed depth bytes require nearest sampling; interpolating them changes the encoded integer.
void CheckDepthEncoding(RenderCache* cache, tgfx::Context* context,
                        const Transform3DDepthPlane& plane, int width, int height,
                        const std::vector<tgfx::Point>& points) {
  std::vector<std::shared_ptr<tgfx::Image>> sources;
  for (bool opaque : {false, true}) {
    tgfx::PictureRecorder recorder;
    auto canvas = recorder.beginRecording();
    tgfx::Paint paint;
    paint.setColor(opaque ? tgfx::Color::White() : tgfx::Color::Transparent());
    if (opaque) {
      for (auto point : points) {
        auto rect = tgfx::Rect::MakeXYWH(point.x, point.y, 1.0f, 1.0f);
        canvas->drawRect(rect, paint);
      }
    } else {
      canvas->drawRect(tgfx::Rect::MakeWH(width, height), paint);
    }
    auto image = tgfx::Image::MakeFrom(recorder.finishRecordingAsPicture(), width, height);
    ASSERT_NE(image, nullptr);
    sources.push_back(std::move(image));
  }
  auto filter = std::make_shared<DepthEncodingFilter>(
      cache, std::vector<std::shared_ptr<tgfx::Image>>{sources[1]},
      std::vector<Transform3DDepthPlane>{plane}, std::vector<BlendMode>{BlendMode::Normal}, width,
      height);
  auto image = sources[0]->makeWithFilter(tgfx::ImageFilter::Runtime(filter));
  ASSERT_NE(image, nullptr);
  auto surface = tgfx::Surface::Make(context, width, height);
  ASSERT_NE(surface, nullptr);
  surface->getCanvas()->drawImage(
      image, tgfx::SamplingOptions(tgfx::FilterMode::Nearest, tgfx::MipmapMode::None));
  tgfx::Bitmap bitmap(width, height, false, false);
  tgfx::Pixmap pixels(bitmap);
  ASSERT_TRUE(surface->readPixels(pixels.info(), pixels.writablePixels()));
  auto bytes = static_cast<const uint8_t*>(pixels.pixels());
  for (auto point : points) {
    SCOPED_TRACE(testing::Message() << point.x << "," << point.y);
    auto reference = plane.x * (2.0 * (point.x + 0.5) / width - 1) +
                     plane.y * (2.0 * (point.y + 0.5) / height - 1) + plane.z;
    auto offset =
        static_cast<size_t>(point.y) * pixels.rowBytes() + static_cast<size_t>(point.x) * 4;
    if (reference < 0 || reference > 1) {
      EXPECT_EQ(bytes[offset + 3], 0);
    } else {
      EXPECT_EQ(bytes[offset + 3], 255);
      auto encoded = static_cast<uint32_t>(bytes[offset]) |
                     (static_cast<uint32_t>(bytes[offset + 1]) << 8) |
                     (static_cast<uint32_t>(bytes[offset + 2]) << 16);
      EXPECT_EQ(encoded, static_cast<uint32_t>(std::floor(reference * 16777215.0 + 0.5)));
    }
  }
}

Transform3D* MakeTransform3D(Point3D position) {
  auto transform = new Transform3D();
  transform->anchorPoint = new Property<Point3D>({0, 0, 0});
  transform->position = new Property<Point3D>(position);
  transform->scale = new Property<Point3D>({1, 1, 1});
  transform->orientation = new Property<Point3D>({0, 0, 0});
  transform->xRotation = new Property<float>(0);
  transform->yRotation = new Property<float>(0);
  transform->zRotation = new Property<float>(0);
  transform->opacity = new Property<Opacity>(255);
  return transform;
}

SolidLayer* MakeSolid3D(Color color, Point3D position, Opacity opacity) {
  auto layer = new SolidLayer();
  layer->duration = 60;
  layer->width = 40;
  layer->height = 40;
  layer->solidColor = color;
  layer->transform3D = MakeTransform3D(position);
  layer->transform3D->opacity->value = opacity;
  return layer;
}

SolidLayer* MakeSolid2D(Color color, Point position, int width, int height) {
  auto layer = MakeSolid3D(color, {0, 0, 0}, 255);
  delete layer->transform3D;
  layer->transform3D = nullptr;
  layer->transform = Transform2D::MakeDefault().release();
  layer->transform->position->value = position;
  layer->width = width;
  layer->height = height;
  return layer;
}

CameraLayer* MakeCamera(float zoom, Frame start, Frame duration) {
  auto camera = new CameraLayer();
  camera->duration = duration;
  camera->startTime = start;
  camera->transform3D = MakeTransform3D({50, 50, -1000});
  camera->transform3D->anchorPoint->value = {50, 50, 0};
  auto option = new CameraOption();
  option->zoom = new Property<float>(zoom);
  option->depthOfField = new Property<bool>(false);
  option->focusDistance = new Property<float>(1000);
  option->aperture = new Property<float>(0);
  option->blurLevel = new Property<Percent>(0);
  option->irisShape = new Property<IrisShapeType>(IrisShapeType{});
  option->irisRotation = new Property<float>(0);
  option->irisRoundness = new Property<Percent>(0);
  option->irisAspectRatio = new Property<float>(1);
  option->irisDiffractionFringe = new Property<float>(0);
  option->highlightGain = new Property<float>(0);
  option->highlightThreshold = new Property<float>(0);
  option->highlightSaturation = new Property<float>(0);
  camera->cameraOption = option;
  return camera;
}

VectorComposition* Make3DComposition(const std::vector<Layer*>& layers, ID id, int width,
                                     int height) {
  auto composition = new VectorComposition();
  composition->id = id;
  composition->width = width;
  composition->height = height;
  composition->duration = 60;
  composition->frameRate = 30;
  composition->layers = layers;
  for (auto layer : layers) {
    layer->containingComposition = composition;
  }
  return composition;
}

std::shared_ptr<PAGFile> Make3DFile(const std::vector<Layer*>& layers) {
  auto composition = Make3DComposition(layers, 1, 100, 100);
  auto file = Codec::VerifyAndMake({composition}, {});
  return file == nullptr ? nullptr : PAGFile::MakeFrom(file);
}

std::array<uint8_t, 4> SamplePixel(const std::shared_ptr<PAGSurface>& surface, int x, int y) {
  std::vector<uint8_t> pixels(surface->width() * surface->height() * 4);
  EXPECT_TRUE(surface->readPixels(ColorType::RGBA_8888, AlphaType::Premultiplied, pixels.data(),
                                  surface->width() * 4));
  auto offset = static_cast<size_t>((y * surface->width() + x) * 4);
  return {pixels[offset], pixels[offset + 1], pixels[offset + 2], pixels[offset + 3]};
}

std::shared_ptr<PAGPlayer> Make3DPlayer(const std::shared_ptr<PAGFile>& file,
                                        const std::shared_ptr<PAGSurface>& surface) {
  auto player = std::make_shared<PAGPlayer>();
  player->setComposition(file);
  player->setSurface(surface);
  player->flush();
  return player;
}

static void ExportTextCalibration(const std::shared_ptr<PAGFile>& file, const std::string& name) {
  nlohmann::json profile;
  for (int index = 0; index < file->numChildren(); index++) {
    auto pagLayer = file->getLayerAt(index);
    if (pagLayer->layerType() != LayerType::Text) {
      continue;
    }
    auto layer = static_cast<TextLayer*>(pagLayer->layer);
    auto document = layer->sourceText->getValueAt(layer->startTime + pagLayer->contentFrame);
    profile["family"] = document->fontFamily;
    profile["style"] = document->fontStyle;
    profile["size"] = document->fontSize;
    auto lines = GetLines(document.get(), layer->pathOption).first;
    for (const auto& line : lines) {
      nlohmann::json positions = nlohmann::json::array();
      for (const auto& glyph : line) {
        auto bounds = glyph->getBounds();
        auto origin = glyph->getMatrix();
        profile["glyphs"][glyph->getName()] = {
            {"advance", glyph->getAdvance()},
            {"bounds", {bounds.left, bounds.top, bounds.right, bounds.bottom}}};
        positions.push_back({glyph->getName(), origin.getTranslateX(), origin.getTranslateY()});
      }
      profile["lines"].push_back(positions);
    }
  }
  ASSERT_FALSE(profile.empty());
  auto path = ProjectPath::Absolute("test/out/PAGTransform3DTest/" + name + "_text_metrics.json");
  std::filesystem::create_directories(std::filesystem::path(path).parent_path());
  std::ofstream output(path);
  ASSERT_TRUE(output.is_open());
  output << profile.dump(2) << std::endl;
  ASSERT_TRUE(output.good());
}

}  // namespace

/**
 * 用例描述: 单图层 3D 变换，验证 3D 矩阵与投影。
 */
PAG_PIXEL_TEST(PAGTransform3DTest, Transform3D) {
  auto font = PAGFont::RegisterFont(ProjectPath::Absolute("resources/font/NotoSansSC-Regular.otf"),
                                    0, "PingFang SC", "Regular");
  ASSERT_FALSE(font.fontFamily.empty());
  ScopedFontRegistration registration(font);
  auto pagFile = LoadPAGFile("resources/filter/3Dtest.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
  ASSERT_NE(pagSurface, nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);
  pagPlayer->flush();
  ExportTextCalibration(pagFile, "Transform3D");
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
  auto font = PAGFont::RegisterFont(ProjectPath::Absolute("resources/font/NotoSansSC-Regular.otf"),
                                    0, "PingFang SC", "Regular");
  ASSERT_FALSE(font.fontFamily.empty());
  ScopedFontRegistration registration(font);
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
  ExportTextCalibration(pagFile, "Transform3D_Sequence");
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGTransform3DTest/Transform3D_Sequence"));
}

/** 验证三维和二维混合父链、分离位置以及父层不继承透明度。 */
PAG_TEST(PAGTransform3DTest, ParentChainAndSeparatedPosition) {
  NullLayer grandparent;
  grandparent.transform3D = MakeTransform3D({11, 12, 13});
  NullLayer parent;
  parent.transform = Transform2D::MakeDefault().release();
  parent.transform->position->value = {7, 8};
  parent.parent = &grandparent;
  NullLayer child;
  child.transform3D = MakeTransform3D({3, 4, 5});
  child.parent = &parent;
  delete child.transform3D->position;
  child.transform3D->position = nullptr;
  child.transform3D->xPosition = new Property<float>(3);
  child.transform3D->yPosition = new Property<float>(4);
  child.transform3D->zPosition = new Property<float>(5);
  auto point = Transform3DFilter::BuildModelMatrix(&child, 0).mapPoint({0, 0, 0});
  EXPECT_FLOAT_EQ(point.x, 21);
  EXPECT_FLOAT_EQ(point.y, 24);
  EXPECT_FLOAT_EQ(point.z, -18);
  EXPECT_TRUE(Has3DTransform(&parent));
  EXPECT_TRUE(child.transform3D->verify());
  delete child.transform3D->zPosition;
  child.transform3D->zPosition = nullptr;
  EXPECT_FALSE(child.transform3D->verify());
}

/** 验证非法相机 zoom 和退化兴趣点不会产生非有限矩阵。 */
PAG_TEST(PAGTransform3DTest, InvalidCameraZoom) {
  CameraLayer camera;
  camera.transform3D = MakeTransform3D({50, 50, -1000});
  camera.transform3D->anchorPoint->value = {50, 50, 0};
  camera.cameraOption = new CameraOption();
  camera.cameraOption->zoom = new Property<float>(1000);
  for (float zoom : {0.0f, -1.0f, std::numeric_limits<float>::infinity(),
                     std::numeric_limits<float>::quiet_NaN()}) {
    camera.cameraOption->zoom->value = zoom;
    auto context = Transform3DFilter::MakeContext(100, 100, &camera, 0);
    ASSERT_NE(context, nullptr);
    auto point = context->viewProjection.mapPoint({50, 50, 0});
    EXPECT_NEAR(point.x, 0, 0.001f);
    EXPECT_NEAR(point.y, 0, 0.001f);
    EXPECT_TRUE(std::isfinite(point.z));
  }
  camera.transform3D->anchorPoint->value = {50, 50, -1000};
  EXPECT_NE(Transform3DFilter::MakeContext(100, 100, &camera, 0), nullptr);
}

/** 验证近层透明片元不会丢弃远层，且不依赖文件图层 ID。 */
PAG_PIXEL_TEST(PAGTransform3DTest, TransparentDepthOrder) {
  auto near = MakeSolid3D(Red, {30, 30, -100}, 128);
  auto far = MakeSolid3D(Blue, {30, 30, 100}, 255);
  auto file = Make3DFile({far, near});
  ASSERT_NE(file, nullptr);
  auto surface = OffscreenSurface::Make(100, 100);
  ASSERT_NE(surface, nullptr);
  auto player = Make3DPlayer(file, surface);
  auto color = SamplePixel(surface, 50, 50);
  EXPECT_NEAR(color[0], 128, 1);
  EXPECT_NEAR(color[2], 127, 1);
  EXPECT_EQ(color[3], 255);
  player->setCacheEnabled(false);
  player->setProgress(0.5);
  player->flush();
  EXPECT_EQ(SamplePixel(surface, 50, 50), color);
}

/** 验证两个相交半透明平面的前后顺序在交线两侧相反。 */
PAG_PIXEL_TEST(PAGTransform3DTest, IntersectingTransparentPlanes) {
  auto red = MakeSolid3D(Red, {50, 50, 0}, 128);
  auto blue = MakeSolid3D(Blue, {50, 50, 0}, 128);
  red->transform3D->anchorPoint->value = {20, 20, 0};
  blue->transform3D->anchorPoint->value = {20, 20, 0};
  red->transform3D->yRotation->value = 45;
  blue->transform3D->yRotation->value = -45;
  auto file = Make3DFile({red, blue});
  ASSERT_NE(file, nullptr);
  auto surface = OffscreenSurface::Make(100, 100);
  ASSERT_NE(surface, nullptr);
  auto player = Make3DPlayer(file, surface);
  auto left = SamplePixel(surface, 43, 50);
  auto right = SamplePixel(surface, 57, 50);
  EXPECT_GT(left[2], left[0]);
  EXPECT_GT(right[0], right[2]);
  EXPECT_NEAR(left[3], 192, 1);
  EXPECT_NEAR(right[3], 192, 1);
}

/** 验证共面图层使用原叠放顺序。 */
PAG_PIXEL_TEST(PAGTransform3DTest, CoplanarOrder) {
  auto red = MakeSolid3D(Red, {30, 30, 0}, 128);
  auto blue = MakeSolid3D(Blue, {30, 30, 0}, 255);
  auto file = Make3DFile({red, blue});
  ASSERT_NE(file, nullptr);
  auto surface = OffscreenSurface::Make(100, 100);
  ASSERT_NE(surface, nullptr);
  auto player = Make3DPlayer(file, surface);
  auto color = SamplePixel(surface, 50, 50);
  EXPECT_NEAR(color[0], 128, 1);
  EXPECT_NEAR(color[2], 127, 1);
  EXPECT_EQ(color[3], 255);
}

/** 验证投影后的边界及命中与像素位置一致。 */
PAG_PIXEL_TEST(PAGTransform3DTest, ProjectedBoundsAndHitTest) {
  auto solid = MakeSolid3D(Red, {50, 20, 0}, 255);
  auto file = Make3DFile({solid});
  ASSERT_NE(file, nullptr);
  auto surface = OffscreenSurface::Make(100, 100);
  ASSERT_NE(surface, nullptr);
  auto player = Make3DPlayer(file, surface);
  auto layer = file->getLayerAt(0);
  auto bounds = player->getBounds(layer);
  EXPECT_NEAR(bounds.left, 50, 1);
  EXPECT_NEAR(bounds.top, 20, 1);
  EXPECT_NEAR(bounds.right, 90, 1);
  EXPECT_NEAR(bounds.bottom, 60, 1);
  EXPECT_TRUE(player->hitTestPoint(layer, 70, 40, false));
  EXPECT_FALSE(player->hitTestPoint(layer, 20, 40, false));
  EXPECT_TRUE(player->hitTestPoint(layer, 70, 40, true));
  EXPECT_FALSE(player->hitTestPoint(layer, 20, 40, true));
  EXPECT_FALSE(file->getLayersUnderPoint(70, 40).empty());
  EXPECT_TRUE(file->getLayersUnderPoint(20, 40).empty());
}

PAG_PIXEL_TEST(PAGTransform3DTest, RuntimeViewportResizeAndMove) {
  auto solid = MakeSolid3D(Red, {150, 30, 0}, 255);
  auto file = Make3DFile({solid});
  ASSERT_NE(file, nullptr);
  file->setContentSize(200, 100);
  auto surface = OffscreenSurface::Make(200, 100);
  ASSERT_NE(surface, nullptr);
  auto player = Make3DPlayer(file, surface);
  auto layer = file->getLayerAt(0);
  auto color = SamplePixel(surface, 170, 50);
  EXPECT_EQ(color[0], 255);
  EXPECT_EQ(color[3], 255);
  EXPECT_EQ(SamplePixel(surface, 140, 50)[3], 0);
  EXPECT_TRUE(player->hitTestPoint(layer, 170, 50, true));
  EXPECT_FALSE(player->hitTestPoint(layer, 140, 50, true));
  auto bounds = player->getBounds(layer);
  EXPECT_NEAR(bounds.left, 150, 1);
  EXPECT_NEAR(bounds.right, 190, 1);
  player->setCacheEnabled(false);
  player->setProgress(0.5);
  player->flush();
  EXPECT_EQ(SamplePixel(surface, 170, 50), color);
  EXPECT_TRUE(player->hitTestPoint(layer, 170, 50, true));

  auto container = PAGComposition::Make(300, 100);
  ASSERT_NE(container, nullptr);
  ASSERT_TRUE(container->addLayer(layer));
  auto movedSurface = OffscreenSurface::Make(300, 100);
  ASSERT_NE(movedSurface, nullptr);
  player->setComposition(container);
  player->setSurface(movedSurface);
  player->flush();
  EXPECT_EQ(SamplePixel(movedSurface, 170, 50), color);
  EXPECT_TRUE(player->hitTestPoint(layer, 170, 50, true));
  player->setCacheEnabled(true);
  player->setProgress(0.75);
  player->flush();
  EXPECT_EQ(SamplePixel(movedSurface, 170, 50), color);
}

/** 验证活动相机的时间区间、切换与用户可见性，并覆盖场景缓存失效。 */
PAG_PIXEL_TEST(PAGTransform3DTest, ActiveCameraTimelineAndVisibility) {
  auto first = MakeCamera(1000, 0, 30);
  auto second = MakeCamera(2000, 30, 30);
  auto solid = MakeSolid3D(Red, {30, 30, 0}, 255);
  auto file = Make3DFile({second, first, solid});
  ASSERT_NE(file, nullptr);
  auto surface = OffscreenSurface::Make(100, 100);
  ASSERT_NE(surface, nullptr);
  auto player = Make3DPlayer(file, surface);
  EXPECT_EQ(SamplePixel(surface, 15, 50)[3], 0);
  player->setProgress(0.75);
  player->flush();
  EXPECT_EQ(SamplePixel(surface, 15, 50)[0], 255);
  for (int i = 0; i < file->numChildren(); i++) {
    auto layer = file->getLayerAt(i);
    if (layer->layer == second) {
      layer->setVisible(false);
    }
  }
  player->flush();
  EXPECT_EQ(SamplePixel(surface, 15, 50)[3], 0);
  auto solidLayer = file->getLayerAt(0);
  for (int i = 0; i < file->numChildren(); i++) {
    if (file->getLayerAt(i)->layer == solid) {
      solidLayer = file->getLayerAt(i);
    }
  }
  tgfx::Rect measured;
  PAGComposition::MeasureChildLayer(&measured, solidLayer.get());
  EXPECT_NEAR(measured.left, 30, 1);
  EXPECT_NEAR(measured.right, 70, 1);
  EXPECT_TRUE(file->getLayersUnderPoint(15, 50).empty());
  EXPECT_FALSE(file->getLayersUnderPoint(50, 50).empty());
}

PAG_TEST(PAGTransform3DTest, CameraInterestRollAppliedOnce) {
  std::unique_ptr<CameraLayer> camera(MakeCamera(1000, 0, 60));
  for (bool orientation : {false, true}) {
    camera->transform3D->zRotation->value = orientation ? 0 : 90;
    camera->transform3D->orientation->value = orientation ? Point3D{0, 0, 90} : Point3D{0, 0, 0};
    auto context = Transform3DFilter::MakeContext(100, 100, camera.get(), 0);
    ASSERT_NE(context, nullptr);
    tgfx::Point point = {60, 50};
    auto matrix = Transform3DFilter::ProjectedMatrix(nullptr, 0, *context);
    matrix.mapPoints(&point, 1);
    EXPECT_NEAR(point.x, 50, 0.001f);
    EXPECT_NEAR(point.y, 40, 0.001f);
  }
}

/** 验证无兴趣点相机与投影平面的深度方程。 */
PAG_TEST(PAGTransform3DTest, CameraWithoutInterestAndDepthPlane) {
  std::unique_ptr<CameraLayer> camera(MakeCamera(1000, 0, 60));
  delete camera->transform3D->anchorPoint;
  camera->transform3D->anchorPoint = nullptr;
  auto context = Transform3DFilter::MakeContext(100, 100, camera.get(), 0);
  ASSERT_NE(context, nullptr);
  std::unique_ptr<SolidLayer> layer(MakeSolid3D(Red, {50, 50, 0}, 255));
  layer->transform3D->yRotation->value = 30;
  Transform3DDepthPlane plane;
  ASSERT_TRUE(Transform3DFilter::DepthPlane(layer.get(), 0, *context, tgfx::Matrix::I(), &plane));
  auto matrix = Transform3DFilter::BuildModelMatrix(layer.get(), 0);
  matrix.postConcat(context->viewProjection);
  for (int x = 0; x <= 40; x += 10) {
    auto point = matrix.mapPoint({static_cast<float>(x), 20, 0});
    auto depth = plane.x * point.x + plane.y * point.y + plane.z;
    EXPECT_NEAR(depth, (point.z + 1) * 0.5f, 0.00001f);
  }
}

/** 验证投影平面深度恒定时不会因逆矩阵舍入产生伪斜率。 */
PAG_TEST(PAGTransform3DTest, ConstantDepthPlanePrecision) {
  Transform3DContext context;
  context.width = 128;
  context.height = 64;
  std::unique_ptr<SolidLayer> layer(MakeSolid3D(Red, {0, 0, 0}, 255));
  for (float scale : {1.0f, 2.0f, 4.0f, 1024.0f}) {
    SCOPED_TRACE(scale);
    context.viewProjection.setRow(0, {scale, 2 * scale, 0, 10000 * scale});
    context.viewProjection.setRow(1, {3 * scale, 5 * scale, 0, 20000 * scale});
    context.viewProjection.setRow(2, {7 * scale, 11 * scale, scale, 30000 * scale});
    context.viewProjection.setRow(3, {14 * scale, 22 * scale, 0, 60000 * scale});
    for (float zScale : {1.0f, 0.0f}) {
      SCOPED_TRACE(zScale);
      layer->transform3D->scale->value = {1, 1, zScale};
      Transform3DDepthPlane plane;
      ASSERT_TRUE(
          Transform3DFilter::DepthPlane(layer.get(), 0, context, tgfx::Matrix::I(), &plane));
      EXPECT_EQ(plane.x, 0);
      EXPECT_EQ(plane.y, 0);
      EXPECT_EQ(plane.z, 0.75f);
    }
  }
}

/** 验证深度平面的微小斜率不会被单精度求逆中的抵消误差覆盖。 */
PAG_TEST(PAGTransform3DTest, SmallDepthSlopePrecision) {
  Transform3DContext context;
  context.width = 128;
  context.height = 64;
  context.viewProjection.setRow(0, {1, 2, 0, 10000});
  context.viewProjection.setRow(1, {3, 5, 0, 20000});
  context.viewProjection.setRow(3, {14, 22, 0, 60000});
  std::unique_ptr<SolidLayer> layer(MakeSolid3D(Red, {0, 0, 0}, 255));
  for (int offset : {1, 2, 4}) {
    SCOPED_TRACE(offset);
    context.viewProjection.setRow(2, {7, 11, 1, static_cast<float>(30000 - offset)});
    Transform3DDepthPlane plane;
    ASSERT_TRUE(Transform3DFilter::DepthPlane(layer.get(), 0, context, tgfx::Matrix::I(), &plane));
    EXPECT_FLOAT_EQ(plane.x, offset / 10000.0f);
    EXPECT_FLOAT_EQ(plane.y, -3 * offset / 20000.0f);
    EXPECT_FLOAT_EQ(plane.z, static_cast<float>(0.75 + offset / 40000.0));
  }
  context.viewProjection.setRow(1, {1, 2, 0, 10000});
  Transform3DDepthPlane plane;
  EXPECT_FALSE(Transform3DFilter::DepthPlane(layer.get(), 0, context, tgfx::Matrix::I(), &plane));
}

/** 验证精确共面保留层序，且相邻可区分深度仍按真实前后排序。 */
PAG_PIXEL_TEST(PAGTransform3DTest, PreciseCoplanarAndAdjacentDepthOrder) {
  DeviceLockScope lock;
  ASSERT_TRUE(static_cast<bool>(lock));
  auto stage = PAGStage::Make(16, 16);
  RenderCache cache(stage.get());
  cache.attachToContext(lock.context(), false);
  cache.setSnapshotEnabled(false);
  Transform3DContext context;
  context.width = 128;
  context.height = 64;
  context.viewProjection.setRow(0, {1, 2, 0, 10000});
  context.viewProjection.setRow(1, {3, 5, 0, 20000});
  context.viewProjection.setRow(2, {7, 11, 1, 30000});
  context.viewProjection.setRow(3, {14, 22, 0, 60000});
  std::unique_ptr<SolidLayer> layer(MakeSolid3D(Red, {0, 0, 0}, 255));
  Transform3DDepthPlane plane;
  ASSERT_TRUE(Transform3DFilter::DepthPlane(layer.get(), 0, context, tgfx::Matrix::I(), &plane));
  tgfx::Path path;
  path.addRect(tgfx::Rect::MakeWH(16, 16));
  for (bool nearDepth : {false, true}) {
    SCOPED_TRACE(nearDepth);
    Transform3DSceneBuilder builder(16, 16);
    auto blueDepth = nearDepth ? std::nextafter(0.75f, 0.0f) : 0.75f;
    builder.layers.push_back({Shape::MakeFrom(UniqueID::Next(), path, tgfx::Color::Blue()),
                              {0, 0, blueDepth},
                              BlendMode::Normal});
    builder.layers.push_back(
        {Shape::MakeFrom(UniqueID::Next(), path, tgfx::Color::Red()), plane, BlendMode::Normal});
    auto graphic = builder.makeGraphic();
    ASSERT_NE(graphic, nullptr);
    auto surface = tgfx::Surface::Make(lock.context(), 16, 16);
    ASSERT_NE(surface, nullptr);
    Canvas canvas(surface.get(), &cache);
    graphic->draw(&canvas);
    tgfx::Bitmap bitmap(16, 16, false, false);
    tgfx::Pixmap pixels(bitmap);
    ASSERT_TRUE(surface->readPixels(pixels.info(), pixels.writablePixels()));
    auto bytes = static_cast<const uint8_t*>(pixels.pixels());
    for (int y = 0; y < 16; y++) {
      for (int x = 0; x < 16; x++) {
        auto point = bytes + y * pixels.rowBytes() + x * 4;
        EXPECT_EQ(point[0], nearDepth ? 0 : 255) << x << "," << y;
        EXPECT_EQ(point[1], 0) << x << "," << y;
        EXPECT_EQ(point[2], nearDepth ? 255 : 0) << x << "," << y;
        EXPECT_EQ(point[3], 255) << x << "," << y;
      }
    }
  }
  cache.detachFromContext();
}

/** Verifies depth encoding at rounding boundaries and over large pixel coordinates. */
PAG_PIXEL_TEST(PAGTransform3DTest, IntegerDepthEncodingReference) {
  DeviceLockScope lock;
  ASSERT_TRUE(static_cast<bool>(lock));
  auto stage = PAGStage::Make(128, 64);
  RenderCache cache(stage.get());
  cache.attachToContext(lock.context(), false);
  cache.setSnapshotEnabled(false);
  std::vector<Transform3DDepthPlane> planes = {
      {0, 0, 0},
      {0, 0, 1},
      {0, 0, 0.5},
      {0, 0, std::nextafter(0.5, 0.0)},
      {0, 0, std::nextafter(0.5, 1.0)},
      {0, 0, std::nextafter(0.0, -1.0)},
      {0, 0, std::nextafter(1.0, 2.0)},
      {0, 0, std::numeric_limits<double>::denorm_min()},
      {1.0 / 16, -1.0 / 32, 0.5},
      {1, -2, 0.5},
      {1024, -512, 0.5},
      {1073741824, -1073741824, 0.5},
      {1.0 / 1000000, -1.0 / 2000000, 16761986.0 / 16777215.0}};
  for (int width :
       {1, 3, 64, 257, std::min(16384, lock.context()->gpu()->limits()->maxTextureDimension2D)}) {
    auto height = width == 257 ? 129 : 3;
    std::vector<tgfx::Point> points = {
        {0, 0},
        {static_cast<float>(width - 1), 0},
        {0, static_cast<float>(height - 1)},
        {static_cast<float>(width - 1), static_cast<float>(height - 1)},
        {static_cast<float>(width / 2), static_cast<float>(height / 2)}};
    for (const auto& plane : planes) {
      SCOPED_TRACE(testing::Message()
                   << width << "x" << height << " " << plane.x << "," << plane.y << "," << plane.z);
      ASSERT_NO_FATAL_FAILURE(
          CheckDepthEncoding(&cache, lock.context(), plane, width, height, points));
    }
  }
  uint32_t state = 48271;
  std::vector<tgfx::Point> randomPoints;
  for (int i = 0; i < 32; i++) {
    state = state * 1664525u + 1013904223u;
    auto x = state % 128;
    state = state * 1664525u + 1013904223u;
    randomPoints.push_back({static_cast<float>(x), static_cast<float>(state % 64)});
  }
  for (int i = 0; i < 96; i++) {
    state = state * 1664525u + 1013904223u;
    auto x = (static_cast<int>(state % 65536) - 32768) / 65536.0;
    state = state * 1664525u + 1013904223u;
    auto y = (static_cast<int>(state % 65536) - 32768) / 65536.0;
    state = state * 1664525u + 1013904223u;
    auto z = 0.5 + (static_cast<int>(state % 65536) - 32768) / 65536.0;
    auto residual = (i % 2 == 0 ? 1 : -1) / 1099511627776.0;
    ASSERT_NO_FATAL_FAILURE(
        CheckDepthEncoding(&cache, lock.context(), {x, y, z + residual}, 128, 64, randomPoints));
  }
  cache.detachFromContext();
}

/** Verifies fixed bounds and clipping on a dyadic pixel grid to isolate conversion from FMA. */
PAG_PIXEL_TEST(PAGTransform3DTest, FixedDepthRangeAndClipBoundaries) {
  DeviceLockScope lock;
  ASSERT_TRUE(static_cast<bool>(lock));
  auto stage = PAGStage::Make(256, 128);
  RenderCache cache(stage.get());
  cache.attachToContext(lock.context(), false);
  cache.setSnapshotEnabled(false);
  auto halfCode = 17.5 / 16777215.0;
  std::vector<Transform3DDepthPlane> planes = {{0, 0, -1.0 / 72057594037927936.0},
                                               {0, 0, 1.0 / 72057594037927936.0},
                                               {0, 0, std::nextafter(0.0, -1.0)},
                                               {0, 0, std::nextafter(1.0, 0.0)},
                                               {0, 0, 1},
                                               {0, 0, std::nextafter(1.0, 2.0)},
                                               {0, 0, std::nextafter(halfCode, 0.0)},
                                               {0, 0, halfCode},
                                               {0, 0, std::nextafter(halfCode, 1.0)},
                                               {0, 0, 128},
                                               {0, 0, 129},
                                               {128, -128, 0.5},
                                               {129, -129, 0.5},
                                               {1073741824, -1073741824, 0.5},
                                               {1073741824, 0, 0.5},
                                               {0, -1073741824, 0.5}};
  for (int width : {1, 4, 256}) {
    auto height = width == 256 ? 128 : width;
    std::vector<tgfx::Point> points = {
        {0, 0},
        {static_cast<float>(width - 1), 0},
        {0, static_cast<float>(height - 1)},
        {static_cast<float>(width - 1), static_cast<float>(height - 1)},
        {static_cast<float>(width / 2), static_cast<float>(height / 2)}};
    for (const auto& plane : planes) {
      SCOPED_TRACE(testing::Message()
                   << width << "x" << height << " " << plane.x << "," << plane.y << "," << plane.z);
      ASSERT_NO_FATAL_FAILURE(
          CheckDepthEncoding(&cache, lock.context(), plane, width, height, points));
    }
  }
  cache.detachFromContext();
}

/** Verifies cube boundary depth codes against binary64 evaluation of the same model matrices. */
PAG_PIXEL_TEST(PAGTransform3DTest, BoxBoundaryDepthEncoding) {
  DeviceLockScope lock;
  ASSERT_TRUE(static_cast<bool>(lock));
  auto file = LoadPAGFile("resources/filter/3D_BOX.pag");
  ASSERT_NE(file, nullptr);
  ASSERT_GT(file->numChildren(), 0);
  auto composition =
      static_cast<VectorComposition*>(file->getLayerAt(0)->layer->containingComposition);
  auto context = Transform3DFilter::MakeContext(composition, 83);
  ASSERT_NE(context, nullptr);
  auto stage = PAGStage::Make(720, 1280);
  RenderCache cache(stage.get());
  cache.attachToContext(lock.context(), false);
  cache.setSnapshotEnabled(false);
  int checked = 0;
  for (int i = 0; i < file->numChildren(); i++) {
    auto layer = file->getLayerAt(i)->layer;
    if (layer->id != 85 && layer->id != 89 && layer->id != 90 && layer->id != 93) {
      continue;
    }
    Transform3DDepthPlane plane;
    ASSERT_TRUE(Transform3DFilter::DepthPlane(layer, 83, *context, tgfx::Matrix::I(), &plane));
    ASSERT_NO_FATAL_FAILURE(CheckDepthEncoding(&cache, lock.context(), plane, 720, 1280,
                                               {{595, 550},
                                                {508, 752},
                                                {467, 796},
                                                {440, 825},
                                                {282, 831},
                                                {413, 854},
                                                {350, 888},
                                                {481, 781},
                                                {399, 869},
                                                {372, 898}}));
    ASSERT_NO_FATAL_FAILURE(
        CheckDepthEncoding(&cache, lock.context(), plane, 360, 640, {{82, 366}, {247, 383}}));
    checked++;
  }
  EXPECT_EQ(checked, 4);
  cache.detachFromContext();
}

/** Verifies accurate ordering at cube boundary pixels with and without scene caching. */
PAG_PIXEL_TEST(PAGTransform3DTest, BoxBoundaryDepthOrder) {
  auto file = LoadPAGFile("resources/filter/3D_BOX.pag");
  ASSERT_NE(file, nullptr);
  auto surface = OffscreenSurface::Make(720, 1280);
  ASSERT_NE(surface, nullptr);
  auto player = Make3DPlayer(file, surface);
  player->setProgress(1.0);
  player->flush();
  for (bool cacheEnabled : {false, true}) {
    SCOPED_TRACE(cacheEnabled);
    player->setCacheEnabled(cacheEnabled);
    surface->clearAll();
    player->flush();
    auto first = SamplePixel(surface, 350, 888);
    auto second = SamplePixel(surface, 399, 869);
    EXPECT_GT(first[2], first[0]);
    EXPECT_GT(second[2], second[0]);
    EXPECT_EQ(first[3], 255);
    EXPECT_EQ(second[3], 255);
  }
  auto scaled = OffscreenSurface::Make(360, 640);
  ASSERT_NE(scaled, nullptr);
  player->setSurface(scaled);
  player->setMatrix(Matrix::MakeScale(0.5f, 0.5f));
  for (bool cacheEnabled : {false, true}) {
    SCOPED_TRACE(cacheEnabled);
    player->setCacheEnabled(cacheEnabled);
    scaled->clearAll();
    player->flush();
    auto pixel = SamplePixel(scaled, 247, 383);
    EXPECT_GT(pixel[2], pixel[0]);
    EXPECT_EQ(pixel[3], 255);
  }
}

PAG_TEST(PAGTransform3DTest, ZeroZScaleDepthPlane) {
  Transform3DContext context;
  context.width = 100;
  context.height = 80;
  context.viewProjection.setRow(2, {2, 4, 1, 5});
  context.viewProjection.setRow(3, {1, 0, 0, 2});
  for (bool parentScale : {false, true}) {
    NullLayer parent;
    parent.transform3D = MakeTransform3D({7, 9, 7});
    parent.transform3D->scale->value = {1, 1, 0};
    std::unique_ptr<SolidLayer> layer(MakeSolid3D(Red, {11, 13, 7}, 255));
    if (parentScale) {
      layer->transform3D->position->value = {11, 13, 90};
      layer->parent = &parent;
    } else {
      layer->transform3D->scale->value = {1, 1, 0};
    }
    Transform3DDepthPlane plane;
    ASSERT_TRUE(Transform3DFilter::DepthPlane(layer.get(), 0, context, tgfx::Matrix::I(), &plane));
    EXPECT_NEAR(plane.x, 1.5f, 0.00001f);
    EXPECT_NEAR(plane.y, 2, 0.00001f);
    EXPECT_NEAR(plane.z, 0, 0.00001f);
    ASSERT_TRUE(Transform3DFilter::DepthPlane(layer.get(), 0, context,
                                              tgfx::Matrix::MakeTrans(50, -40), &plane));
    EXPECT_NEAR(plane.x, 1.5f, 0.00001f);
    EXPECT_NEAR(plane.y, 2, 0.00001f);
    EXPECT_NEAR(plane.z, 0.5f, 0.00001f);
  }
}

PAG_PIXEL_TEST(PAGTransform3DTest, ZeroZScaleMatchesVisiblePlane) {
  for (bool parentScale : {false, true}) {
    std::vector<uint8_t> reference;
    for (bool zeroScale : {false, true}) {
      auto red = MakeSolid3D(Red, {30, 30, 100}, 128);
      auto blue = MakeSolid3D(Blue, {30, 30, 200}, 255);
      std::vector<Layer*> layers = {red, blue};
      if (parentScale) {
        auto parent = new NullLayer();
        parent->id = 1;
        parent->duration = 60;
        parent->transform3D = MakeTransform3D({10, 10, 100});
        parent->transform3D->scale->value = {1, 1, zeroScale ? 0.0f : 1.0f};
        parent->transform3D->yRotation->value = 30;
        red->id = 2;
        red->parent = parent;
        red->transform3D->position->value = {20, 20, zeroScale ? 90.0f : 0.0f};
        layers.push_back(parent);
      } else {
        red->transform3D->scale->value = {1, 1, zeroScale ? 0.0f : 1.0f};
        red->transform3D->yRotation->value = 30;
      }
      auto file = Make3DFile(layers);
      ASSERT_NE(file, nullptr);
      auto surface = OffscreenSurface::Make(100, 100);
      ASSERT_NE(surface, nullptr);
      auto player = Make3DPlayer(file, surface);
      EXPECT_GT(SamplePixel(surface, 50, 50)[0], 0);
      std::vector<uint8_t> pixels(100 * 100 * 4);
      ASSERT_TRUE(surface->readPixels(ColorType::RGBA_8888, AlphaType::Premultiplied, pixels.data(),
                                      100 * 4));
      if (zeroScale) {
        EXPECT_EQ(pixels, reference);
      } else {
        reference = std::move(pixels);
      }
    }
  }
}

/** 验证非零特效原点不会在三维阶段被重复累计。 */
PAG_PIXEL_TEST(PAGTransform3DTest, CornerPinOffset) {
  auto solid = MakeSolid3D(Red, {30, 30, 0}, 255);
  auto effect = new CornerPinEffect();
  effect->upperLeft = new Property<Point>({-10, -10});
  effect->upperRight = new Property<Point>({30, -10});
  effect->lowerLeft = new Property<Point>({-10, 30});
  effect->lowerRight = new Property<Point>({30, 30});
  solid->effects.push_back(effect);
  auto file = Make3DFile({solid});
  ASSERT_NE(file, nullptr);
  auto surface = OffscreenSurface::Make(100, 100);
  ASSERT_NE(surface, nullptr);
  auto player = Make3DPlayer(file, surface);
  EXPECT_EQ(SamplePixel(surface, 25, 25)[0], 255);
  EXPECT_EQ(SamplePixel(surface, 65, 65)[3], 0);
  auto bounds = player->getBounds(file->getLayerAt(0));
  EXPECT_NEAR(bounds.left, 20, 1);
  EXPECT_NEAR(bounds.right, 60, 1);
  EXPECT_FALSE(file->getLayersUnderPoint(25, 25).empty());
  EXPECT_TRUE(file->getLayersUnderPoint(65, 65).empty());
  EXPECT_TRUE(player->hitTestPoint(file->getLayerAt(0), 25, 25, true));
  EXPECT_FALSE(player->hitTestPoint(file->getLayerAt(0), 65, 65, true));
}

/** 三维场景所有混合模式均须与现有二维 tgfx 合成得到相同像素。 */
PAG_PIXEL_TEST(PAGTransform3DTest, BlendModesMatch2D) {
  for (int mode = 0; mode <= static_cast<int>(BlendMode::Add); mode++) {
    auto back3D = MakeSolid2D({100, 150, 200}, {0, 0}, 100, 100);
    auto lower3D = MakeSolid3D({150, 70, 30}, {30, 30, 0}, 180);
    auto upper3D = MakeSolid3D({50, 170, 230}, {30, 30, 0}, 128);
    upper3D->blendMode = static_cast<BlendMode>(mode);
    auto file3D = Make3DFile({upper3D, lower3D, back3D});
    ASSERT_NE(file3D, nullptr);
    auto surface3D = OffscreenSurface::Make(100, 100);
    ASSERT_NE(surface3D, nullptr);
    auto player3D = Make3DPlayer(file3D, surface3D);
    auto color3D = SamplePixel(surface3D, 50, 50);
    auto back2D = MakeSolid2D({100, 150, 200}, {0, 0}, 100, 100);
    auto lower2D = MakeSolid2D({150, 70, 30}, {30, 30}, 40, 40);
    auto upper2D = MakeSolid2D({50, 170, 230}, {30, 30}, 40, 40);
    lower2D->transform->opacity->value = 180;
    upper2D->transform->opacity->value = 128;
    upper2D->blendMode = static_cast<BlendMode>(mode);
    auto file2D = Make3DFile({upper2D, lower2D, back2D});
    ASSERT_NE(file2D, nullptr);
    auto surface2D = OffscreenSurface::Make(100, 100);
    ASSERT_NE(surface2D, nullptr);
    auto player2D = Make3DPlayer(file2D, surface2D);
    auto color2D = SamplePixel(surface2D, 50, 50);
    for (size_t channel = 0; channel < color3D.size(); channel++) {
      EXPECT_NEAR(color3D[channel], color2D[channel], 1) << mode << " " << channel;
    }
  }
}

/** 验证二维子层继承三维父链，且图层自身透明度只应用一次。 */
PAG_PIXEL_TEST(PAGTransform3DTest, Child2DIn3DParent) {
  auto parent = new NullLayer();
  parent->id = 1;
  parent->duration = 60;
  parent->transform3D = MakeTransform3D({30, 20, 0});
  auto child = MakeSolid2D(Red, {10, 10}, 40, 40);
  child->id = 2;
  child->parent = parent;
  child->transform->opacity->value = 128;
  auto file = Make3DFile({child, parent});
  ASSERT_NE(file, nullptr);
  auto surface = OffscreenSurface::Make(100, 100);
  ASSERT_NE(surface, nullptr);
  auto player = Make3DPlayer(file, surface);
  EXPECT_NEAR(SamplePixel(surface, 50, 50)[3], 128, 1);
  EXPECT_EQ(SamplePixel(surface, 20, 20)[3], 0);
}

/** 验证相机后方的三维层既不绘制，也不参与边界和命中查询。 */
PAG_PIXEL_TEST(PAGTransform3DTest, BehindCameraIsInvisible) {
  auto solid = MakeSolid3D(Red, {30, 30, -2000}, 255);
  auto file = Make3DFile({solid});
  ASSERT_NE(file, nullptr);
  auto surface = OffscreenSurface::Make(100, 100);
  ASSERT_NE(surface, nullptr);
  auto player = Make3DPlayer(file, surface);
  auto layer = file->getLayerAt(0);
  EXPECT_EQ(SamplePixel(surface, 50, 50)[3], 0);
  EXPECT_TRUE(player->getBounds(layer).isEmpty());
  EXPECT_FALSE(player->hitTestPoint(layer, 50, 50, false));
  EXPECT_FALSE(player->hitTestPoint(layer, 50, 50, true));
  EXPECT_TRUE(file->getLayersUnderPoint(50, 50).empty());
}

/** 验证三维所需格式和资源能力不足时不启动错误的合成。 */
PAG_TEST(PAGTransform3DTest, GPUCapabilityFallback) {
  Test3DGPU gpu;
  EXPECT_FALSE(Has3DSupport(nullptr));
  EXPECT_TRUE(Has3DSupport(&gpu));
  gpu.featureData.stencilAttachmentSupported = false;
  EXPECT_FALSE(Has3DSupport(&gpu));
  gpu.featureData.stencilAttachmentSupported = true;
  gpu.depthRenderable = false;
  EXPECT_TRUE(Has3DSupport(&gpu));
  gpu.depthRenderable = true;
  gpu.colorRenderable = false;
  EXPECT_FALSE(Has3DSupport(&gpu));
  gpu.colorRenderable = true;
  gpu.limitData.maxSamplersPerShaderStage = 2;
  EXPECT_FALSE(Has3DSupport(&gpu));
  gpu.limitData.maxSamplersPerShaderStage = 3;
  gpu.limitData.maxUniformBufferBindingSize = 63;
  EXPECT_FALSE(Has3DSupport(&gpu));
  gpu.limitData.maxUniformBufferBindingSize = 64;
  gpu.limitData.maxTextureDimension2D = 0;
  EXPECT_FALSE(Has3DSupport(&gpu));
  Transform3DSceneFilter filter(nullptr, {}, {}, {}, 64, 64);
  Transform3DSceneFilter::Resources resources;
  EXPECT_FALSE(filter.prepareTextures(&resources, &gpu));
  EXPECT_EQ(gpu.textureCount, 0);
}

/** 验证整数极限尺寸、高倍缩放在纹理上限内，并受显存预算降采样约束。 */
PAG_TEST(PAGTransform3DTest, GPUScaleLimits) {
  Test3DGPU gpu;
  EXPECT_FLOAT_EQ(Clamp3DScale(&gpu, 0, 0, 100, 100, 1), 1);
  for (float width : {100.0f, static_cast<float>(INT_MAX), std::numeric_limits<float>::max()}) {
    auto scale = Clamp3DScale(&gpu, 0, 0, width, 100, std::numeric_limits<float>::max());
    ASSERT_GT(scale, 0);
    EXPECT_LE(std::ceil(static_cast<double>(width) * scale), 1024);
    EXPECT_LE(std::ceil(100.0 * scale), 1024);
  }
  EXPECT_EQ(Clamp3DScale(&gpu, 0, 0, 100, 100, 0), 0);
  EXPECT_EQ(Clamp3DScale(&gpu, 0, 0, -1, 100, 1), 0);
  EXPECT_EQ(Clamp3DScale(&gpu, 0, 0, std::numeric_limits<float>::infinity(), 100, 1), 0);
  EXPECT_EQ(Clamp3DScale(&gpu, 0, 0, 100, 100, std::numeric_limits<float>::quiet_NaN()), 0);
  EXPECT_EQ(Clamp3DScale(nullptr, 0, 0, 100, 100, 1), 0);
  // 显存预算：10 张每像素 4 字节的 100x100 全幅纹理在 scale=1 需 400000 字节，
  // 预算 100000 应按面积比降采样到 0.5；预算或纹理数为 0 时不约束。
  EXPECT_FLOAT_EQ(Clamp3DScale(&gpu, 100000, 10, 100, 100, 1), 0.5f);
  EXPECT_FLOAT_EQ(Clamp3DScale(&gpu, 0, 10, 100, 100, 1), 1);
  EXPECT_FLOAT_EQ(Clamp3DScale(&gpu, 100000, 0, 100, 100, 1), 1);
  EXPECT_LE(Clamp3DScale(&gpu, 400000, 10, 100, 100, 1), 1);
}

/** 验证持续使用的深度纹理不会被空闲回收，尺寸变化和分配失败可恢复。 */
PAG_TEST(PAGTransform3DTest, SceneResourceReuseAndResize) {
  DeviceLockScope lock;
  ASSERT_TRUE(static_cast<bool>(lock));
  Test3DGPU gpu(lock.context()->gpu());
  Transform3DSceneFilter filter(nullptr, {}, {}, {}, 64, 64);
  Transform3DSceneFilter::Resources resources;
  ASSERT_TRUE(filter.prepareTextures(&resources, &gpu));
  auto depth = resources.depth;
  auto color = resources.color;
  EXPECT_EQ(gpu.textureCount, 8);
  for (int frame = 0; frame < 5; frame++) {
    resources.onFrameEnd();
    ASSERT_TRUE(filter.prepareTextures(&resources, &gpu));
    EXPECT_EQ(resources.depth, depth);
    EXPECT_EQ(resources.color, color);
  }
  EXPECT_EQ(gpu.textureCount, 8);
  resources.onFrameEnd();
  resources.onFrameEnd();
  EXPECT_EQ(resources.depth, depth);
  resources.onFrameEnd();
  EXPECT_EQ(resources.depth, nullptr);
  ASSERT_TRUE(filter.prepareTextures(&resources, &gpu));
  EXPECT_EQ(gpu.textureCount, 16);
  Transform3DSceneFilter resized(nullptr, {}, {}, {}, 128, 32);
  ASSERT_TRUE(resized.prepareTextures(&resources, &gpu));
  EXPECT_EQ(resources.depth->width(), 128);
  EXPECT_EQ(resources.depth->height(), 32);
  gpu.failTexture = gpu.textureCount + 3;
  EXPECT_FALSE(filter.prepareTextures(&resources, &gpu));
  EXPECT_EQ(resources.width, 0);
  EXPECT_EQ(resources.height, 0);
  ASSERT_TRUE(filter.prepareTextures(&resources, &gpu));
  EXPECT_EQ(resources.depth->width(), 64);
  EXPECT_EQ(resources.depth->height(), 64);
  Transform3DSceneFilter tooLarge(nullptr, {}, {}, {}, gpu.limitData.maxTextureDimension2D + 1, 64);
  auto count = gpu.textureCount;
  EXPECT_FALSE(tooLarge.prepareTextures(&resources, &gpu));
  EXPECT_EQ(gpu.textureCount, count);
}

/** 验证静态三维场景复用录制和快照，修改属性后正确失效。 */
PAG_PIXEL_TEST(PAGTransform3DTest, ModifiedSceneCacheInvalidation) {
  auto near = MakeSolid3D(Red, {30, 30, -100}, 128);
  auto far = MakeSolid3D(Blue, {30, 30, 100}, 255);
  auto file = Make3DFile({far, near});
  ASSERT_NE(file, nullptr);
  auto edited = file->getLayerAt(0);
  edited->setAlpha(0.5f);
  auto surface = OffscreenSurface::Make(100, 100);
  ASSERT_NE(surface, nullptr);
  auto player = Make3DPlayer(file, surface);
  auto first = GetCached3DGraphic(file.get());
  ASSERT_NE(first, nullptr);
  auto cacheSize = player->renderCache->snapshotCaches.size();
  auto color = SamplePixel(surface, 50, 50);
  for (int i = 0; i < 4; i++) {
    player->setProgress((i + 1) * 0.1);
    Recorder recorder;
    file->draw(&recorder);
    EXPECT_EQ(GetCached3DGraphic(file.get()), first);
    surface->clearAll();
    player->flush();
    EXPECT_EQ(player->renderCache->snapshotCaches.size(), cacheSize);
    EXPECT_EQ(SamplePixel(surface, 50, 50), color);
  }
  edited->setAlpha(1);
  player->flush();
  EXPECT_NE(GetCached3DGraphic(file.get()), first);
  EXPECT_NE(SamplePixel(surface, 50, 50), color);
  auto second = GetCached3DGraphic(file.get());
  edited->setVisible(false);
  player->flush();
  EXPECT_NE(GetCached3DGraphic(file.get()), second);
}

/** 验证相机动画会使已修改合成的录制缓存失效。 */
PAG_PIXEL_TEST(PAGTransform3DTest, AnimatedSceneCacheInvalidation) {
  auto first = MakeCamera(1000, 0, 30);
  auto second = MakeCamera(2000, 30, 30);
  auto solid = MakeSolid3D(Red, {30, 30, 0}, 255);
  auto file = Make3DFile({second, first, solid});
  ASSERT_NE(file, nullptr);
  file->getLayerAt(0)->setAlpha(0.5f);
  auto surface = OffscreenSurface::Make(100, 100);
  ASSERT_NE(surface, nullptr);
  auto player = Make3DPlayer(file, surface);
  auto graphic = GetCached3DGraphic(file.get());
  ASSERT_NE(graphic, nullptr);
  player->setProgress(0.75);
  player->flush();
  EXPECT_NE(GetCached3DGraphic(file.get()), graphic);
  EXPECT_GT(SamplePixel(surface, 15, 50)[3], 0);
}

/** 验证同一文件的不同尺寸合成独立清除深度，不互相污染。 */
PAG_PIXEL_TEST(PAGTransform3DTest, MultipleCompositionDepthIsolation) {
  auto nearA = MakeSolid3D(Red, {20, 30, -100}, 128);
  auto farA = MakeSolid3D(Blue, {20, 30, 100}, 255);
  auto first = Make3DComposition({farA, nearA}, 1, 80, 100);
  auto nearB = MakeSolid3D(Green, {20, 30, -100}, 128);
  auto farB = MakeSolid3D(Red, {20, 30, 100}, 255);
  auto second = Make3DComposition({farB, nearB}, 2, 100, 100);
  auto firstLayer = PreComposeLayer::Wrap(first).release();
  auto secondLayer = PreComposeLayer::Wrap(second).release();
  secondLayer->transform->position->value = {100, 0};
  auto root = Make3DComposition({secondLayer, firstLayer}, 3, 200, 100);
  auto rawFile = Codec::VerifyAndMake({first, second, root}, {});
  ASSERT_NE(rawFile, nullptr);
  auto file = PAGFile::MakeFrom(rawFile);
  ASSERT_NE(file, nullptr);
  auto surface = OffscreenSurface::Make(200, 100);
  ASSERT_NE(surface, nullptr);
  auto player = Make3DPlayer(file, surface);
  auto colorA = SamplePixel(surface, 40, 50);
  auto colorB = SamplePixel(surface, 140, 50);
  EXPECT_NEAR(colorA[0], 128, 1);
  EXPECT_NEAR(colorA[2], 127, 1);
  EXPECT_EQ(colorA[3], 255);
  EXPECT_NEAR(colorB[1], 128, 1);
  EXPECT_NEAR(colorB[0], 127, 1);
  EXPECT_EQ(colorB[3], 255);
  player->setCacheEnabled(false);
  for (int i = 0; i < 3; i++) {
    surface->clearAll();
    player->flush();
    EXPECT_EQ(SamplePixel(surface, 40, 50), colorA);
    EXPECT_EQ(SamplePixel(surface, 140, 50), colorB);
  }
}

/** 验证图层输入倍率不同时仍共用统一视口，输出尺寸改变后不复用旧深度。 */
PAG_PIXEL_TEST(PAGTransform3DTest, DifferentInputScalesAndSurfaceResize) {
  auto near = MakeSolid3D(Red, {30, 30, -100}, 128);
  auto far = MakeSolid3D(Blue, {30, 30, 100}, 255);
  auto effect = new CornerPinEffect();
  effect->upperLeft = new Property<Point>({0, 0});
  effect->upperRight = new Property<Point>({80, 0});
  effect->lowerLeft = new Property<Point>({0, 80});
  effect->lowerRight = new Property<Point>({80, 80});
  near->effects.push_back(effect);
  auto file = Make3DFile({far, near});
  ASSERT_NE(file, nullptr);
  auto surface = OffscreenSurface::Make(100, 100);
  ASSERT_NE(surface, nullptr);
  auto player = Make3DPlayer(file, surface);
  player->setCacheEnabled(false);
  for (int size : {100, 50, 120, 25, 100}) {
    auto resized = OffscreenSurface::Make(size, size);
    ASSERT_NE(resized, nullptr);
    player->setSurface(resized);
    player->flush();
    auto color = SamplePixel(resized, size / 2, size / 2);
    EXPECT_NEAR(color[0], 128, 1);
    EXPECT_NEAR(color[2], 127, 1);
    EXPECT_EQ(color[3], 255);
  }
}

/** Checks discrete projected coverage independently of backend linear-filter rounding. */
PAG_PIXEL_TEST(PAGTransform3DTest, FractionalViewportPreservesProjection) {
  DeviceLockScope lock;
  ASSERT_TRUE(static_cast<bool>(lock));
  auto stage = PAGStage::Make(101, 101);
  RenderCache cache(stage.get());
  cache.attachToContext(lock.context(), false);
  cache.setSnapshotEnabled(false);
  std::unique_ptr<CameraLayer> camera(MakeCamera(1000, 0, 60));
  camera->transform3D->position->value = {51, 51, -1000};
  camera->transform3D->anchorPoint->value = {51, 51, 0};
  auto context = Transform3DFilter::MakeContext(101, 101, camera.get(), 0);
  ASSERT_NE(context, nullptr);
  std::unique_ptr<SolidLayer> layer(MakeSolid3D(Red, {63, 31, 0}, 255));
  layer->width = 20;
  layer->height = 20;
  for (float sourceScale : {0.5f, 1.5f}) {
    SCOPED_TRACE(sourceScale);
    auto size = static_cast<int>(20 * sourceScale);
    tgfx::PictureRecorder recorder;
    auto inputCanvas = recorder.beginRecording();
    tgfx::Paint paint;
    paint.setColor(tgfx::Color::Red());
    inputCanvas->drawRect(tgfx::Rect::MakeWH(size, size), paint);
    auto input = tgfx::Image::MakeFrom(recorder.finishRecordingAsPicture(), size, size);
    ASSERT_NE(input, nullptr);
    tgfx::Point offset;
    auto output =
        Transform3DFilter::Apply(input, &cache, layer.get(), 0, tgfx::Rect::MakeWH(20, 20),
                                 {sourceScale, sourceScale}, *context, &offset);
    ASSERT_NE(output, nullptr);
    auto surface = tgfx::Surface::Make(lock.context(), 51, 51);
    ASSERT_NE(surface, nullptr);
    auto canvas = surface->getCanvas();
    canvas->scale(0.5f / sourceScale, 0.5f / sourceScale);
    canvas->drawImage(output,
                      tgfx::SamplingOptions(tgfx::FilterMode::Nearest, tgfx::MipmapMode::None));
    auto inside = surface->getColor(31, 16);
    EXPECT_NEAR(inside.red, 1, 1.0f / 255);
    EXPECT_NEAR(inside.alpha, 1, 1.0f / 255);
    EXPECT_EQ(inside.green, 0);
    EXPECT_EQ(inside.blue, 0);
    EXPECT_EQ(inside.red, inside.alpha);
    EXPECT_FLOAT_EQ(surface->getColor(30, 16).alpha, 0);
    EXPECT_EQ(surface->getColor(36, 20), tgfx::PMColor::Red());
  }
  cache.detachFromContext();
}

PAG_PIXEL_TEST(PAGTransform3DTest, FractionalViewportSlopedDepth) {
  DeviceLockScope lock;
  ASSERT_TRUE(static_cast<bool>(lock));
  auto stage = PAGStage::Make(101, 101);
  RenderCache cache(stage.get());
  cache.attachToContext(lock.context(), false);
  cache.setSnapshotEnabled(false);
  tgfx::Path path;
  path.addRect(tgfx::Rect::MakeWH(101, 101));
  Transform3DSceneBuilder builder(101, 101);
  // 交线 2*x+y=168.5 两侧的稳定颜色区域用于对照，并非相同几何点。
  // full(61,43) 的像素 cell 中 2*x+y 为 165..168，全红；full(63,43) 为 169..172，全蓝。
  builder.layers.push_back({Shape::MakeFrom(UniqueID::Next(), path, tgfx::Color::Red()),
                            {0.25f, 0.125f, 0.5f},
                            BlendMode::Normal});
  builder.layers.push_back({Shape::MakeFrom(UniqueID::Next(), path, tgfx::Color::Blue()),
                            {0, 0, 0.125f + 337.0f / 808},
                            BlendMode::Normal});
  auto graphic = builder.makeGraphic();
  ASSERT_NE(graphic, nullptr);
  auto full = tgfx::Surface::Make(lock.context(), 102, 102);
  auto half = tgfx::Surface::Make(lock.context(), 51, 51);
  ASSERT_NE(full, nullptr);
  ASSERT_NE(half, nullptr);
  Canvas fullCanvas(full.get(), &cache);
  graphic->draw(&fullCanvas);
  Canvas halfCanvas(half.get(), &cache);
  halfCanvas.scale(0.5f, 0.5f);
  graphic->draw(&halfCanvas);
  EXPECT_FLOAT_EQ(half->getColor(30, 21).red, 1);
  EXPECT_FLOAT_EQ(half->getColor(31, 21).blue, 1);
  EXPECT_EQ(half->getColor(30, 21), tgfx::PMColor::Red());
  EXPECT_EQ(half->getColor(31, 21), tgfx::PMColor::Blue());
  EXPECT_EQ(half->getColor(30, 21), full->getColor(61, 43));
  EXPECT_EQ(half->getColor(31, 21), full->getColor(63, 43));
  cache.detachFromContext();
}

PAG_PIXEL_TEST(PAGTransform3DTest, FractionalCornerPinIntersectingPlanes) {
  auto red = MakeSolid3D(Red, {38, 43, 0}, 128);
  auto blue = MakeSolid3D(Blue, {38, 43, 0}, 128);
  red->transform3D->anchorPoint->value = {20, 20, 0};
  red->transform3D->scale->value = {3, 3, 1};
  blue->transform3D->anchorPoint->value = {60, 60, 0};
  red->transform3D->yRotation->value = 45;
  blue->transform3D->yRotation->value = -45;
  auto effect = new CornerPinEffect();
  effect->upperLeft = new Property<Point>({0, 0});
  effect->upperRight = new Property<Point>({120, 0});
  effect->lowerLeft = new Property<Point>({0, 120});
  effect->lowerRight = new Property<Point>({120, 120});
  blue->effects.push_back(effect);
  auto file = Make3DFile({red, blue});
  ASSERT_NE(file, nullptr);
  file->setContentSize(101, 101);
  auto full = OffscreenSurface::Make(102, 102);
  ASSERT_NE(full, nullptr);
  auto player = std::make_shared<PAGPlayer>();
  player->setComposition(file);
  player->setSurface(full);
  player->setMatrix(Matrix::MakeTrans(0.5f, 0.5f));
  player->setCacheEnabled(false);
  player->flush();
  auto left = SamplePixel(full, 29, 43);
  auto right = SamplePixel(full, 47, 43);
  EXPECT_GT(left[2], left[0]);
  EXPECT_GT(right[0], right[2]);
  auto half = OffscreenSurface::Make(51, 51);
  ASSERT_NE(half, nullptr);
  player->setSurface(half);
  player->setMatrix(Matrix::MakeScale(0.5f, 0.5f));
  for (bool cacheEnabled : {false, true}) {
    player->setCacheEnabled(cacheEnabled);
    player->setProgress(cacheEnabled ? 0.75f : 0.5f);
    player->flush();
    auto halfLeft = SamplePixel(half, 14, 21);
    auto halfRight = SamplePixel(half, 23, 21);
    for (size_t channel = 0; channel < left.size(); channel++) {
      EXPECT_NEAR(halfLeft[channel], left[channel], 1);
      EXPECT_NEAR(halfRight[channel], right[channel], 1);
    }
    EXPECT_GT(halfLeft[2], halfLeft[0]);
    EXPECT_GT(halfRight[0], halfRight[2]);
  }
}

/** 验证隐藏相机后轨道遮罩的测量、用户矩阵命中与透明度和渲染一致。 */
PAG_PIXEL_TEST(PAGTransform3DTest, HiddenCameraTrackMatteBounds) {
  auto camera = MakeCamera(2000, 0, 60);
  auto target = MakeSolid3D(Red, {30, 30, 0}, 255);
  auto matte = MakeSolid3D(White, {50, 30, 0}, 255);
  matte->id = 1;
  target->id = 2;
  matte->width = 20;
  matte->isActive = false;
  target->trackMatteLayer = matte;
  target->trackMatteType = TrackMatteType::Alpha;
  auto file = Make3DFile({camera, matte, target});
  ASSERT_NE(file, nullptr);
  std::shared_ptr<PAGLayer> targetLayer;
  for (int i = 0; i < file->numChildren(); i++) {
    auto layer = file->getLayerAt(i);
    if (layer->layer == camera) {
      layer->setVisible(false);
    }
    if (layer->layer == target) {
      targetLayer = layer;
    }
  }
  ASSERT_NE(targetLayer, nullptr);
  targetLayer->setMatrix(Matrix::MakeTrans(20, 0));
  auto surface = OffscreenSurface::Make(100, 100);
  ASSERT_NE(surface, nullptr);
  auto player = Make3DPlayer(file, surface);
  EXPECT_EQ(SamplePixel(surface, 60, 50)[0], 255);
  EXPECT_EQ(SamplePixel(surface, 60, 50)[3], 255);
  EXPECT_EQ(SamplePixel(surface, 80, 50)[3], 0);
  EXPECT_TRUE(player->hitTestPoint(targetLayer, 60, 50, true));
  EXPECT_FALSE(player->hitTestPoint(targetLayer, 80, 50, true));
  tgfx::Rect bounds;
  PAGComposition::MeasureChildLayer(&bounds, targetLayer.get());
  EXPECT_NEAR(bounds.left, 50, 1);
  EXPECT_NEAR(bounds.right, 70, 1);
  EXPECT_FALSE(file->getLayersUnderPoint(60, 50).empty());
  EXPECT_TRUE(file->getLayersUnderPoint(80, 50).empty());

  targetLayer->setAlpha(0);
  ASSERT_TRUE(player->flush());
  EXPECT_EQ(SamplePixel(surface, 60, 50)[3], 0);
  EXPECT_FALSE(player->hitTestPoint(targetLayer, 60, 50, true));
  targetLayer->setAlpha(1);
  targetLayer->setMatrix(Matrix::MakeScale(0, 1));
  EXPECT_FALSE(player->hitTestPoint(targetLayer, 60, 50, true));
  targetLayer->setMatrix(Matrix::MakeTrans(std::numeric_limits<float>::quiet_NaN(), 0));
  EXPECT_FALSE(player->hitTestPoint(targetLayer, 60, 50, true));
}

/** 验证三维和二维父链的旋转、缩放及锚点顺序符合独立坐标计算。 */
PAG_TEST(PAGTransform3DTest, ParentChainRotationScaleReference) {
  NullLayer grandparent;
  grandparent.transform3D = MakeTransform3D({10, 20, 30});
  grandparent.transform3D->scale->value = {2, 2, 2};
  grandparent.transform3D->yRotation->value = 90;
  NullLayer parent;
  parent.transform = Transform2D::MakeDefault().release();
  parent.transform->scale->value = {2, 3};
  parent.transform->rotation->value = 90;
  parent.transform->position->value = {4, 5};
  parent.parent = &grandparent;
  NullLayer child;
  child.transform3D = MakeTransform3D({7, 8, 9});
  child.transform3D->anchorPoint->value = {1, 2, 3};
  child.transform3D->scale->value = {2, 3, 4};
  child.transform3D->zRotation->value = 90;
  child.parent = &parent;
  auto result = Transform3DFilter::BuildModelMatrix(&child, 0).mapPoint({4, 5, 6});
  EXPECT_NEAR(result.x, -44, 0.0001f);
  EXPECT_NEAR(result.y, 22, 0.0001f);
  EXPECT_NEAR(result.z, -106, 0.0001f);
}

/** 验证无效三维矩阵不会上传 GPU，也不会留下可命中的未变换内容。 */
PAG_PIXEL_TEST(PAGTransform3DTest, NonFiniteTransformIsInvisible) {
  auto solid = MakeSolid3D(Red, {30, 30, 0}, 255);
  solid->transform3D->xRotation->value = std::numeric_limits<float>::quiet_NaN();
  auto file = Make3DFile({solid});
  ASSERT_NE(file, nullptr);
  auto surface = OffscreenSurface::Make(100, 100);
  ASSERT_NE(surface, nullptr);
  auto player = Make3DPlayer(file, surface);
  auto layer = file->getLayerAt(0);
  EXPECT_EQ(SamplePixel(surface, 50, 50)[3], 0);
  EXPECT_TRUE(player->getBounds(layer).isEmpty());
  EXPECT_TRUE(file->getLayersUnderPoint(50, 50).empty());
  EXPECT_FALSE(player->hitTestPoint(layer, 50, 50, true));
}

/** 验证非居中平面沿 Y 方向相交时深度顺序随位置改变。 */
PAG_PIXEL_TEST(PAGTransform3DTest, VerticalDepthAndOffCenterViewport) {
  auto red = MakeSolid3D(Red, {50, 35, 0}, 128);
  auto blue = MakeSolid3D(Blue, {50, 35, 0}, 128);
  red->transform3D->anchorPoint->value = {20, 20, 0};
  blue->transform3D->anchorPoint->value = {20, 20, 0};
  red->transform3D->xRotation->value = 45;
  blue->transform3D->xRotation->value = -45;
  auto file = Make3DFile({red, blue});
  ASSERT_NE(file, nullptr);
  auto surface = OffscreenSurface::Make(100, 100);
  ASSERT_NE(surface, nullptr);
  auto player = Make3DPlayer(file, surface);
  auto top = SamplePixel(surface, 50, 28);
  auto bottom = SamplePixel(surface, 50, 42);
  EXPECT_GT(top[0], top[2]);
  EXPECT_GT(bottom[2], bottom[0]);
  EXPECT_NEAR(top[3], 192, 1);
  EXPECT_NEAR(bottom[3], 192, 1);
  EXPECT_EQ(SamplePixel(surface, 50, 75)[3], 0);
  player->setCacheEnabled(false);
  surface->clearAll();
  player->flush();
  EXPECT_EQ(SamplePixel(surface, 50, 28), top);
  EXPECT_EQ(SamplePixel(surface, 50, 42), bottom);
}

PAG_PIXEL_TEST(PAGTransform3DTest, NestedProjectedHitTest) {
  for (bool child3D : {false, true}) {
    SCOPED_TRACE(child3D);
    auto solid = child3D ? MakeSolid3D(Red, {0, 0, 0}, 255) : MakeSolid2D(Red, {0, 0}, 40, 20);
    solid->height = 20;
    auto child = Make3DComposition({solid}, 1, 40, 40);
    auto layer = PreComposeLayer::Wrap(child).release();
    delete layer->transform;
    layer->transform = nullptr;
    layer->transform3D = MakeTransform3D({20, 10, 0});
    auto root = Make3DComposition({layer}, 2, 100, 100);
    auto rawFile = Codec::VerifyAndMake({child, root}, {});
    ASSERT_NE(rawFile, nullptr);
    auto file = PAGFile::MakeFrom(rawFile);
    ASSERT_NE(file, nullptr);
    auto projectedContainer = std::static_pointer_cast<PAGComposition>(file->getLayerAt(0));
    auto target = projectedContainer->getLayerAt(0);
    ASSERT_NE(target, nullptr);
    auto surface = OffscreenSurface::Make(100, 100);
    ASSERT_NE(surface, nullptr);
    auto player = Make3DPlayer(file, surface);
    EXPECT_EQ(SamplePixel(surface, 50, 20)[3], 255);
    EXPECT_EQ(SamplePixel(surface, 10, 10)[3], 0);
    for (bool pixelHitTest : {false, true}) {
      SCOPED_TRACE(pixelHitTest);
      EXPECT_TRUE(player->hitTestPoint(target, 50, 20, pixelHitTest));
      EXPECT_FALSE(player->hitTestPoint(target, 10, 10, pixelHitTest));
    }

    player->setScaleMode(PAGScaleMode::None);
    player->setComposition(nullptr);
    auto outer = PAGComposition::Make(200, 100);
    ASSERT_NE(outer, nullptr);
    ASSERT_TRUE(outer->addLayer(file));
    file->setMatrix(Matrix::MakeTrans(10, 20));
    projectedContainer->setMatrix(Matrix::MakeTrans(10, 0));
    target->setMatrix(Matrix::MakeTrans(0, 5));
    auto outerSurface = OffscreenSurface::Make(200, 100);
    ASSERT_NE(outerSurface, nullptr);
    player->setComposition(outer);
    player->setSurface(outerSurface);
    ASSERT_TRUE(player->flush());
    EXPECT_EQ(SamplePixel(outerSurface, 70, 45)[3], 255);
    EXPECT_EQ(SamplePixel(outerSurface, 25, 45)[3], 0);
    for (bool pixelHitTest : {false, true}) {
      SCOPED_TRACE(pixelHitTest);
      EXPECT_TRUE(player->hitTestPoint(target, 70, 45, pixelHitTest));
      EXPECT_FALSE(player->hitTestPoint(target, 25, 45, pixelHitTest));
    }

    projectedContainer->setMatrix(Matrix::MakeScale(0, 1));
    for (bool pixelHitTest : {false, true}) {
      EXPECT_FALSE(player->hitTestPoint(target, 70, 45, pixelHitTest));
    }
    projectedContainer->setMatrix(Matrix::MakeTrans(std::numeric_limits<float>::quiet_NaN(), 0));
    for (bool pixelHitTest : {false, true}) {
      EXPECT_FALSE(player->hitTestPoint(target, 70, 45, pixelHitTest));
    }
    projectedContainer->setMatrix(Matrix::MakeTrans(10, 0));
    file->setContentSize(0, 100);
    for (bool pixelHitTest : {false, true}) {
      EXPECT_FALSE(player->hitTestPoint(target, 70, 45, pixelHitTest));
    }
  }
}

/** 验证三维预合成内部非对称颜色内容不会上下颠倒。 */
PAG_PIXEL_TEST(PAGTransform3DTest, AsymmetricProjectedContent) {
  auto top = MakeSolid2D(Red, {0, 0}, 40, 20);
  auto bottom = MakeSolid2D(Blue, {0, 20}, 40, 20);
  auto child = Make3DComposition({top, bottom}, 1, 40, 40);
  auto layer = PreComposeLayer::Wrap(child).release();
  delete layer->transform;
  layer->transform = nullptr;
  layer->transform3D = MakeTransform3D({20, 10, 0});
  auto root = Make3DComposition({layer}, 2, 100, 100);
  auto rawFile = Codec::VerifyAndMake({child, root}, {});
  ASSERT_NE(rawFile, nullptr);
  auto file = PAGFile::MakeFrom(rawFile);
  ASSERT_NE(file, nullptr);
  auto surface = OffscreenSurface::Make(100, 100);
  ASSERT_NE(surface, nullptr);
  auto player = Make3DPlayer(file, surface);
  EXPECT_EQ(SamplePixel(surface, 30, 20)[0], 255);
  EXPECT_EQ(SamplePixel(surface, 30, 20)[2], 0);
  EXPECT_EQ(SamplePixel(surface, 30, 40)[0], 0);
  EXPECT_EQ(SamplePixel(surface, 30, 40)[2], 255);
  EXPECT_EQ(SamplePixel(surface, 30, 80)[3], 0);
}

}  // namespace pag
