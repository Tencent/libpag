/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "ByteArray.h"
#include <fstream>
#include <iostream>

namespace exporter {

bool CheckBigEndian() {
  const int i = 1;
  const auto p = reinterpret_cast<const char*>(&i);
  return *p == 0;
}

void ByteArray::skip(uint32_t size) {
  if (size <= bytesAvailable()) {
    _position += size;
  } else {
    std::cout << "End of file was encountered.";
    _position = _length;
  }
}

ByteArray ByteArray::readBytes(uint32_t length) {
  if (length == UINT32_MAX) {
    length = _length - _position;
  }
  if (_position + length <= _length) {
    ByteArray byteBuffer(bytes + _position, length);
    _position += length;
    return byteBuffer;
  }
  std::cout << "End of file was encountered.";
  _position = _length;
  return {};
}

std::string ByteArray::readUTF8String() {
  if (_position == _length) {
    return "";
  }
  if (_position < _length) {
    auto text = reinterpret_cast<const char*>(bytes + _position);
    auto textLength = strlen(text);
    if (textLength > _length - _position) {
      textLength = _length - _position;
    }
    _position += textLength + 1;
    return {text, textLength};
  }
  std::cout << "End of file was encountered.";
  _position = _length;
  return "";
}

Bit8 ByteArray::readBit8() {
  Bit8 data = {};
  if (_position < _length) {
    data.uintValue = bytes[_position++];
  } else {
    std::cout << "End of file was encountered.";
    _position = _length;
  }
  return data;
}

Bit16 ByteArray::readBit16() {
  Bit16 data = {};
  if (_position < _length - 1) {
    if (IS_BIG_ENDIAN) {
      data.bytes[0] = bytes[_position++];
      data.bytes[1] = bytes[_position++];
    } else {
      data.bytes[1] = bytes[_position++];
      data.bytes[0] = bytes[_position++];
    }
  } else {
    std::cout << "End of file was encountered.";
    _position = _length;
  }
  return data;
}

Bit32 ByteArray::readBit32() {
  Bit32 data = {};
  if (_position < _length - 3) {
    if (IS_BIG_ENDIAN) {
      for (int i = 0; i < 4; i++) {
        data.bytes[i] = bytes[_position++];
      }
    } else {
      for (int i = 3; i >= 0; i--) {
        data.bytes[i] = bytes[_position++];
      }
    }
  } else {
    std::cout << "End of file was encountered.";
    _position = _length;
  }
  return data;
}

Bit64 ByteArray::readBit64() {
  Bit64 data = {};
  if (_position < _length - 7) {
    if (IS_BIG_ENDIAN) {
      for (int i = 0; i < 8; i++) {
        data.bytes[i] = bytes[_position++];
      }
    } else {
      for (int i = 7; i >= 0; i--) {
        data.bytes[i] = bytes[_position++];
      }
    }
  } else {
    std::cout << "End of file was encountered.";
    _position = _length;
  }
  return data;
}

}  // namespace exporter
