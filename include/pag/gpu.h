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

#include <cinttypes>
#include <memory>
#include <mutex>
#include "pag/defines.h"

namespace pag {
/**
 * Possible GPU backend APIs that may be used by PAG.
 */
enum class Backend {
  /**
   * Mock is a backend that does not draw anything. It is used for unit tests and to measure CPU
   * overhead.
   */
  MOCK,
  OPENGL,
  METAL,
  VULKAN,
};

/**
 * Textures and FrameBuffers can be stored such that (0, 0) in texture space may correspond to
 * either the top-left or bottom-left content pixel.
 */
enum class ImageOrigin {
  /**
   * The default origin for all off-screen rendering. Use ImageOrigin::TopLeft origin for textures
   * which are from HardwareBuffers (Android), CVPixelBuffers (iOS), or normally created by backend
   * APIs. Note: ImageOrigin::TopLeft is actual bottom-left origin for OpenGL backend.
   */
  TopLeft,
  /**
   * Use this origin to flip the content on y-axis if the GPU backend has different origin to your
   * system views. It is usually used when on-screen rendering.
   */
  BottomLeft
};

/**
 * Types for interacting with GL textures created externally to PAG.
 */
struct GLTextureInfo {
  /**
   * the id of this texture.
   */
  unsigned id = 0;
  /**
   * The target of this texture.
   */
  unsigned target = 0x0DE1;  // GL_TEXTURE_2D;
  /**
   * The pixel format of this texture.
   */
  unsigned format = 0x8058;  // GL_RGBA8;
};

/**
 * Types for interacting with GL frame buffers created externally to PAG.
 */
struct GLFrameBufferInfo {
  /**
   * The id of this frame buffer.
   */
  unsigned id = 0;

  /**
   * The pixel format of this frame buffer.
   */
  unsigned format = 0x8058;  // GL_RGBA8;
};

/**
 * Types for interacting with Metal resources created externally to PAG. Holds the MTLTexture as a
 * const void*.
 */
struct MtlTextureInfo {
  /**
   * Pointer to MTLTexture.
   */
  const void* texture = nullptr;
};

class PAG_API BackendTexture {
 public:
  /**
   * Creates an invalid backend texture.
   */
  BackendTexture() : _width(0), _height(0) {
  }

  /**
   * Creates an OpenGL backend texture.
   */
  BackendTexture(const GLTextureInfo& glInfo, int width, int height);

  /**
   * Creates an Metal backend texture.
   */
  BackendTexture(const MtlTextureInfo& mtlInfo, int width, int height);

  BackendTexture(const BackendTexture& that);

  BackendTexture& operator=(const BackendTexture& that);

  /**
   * Returns true if the backend texture has been initialized.
   */
  bool isValid() const {
    return _width > 0 && _height > 0;
  }

  /**
   * Returns the width of this texture.
   */
  int width() const {
    return _width;
  }

  /**
   * Returns the height of this texture.
   */
  int height() const {
    return _height;
  }

  /**
   * Returns the backend API of this texture.
   */
  Backend backend() const {
    return _backend;
  }

  /**
   * If the backend API is GL, copies a snapshot of the GLTextureInfo struct into the passed in
   * pointer and returns true. Otherwise returns false if the backend API is not GL.
   */
  bool getGLTextureInfo(GLTextureInfo* glTextureInfo) const;

  /**
   * If the backend API is Metal, copies a snapshot of the GrMtlTextureInfo struct into the passed
   * in pointer and returns true. Otherwise returns false if the backend API is not Metal.
   */
  bool getMtlTextureInfo(MtlTextureInfo* mtlTextureInfo) const;

 private:
  Backend _backend = Backend::MOCK;
  int _width = 0;
  int _height = 0;

  union {
    GLTextureInfo glInfo;
    MtlTextureInfo mtlInfo;
  };
};

class PAG_API BackendRenderTarget {
 public:
  /**
   * Creates an invalid backend render target.
   */
  BackendRenderTarget() : _width(0), _height(0) {
  }

  /**
   * Creates an OpenGL backend render target.
   */
  BackendRenderTarget(const GLFrameBufferInfo& glInfo, int width, int height);

  /**
   * Creates an Metal backend render target.
   */
  BackendRenderTarget(const MtlTextureInfo& mtlInfo, int width, int height);

  BackendRenderTarget(const BackendRenderTarget& that);

  BackendRenderTarget& operator=(const BackendRenderTarget&);

  /**
   * Returns true if the backend texture has been initialized.
   */
  bool isValid() const {
    return _width > 0 && _height > 0;
  }

  /**
   * Returns the width of this render target.
   */
  int width() const {
    return _width;
  }

  /**
   * Returns the height of this render target.
   */
  int height() const {
    return _height;
  }

  /**
   * Returns the backend API of this render target.
   */
  Backend backend() const {
    return _backend;
  }

  /**
   * If the backend API is GL, copies a snapshot of the GLFramebufferInfo struct into the passed
   * in pointer and returns true. Otherwise returns false if the backend API is not GL.
   */
  bool getGLFramebufferInfo(GLFrameBufferInfo* glFrameBufferInfo) const;

  /**
   * If the backend API is Metal, copies a snapshot of the MtlTextureInfo struct into the passed
   * in pointer and returns true. Otherwise returns false if the backend API is not Metal.
   */
  bool getMtlTextureInfo(MtlTextureInfo* mtlTextureInfo) const;

 private:
  Backend _backend = Backend::MOCK;
  int _width = 0;
  int _height = 0;
  union {
    GLFrameBufferInfo glInfo;
    MtlTextureInfo mtlInfo;
  };
};

/**
 * Wrapper class for passing into and receiving data from PAG about a backend semaphore object.
 */
class PAG_API BackendSemaphore {
 public:
  BackendSemaphore();

  bool isInitialized() const {
    return _isInitialized;
  }

  void initGL(void* sync);

  void* glSync() const;

 private:
  Backend _backend = Backend::MOCK;
  union {
    void* _glSync;
  };
  bool _isInitialized;
};
}  // namespace pag
