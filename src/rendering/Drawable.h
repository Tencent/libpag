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

#pragma once

#include "pag/gpu.h"
#include "tgfx/gpu/Surface.h"

namespace pag {

class Drawable {
 public:
  virtual ~Drawable() = default;

  virtual int width() const = 0;

  virtual int height() const = 0;

  virtual void updateSize() = 0;

  virtual std::shared_ptr<tgfx::Surface> createSurface(tgfx::Context* context) = 0;

  virtual void present(tgfx::Context* context) = 0;

  virtual void setTimeStamp(int64_t) {
  }

  virtual tgfx::Context* lockContext();

  virtual void unlockContext();

  virtual bool prepareDevice();

  virtual void freeDevice();

 protected:
  virtual std::shared_ptr<tgfx::Device> getDevice() = 0;

 private:
  std::shared_ptr<tgfx::Device> currentDevice;
};

class RenderTargetDrawable : public Drawable {
 public:
  RenderTargetDrawable(std::shared_ptr<tgfx::Device> device,
                       const BackendRenderTarget& renderTarget, tgfx::ImageOrigin origin);

  int width() const override {
    return renderTarget.width();
  }

  int height() const override {
    return renderTarget.height();
  }

  void updateSize() override {
  }

  std::shared_ptr<tgfx::Device> getDevice() override {
    return device;
  }

  std::shared_ptr<tgfx::Surface> createSurface(tgfx::Context* context) override;

  void present(tgfx::Context*) override {
  }

 private:
  std::shared_ptr<tgfx::Device> device = nullptr;
  BackendRenderTarget renderTarget = {};
  tgfx::ImageOrigin origin = tgfx::ImageOrigin::TopLeft;
};

class TextureDrawable : public Drawable {
 public:
  TextureDrawable(std::shared_ptr<tgfx::Device> device, const BackendTexture& texture,
                  tgfx::ImageOrigin origin);

  int width() const override {
    return texture.width();
  }

  int height() const override {
    return texture.height();
  }

  void updateSize() override {
  }

  std::shared_ptr<tgfx::Device> getDevice() override {
    return device;
  }

  std::shared_ptr<tgfx::Surface> createSurface(tgfx::Context* context) override;

  void present(tgfx::Context*) override {
  }

 private:
  std::shared_ptr<tgfx::Device> device = nullptr;
  BackendTexture texture = {};
  tgfx::ImageOrigin origin = tgfx::ImageOrigin::TopLeft;
};

class OffscreenDrawable : public Drawable {
 public:
  OffscreenDrawable(int width, int height, std::shared_ptr<tgfx::Device> device);

  int width() const override {
    return _width;
  }

  int height() const override {
    return _height;
  }

  void updateSize() override {
  }

  std::shared_ptr<tgfx::Device> getDevice() override {
    return device;
  }

  std::shared_ptr<tgfx::Surface> createSurface(tgfx::Context* context) override;

  void present(tgfx::Context*) override {
  }

 private:
  int _width = 0;
  int _height = 0;
  std::shared_ptr<tgfx::Device> device = nullptr;
};
}  // namespace pag
