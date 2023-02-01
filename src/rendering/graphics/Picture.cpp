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

#include "Picture.h"
#include "base/utils/MatrixUtil.h"
#include "rendering/caches/RenderCache.h"
#include "tgfx/core/Clock.h"
#include "tgfx/gpu/Surface.h"
#include "tgfx/gpu/opengl/GLDevice.h"
#include "tgfx/gpu/opengl/GLTexture.h"

namespace pag {
// 若当前直接绘制纹理性能是最好的，就直接绘制，否则返回 false。
static bool TryDrawDirectly(tgfx::Canvas* canvas, std::shared_ptr<tgfx::Image> image, bool isFlat) {
  if (image == nullptr) {
    return false;
  }
  if (isFlat && !image->isRGBAAA()) {
    // RGBA 纹理始终可以直接上屏。
    canvas->drawImage(std::move(image));
    return true;
  }
  auto totalMatrix = canvas->getMatrix();
  auto scaleFactor = GetMaxScaleFactor(totalMatrix);
  if (scaleFactor <= 1.0f) {
    // 纹理格式为 YUV 或含有 RGBAAALayout 时在缩放值小于等于 1.0f 时才直接上屏会有更好的性能。
    canvas->drawImage(std::move(image));
    return true;
  }
  return false;
}

// 强制直接绘制纹理到 canvas。
static void DrawDirectly(tgfx::Canvas* canvas, std::shared_ptr<tgfx::Image> image, bool isFlat) {
  if (image == nullptr || TryDrawDirectly(canvas, image, isFlat)) {
    return;
  }
  auto width = image->width();
  auto height = image->height();
  auto surface = tgfx::Surface::Make(canvas->getContext(), width, height);
  if (surface == nullptr) {
    return;
  }
  auto newCanvas = surface->getCanvas();
  newCanvas->drawImage(std::move(image));
  canvas->drawTexture(surface->getTexture());
  // 防止临时纹理析构
  canvas->flush();
}

static std::shared_ptr<tgfx::Image> RescaleImage(tgfx::Context* context,
                                                 std::shared_ptr<tgfx::Image> image,
                                                 float scaleFactor, bool mipMapped) {
  if (image == nullptr || scaleFactor == 0) {
    return nullptr;
  }
  auto width = static_cast<int>(ceilf(static_cast<float>(image->width()) * scaleFactor));
  auto height = static_cast<int>(ceilf(static_cast<float>(image->height()) * scaleFactor));
  auto surface = tgfx::Surface::Make(context, width, height, false, 1, mipMapped);
  if (surface == nullptr) {
    return nullptr;
  }
  auto canvas = surface->getCanvas();
  canvas->setMatrix(tgfx::Matrix::MakeScale(scaleFactor));
  canvas->drawImage(std::move(image));
  return tgfx::Image::MakeFromTexture(surface->getTexture());
}

//================================= ImageProxyPicture ====================================
class ImageProxyPicture : public Picture {
 public:
  ImageProxyPicture(ID assetID, std::shared_ptr<ImageProxy> proxy)
      : Picture(assetID), proxy(proxy) {
  }

  void measureBounds(tgfx::Rect* bounds) const override {
    bounds->setWH(static_cast<float>(proxy->width()), static_cast<float>(proxy->height()));
  }

  bool hitTest(RenderCache* cache, float x, float y) override {
    // 碰撞检测过程不允许生成新的GPU缓存，因为碰撞结束不会触发缓存清理操作，长时间不进入下一次绘制有可能导致显存泄露。
    auto snapshot = cache->getSnapshot(assetID);
    if (snapshot) {
      return snapshot->hitTest(cache, x, y);
    }
    auto image = proxy->getImage(cache);
    if (image == nullptr) {
      return false;
    }
    auto surface = tgfx::Surface::Make(cache->getContext(), 1, 1);
    if (surface == nullptr) {
      return false;
    }
    auto canvas = surface->getCanvas();
    canvas->setMatrix(tgfx::Matrix::MakeTrans(-x, -y));
    canvas->drawImage(std::move(image));
    return surface->hitTest(0, 0);
  }

  bool getPath(tgfx::Path*) const override {
    return false;
  }

  void prepare(RenderCache* cache) const override {
    proxy->prepareImage(cache);
  }

  void draw(tgfx::Canvas* canvas, RenderCache* cache) const override {
    auto image = proxy->getImage(cache);
    if (proxy->isTemporary()) {
      if (TryDrawDirectly(canvas, image, proxy->isFlat())) {
        return;
      }
    }
    auto snapshot = cache->getSnapshot(this);
    if (snapshot) {
      canvas->drawImage(snapshot->getImage(), snapshot->getMatrix());
    } else {
      DrawDirectly(canvas, image, proxy->isFlat());
    }
  }

