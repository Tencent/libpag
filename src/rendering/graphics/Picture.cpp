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
static bool TryDrawDirectly(tgfx::Canvas* canvas, const tgfx::Texture* texture,
                            const tgfx::RGBAAALayout* layout) {
  if (texture == nullptr) {
    return false;
  }
  if (!texture->isYUV() && layout == nullptr) {
    // RGBA 纹理始终可以直接上屏。
    canvas->drawTexture(texture);
    return true;
  }
  auto totalMatrix = canvas->getMatrix();
  auto scaleFactor = GetMaxScaleFactor(totalMatrix);
  if (scaleFactor <= 1.0f) {
    // 纹理格式为 YUV 或含有 RGBAAALayout 时在缩放值小于等于 1.0f 时才直接上屏会有更好的性能。
    canvas->drawTexture(texture, layout);
    return true;
  }
  return false;
}

// 强制直接绘制纹理到 canvas。
static void DrawDirectly(tgfx::Canvas* canvas, const tgfx::Texture* texture,
                         const tgfx::RGBAAALayout* layout) {
  if (texture == nullptr || TryDrawDirectly(canvas, texture, layout)) {
    return;
  }
  auto width = layout ? layout->width : texture->width();
  auto height = layout ? layout->height : texture->height();
  auto surface = tgfx::Surface::Make(canvas->getContext(), width, height);
  if (surface == nullptr) {
    return;
  }
  auto newCanvas = surface->getCanvas();
  newCanvas->drawTexture(texture, layout);
  canvas->drawTexture(surface->getTexture().get());
  // 防止临时纹理析构
  canvas->flush();
}

static std::shared_ptr<tgfx::Texture> RescaleTexture(tgfx::Context* context, tgfx::Texture* texture,
                                                     float scaleFactor) {
  if (texture == nullptr || scaleFactor == 0) {
    return nullptr;
  }
  auto width = static_cast<int>(ceilf(texture->width() * scaleFactor));
  auto height = static_cast<int>(ceilf(texture->height() * scaleFactor));
  auto surface = tgfx::Surface::Make(context, width, height);
  if (surface == nullptr) {
    return nullptr;
  }
  auto canvas = surface->getCanvas();
  canvas->setMatrix(tgfx::Matrix::MakeScale(scaleFactor));
  canvas->drawTexture(texture);
  return surface->getTexture();
}

//================================= TextureProxySnapshotPicture ====================================
class TextureProxyPicture : public Picture {
 public:
  TextureProxyPicture(ID assetID, TextureProxy* proxy, bool externalMemory)
      : Picture(assetID), proxy(proxy), externalMemory(externalMemory) {
  }

  ~TextureProxyPicture() override {
    delete proxy;
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
    auto texture = proxy->getTexture(cache);
    if (texture == nullptr) {
      return false;
    }
    auto surface = tgfx::Surface::Make(cache->getContext(), 1, 1);
    if (surface == nullptr) {
      return false;
    }
    auto canvas = surface->getCanvas();
    canvas->setMatrix(tgfx::Matrix::MakeTrans(-x, -y));
    canvas->drawTexture(texture.get());
    return surface->hitTest(0, 0);
  }

  bool getPath(tgfx::Path*) const override {
    return false;
  }

  void prepare(RenderCache* cache) const override {
    proxy->prepare(cache);
  }

  void draw(tgfx::Canvas* canvas, RenderCache* cache) const override {
    auto oldMatrix = canvas->getMatrix();
    canvas->concat(extraMatrix);
    if (proxy->cacheEnabled()) {
      auto texture = proxy->getTexture(cache);
      if (TryDrawDirectly(canvas, texture.get(), nullptr)) {
        canvas->setMatrix(oldMatrix);
        return;
      }
    }
    auto snapshot = cache->getSnapshot(this);
    if (snapshot) {
      canvas->drawTexture(snapshot->getTexture(), snapshot->getMatrix());
    } else {
      auto texture = proxy->getTexture(cache);
      DrawDirectly(canvas, texture.get(), nullptr);
    }
    canvas->setMatrix(oldMatrix);
  }

