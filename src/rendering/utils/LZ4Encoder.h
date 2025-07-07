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
class LZ4Encoder {
 public:
  static std::unique_ptr<LZ4Encoder> Make();

  /**
   * Provides the maximum size that LZ4 compression may output in a "worst case" scenario (input
   * data not compressible) This function is primarily useful for memory allocation purposes
   * (destination buffer size).
   */
  static size_t GetMaxOutputSize(size_t inputSize);

  virtual ~LZ4Encoder() = default;

  /**
   * Compresses the contents of a source buffer into a destination buffer. Returns the number of
   * bytes written to the destination buffer after compressing the input. If the method can't
   * compress the entire input to fit into the provided destination buffer, or an error occurs, 0
   * is returned.
   */
  virtual size_t encode(uint8_t* dstBuffer, size_t dstSize, const uint8_t* srcBuffer,
                        size_t srcSize) const = 0;
};
}  // namespace pag