 private:
  std::shared_ptr<ImageProxy> proxy = nullptr;

  float getScaleFactor(float maxScaleFactor) const override {
    // 图片缩放值不需要大于 1.0f，清晰度无法继续提高。
    return std::min(maxScaleFactor, 1.0f);
  }

  std::unique_ptr<Snapshot> makeSnapshot(RenderCache* cache, float scaleFactor,
                                         bool mipMapped) const override {
    auto image = proxy->getImage(cache);
    if (image == nullptr) {
      return nullptr;
    }
    bool needRescale = image->isTextureBacked() ? false : scaleFactor != 1.0f;
    if (needRescale || image->isRGBAAA() || !proxy->isFlat()) {
      image = RescaleImage(cache->getContext(), image, scaleFactor, mipMapped);
    } else {
      image = image->makeTextureImage(cache->getContext());
    }
    if (image == nullptr) {
      return nullptr;
    }
    auto snapshot = new Snapshot(image, tgfx::Matrix::MakeScale(1 / scaleFactor));
    return std::unique_ptr<Snapshot>(snapshot);
  }

  friend class Picture;
};
//=================================== ImageProxyPicture==========================================

//====================================== SnapshotPicture ===========================================
class SnapshotPictureOptions : public tgfx::SurfaceOptions {
 public:
  bool skipSnapshotPictureCache = false;
};

class SnapshotPicture : public Picture {
 public:
  SnapshotPicture(ID assetID, std::shared_ptr<Graphic> graphic)
      : Picture(assetID), graphic(std::move(graphic)) {
  }

  void measureBounds(tgfx::Rect* bounds) const override {
    graphic->measureBounds(bounds);
  }

  bool hitTest(RenderCache* cache, float x, float y) override {
    // 碰撞检测过程不允许生成新的GPU缓存，因为碰撞结束不会触发缓存清理操作，长时间不进入下一次绘制有可能导致显存泄露。
    auto snapshot = cache->getSnapshot(assetID);
    if (snapshot) {
      return snapshot->hitTest(cache, x, y);
    }
    return graphic->hitTest(cache, x, y);
  }

  bool getPath(tgfx::Path* path) const override {
    return graphic->getPath(path);
  }

  void prepare(RenderCache* cache) const override {
    graphic->prepare(cache);
  }

  void draw(tgfx::Canvas* canvas, RenderCache* cache) const override {
    auto options = static_cast<const SnapshotPictureOptions*>(canvas->surfaceOptions());
    if (options && options->skipSnapshotPictureCache) {
      graphic->draw(canvas, cache);
      return;
    }
    auto snapshot = cache->getSnapshot(this);
    if (snapshot == nullptr) {
      graphic->draw(canvas, cache);
      return;
    }
    canvas->drawImage(snapshot->getImage(), snapshot->getMatrix());
  }

 protected:
  float getScaleFactor(float maxScaleFactor) const override {
    return maxScaleFactor;
  }

  std::unique_ptr<Snapshot> makeSnapshot(RenderCache* cache, float scaleFactor,
                                         bool mipMapped) const override {
    tgfx::Rect bounds = tgfx::Rect::MakeEmpty();
    graphic->measureBounds(&bounds);
    auto width = static_cast<int>(ceilf(bounds.width() * scaleFactor));
    auto height = static_cast<int>(ceilf(bounds.height() * scaleFactor));
    auto surface = tgfx::Surface::Make(cache->getContext(), width, height, false, 1, mipMapped);
    if (surface == nullptr) {
      return nullptr;
    }
    auto options = std::make_unique<SnapshotPictureOptions>();
    options->skipSnapshotPictureCache = true;
    surface->setOptions(std::move(options));
    auto canvas = surface->getCanvas();
    auto matrix = tgfx::Matrix::MakeScale(scaleFactor);
    matrix.preTranslate(-bounds.x(), -bounds.y());
    canvas->setMatrix(matrix);
    graphic->draw(canvas, cache);
    auto drawingMatrix = tgfx::Matrix::I();
    matrix.invert(&drawingMatrix);
    auto image = tgfx::Image::MakeFromTexture(surface->getTexture());
    auto snapshot = new Snapshot(std::move(image), drawingMatrix);
    return std::unique_ptr<Snapshot>(snapshot);
  }

