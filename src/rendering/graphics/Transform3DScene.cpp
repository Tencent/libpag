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

#include "Transform3DScene.h"
#include <cmath>
#include "base/utils/MatrixUtil.h"
#include "rendering/caches/RenderCache.h"
#include "rendering/filters/Transform3DSceneFilter.h"
#include "rendering/filters/utils/Filter3DFactory.h"
#include "rendering/graphics/Picture.h"
#include "tgfx/core/ImageFilter.h"
#include "tgfx/core/PictureRecorder.h"
#include "tgfx/core/Surface.h"

namespace pag {
namespace {

std::shared_ptr<tgfx::Image> MakeSource(const std::shared_ptr<Graphic>& graphic, RenderCache* cache,
                                        int width, int height, float scale) {
  tgfx::PictureRecorder recorder;
  auto rawCanvas = recorder.beginRecording();
  Canvas canvas(rawCanvas, cache);
  canvas.scale(scale, scale);
  canvas.clipRect(tgfx::Rect::MakeWH(width / scale, height / scale));
  if (graphic != nullptr) {
    graphic->draw(&canvas);
  } else {
    tgfx::Paint paint;
    paint.setColor(tgfx::Color::Transparent());
    canvas.drawRect(tgfx::Rect::MakeWH(width / scale, height / scale), paint);
  }
  return tgfx::Image::MakeFrom(recorder.finishRecordingAsPicture(), width, height);
}

class Transform3DScene : public Graphic {
 public:
  Transform3DScene(std::shared_ptr<Graphic> background, std::vector<Transform3DSceneLayer> layers,
                   int width, int height)
      : background(std::move(background)), layers(std::move(layers)), width(width), height(height) {
  }

  GraphicType type() const override {
    return GraphicType::Unknown;
  }
  bool getPath(tgfx::Path*) const override {
    return false;
  }

  void measureBounds(tgfx::Rect* bounds) const override {
    bounds->setEmpty();
    if (background != nullptr) {
      background->measureBounds(bounds);
    }
    for (const auto& layer : layers) {
      tgfx::Rect rect;
      layer.graphic->measureBounds(&rect);
      bounds->join(rect);
    }
    if (!bounds->intersect(tgfx::Rect::MakeWH(width, height))) {
      bounds->setEmpty();
    }
  }

  void prepare(RenderCache* cache) const override {
    if (background != nullptr) {
      background->prepare(cache);
    }
    for (const auto& layer : layers) {
      layer.graphic->prepare(cache);
    }
  }

  bool hitTest(RenderCache* cache, float x, float y) override {
    auto surface = tgfx::Surface::Make(cache->getContext(), 1, 1);
    if (surface == nullptr) {
      return false;
    }
    Canvas canvas(surface.get(), cache);
    canvas.translate(-x, -y);
    draw(&canvas);
    return surface->getColor(0, 0).alpha > 0;
  }

  void draw(Canvas* canvas) const override {
    if (!Has3DSupport(canvas->getContext()->gpu())) {
      if (background != nullptr) {
        background->draw(canvas);
      }
      return;
    }
    if (layers.size() == 1) {
      if (background != nullptr) {
        background->draw(canvas);
      }
      layers[0].graphic->draw(canvas);
      return;
    }
    auto cache = canvas->getCache();
    auto limit = cache->getContext()->gpu()->limits()->maxTextureDimension2D;
    auto scale = Clamp3DScale(cache->getContext()->gpu(), width, height,
                              GetMaxScaleFactor(canvas->getMatrix()));
    if (scale <= 0) {
      return;
    }
    auto targetWidth = static_cast<int>(std::ceil(static_cast<double>(width) * scale));
    auto targetHeight = static_cast<int>(std::ceil(static_cast<double>(height) * scale));
    targetWidth = std::min(targetWidth, limit);
    targetHeight = std::min(targetHeight, limit);
    auto source = MakeSource(background, cache, targetWidth, targetHeight, scale);
    if (source == nullptr) {
      return;
    }
    std::vector<std::shared_ptr<tgfx::Image>> images;
    std::vector<Transform3DDepthPlane> planes;
    std::vector<BlendMode> modes;
    // 深度方程使用逻辑视口，采样坐标则来自向上取整后的纹理。
    auto viewportScaleX = targetWidth / (static_cast<double>(width) * scale);
    auto viewportScaleY = targetHeight / (static_cast<double>(height) * scale);
    for (const auto& layer : layers) {
      auto image = MakeSource(layer.graphic, cache, targetWidth, targetHeight, scale);
      if (image == nullptr) {
        return;
      }
      images.push_back(std::move(image));
      auto plane = layer.depthPlane;
      planes.push_back({plane.x * viewportScaleX, plane.y * viewportScaleY,
                        plane.z + plane.x * (viewportScaleX - 1) + plane.y * (viewportScaleY - 1)});
      modes.push_back(layer.blendMode);
    }
    auto filter = std::make_shared<Transform3DSceneFilter>(cache, images, planes, modes,
                                                           targetWidth, targetHeight);
    auto image = source->makeWithFilter(tgfx::ImageFilter::Runtime(filter));
    if (image == nullptr) {
      return;
    }
    canvas->save();
    canvas->scale(1.0f / scale, 1.0f / scale);
    canvas->drawImage(std::move(image));
    canvas->restore();
  }

