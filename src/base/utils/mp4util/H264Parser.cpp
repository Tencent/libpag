//
// Created by katacai on 2022/3/8.
//

#include "H264Parser.h"
#include "CommonConfig.h"
#include "codec/utils/ByteBuffer.h"

namespace pag {
H264Frame::H264Frame(std::vector<H264Nalu*> units, bool isKeyFrame) {
  this->units = units;
  this->isKeyFrame = isKeyFrame;
}

H264Frame::~H264Frame() {}

std::vector<ByteData*> H264Parser::getNaluFromSequence(VideoSequence& sequence) {
  int splitSize = 4;
  std::vector<ByteData*> naluDatas;
  for (auto header : sequence.headers) {
    int payLoadSize = header->length() - splitSize;
    ByteArray headerPayload(nullptr, payLoadSize);
    headerPayload.setOrder(byteOrder);
    headerPayload.writeBytes(header->data(), payLoadSize, splitSize);
    naluDatas.emplace_back(headerPayload.release().release());
  }
  for (auto frame : sequence.frames) {
    int payLoadSize = frame->fileBytes->length() - splitSize;
    ByteArray framePayload(nullptr, payLoadSize);
    framePayload.setOrder(byteOrder);
    framePayload.writeBytes(frame->fileBytes->data(), payLoadSize, splitSize);
    naluDatas.emplace_back(framePayload.release().release());
  }
  return naluDatas;
}

std::vector<H264Frame*> H264Parser::getH264Frames(std::vector<ByteData*> nalus) {
  std::vector<H264Frame*> frames;
  if (nalus.empty()) {
    LOGE("no h264 nalus\n");
    return frames;
  }
  std::vector<H264Nalu*> units;
  bool isKeyFrame = false;
  bool isVcl = false;
  for (auto nalu : nalus) {
    H264Nalu* unit = new H264Nalu(nalu->data(), nalu->length());
    if (unit->isVcl) {
      parseHeader(unit);
    }
    bool isLastFrameFinish = !units.empty() && isVcl && (unit->firstMb || !unit->isVcl);
    if (isLastFrameFinish) {
      frames.emplace_back(new H264Frame(units, isKeyFrame));
      units = {};
      isKeyFrame = false;
      isVcl = false;
    }
    units.emplace_back(unit);
    isKeyFrame = isKeyFrame || unit->isKeyframe();
    isVcl = isVcl || unit->isVcl;
  }
  // 处理最后一个Frame
  if (!units.empty()) {
    if (isVcl) {
      frames.emplace_back(new H264Frame(units, isKeyFrame));
    } else {
      auto lastFrameIndex = frames.size() - 1;
      H264Frame* lastFrame = frames.at(lastFrameIndex);
      lastFrame->units.insert(lastFrame->units.end(), units.begin(), units.end());
    }
  }
  return frames;
}

void H264Parser::parseHeader(H264Nalu* nalu) {
  ExpGolomb decoder(nalu->payload, nalu->size);
  decoder.readUByte();  // skip NALu type
  nalu->firstMb = decoder.readUEG() == 0;
  nalu->sliceType = decoder.readUEG();
}

SpsData H264Parser::parseSPS(H264Nalu* h264Nalu) {
  ByteBuffer byteArray(nullptr, h264Nalu->payload, h264Nalu->size);
  byteArray.skip(1);

  std::string codec = "avc1.";
  for (int i = 0; i < 3; ++i) {
    uint8_t h = byteArray.readUint8();
    std::string out;
    std::stringstream ss;
    ss << std::hex << h;
    ss >> out;

    if (out.size() < 2) {
      out = "0" + out;
    }
    codec += out;
  }

  std::pair<int, int> size = readSPS(h264Nalu);
  SpsData spsData;
  spsData.sps = ByteData::MakeAdopted(h264Nalu->payload, h264Nalu->size).release();
  spsData.codec = codec;
  spsData.width = size.first;
  spsData.height = size.second;
  return spsData;
}

void H264Parser::skipScalingList(ExpGolomb& decoder, int count) {
  int lastScale = 8;
  int nextScale = 8;
  int deltaScale;
  for (int j = 0; j < count; j++) {
    if (nextScale != 0) {
      deltaScale = decoder.readEG();
      nextScale = (lastScale + deltaScale + 256) % 256;
    }
    lastScale = nextScale == 0 ? lastScale : nextScale;
  }
}

std::pair<int, int> H264Parser::readSPS(H264Nalu* h264Nalu) {
  ExpGolomb decoder(h264Nalu->payload, h264Nalu->size);
  uint8_t frameCropLeftOffset = 0;
  uint8_t frameCropRightOffset = 0;
  uint8_t frameCropTopOffset = 0;
  uint8_t frameCropBottomOffset = 0;
  float sarScale = 1;
  uint8_t numRefFramesInPicOrderCntCycle = 0;
  uint8_t scalingListCount = 0;
  decoder.readBits(1);                       // forbidden_zero_bit
  decoder.readBits(2);                       // nal_ref_idc
  decoder.readBits(5);                       // nal_unit_type
  uint8_t profileIdc = decoder.readUByte();  // profile_idc
  decoder.readBits(6);                       // constraint_set[0-5]_flag, u(6)
  decoder.readBits(2);                       // reserved_zero_3bits u(2),
  decoder.readUByte();                       // level_idc u(8)
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
            skipScalingList(decoder, 16);
          } else {
            skipScalingList(decoder, 64);
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
  decoder.readBits(1);  // direct_8x8_inference_flag
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
      ((picWidthInMbsMinus1 + 1) * 16 - frameCropLeftOffset * 2 - frameCropRightOffset * 2) *
      sarScale);
  int height = (2 - frameMbsOnlyFlag) * (picHeightInMapUnitsMinus1 + 1) * 16 -
               (frameMbsOnlyFlag ? 2 : 4) * (frameCropTopOffset + frameCropBottomOffset);
  return std::pair<int, int>(width, height);
}
}  // namespace pag
