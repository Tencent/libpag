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

#include <string>
#include "gpu/Resource.h"
#include "gpu/TextureSampler.h"

namespace pag {
/**
 * Texture describes a two dimensional array of pixels in the GPU backend for drawing.
 */
class Texture : public Resource {
 public:
  /**
   * Creates a new texture which wraps the BackendTexture.
   */
  static std::shared_ptr<Texture> MakeFrom(Context* context, const BackendTexture& backendTexture,
                                           ImageOrigin origin);

  /**
   * Creates a single-plane texture from a hardware buffer. The type of hardwareBuffer should be
   * either AHardwareBuffer* on android platform or CVPixelBufferRef on apple platform. The returned
   * Texture takes a reference on the buffer.
   */
  static std::shared_ptr<Texture> MakeFrom(Context* context, void* hardwareBuffer);

  /**
   * Creates a new texture from specified pixels data with each pixel stored as 32-bit RGBA data.
   */
  static std::shared_ptr<Texture> MakeRGBA(Context* context, int width, int height, void* pixels,
                                           size_t rowBytes,
                                           ImageOrigin origin = ImageOrigin::TopLeft);

  /**
   * Creates a new texture from specified pixels data with each pixel stored as a single
   * translucency (alpha) channel.
   */
  static std::shared_ptr<Texture> MakeAlpha(Context* context, int width, int height, void* pixels,
                                            size_t rowBytes,
                                            ImageOrigin origin = ImageOrigin::TopLeft);

  Texture(int width, int height, ImageOrigin origin)
      : _width(width), _height(height), _origin(origin) {
  }

  /**
   * Returns the display width of this texture.
   */
  int width() const {
    return _width;
  }

  /**
   * Returns the display height of this texture.
   */
  int height() const {
    return _height;
  }

  /**
   * Returns the origin of this texture, either ImageOrigin::TopLeft or ImageOrigin::BottomLeft.
   */
  ImageOrigin origin() const {
    return _origin;
  }

  /**
   * Returns the texture coordinates in backend units corresponding to specified position in pixels.
   */
  virtual Point getTextureCoord(float x, float y) const = 0;

  /**
   * Returns memory usage information for this Texture.
   */
  virtual size_t memoryUsage() const = 0;

  /**
   * Returns texture sampler.
   */
  virtual const TextureSampler* getSampler() const = 0;

  /**
   * Returns true if this is a YUVTexture.
   */
  virtual bool isYUV() const {
    return false;
  }

 private:
  int _width = 0;
  int _height = 0;
  ImageOrigin _origin = ImageOrigin::TopLeft;

  // Two texture types: Alpha, RGBA
  static std::shared_ptr<Texture> Make(Context* context, int width, int height, void* pixels,
                                       size_t rowBytes, ImageOrigin origin, bool alphaOnly);
  // for Trace() only.
  Context* getContext() const {
    return context;
  }

  friend void Trace(const Texture* texture, const std::string& path);
};

/**
 * Trace() provides a utility to view the texture.
 */
void Trace(const Texture* texture, const std::string& path = "");

}  // namespace pag
