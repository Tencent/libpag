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
//  or required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "AS IS" basis, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstddef>
#include <memory>
#include <vector>
#include "codec/utils/DecodeStream.h"
#include "codec/utils/EncodeStream.h"

namespace pagx {

// Attribute block IO (spec/pag_binary_spec.md §4): a tag's content is a flag bit stream
// (one exist bit per field, in declaration order, LSB-first) followed by the value region
// with one value per set flag. The new format has no animatable or spatial flag bits; key
// frames live in their own tags.

/**
 * Writes one attribute block. The writer accumulates flag bits and value bytes in two
* streams and concatenates them on release(), mirroring the legacy WriteTagBlock layout:
 * flags aligned to a byte boundary first, values after.
 */
class PAGAttributeBlockWriter {
 public:
  explicit PAGAttributeBlockWriter(pag::StreamContext* context)
      : _flagBytes(context), _valueBytes(context) {
  }

  /**
   * Records one exist bit. Call once per field in declaration order; skip writing the
   * value whenever the bit is false.
   */
  void writeFlag(bool exist) {
    _flagBytes.writeUBits(exist ? 1 : 0, 1);
  }

  /**
   * The stream that collects the value region. Callers write set-flag values in
   * declaration order.
   */
  pag::EncodeStream* valueStream() {
    return &_valueBytes;
  }

  /**
   * Returns flags (byte-aligned) followed by values as a single byte block.
   */
  std::unique_ptr<pag::ByteData> release() {
    _flagBytes.alignWithBytes();
    _flagBytes.writeBytes(&_valueBytes);
    return _flagBytes.release();
  }

 private:
  pag::EncodeStream _flagBytes;
  pag::EncodeStream _valueBytes;
};

/**
 * Reads one attribute block. The constructor consumes flagCount exist bits, aligns the
 * stream to the value region, and exposes the flags by index. Values are then read from
 * the same stream by the caller in declaration order.
 */
class PAGAttributeBlockReader {
 public:
  PAGAttributeBlockReader(pag::DecodeStream* content, size_t flagCount) : _flags(flagCount, false) {
    for (size_t i = 0; i < flagCount; i++) {
      _flags[i] = content->readUBits(1) != 0;
    }
    content->alignWithBytes();
  }

  /**
   * Returns the exist bit of the field at the given declaration index.
   */
  bool flag(size_t index) const {
    return _flags[index];
  }

 private:
  std::vector<bool> _flags;
};

}  // namespace pagx