 private:
  TextureProxy* proxy = nullptr;
  bool externalMemory = false;
  tgfx::Matrix extraMatrix = tgfx::Matrix::I();

  float getScaleFactor(float maxScaleFactor) const override {
    if (externalMemory) {
      return 1.0f;
    }
    // 图片缩放值不需要大于 1.0f，清晰度无法继续提高。
    return std::min(maxScaleFactor, 1.0f);
  }

  std::unique_ptr<Snapshot> makeSnapshot(RenderCache* cache, float scaleFactor) const override {
    auto texture = proxy->getTexture(cache);
    if (texture == nullptr) {
      return nullptr;
    }
    if (scaleFactor != 1.0f || texture->isYUV()) {
      texture = RescaleTexture(cache->getContext(), texture.get(), scaleFactor);
    }
    if (texture == nullptr) {
      return nullptr;
    }
    auto snapshot = new Snapshot(texture, tgfx::Matrix::MakeScale(1 / scaleFactor));
    return std::unique_ptr<Snapshot>(snapshot);
  }

  friend class Picture;
};
//=================================== TextureProxyPicture ==========================================

//===================================== RGBAAAPicture ==============================================
class RGBAAAPicture : public Picture {
 public:
  RGBAAAPicture(ID assetID, TextureProxy* proxy, const tgfx::RGBAAALayout& layout)
      : Picture(assetID), proxy(proxy), layout(layout) {
  }

  ~RGBAAAPicture() override {
    delete proxy;
  }

  void measureBounds(tgfx::Rect* bounds) const override {
    bounds->setWH(static_cast<float>(layout.width), static_cast<float>(layout.height));
  }

  bool hitTest(RenderCache* cache, float x, float y) override {
    // 碰撞检测过程不允许生成新的GPU缓存，因为碰撞结束不会触发缓存清理操作，长时间不进入下一次绘制有可能导致显存泄露。
    auto snapshot = cache->getSnapshot(assetID);
    if (snapshot) {
      return snapshot->hitTest(cache, x, y);
    }
    auto texture = proxy->getTexture(cache);
    auto surface = tgfx::Surface::Make(cache->getContext(), 1, 1);
    if (surface == nullptr) {
      return false;
    }
    auto canvas = surface->getCanvas();
    auto matrix = tgfx::Matrix::MakeTrans(static_cast<float>(-x), static_cast<float>(-y));
    canvas->setMatrix(matrix);
    canvas->drawTexture(texture.get(), &layout);
    return surface->hitTest(0, 0);
  }

  bool getPath(tgfx::Path*) const override {
    return false;
  }

  void prepare(RenderCache* cache) const override {
    proxy->prepare(cache);
  }

  void draw(tgfx::Canvas* canvas, RenderCache* cache) const override {
    if (proxy->cacheEnabled()) {
      // proxy在纯静态视频序列帧中不会缓存解码器
      // 如果将texture获取放在snapshot获取之前，会导致每帧都创建解码器
      auto texture = proxy->getTexture(cache);
      if (TryDrawDirectly(canvas, texture.get(), &layout)) {
        return;
      }
    }
    // 因为视频绘制会涉及自定义的 OpenGL 操作。
    canvas->flush();
    auto snapshot = cache->getSnapshot(this);
    if (snapshot) {
      canvas->drawTexture(snapshot->getTexture(), snapshot->getMatrix());
      return;
    }
    auto texture = proxy->getTexture(cache);
    DrawDirectly(canvas, texture.get(), &layout);
  }

 private:
  TextureProxy* proxy = nullptr;
  tgfx::RGBAAALayout layout = {};

  float getScaleFactor(float maxScaleFactor) const override {
    // 视频帧缩放值不需要大于 1.0f，清晰度无法继续提高。
    return std::min(maxScaleFactor, 1.0f);
  }

