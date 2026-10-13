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

#include "H264SPSParser.h"
#include <vector>

namespace pag {
namespace {
// A frame wider or taller than this cannot come from a stream that any decoder supports, and
// rejecting it also keeps the size arithmetic below well inside the range of an int.
constexpr uint32_t MAX_FRAME_DIMENSION = 16384;
// The coded size is measured in 16 pixel macroblocks, so this is the same bound as a macroblock
// count, which also keeps the multiplication below in range.
constexpr uint32_t MAX_FRAME_DIMENSION_IN_MBS = MAX_FRAME_DIMENSION / 16;

// The offset cycle in pic_order_cnt_type 1 holds at most this many entries.
constexpr uint32_t MAX_ORDER_COUNT_CYCLE = 255;

// The NAL unit header of a sequence parameter set, which is the first byte of the parameter set.
constexpr uint32_t SEQUENCE_PARAMETER_SET_TYPE = 7;

// Profiles that carry the extra chroma and bit depth fields before the frame size fields.
bool IsHighProfile(uint32_t profileIdc) {
  switch (profileIdc) {
    case 100:
    case 110:
    case 122:
    case 244:
    case 44:
    case 83:
    case 86:
    case 118:
    case 128:
    case 138:
    case 139:
    case 134:
    case 135:
      return true;
    default:
      return false;
  }
}

// Reads the bit stream of a sequence parameter set. Encoders insert an emulation prevention byte
// after every two zero bytes so that no start code appears inside the payload, and those bytes are
// removed here so the parser sees the raw bits.
class BitReader {
 public:
  BitReader(const uint8_t* data, size_t length) {
    bytes.reserve(length);
    for (size_t i = 0; i < length; i++) {
      if (i >= 2 && data[i] == 0x03 && data[i - 1] == 0x00 && data[i - 2] == 0x00) {
        continue;
      }
      bytes.push_back(data[i]);
    }
  }

  bool readBit(uint32_t* value) {
    if (bitPosition >= bytes.size() * 8) {
      return false;
    }
    auto byte = bytes[bitPosition / 8];
    *value = (byte >> (7 - (bitPosition % 8))) & 1;
    bitPosition++;
    return true;
  }

  bool readBits(int count, uint32_t* value) {
    uint32_t result = 0;
    for (int i = 0; i < count; i++) {
      uint32_t bit = 0;
      if (!readBit(&bit)) {
        return false;
      }
      result = (result << 1) | bit;
    }
    *value = result;
    return true;
  }

  // Unsigned exponential Golomb code.
  bool readUnsigned(uint32_t* value) {
    uint32_t bit = 0;
    int leadingZeros = 0;
    while (true) {
      if (!readBit(&bit)) {
        return false;
      }
      if (bit != 0) {
        break;
      }
      leadingZeros++;
      if (leadingZeros > 31) {
        return false;
      }
    }
    uint32_t rest = 0;
    if (leadingZeros > 0 && !readBits(leadingZeros, &rest)) {
      return false;
    }
    *value = (1u << leadingZeros) - 1 + rest;
    return true;
  }

  // Signed exponential Golomb code.
  bool readSigned(int32_t* value) {
    uint32_t code = 0;
    if (!readUnsigned(&code)) {
      return false;
    }
    auto magnitude = static_cast<int32_t>((code + 1) / 2);
    *value = (code & 1) != 0 ? magnitude : -magnitude;
    return true;
  }

  // Scale lists are not needed for the frame size but they have to be consumed to reach it.
  bool skipScalingList(int size) {
    int32_t lastScale = 8;
    int32_t nextScale = 8;
    for (int i = 0; i < size; i++) {
      if (nextScale != 0) {
        int32_t deltaScale = 0;
        if (!readSigned(&deltaScale)) {
          return false;
        }
        nextScale = (lastScale + deltaScale + 256) % 256;
      }
      if (nextScale != 0) {
        lastScale = nextScale;
      }
    }
    return true;
  }

