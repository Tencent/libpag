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
//  license is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#if defined(TGFX_USE_METAL)

#import <Metal/Metal.h>

namespace pag {

unsigned GetMetalTexturePixelFormat(const void* mtlTexture) {
  if (mtlTexture == nullptr) {
    return 0;
  }
  return static_cast<unsigned>(((__bridge id<MTLTexture>)mtlTexture).pixelFormat);
}

}  // namespace pag

#endif  // TGFX_USE_METAL