  std::unique_ptr<Snapshot> makeSnapshot(RenderCache* cache, float scaleFactor) const override {
    auto width = static_cast<int>(ceilf(static_cast<float>(layout.width) * scaleFactor));
    auto height = static_cast<int>(ceilf(static_cast<float>(layout.height) * scaleFactor));
    auto surface = tgfx::Surface::Make(cache->getContext(), width, height);
    if (surface == nullptr) {
      return nullptr;
    }
    auto texture = proxy->getTexture(cache);
    auto canvas = surface->getCanvas();
    canvas->setMatrix(tgfx::Matrix::MakeScale(scaleFactor));
    canvas->drawTexture(texture.get(), &layout);
    auto snapshot = new Snapshot(surface->getTexture(), tgfx::Matrix::MakeScale(1 / scaleFactor));
    return std::unique_ptr<Snapshot>(snapshot);
  }

  friend class Graphic;
};
//====================================== RGBAAAPicture =============================================

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
    canvas->drawTexture(snapshot->getTexture(), snapshot->getMatrix());
  }

 protected:
  float getScaleFactor(float maxScaleFactor) const override {
    return maxScaleFactor;
  }

  std::unique_ptr<Snapshot> makeSnapshot(RenderCache* cache, float scaleFactor) const override {
    tgfx::Rect bounds = tgfx::Rect::MakeEmpty();
    graphic->measureBounds(&bounds);
    auto width = static_cast<int>(ceilf(bounds.width() * scaleFactor));
    auto height = static_cast<int>(ceilf(bounds.height() * scaleFactor));
    auto surface = tgfx::Surface::Make(cache->getContext(), width, height);
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
    auto snapshot = new Snapshot(surface->getTexture(), drawingMatrix);
    return std::unique_ptr<Snapshot>(snapshot);
  }

 private:
  std::shared_ptr<Graphic> graphic = nullptr;
};
//===================================== SnapshotPicture ============================================

//==================================== Texture Proxies =============================================
class TextureBufferProxy : public TextureProxy {
 public:
  explicit TextureBufferProxy(const std::shared_ptr<tgfx::TextureBuffer> buffer)
      : TextureProxy(buffer->width(), buffer->height()), buffer(buffer) {
  }

  bool cacheEnabled() const override {
    return false;
  }

  void prepare(RenderCache*) const override {
  }

  std::shared_ptr<tgfx::Texture> getTexture(RenderCache* cache) const override {
    tgfx::Clock clock = {};
    auto texture = buffer->makeTexture(cache->getContext());
    cache->recordTextureUploadingTime(clock.measure());
    return texture;
  }

 private:
  std::shared_ptr<tgfx::TextureBuffer> buffer = nullptr;
};

class ImageTextureProxy : public TextureProxy {
 public:
  explicit ImageTextureProxy(ID assetID, int width, int height, std::shared_ptr<tgfx::Image> image)
      : TextureProxy(width, height), assetID(assetID), image(std::move(image)) {
  }

  bool cacheEnabled() const override {
    return false;
  }

  void prepare(RenderCache* cache) const override {
    cache->prepareImage(assetID, image);
  }

  std::shared_ptr<tgfx::Texture> getTexture(RenderCache* cache) const override {
    tgfx::Clock clock = {};
    auto buffer = cache->getImageBuffer(assetID);
    if (buffer == nullptr) {
      buffer = image->makeBuffer();
    }
    cache->recordImageDecodingTime(clock.measure());
    if (buffer == nullptr) {
      return nullptr;
    }
    clock.reset();
    auto texture = buffer->makeTexture(cache->getContext());
    cache->recordTextureUploadingTime(clock.measure());
    return texture;
  }

 private:
  ID assetID = 0;
  std::shared_ptr<tgfx::Image> image = nullptr;
};

class BackendTextureProxy : public TextureProxy {
 public:
  BackendTextureProxy(const BackendTexture& texture, tgfx::ImageOrigin origin, void* sharedContext)
      : TextureProxy(texture.width(), texture.height()), backendTexture(texture), origin(origin),
        sharedContext(sharedContext) {
  }