 private:
  std::vector<uint8_t> bytes = {};
  size_t bitPosition = 0;
};

bool SkipFieldsBeforeFrameSize(BitReader* reader, uint32_t profileIdc) {
  uint32_t constraintFlags = 0;
  uint32_t levelIdc = 0;
  uint32_t spsId = 0;
  if (!reader->readBits(8, &constraintFlags) || !reader->readBits(8, &levelIdc) ||
      !reader->readUnsigned(&spsId)) {
    return false;
  }

  if (IsHighProfile(profileIdc)) {
    uint32_t chromaFormatIdc = 0;
    uint32_t separateColourPlaneFlag = 0;
    uint32_t bitDepthLumaMinus8 = 0;
    uint32_t bitDepthChromaMinus8 = 0;
    uint32_t transformBypassFlag = 0;
    uint32_t scalingMatrixPresentFlag = 0;
    if (!reader->readUnsigned(&chromaFormatIdc) || chromaFormatIdc > 3) {
      return false;
    }
    if (chromaFormatIdc == 3 && !reader->readBit(&separateColourPlaneFlag)) {
      return false;
    }
    if (!reader->readUnsigned(&bitDepthLumaMinus8) ||
        !reader->readUnsigned(&bitDepthChromaMinus8) || !reader->readBit(&transformBypassFlag) ||
        !reader->readBit(&scalingMatrixPresentFlag)) {
      return false;
    }
    if (scalingMatrixPresentFlag != 0) {
      auto count = chromaFormatIdc == 3 ? 12 : 8;
      for (int i = 0; i < count; i++) {
        uint32_t scalingListPresentFlag = 0;
        if (!reader->readBit(&scalingListPresentFlag)) {
          return false;
        }
        if (scalingListPresentFlag != 0 && !reader->skipScalingList(i < 6 ? 16 : 64)) {
          return false;
        }
      }
    }
  }

  uint32_t log2MaxFrameNumMinus4 = 0;
  uint32_t orderCountType = 0;
  if (!reader->readUnsigned(&log2MaxFrameNumMinus4) || !reader->readUnsigned(&orderCountType)) {
    return false;
  }
  if (orderCountType == 0) {
    uint32_t log2MaxPicOrderCntLsbMinus4 = 0;
    if (!reader->readUnsigned(&log2MaxPicOrderCntLsbMinus4)) {
      return false;
    }
  } else if (orderCountType == 1) {
    uint32_t deltaOrderAlwaysZeroFlag = 0;
    int32_t offsetForNonRefPic = 0;
    int32_t offsetForTopToBottomField = 0;
    uint32_t cycleCount = 0;
    if (!reader->readBit(&deltaOrderAlwaysZeroFlag) || !reader->readSigned(&offsetForNonRefPic) ||
        !reader->readSigned(&offsetForTopToBottomField) || !reader->readUnsigned(&cycleCount)) {
      return false;
    }
    if (cycleCount > MAX_ORDER_COUNT_CYCLE) {
      return false;
    }
    for (uint32_t i = 0; i < cycleCount; i++) {
      int32_t offsetForRefFrame = 0;
      if (!reader->readSigned(&offsetForRefFrame)) {
        return false;
      }
    }
  }
  uint32_t maxNumRefFrames = 0;
  uint32_t gapsInFrameNumAllowedFlag = 0;
  return reader->readUnsigned(&maxNumRefFrames) && reader->readBit(&gapsInFrameNumAllowedFlag);
}
}  // namespace

bool ParseH264SPSFrameSize(const uint8_t* data, size_t length, H264FrameSize* frameSize) {
  if (data == nullptr || frameSize == nullptr || length <= 4) {
    return false;
  }
  BitReader reader(data + 4, length - 4);
  uint32_t nalHeader = 0;
  uint32_t profileIdc = 0;
  if (!reader.readBits(8, &nalHeader) || !reader.readBits(8, &profileIdc)) {
    return false;
  }
  if ((nalHeader & 0x1F) != SEQUENCE_PARAMETER_SET_TYPE) {
    return false;
  }
  if (!SkipFieldsBeforeFrameSize(&reader, profileIdc)) {
    return false;
  }
  uint32_t widthInMbs = 0;
  uint32_t heightInMapUnits = 0;
  uint32_t frameMbsOnly = 0;
  if (!reader.readUnsigned(&widthInMbs) || !reader.readUnsigned(&heightInMapUnits) ||
      !reader.readBit(&frameMbsOnly)) {
    return false;
  }
  if (frameMbsOnly == 0) {
    uint32_t mbAdaptiveFrameFieldFlag = 0;
    if (!reader.readBit(&mbAdaptiveFrameFieldFlag)) {
      return false;
    }
  }
  if (widthInMbs >= MAX_FRAME_DIMENSION_IN_MBS || heightInMapUnits >= MAX_FRAME_DIMENSION_IN_MBS) {
    return false;
  }
  auto width = (widthInMbs + 1) * 16;
  auto height = (heightInMapUnits + 1) * 16 * (2 - frameMbsOnly);
  if (width == 0 || height == 0 || width > MAX_FRAME_DIMENSION || height > MAX_FRAME_DIMENSION) {
    return false;
  }
  frameSize->width = static_cast<int>(width);
  frameSize->height = static_cast<int>(height);
  return true;
}
}  // namespace pag
