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

#include "TestUtils.h"

#if defined(TGFX_USE_METAL)

#import <Metal/Metal.h>
#include "tgfx/gpu/Context.h"
#include "tgfx/gpu/metal/MetalDevice.h"

namespace pag {

BackendTexture ToBackendTexture(const tgfx::MetalTextureInfo& texture, int width, int height) {
  MtlTextureInfo info = {};
  // pag::MtlTextureInfo::texture is void*; tgfx::MetalTextureInfo::texture is const void*.
  // The pointer is treated as an opaque id<MTLTexture> handle downstream.
  info.texture = const_cast<void*>(texture.texture);
  info.format = texture.format;
  return {info, width, height};
}

BackendRenderTarget ToBackendRenderTarget(const tgfx::MetalTextureInfo& texture, int width,
                                          int height) {
  MtlTextureInfo info = {};
  info.texture = const_cast<void*>(texture.texture);
  info.format = texture.format;
  return {info, width, height};
}

bool CreateMetalTexture(tgfx::Context* context, int width, int height,
                        tgfx::MetalTextureInfo* texture) {
  if (context == nullptr || texture == nullptr || width <= 0 || height <= 0) {
    return false;
  }
  auto* metalDevice = static_cast<tgfx::MetalDevice*>(context->device());
  if (metalDevice == nullptr) {
    return false;
  }
  id<MTLDevice> mtlDevice = (id<MTLDevice>)metalDevice->metalDevice();
  if (mtlDevice == nil) {
    return false;
  }
  MTLTextureDescriptor* descriptor =
      [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                         width:(NSUInteger)width
                                                        height:(NSUInteger)height
                                                     mipmapped:NO];
  descriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
  id<MTLTexture> mtlTexture = [mtlDevice newTextureWithDescriptor:descriptor];
  if (mtlTexture == nil) {
    return false;
  }
  // libpag test builds without ARC; the -newTextureWithDescriptor: call above returns an object
  // with a +1 retain count that we hand over to the caller. ReleaseMetalTexture() balances the
  // retain — letting the void* fall out of scope leaks the MTLTexture.
  texture->texture = mtlTexture;
  texture->format = MTLPixelFormatRGBA8Unorm;
  return true;
}

void ReleaseMetalTexture(tgfx::MetalTextureInfo* texture) {
  if (texture == nullptr || texture->texture == nullptr) {
    return;
  }
  // Drop the +1 retain acquired in CreateMetalTexture. Cast via id to route through the ObjC
  // runtime instead of CFRelease (id<MTLTexture> is a proper NSObject subclass).
  id<MTLTexture> mtlTexture = (id<MTLTexture>)texture->texture;
  [mtlTexture release];
  texture->texture = nullptr;
}

}  // namespace pag

#endif  // TGFX_USE_METAL
