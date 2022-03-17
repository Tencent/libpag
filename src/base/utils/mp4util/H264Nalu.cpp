//
// Created by katacai on 2022/3/8.
//

#include "H264Nalu.h"
#include "CommonConfig.h"

namespace pag {
H264Nalu::H264Nalu(uint8_t* payload, int size) {
  this->payload = payload;
  this->size = size;
  refIdc = (payload[0] & 0x60) >> 5;
  type = payload[0] & 0x1f;
  isVcl = type == NaluType::NAL_SLICE || type == NaluType::NAL_SLICE_IDR;
  firstMb = 0;

  ByteArray byteArray(nullptr, getSize());
  byteArray.setOrder(byteOrder);
  byteArray.writeInt32(size);
  byteArray.writeBytes(payload, size);
  data = byteArray.release();
}

H264Nalu::~H264Nalu() {
  payload = nullptr;
  data = nullptr;
}

bool H264Nalu::isKeyframe() { return type == NaluType::NAL_SLICE_IDR; }

int H264Nalu::getPayloadSize() { return size; }

int H264Nalu::getSize() { return 4 + size; }

uint8_t* H264Nalu::getData() { return data->data(); }
}  // namespace pag
