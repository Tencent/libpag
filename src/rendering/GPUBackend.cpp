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

#include "pag/gpu.h"

namespace pag {
BackendTexture::BackendTexture(const GLTextureInfo& glInfo, int width, int height)
    : _backend(Backend::OPENGL), _width(width), _height(height), glInfo(glInfo) {
}

BackendTexture::BackendTexture(const MtlTextureInfo& mtlInfo, int width, int height)
    : _backend(Backend::METAL), _width(width), _height(height), mtlInfo(mtlInfo) {
}

BackendTexture::BackendTexture(const VkImageInfo& vkInfo, int width, int height)
    : _backend(Backend::VULKAN), _width(width), _height(height), vkInfo(vkInfo) {
}

BackendTexture::BackendTexture(const DX11TextureInfo& dx11Info, int width, int height)
    : _backend(Backend::DIRECTX11), _width(width), _height(height), dx11Info(dx11Info) {
}

BackendTexture::BackendTexture(const DX12ResourceInfo& dx12Info, int width, int height)
    : _backend(Backend::DIRECTX12), _width(width), _height(height), dx12Info(dx12Info) {
}

BackendTexture::BackendTexture(const BackendTexture& that) {
  *this = that;
}

BackendTexture& BackendTexture::operator=(const BackendTexture& that) {
  if (!that.isValid()) {
    _width = _height = 0;
    return *this;
  }
  _width = that._width;
  _height = that._height;
  _backend = that._backend;
  switch (that._backend) {
    case Backend::OPENGL:
      glInfo = that.glInfo;
      break;
    case Backend::METAL:
      mtlInfo = that.mtlInfo;
      break;
    case Backend::VULKAN:
      vkInfo = that.vkInfo;
      break;
    case Backend::DIRECTX11:
      dx11Info = that.dx11Info;
      break;
    case Backend::DIRECTX12:
      dx12Info = that.dx12Info;
      break;
    default:
      break;
  }
  return *this;
}

bool BackendTexture::getGLTextureInfo(GLTextureInfo* glTextureInfo) const {
  if (!isValid() || _backend != Backend::OPENGL) {
    return false;
  }
  *glTextureInfo = glInfo;
  return true;
}

bool BackendTexture::getMtlTextureInfo(MtlTextureInfo* mtlTextureInfo) const {
  if (!isValid() || _backend != Backend::METAL) {
    return false;
  }
  *mtlTextureInfo = mtlInfo;
  return true;
}

bool BackendTexture::getVkImageInfo(VkImageInfo* vkImageInfo) const {
  if (!isValid() || _backend != Backend::VULKAN) {
    return false;
  }
  *vkImageInfo = vkInfo;
  return true;
}

bool BackendTexture::getDX11TextureInfo(DX11TextureInfo* dx11TextureInfo) const {
  if (!isValid() || _backend != Backend::DIRECTX11) {
    return false;
  }
  *dx11TextureInfo = dx11Info;
  return true;
}

bool BackendTexture::getDX12ResourceInfo(DX12ResourceInfo* dx12ResourceInfo) const {
  if (!isValid() || _backend != Backend::DIRECTX12) {
    return false;
  }
  *dx12ResourceInfo = dx12Info;
  return true;
}

BackendRenderTarget::BackendRenderTarget(const GLFrameBufferInfo& glInfo, int width, int height)
    : _backend(Backend::OPENGL), _width(width), _height(height), glInfo(glInfo) {
}

BackendRenderTarget::BackendRenderTarget(const MtlTextureInfo& mtlInfo, int width, int height)
    : _backend(Backend::METAL), _width(width), _height(height), mtlInfo(mtlInfo) {
}

BackendRenderTarget::BackendRenderTarget(const VkImageInfo& vkInfo, int width, int height)
    : _backend(Backend::VULKAN), _width(width), _height(height), vkInfo(vkInfo) {
}

BackendRenderTarget::BackendRenderTarget(const DX11TextureInfo& dx11Info, int width, int height)
    : _backend(Backend::DIRECTX11), _width(width), _height(height), dx11Info(dx11Info) {
}

BackendRenderTarget::BackendRenderTarget(const DX12ResourceInfo& dx12Info, int width, int height)
    : _backend(Backend::DIRECTX12), _width(width), _height(height), dx12Info(dx12Info) {
}

BackendRenderTarget::BackendRenderTarget(const BackendRenderTarget& that) {
  *this = that;
}

BackendRenderTarget& BackendRenderTarget::operator=(const BackendRenderTarget& that) {
  if (!that.isValid()) {
    _width = _height = 0;
    return *this;
  }
  _width = that._width;
  _height = that._height;
  _backend = that._backend;
  switch (that._backend) {
    case Backend::OPENGL:
      glInfo = that.glInfo;
      break;
    case Backend::METAL:
      mtlInfo = that.mtlInfo;
      break;
    case Backend::VULKAN:
      vkInfo = that.vkInfo;
      break;
    case Backend::DIRECTX11:
      dx11Info = that.dx11Info;
      break;
    case Backend::DIRECTX12:
      dx12Info = that.dx12Info;
      break;
    default:
      break;
  }
  return *this;
}

bool BackendRenderTarget::getGLFramebufferInfo(GLFrameBufferInfo* glFrameBufferInfo) const {
  if (!isValid() || _backend != Backend::OPENGL) {
    return false;
  }
  *glFrameBufferInfo = glInfo;
  return true;
}

bool BackendRenderTarget::getMtlTextureInfo(MtlTextureInfo* mtlTextureInfo) const {
  if (!isValid() || _backend != Backend::METAL) {
    return false;
  }
  *mtlTextureInfo = mtlInfo;
  return true;
}

bool BackendRenderTarget::getVkImageInfo(VkImageInfo* vkImageInfo) const {
  if (!isValid() || _backend != Backend::VULKAN) {
    return false;
  }
  *vkImageInfo = vkInfo;
  return true;
}

bool BackendRenderTarget::getDX11TextureInfo(DX11TextureInfo* dx11TextureInfo) const {
  if (!isValid() || _backend != Backend::DIRECTX11) {
    return false;
  }
  *dx11TextureInfo = dx11Info;
  return true;
}

bool BackendRenderTarget::getDX12ResourceInfo(DX12ResourceInfo* dx12ResourceInfo) const {
  if (!isValid() || _backend != Backend::DIRECTX12) {
    return false;
  }
  *dx12ResourceInfo = dx12Info;
  return true;
}

BackendSemaphore::BackendSemaphore()
    : _backend(Backend::MOCK), _glSync(nullptr), _isInitialized(false) {
}

void BackendSemaphore::initGL(void* sync) {
  if (sync == nullptr) {
    return;
  }
  _backend = Backend::OPENGL;
  _glSync = sync;
  _isInitialized = true;
}

void* BackendSemaphore::glSync() const {
  if (!_isInitialized || _backend != Backend::OPENGL) {
    return nullptr;
  }
  return _glSync;
}
}  // namespace pag