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

#include "LZ4Decoder.h"

#ifdef PAG_USE_SYSTEM_LZ4
#include <compression.h>
#else
#include "lz4.h"
#endif

namespace pag {
#ifdef PAG_USE_SYSTEM_LZ4
class AppleLZ4Decoder : public LZ4Decoder {
 public:
  AppleLZ4Decoder() {
    auto scratchSize = compression_decode_scratch_buffer_size(COMPRESSION_LZ4);
    if (scratchSize > 0) {
      scratchBuffer = new (std::nothrow) uint8_t[scratchSize];
    }
  }

  ~AppleLZ4Decoder() override {
    delete[] scratchBuffer;
  }

  size_t decode(uint8_t* dstBuffer, size_t dstSize, const uint8_t* srcBuffer,
                size_t srcSize) const override {
    return compression_decode_buffer(dstBuffer, dstSize, srcBuffer, srcSize, scratchBuffer,
                                     COMPRESSION_LZ4);
  }

 private:
  uint8_t* scratchBuffer = nullptr;
};

std::unique_ptr<LZ4Decoder> LZ4Decoder::Make() {
  return std::make_unique<AppleLZ4Decoder>();
}

#else

class DefaultLZ4Decoder : public LZ4Decoder {
 public:
  size_t decode(uint8_t* dstBuffer, size_t dstSize, const uint8_t* srcBuffer,
                size_t srcSize) const override {
    return LZ4_decompress_safe(reinterpret_cast<const char*>(srcBuffer),
                               reinterpret_cast<char*>(dstBuffer), static_cast<int>(srcSize),
                               static_cast<int>(dstSize));
  }
};

std::unique_ptr<LZ4Decoder> LZ4Decoder::Make() {
  return std::make_unique<DefaultLZ4Decoder>();
}

#endif
}  // namespace pag