 private:
  std::shared_ptr<Graphic> graphic = nullptr;
};
//===================================== SnapshotPicture ============================================

class DefaultImageProxy : public ImageProxy {
 public:
  DefaultImageProxy(ID assetID, std::shared_ptr<tgfx::Image> image)
      : assetID(assetID), image(std::move(image)) {
  }

  int width() const override {
    return image->width();
  }

  int height() const override {
    return image->height();
  }

  bool isTemporary() const override {
    return false;
  }

  bool isFlat() const override {
    return true;
  }

  void prepareImage(RenderCache* cache) const override {
    cache->prepareAssetImage(assetID, this);
  }

  std::shared_ptr<tgfx::Image> getImage(RenderCache* cache) const override {
    return cache->getAssetImage(assetID, this);
  }

 protected:
  std::shared_ptr<tgfx::Image> makeImage(RenderCache*) const override {
    return image;
  }

 private:
  ID assetID = 0;
  std::shared_ptr<tgfx::Image> image = nullptr;
};

class BackendTextureProxy : public ImageProxy {
 public:
  BackendTextureProxy(ID assetID, const BackendTexture& texture, tgfx::ImageOrigin origin,
                      void* sharedContext)
      : assetID(assetID), backendTexture(texture), origin(origin), sharedContext(sharedContext) {
  }

  int width() const override {
    return backendTexture.width();
  }

  int height() const override {
    return backendTexture.height();
  }

  bool isTemporary() const override {
    return false;
  }

  bool isFlat() const override {
    GLTextureInfo glInfo = {};
    if (!backendTexture.getGLTextureInfo(&glInfo)) {
      return false;
    }
    return glInfo.target == GL_TEXTURE_2D;
  }

  void prepareImage(RenderCache*) const override {
  }

  std::shared_ptr<tgfx::Image> getImage(RenderCache* cache) const override {
    return cache->getAssetImage(assetID, this);
  }

 protected:
  std::shared_ptr<tgfx::Image> makeImage(RenderCache* cache) const override {
    auto context = cache->getContext();
    if (!checkContext(context)) {
      return nullptr;
    }
    tgfx::GLSampler sampler = {};
    if (!GetGLSampler(backendTexture, &sampler)) {
      return nullptr;
    }
    auto texture = tgfx::GLTexture::MakeFrom(context, sampler, backendTexture.width(),
                                             backendTexture.height(), origin);
    return tgfx::Image::MakeFromTexture(std::move(texture));
  }

 private:
  ID assetID = 0;
  BackendTexture backendTexture = {};
  tgfx::ImageOrigin origin = tgfx::ImageOrigin::TopLeft;
  void* sharedContext = nullptr;

  bool checkContext(tgfx::Context* context) const {
    auto glDevice = static_cast<tgfx::GLDevice*>(context->device());
    if (!glDevice->sharableWith(sharedContext)) {
      LOGE(
          "A Graphic made from a texture can not be drawn on to a PAGSurface"
          " if its GPU context is not a share context to the PAGSurface.");
      return false;
    }
    return true;
  }
};

static std::atomic_uint64_t IDCount = {1};

Picture::Picture(ID assetID) : assetID(assetID), uniqueKey(IDCount++) {
}

std::shared_ptr<Graphic> Picture::MakeFrom(ID assetID, std::shared_ptr<tgfx::Image> image) {
  if (image == nullptr) {
    return nullptr;
  }
  auto proxy = std::make_shared<DefaultImageProxy>(assetID, std::move(image));
  return MakeFrom(assetID, std::move(proxy));
}

std::shared_ptr<Graphic> Picture::MakeFrom(ID assetID, std::shared_ptr<ImageProxy> proxy) {
  if (assetID == 0 || proxy == nullptr) {
    return nullptr;
  }
  return std::make_shared<ImageProxyPicture>(assetID, std::move(proxy));
}

std::shared_ptr<Graphic> Picture::MakeFrom(ID assetID, const BackendTexture& texture,
                                           tgfx::ImageOrigin origin) {
  if (!texture.isValid()) {
    return nullptr;
  }
  auto context = tgfx::GLDevice::CurrentNativeHandle();
  if (context == nullptr) {
    return nullptr;
  }
  auto proxy = std::make_shared<BackendTextureProxy>(assetID, texture, origin, context);
  return std::make_shared<ImageProxyPicture>(assetID, proxy);
}

std::shared_ptr<Graphic> Picture::MakeFrom(ID assetID, std::shared_ptr<Graphic> graphic) {
  if (assetID == 0 || graphic == nullptr || graphic->type() == GraphicType::Picture) {
    return graphic;
  }
  return std::make_shared<SnapshotPicture>(assetID, graphic);
}
}  // namespace pag