 private:
  std::shared_ptr<Graphic> background;
  std::vector<Transform3DSceneLayer> layers;
  int width = 0;
  int height = 0;
};

class CachedTransform3DScene : public Graphic {
 public:
  CachedTransform3DScene(std::shared_ptr<Graphic> scene, int width, int height)
      : scene(std::move(scene)), picture(std::static_pointer_cast<Picture>(
                                     Picture::MakeFrom(UniqueID::Next(), this->scene))),
        width(width), height(height) {
  }

  GraphicType type() const override {
    return GraphicType::Unknown;
  }
  bool getPath(tgfx::Path* path) const override {
    return scene->getPath(path);
  }
  void measureBounds(tgfx::Rect* bounds) const override {
    scene->measureBounds(bounds);
  }
  void prepare(RenderCache* cache) const override {
    scene->prepare(cache);
  }
  bool hitTest(RenderCache* cache, float x, float y) override {
    return scene->hitTest(cache, x, y);
  }

  void draw(Canvas* canvas) const override {
    auto cache = canvas->getCache();
    auto gpu = cache->getContext()->gpu();
    auto scale = Clamp3DScale(gpu, width, height, GetMaxScaleFactor(canvas->getMatrix()));
    if (!Has3DSupport(gpu)) {
      scene->draw(canvas);
      return;
    }
    if (scale <= 0) {
      return;
    }
    auto snapshot = canvas->renderFlags() & tgfx::RenderFlags::DisableCache
                        ? nullptr
                        : cache->getSnapshot(picture.get(), scale, scale);
    if (snapshot == nullptr) {
      scene->draw(canvas);
      return;
    }
    canvas->save();
    canvas->concat(snapshot->getMatrix());
    canvas->drawImage(snapshot->getImage());
    canvas->restore();
  }

 private:
  std::shared_ptr<Graphic> scene;
  std::shared_ptr<Picture> picture;
  int width = 0;
  int height = 0;
};

}  // namespace

void Transform3DSceneBuilder::addLayer(std::shared_ptr<Graphic> graphic, Layer* layer, Frame frame,
                                       const std::shared_ptr<Transform3DContext>& context,
                                       const tgfx::Matrix& extraMatrix) {
  if (graphic == nullptr) {
    return;
  }
  if (!Has3DTransform(layer)) {
    flushScene();
    background.push_back(std::move(graphic));
    return;
  }
  Transform3DDepthPlane plane;
  if (context != nullptr &&
      Transform3DFilter::DepthPlane(layer, frame, *context, extraMatrix, &plane)) {
    layers.push_back({std::move(graphic), plane, layer->blendMode});
  }
}

void Transform3DSceneBuilder::flushScene() {
  if (layers.empty()) {
    return;
  }
  auto scene = std::make_shared<Transform3DScene>(Graphic::MakeCompose(std::move(background)),
                                                  std::move(layers), width, height);
  background = {std::make_shared<CachedTransform3DScene>(std::move(scene), width, height)};
  layers.clear();
}

std::shared_ptr<Graphic> Transform3DSceneBuilder::makeGraphic() {
  flushScene();
  return Graphic::MakeCompose(background);
}

}  // namespace pag