  bool cacheEnabled() const override {
    return false;
  }

  void prepare(RenderCache*) const override {
  }

  std::shared_ptr<tgfx::Texture> getTexture(RenderCache* cache) const override {
    auto context = cache->getContext();
    if (!checkContext(context)) {
      return nullptr;
    }
    tgfx::GLSampler sampler = {};
    if (!GetGLSampler(backendTexture, &sampler)) {
      return nullptr;
    }
    return tgfx::GLTexture::MakeFrom(context, sampler, backendTexture.width(),
                                     backendTexture.height(), origin);
  }

 private:
  BackendTexture backendTexture = {};
  tgfx::ImageOrigin origin = tgfx::ImageOrigin::TopLeft;
  void* sharedContext = nullptr;

  bool checkContext(tgfx::Context* context) const {
    auto glDevice = static_cast<tgfx::GLDevice*>(context->device());
    if (!glDevice->sharableWith(sharedContext)) {
      LOGE(
          "A Graphic which made from a texture can not be drawn on to a PAGSurface"
          " if its GPU context is not share context to the PAGSurface.");
      return false;
    }
    return true;
  }
};
//==================================== Texture Proxies =============================================

static std::atomic_uint64_t IDCount = {1};

Picture::Picture(ID assetID) : assetID(assetID), uniqueKey(IDCount++) {
}

std::shared_ptr<Graphic> Picture::MakeFrom(ID assetID, std::shared_ptr<tgfx::Image> image) {
  if (image == nullptr) {
    return nullptr;
  }
  auto extraMatrix = OrientationToMatrix(image->orientation(), image->width(), image->height());
  auto width = image->width();
  auto height = image->height();
  ApplyOrientation(image->orientation(), &width, &height);
  auto textureProxy =
      new ImageTextureProxy(assetID, static_cast<int>(width), static_cast<int>(height), image);
  auto picture = std::make_shared<TextureProxyPicture>(assetID, textureProxy, false);
  picture->extraMatrix = extraMatrix;
  return picture;
}

std::shared_ptr<Graphic> Picture::MakeFrom(ID assetID,
                                           std::shared_ptr<tgfx::TextureBuffer> buffer) {
  if (buffer == nullptr) {
    return nullptr;
  }
  auto proxy = new TextureBufferProxy(buffer);
  return std::shared_ptr<Graphic>(new TextureProxyPicture(assetID, proxy, false));
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
  auto proxy = new BackendTextureProxy(texture, origin, context);
  return std::shared_ptr<Graphic>(new TextureProxyPicture(assetID, proxy, true));
}

std::shared_ptr<Graphic> Picture::MakeFrom(ID assetID, std::unique_ptr<TextureProxy> proxy) {
  if (proxy == nullptr) {
    return nullptr;
  }
  return std::shared_ptr<Graphic>(new TextureProxyPicture(assetID, proxy.release(), true));
}

std::shared_ptr<Graphic> Picture::MakeFrom(ID assetID, std::unique_ptr<TextureProxy> proxy,
                                           const tgfx::RGBAAALayout& layout) {
  if (layout.alphaStartX == 0 && layout.alphaStartY == 0) {
    return Picture::MakeFrom(assetID, std::move(proxy));
  }
  if (proxy == nullptr || layout.alphaStartX + layout.width > proxy->width() ||
      layout.alphaStartY + layout.height > proxy->height()) {
    return nullptr;
  }
  return std::shared_ptr<RGBAAAPicture>(new RGBAAAPicture(assetID, proxy.release(), layout));
}

std::shared_ptr<Graphic> Picture::MakeFrom(ID assetID, std::shared_ptr<Graphic> graphic) {
  if (assetID == 0 || graphic == nullptr || graphic->type() == GraphicType::Picture) {
    return graphic;
  }
  return std::make_shared<SnapshotPicture>(assetID, graphic);
}

}  // namespace pag
