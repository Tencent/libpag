/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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

#include <memory>

namespace pag {
class LZ4Decoder {
 public:
  static std::unique_ptr<LZ4Decoder> Make();

  virtual ~LZ4Decoder() = default;

  /**
   * Decompresses the contents of a source buffer into a destination buffer. Returns the number of
   * bytes written to the destination buffer after decompressing the input.
   */
  virtual size_t decode(uint8_t* dstBuffer, size_t dstSize, const uint8_t* srcBuffer,
                        size_t srcSize) const = 0;
};
}  // namespace pag
