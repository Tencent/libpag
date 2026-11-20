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
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "base/utils/cast/D3D12Texture.h"
#include <d3d12.h>

namespace pag {

unsigned GetD3D12TexturePixelFormat(void* resource) {
  if (resource == nullptr) {
    return 0;
  }
  auto desc = static_cast<ID3D12Resource*>(resource)->GetDesc();
  return static_cast<unsigned>(desc.Format);
}

}  // namespace pag
