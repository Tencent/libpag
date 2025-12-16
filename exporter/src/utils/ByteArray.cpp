/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 Tencent. All rights reserved.
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
#include <iostream>
#include "src/base/utils/Log.h"
namespace exporter {

void ByteArray::setPosition(size_t value) {
  if (value < dataView.size()) {
    _position = value;
    _bitPosition = _position * 8;
  }
}

void ByteArray::skip(size_t numBytes) {
  if (!checkEndOfFile(numBytes)) {
    positionChanged(numBytes);
  }
}

bool ByteArray::readBoolean() {
  if (!checkEndOfFile(1)) {
    auto value = dataView.getBoolean(_position);
    positionChanged(1);
    return value;
  }
  return false;
}

int8_t ByteArray::readInt8() {
  if (!checkEndOfFile(1)) {
    auto value = dataView.getInt8(_position);
    positionChanged(1);
    return value;
  }
  return 0;
}

uint8_t ByteArray::readUint8() {
  if (!checkEndOfFile(1)) {
    auto value = dataView.getUint8(_position);
    positionChanged(1);
    return value;
  }
  return 0;
}

int16_t ByteArray::readInt16() {
  if (!checkEndOfFile(2)) {
    auto value = dataView.getInt16(_position);
    positionChanged(2);
    return value;
  }
  return 0;
}

uint16_t ByteArray::readUint16() {
  if (!checkEndOfFile(2)) {
    auto value = dataView.getUint16(_position);
    positionChanged(2);
    return value;
  }
  return 0;
}

int32_t ByteArray::readInt32() {
  if (!checkEndOfFile(4)) {
    auto value = dataView.getInt32(_position);
    positionChanged(4);
    return value;
  }
  return 0;
}

uint32_t ByteArray::readUint32() {
  if (!checkEndOfFile(4)) {
    auto value = dataView.getUint32(_position);
    positionChanged(4);
    return value;
  }
  return 0;
}

int64_t ByteArray::readInt64() {
  if (!checkEndOfFile(8)) {
    auto value = dataView.getInt64(_position);
    positionChanged(8);
    return value;
  }
  return 0;
}

uint64_t ByteArray::readUint64() {
  if (!checkEndOfFile(8)) {
    auto value = dataView.getUint64(_position);
    positionChanged(8);
    return value;
  }
  return 0;
}

float ByteArray::readFloat() {
  if (!checkEndOfFile(4)) {
    auto value = dataView.getFloat(_position);
    positionChanged(4);
    return value;
  }
  return 0;
}

double ByteArray::readDouble() {
  if (!checkEndOfFile(8)) {
    auto value = dataView.getDouble(_position);
    positionChanged(8);
    return value;
  }
  return 0;
}

ByteArray ByteArray::readBytes(uint32_t length) {
  if (!checkEndOfFile(length)) {
    ByteArray byteBuffer(dataView.bytes() + _position, length);
    positionChanged(length);
    return byteBuffer;
  }
  return {};
}

std::string ByteArray::readUTF8String() {
  if (_position < dataView.size()) {
    auto text = reinterpret_cast<const char*>(dataView.bytes() + _position);
    auto maxLength = dataView.size() - _position;
    auto textLength = strnlen(text, maxLength);
    textLength = std::min(textLength, maxLength);
    positionChanged(textLength);
    return {text, textLength};
  }
  LOGI("End of file was encountered.");
  return "";
}

void ByteArray::bitPositionChanged(size_t offset) {
  _bitPosition += offset;
  _position = BitsToBytes(_bitPosition);
}

void ByteArray::positionChanged(size_t offset) {
  _position += offset;
  _bitPosition = _position * 8;
}

bool ByteArray::checkEndOfFile(size_t bytesToRead) {
  // The _position must not use the uint32_t type, otherwise it will overflow.
  if (_position + bytesToRead > dataView.size()) {
    LOGI("End of file was encountered.");
    return true;
  }
  return false;
}

}  // namespace exporter