//
// Created by katacai on 2022/3/28.
//

#include "SimpleArray.h"
namespace pag {
SimpleArray::SimpleArray(uint32_t capacity) {
  this->capacity = capacity;
  bytes = new uint8_t[capacity];
}

SimpleArray::~SimpleArray() { delete[] bytes; }

void SimpleArray::setOrder(ByteOrder order) { _order = order; }

std::unique_ptr<ByteData> SimpleArray::release() {
  auto data = ByteData::MakeAdopted(bytes, _length);
  capacity = 256;
  _position = 0;
  _length = 0;
  bytes = new uint8_t[capacity];
  return data;
}

void SimpleArray::writeUint8(uint8_t value) {
  ensureCapacity(_position + 1);
  bytes[_position++] = value;
  positionChanged();
}

void SimpleArray::writeInt32(int32_t value) {
  ensureCapacity(_position + 4);
  if (_order == NATIVE_BYTE_ORDER) {
    for (int i = 0; i < 4; i++) {
      bytes[_position++] = value >> i * 8;
    }
  } else {
    for (int i = 3; i >= 0; i--) {
      bytes[_position++] = value >> i * 8;
    }
  }
  positionChanged();
}

void SimpleArray::writeBytes(uint8_t *byteArray, uint32_t length, uint32_t offset) {
  ensureCapacity(_position + length);
  memcpy(bytes + _position, byteArray + offset, length);
  _position += length;
  positionChanged();
}

void SimpleArray::ensureCapacity(uint32_t length) {
  if (length > capacity) {
    expandCapacity(length);
  }
}

void SimpleArray::expandCapacity(uint32_t length) {
  while (capacity < length) {
    capacity = static_cast<uint32_t>(capacity * 1.5);
  }
  auto newBytes = new uint8_t[capacity];
  memcpy(newBytes, bytes, _length);
  delete[] bytes;
  bytes = newBytes;
}

void SimpleArray::positionChanged() {
  if (_position > _length) {
    _length = _position;
  }
}

}