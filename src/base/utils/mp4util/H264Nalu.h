//
// Created by katacai on 2022/3/8.
//

#pragma once
#include "pag/types.h"

namespace pag {
enum NaluType {
  NAL_UNKNOWN = 0,
  NAL_SLICE = 1,
  NAL_SLICE_DPA = 2,
  NAL_SLICE_DPB = 3,
  NAL_SLICE_DPC = 4,
  NAL_SLICE_IDR = 5, /* ref_idc != 0 */
  NAL_SEI = 6,       /* ref_idc == 0 */
  NAL_SPS = 7,
  NAL_PPS = 8,
  NAL_AUD = 9,
  NAL_FILLER = 12,
  /* ref_idc == 0 for 6,9,10,11,12 */
};

class H264Nalu {
 public:
  H264Nalu(uint8_t* payload, int size);
  ~H264Nalu();
  bool isKeyframe();
  int getPayloadSize();
  int getSize();
  uint8_t* getData();

  bool isVcl = false;
  int sliceType = 0;
  int refIdc;
  int type;
  int firstMb;
  int size;
  uint8_t* payload;

 private:
  std::unique_ptr<ByteData> data = nullptr;
};
}  // namespace pag
