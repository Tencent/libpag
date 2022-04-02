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

#include <memory>
#include "codec/utils/ByteOrder.h"
#include "pag/types.h"

namespace pag {

class SimpleArray {
 public:
  explicit SimpleArray(uint32_t capacity = 128);

  ~SimpleArray();

  /**
   * Modifies this ByteArray's byte order.
   */
  void setOrder(ByteOrder order);

  /**
   * Call the this method to take ownership of the current bytes. Once the release method is called,
   * the ByteArray object will be reset to a new one, and the returned bytes will be managed by the
   * ByteData object.
   */
  std::unique_ptr<ByteData> release();

  /**
   * Writes a unsigned 8-bit integer to the byte stream.
   */
  void writeUint8(uint8_t value);

  /**
   * Writes a 32-bit signed integer to the byte stream.
   */
  void writeInt32(int32_t value);

  void writeBytes(uint8_t* byteArray, uint32_t length, uint32_t offset = 0);

 private:
  void ensureCapacity(uint32_t length);
  void expandCapacity(uint32_t length);
  void positionChanged();

  ByteOrder _order = ByteOrder::BigEndian;
  uint8_t* bytes = nullptr;
  uint32_t capacity = 0;
  uint32_t _length = 0;
  uint32_t _position = 0;
};

}  // namespace pag
