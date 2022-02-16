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

#include "gpu/metal/MtlTypes.h"
#include "gpu/opengl/GLTypes.h"

namespace tgfx {
/**
 * Possible GPU backend APIs that may be used by TGFX.
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
 * Wrapper class for passing into and receiving data from TGFX about a backend texture object.
 */
class BackendTexture {
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

/**
 * Wrapper class for passing into and receiving data from TGFX about a backend render target object.
 */
class BackendRenderTarget {
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
 * Wrapper class for passing into and receiving data from TGFX about a backend semaphore object.
 */
class BackendSemaphore {
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
}  // namespace tgfx
