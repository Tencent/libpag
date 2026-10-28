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

#pragma once

#include <cstddef>
#include <type_traits>

namespace pag {
/**
 * Checks the representation and tail alignment of a manually packed std140 uniform block.
 * Member offsets and array/matrix strides must still be checked against the shader declaration.
 */
template <typename Uniforms, size_t ExpectedSize>
constexpr bool HasStd140UniformLayout() {
  return std::is_standard_layout_v<Uniforms> && std::is_trivially_copyable_v<Uniforms> &&
         alignof(Uniforms) >= 16 && sizeof(Uniforms) == ExpectedSize && ExpectedSize % 16 == 0;
}
}  // namespace pag
