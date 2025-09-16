/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#define RIFF_HEADER_SIZE 12  // Size of the RIFF header ("RIFFnnnnWEBP").
#define CHUNK_HEADER_SIZE 8  // Size of a chunk header.
#define VP8X_CHUNK_SIZE 10   // Size of a VP8X chunk.
#define TAG_SIZE 4           // Size of a chunk tag (e.g. "VP8L").
#define WEBP_INLINE inline
#define MAX_IMAGE_AREA (1ULL << 32)  // 32-bit max for width x height.
#define MAX_CHUNK_PAYLOAD (~0U - CHUNK_HEADER_SIZE - 1)
#define VP8L_FRAME_HEADER_SIZE 5  // Size of the VP8L frame header.
#define VP8L_MAGIC_BYTE 0x2f      // VP8L signature byte.
#define VP8_FRAME_HEADER_SIZE 10  // Size of the frame header within VP8 data.
#define VP8L_MAX_NUM_BIT_READ 24
#define VP8L_LBITS 64            // Number of bits prefetched (= bit-size of vp8l_val_t).
#define VP8L_IMAGE_SIZE_BITS 14  // Number of bits used to store
#define VP8L_VERSION_BITS 3      // 3 bits reserved for version.

#include <cstdint>
#include <cstdio>

namespace pag {
typedef uint64_t vp8l_val_t;  // right now, this bit-reader can only use 64bit.

struct WebPBitstreamFeatures {
  int width;          // Width in pixels, as read from the bitstream.
  int height;         // Height in pixels, as read from the bitstream.
  int has_alpha;      // True if the bitstream contains an alpha channel.
  int has_animation;  // True if the bitstream is an animation.
  int format;         // 0 = undefined (/mixed), 1 = lossy, 2 = lossless

  uint32_t pad[5];  // padding for later use
};

typedef enum VP8StatusCode {
  VP8_STATUS_OK = 0,
  VP8_STATUS_OUT_OF_MEMORY,
  VP8_STATUS_INVALID_PARAM,
  VP8_STATUS_BITSTREAM_ERROR,
  VP8_STATUS_UNSUPPORTED_FEATURE,
  VP8_STATUS_SUSPENDED,
  VP8_STATUS_USER_ABORT,
  VP8_STATUS_NOT_ENOUGH_DATA
} VP8StatusCode;

typedef struct {
  const uint8_t* data;        // input buffer
  size_t data_size;           // input buffer size
  int have_all_data;          // true if all data is known to be available
  size_t offset;              // offset to main data chunk (VP8 or VP8L)
  const uint8_t* alpha_data;  // points to alpha chunk (if present)
  size_t alpha_data_size;     // alpha chunk size
  size_t compressed_size;     // VP8/VP8L compressed data size
  size_t riff_size;           // size of the riff payload (or 0 if absent)
  int is_lossless;            // true if a VP8L chunk is present
} WebPHeaderStructure;

typedef enum WebPFeatureFlags {
  ANIMATION_FLAG = 0x00000002,
  XMP_FLAG = 0x00000004,
  EXIF_FLAG = 0x00000008,
  ALPHA_FLAG = 0x00000010,
  ICCP_FLAG = 0x00000020,

  ALL_VALID_FLAGS = 0x0000003e
} WebPFeatureFlags;

typedef struct {
  vp8l_val_t val_;      // pre-fetched bits
  const uint8_t* buf_;  // input byte buffer
  size_t len_;          // buffer length
  size_t pos_;          // byte position in buf_
  int bit_pos_;         // current bit-reading position in val_
  int eos_;             // true if a bit was read past the end of buffer
} VP8LBitReader;

static const uint32_t kBitMask[VP8L_MAX_NUM_BIT_READ + 1] = {
    0,        0x000001, 0x000003, 0x000007, 0x00000f, 0x00001f, 0x00003f, 0x00007f, 0x0000ff,
    0x0001ff, 0x0003ff, 0x0007ff, 0x000fff, 0x001fff, 0x003fff, 0x007fff, 0x00ffff, 0x01ffff,
    0x03ffff, 0x07ffff, 0x0fffff, 0x1fffff, 0x3fffff, 0x7fffff, 0xffffff};

// Retrieve basic header information: width, height.
// This function will also validate the header, returning true on success,
// false otherwise. '*width' and '*height' are only valid on successful return.
// Pointers 'width' and 'height' can be passed NULL if deemed irrelevant.
int WebPGetInfo(const uint8_t* data, size_t data_size, int* width, int* height);
}  // namespace pag
