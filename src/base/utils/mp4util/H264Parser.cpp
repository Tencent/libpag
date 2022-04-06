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

#include "H264Parser.h"
#include "codec/utils/DecodeStream.h"

namespace pag {
SpsData H264Parser::ParseSPS(ByteData* sysBytes) {
  DecodeStream byteArray(nullptr, sysBytes->data(), sysBytes->length());
  byteArray.skip(4);
  byteArray.skip(1);

  std::string numberStr = "0123456789ABCDEF";
  std::string codec = "avc1.";
  for (int i = 0; i < 3; ++i) {
    uint8_t num = byteArray.readUint8();
    std::string out;
    out.push_back(numberStr[(num >> 4) & 0xF]);
    out.push_back(numberStr[num & 0xF]);

    if (out.size() < 2) {
      out = "0" + out;
    }
    codec += out;
  }

  std::pair<int, int> size = ReadSPS(sysBytes);
  SpsData spsData;
  spsData.sps = sysBytes;
  spsData.codec = codec;
  spsData.width = size.first;
  spsData.height = size.second;
  return spsData;
}

void H264Parser::SkipScalingList(ExpGolomb *decoder, int count) {
  int lastScale = 8;
  int nextScale = 8;
  int deltaScale;
  for (int j = 0; j < count; j++) {
    if (nextScale != 0) {
      deltaScale = decoder->readEG();
      nextScale = (lastScale + deltaScale + 256) % 256;
    }
    lastScale = nextScale == 0 ? lastScale : nextScale;
  }
}

std::pair<int, int> H264Parser::ReadSPS(ByteData* spsBytes) {
  ExpGolomb decoder(spsBytes);
  decoder.skipBytes(4);
  uint8_t frameCropLeftOffset = 0;
  uint8_t frameCropRightOffset = 0;
  uint8_t frameCropTopOffset = 0;
  uint8_t frameCropBottomOffset = 0;
  float sarScale = 1;
  uint8_t numRefFramesInPicOrderCntCycle = 0;
  uint8_t scalingListCount = 0;
  decoder.skipBits(1);                       // forbidden_zero_bit
  decoder.skipBits(2);                       // nal_ref_idc
  decoder.skipBits(5);                       // nal_unit_type
  uint8_t profileIdc = decoder.readUByte();  // profile_idc
  decoder.skipBits(6);                       // constraint_set[0-5]_flag, u(6)
  decoder.skipBits(2);                       // reserved_zero_3bits u(2),
  decoder.skipBytes(1);                      // level_idc u(8)
  decoder.readUE();                          // seq_parameter_set_id

  std::vector<int> profileIdcMap = {100, 110, 122, 244, 44, 83, 86, 118, 128};

  if (std::find(profileIdcMap.begin(), profileIdcMap.end(), profileIdc) != profileIdcMap.end()) {
    uint8_t chromaFormatIdc = decoder.readUE();
    if (chromaFormatIdc == 3) {
      decoder.skipBits(1);  // separate_colour_plane_flag
    }
    decoder.readUE();     // bit_depth_luma_minus8
    decoder.readUE();     // bit_depth_chroma_minus8
    decoder.skipBits(1);  // qpprime_y_zero_transform_bypass_flag
    uint8_t seqScalingMatrixPresentFlag = decoder.readBoolean();
    if (seqScalingMatrixPresentFlag) {
      scalingListCount = chromaFormatIdc != 3 ? 8 : 12;
      for (int i = 0; i < scalingListCount; ++i) {
        if (decoder.readBoolean()) {
          // seq_scaling_list_present_flag[ i ]
          if (i < 6) {
            SkipScalingList(&decoder, 16);
          } else {
            SkipScalingList(&decoder, 64);
          }
        }
      }
    }
  }

  decoder.readUE();  // log2_max_frame_num_minus4
  int picOrderCntType = decoder.readUE();
  if (picOrderCntType == 0) {
    decoder.readUE();  // log2_max_pic_order_cnt_lsb_minus4
  } else if (picOrderCntType == 1) {
    decoder.skipBits(1);  // delta_pic_order_always_zero_flag
    decoder.readUE();     // offset_for_non_ref_pic
    decoder.readUE();     // offset_for_top_to_bottom_field
    numRefFramesInPicOrderCntCycle = decoder.readUE();
    for (int i = 0; i < numRefFramesInPicOrderCntCycle; ++i) {
      decoder.readUE();  // offset_for_ref_frame[ i ]
    }
  }
  decoder.readUE();     // max_num_ref_frames
  decoder.skipBits(1);  // gaps_in_frame_num_value_allowed_flag
  int picWidthInMbsMinus1 = decoder.readUE();
  int picHeightInMapUnitsMinus1 = decoder.readUE();
  uint8_t frameMbsOnlyFlag = decoder.readBits(1);
  if (frameMbsOnlyFlag == 0) {
    decoder.skipBits(1);  // mb_adaptive_frame_field_flag
  }
  decoder.skipBits(1);  // direct_8x8_inference_flag
  uint8_t frameCroppingFlag = decoder.readBoolean();
  if (frameCroppingFlag) {
    frameCropLeftOffset = decoder.readUE();
    frameCropRightOffset = decoder.readUE();
    frameCropTopOffset = decoder.readUE();
    frameCropBottomOffset = decoder.readUE();
  }
  uint8_t vuiParametersPresentFlag = decoder.readBoolean();
  if (vuiParametersPresentFlag) {
    uint8_t aspectRatioInfoPresentFlag = decoder.readBoolean();
    if (aspectRatioInfoPresentFlag) {
      std::vector<int> sarRatio;
      uint8_t aspectRatioIdc = decoder.readUByte();
      switch (aspectRatioIdc) {
        case 1:
          sarRatio = {1, 1};
          break;
        case 2:
          sarRatio = {12, 11};
          break;
        case 3:
          sarRatio = {10, 11};
          break;
        case 4:
          sarRatio = {16, 11};
          break;
        case 5:
          sarRatio = {40, 33};
          break;
        case 6:
          sarRatio = {24, 11};
          break;
        case 7:
          sarRatio = {20, 11};
          break;
        case 8:
          sarRatio = {32, 11};
          break;
        case 9:
          sarRatio = {80, 33};
          break;
        case 10:
          sarRatio = {18, 11};
          break;
        case 11:
          sarRatio = {15, 11};
          break;
        case 12:
          sarRatio = {64, 33};
          break;
        case 13:
          sarRatio = {160, 99};
          break;
        case 14:
          sarRatio = {4, 3};
          break;
        case 15:
          sarRatio = {3, 2};
          break;
        case 16:
          sarRatio = {2, 1};
          break;
        case 255: {
          sarRatio = {(decoder.readUByte() << 8) | decoder.readUByte(),
                      (decoder.readUByte() << 8) | decoder.readUByte()};
          break;
        }
      }
      if (!sarRatio.empty()) {
        sarScale = (float)sarRatio[0] / (float)sarRatio[1];
      }
    }
  }
  int width = std::ceil(
      (float)((picWidthInMbsMinus1 + 1) * 16 - frameCropLeftOffset * 2 - frameCropRightOffset * 2) *
      sarScale);
  int height = (2 - frameMbsOnlyFlag) * (picHeightInMapUnitsMinus1 + 1) * 16 -
               (frameMbsOnlyFlag ? 2 : 4) * (frameCropTopOffset + frameCropBottomOffset);
  return std::pair<int, int>(width, height);
}
}  // namespace pag
