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

#include "core/images/webp/WebpUtility.h"
#include <cstdint>
#include <cstdio>
#include "base/utils/Log.h"
#include "core/utils/OrientationHelper.h"
#include "webp/decode.h"
#include "webp/demux.h"
#include "webp/encode.h"

namespace pag {

#define VP8L_MAGIC_BYTE 0x2f      // VP8L signature byte.
#define VP8L_FRAME_HEADER_SIZE 5  // Size of the VP8L frame header.
#define VP8_FRAME_HEADER_SIZE 10  // Size of the frame header within VP8 data.
#define TAG_SIZE 4                // Size of a chunk tag (e.g. "VP8L").
#define CHUNK_SIZE_BYTES 4        // Size needed to store chunk's size.
#define CHUNK_HEADER_SIZE 8       // Size of a chunk header.
#define RIFF_HEADER_SIZE 12       // Size of the RIFF header ("RIFFnnnnWEBP").
#define ANMF_CHUNK_SIZE 16        // Size of an ANMF chunk.
#define ANIM_CHUNK_SIZE 6         // Size of an ANIM chunk.
#define VP8X_CHUNK_SIZE 10        // Size of a VP8X chunk.
#define MAX_CHUNK_PAYLOAD (~0U - CHUNK_HEADER_SIZE - 1)
// Create fourcc of the chunk from the chunk tag characters.
#define MKFOURCC(a, b, c, d) ((a) | (b) << 8 | (c) << 16 | (uint32_t)(d) << 24)
// maximum number of bits (inclusive) the bit-reader can handle:
#define VP8L_MAX_NUM_BIT_READ 24
#define VP8L_LBITS 64            // Number of bits prefetched (= bit-size of vp8l_val_t).
#define VP8L_MAGIC_BYTE 0x2f     // VP8L signature byte.
#define VP8L_IMAGE_SIZE_BITS 14  // Number of bits used to store
#define VP8L_VERSION_BITS 3      // 3 bits reserved for version.

typedef uint64_t vp8l_val_t;  // right now, this bit-reader can only use 64bit.

typedef struct {
  vp8l_val_t val_;      // pre-fetched bits
  const uint8_t* buf_;  // input byte buffer
  size_t len_;          // buffer length
  size_t pos_;          // byte position in buf_
  int bit_pos_;         // current bit-reading position in val_
  int eos_;             // true if a bit was read past the end of buffer
} VP8LBitReader;

struct WebpFile {
  FILE* inFile;
  size_t _start;
  size_t _end;
  size_t _riff_end;
};

static const uint32_t kBitMask[VP8L_MAX_NUM_BIT_READ + 1] = {
    0,        0x000001, 0x000003, 0x000007, 0x00000f, 0x00001f, 0x00003f, 0x00007f, 0x0000ff,
    0x0001ff, 0x0003ff, 0x0007ff, 0x000fff, 0x001fff, 0x003fff, 0x007fff, 0x00ffff, 0x01ffff,
    0x03ffff, 0x07ffff, 0x0fffff, 0x1fffff, 0x3fffff, 0x7fffff, 0xffffff};

static inline int GetLE16(const uint8_t* const data) {
  return static_cast<int>((data[0] << 0) | (data[1] << 8));
}
static inline int GetLE24(const uint8_t* const data) {
  return GetLE16(data) | (data[2] << 16);
}
static inline uint32_t GetLE32(const uint8_t* const data) {
  return GetLE16(data) | ((uint32_t)GetLE16(data + 2) << 16);
}

static inline void Skip(WebpFile* const ptr, size_t size) {
  ptr->_start += size;
}
static bool VP8CheckSignature(const uint8_t* const data) {
  return (data[0] == 0x9d && data[1] == 0x01 && data[2] == 0x2a);
}

static bool VP8LCheckSignature(const uint8_t* const data) {
  return (data[0] == VP8L_MAGIC_BYTE && (data[4] >> 5) == 0);  // version
}

// Return the prefetched bits, so they can be looked up.
static inline uint32_t VP8LPrefetchBits(VP8LBitReader* const br) {
  return (uint32_t)(br->val_ >> (br->bit_pos_ & (VP8L_LBITS - 1)));
}

// Returns true if there was an attempt at reading bit past the end of
// the buffer. Doesn't set br->eos_ flag.
static inline int VP8LIsEndOfStream(const VP8LBitReader* const br) {
  ASSERT(br->pos_ <= br->len_);
  return br->eos_ || ((br->pos_ == br->len_) && (br->bit_pos_ > VP8L_LBITS));
}

static void VP8LSetEndOfStream(VP8LBitReader* const br) {
  br->eos_ = 1;
  br->bit_pos_ = 0;  // To avoid undefined behaviour with shifts.
}

// If not at EOS, reload up to VP8L_LBITS byte-by-byte
static void ShiftBytes(VP8LBitReader* const br) {
  while (br->bit_pos_ >= 8 && br->pos_ < br->len_) {
    br->val_ >>= 8;
    br->val_ |= ((vp8l_val_t)br->buf_[br->pos_]) << (VP8L_LBITS - 8);
    ++br->pos_;
    br->bit_pos_ -= 8;
  }
  if (VP8LIsEndOfStream(br)) {
    VP8LSetEndOfStream(br);
  }
}

static void VP8LInitBitReader(VP8LBitReader* const br, const uint8_t* const start, size_t length) {
  size_t i;
  vp8l_val_t value = 0;
  ASSERT(br != NULL);
  ASSERT(start != NULL);
  ASSERT(length < 0xfffffff8u);  // can't happen with a RIFF chunk.

  br->len_ = length;
  br->val_ = 0;
  br->bit_pos_ = 0;
  br->eos_ = 0;

  if (length > sizeof(br->val_)) {
    length = sizeof(br->val_);
  }
  for (i = 0; i < length; ++i) {
    value |= (vp8l_val_t)start[i] << (8 * i);
  }
  br->val_ = value;
  br->pos_ = length;
  br->buf_ = start;
}

static uint32_t VP8LReadBits(VP8LBitReader* const br, int n_bits) {
  ASSERT(n_bits >= 0);
  // Flag an error if end_of_stream or n_bits is more than allowed limit.
  if (!br->eos_ && n_bits <= VP8L_MAX_NUM_BIT_READ) {
    const uint32_t val = VP8LPrefetchBits(br) & kBitMask[n_bits];
    const int new_bits = br->bit_pos_ + n_bits;
    br->bit_pos_ = new_bits;
    ShiftBytes(br);
    return val;
  } else {
    VP8LSetEndOfStream(br);
    return 0;
  }
}
static int ReadImageInfo(VP8LBitReader* const br, int* const width, int* const height,
                         int* const has_alpha) {
  if (VP8LReadBits(br, 8) != VP8L_MAGIC_BYTE) return 0;
  *width = VP8LReadBits(br, VP8L_IMAGE_SIZE_BITS) + 1;
  *height = VP8LReadBits(br, VP8L_IMAGE_SIZE_BITS) + 1;
  *has_alpha = VP8LReadBits(br, 1);
  if (VP8LReadBits(br, VP8L_VERSION_BITS) != 0) return 0;
  return !br->eos_;
}

static bool ParseVP8L(WebpFile& webpFile, DecodeInfo& decodeInfo,
                      const uint32_t chunk_size_padded) {
  if (decodeInfo.width <= 0) {
    if (chunk_size_padded < VP8L_FRAME_HEADER_SIZE) return false;
    fseek(webpFile.inFile, webpFile._start + CHUNK_HEADER_SIZE, SEEK_SET);
    auto vp8xHeader = static_cast<uint8_t*>(malloc(VP8L_FRAME_HEADER_SIZE));
    if (fread(vp8xHeader, 1, VP8L_FRAME_HEADER_SIZE, webpFile.inFile) != VP8L_FRAME_HEADER_SIZE)
      return false;
    if (!VP8LCheckSignature(vp8xHeader)) return false;
    int w, h, a;
    VP8LBitReader br;
    VP8LInitBitReader(&br, vp8xHeader, 10);
    if (!ReadImageInfo(&br, &w, &h, &a)) {
      free(vp8xHeader);
      return false;
    }
    decodeInfo.width = w;
    decodeInfo.height = h;
    free(vp8xHeader);
  }
  if (chunk_size_padded <= webpFile._end - webpFile._start) {
    Skip(&webpFile, chunk_size_padded + CHUNK_HEADER_SIZE);
  } else {
    return false;
  }
  return true;
}

static bool ParseVP8(WebpFile& webpFile, DecodeInfo& decodeInfo, const uint32_t chunk_size_padded) {
  if (decodeInfo.width <= 0) {
    if (chunk_size_padded < VP8_FRAME_HEADER_SIZE) return false;
    fseek(webpFile.inFile, webpFile._start + CHUNK_HEADER_SIZE, SEEK_SET);
    auto vp8xHeader = static_cast<uint8_t*>(malloc(15));
    if (fread(vp8xHeader, 1, VP8_FRAME_HEADER_SIZE, webpFile.inFile) != VP8_FRAME_HEADER_SIZE)
      return false;
    if (VP8CheckSignature(vp8xHeader)) return false;
    decodeInfo.width = ((vp8xHeader[7] << 8) | vp8xHeader[6]) & 0x3fff;
    decodeInfo.height = ((vp8xHeader[9] << 8) | vp8xHeader[8]) & 0x3fff;
    free(vp8xHeader);
  }
  if (chunk_size_padded <= webpFile._end - webpFile._start) {
    Skip(&webpFile, chunk_size_padded + CHUNK_HEADER_SIZE);
  } else {
    return false;
  }
  return true;
}

static bool ParseVP8X(WebpFile& webpFile, DecodeInfo& decodeInfo,
                      const uint32_t chunk_size_padded) {
  if (decodeInfo.width <= 0) {
    fseek(webpFile.inFile, webpFile._start + RIFF_HEADER_SIZE, SEEK_SET);
    auto vp8xHeader = static_cast<uint8_t*>(malloc(6));
    if (fread(vp8xHeader, 1, 6, webpFile.inFile) != 6) return false;
    decodeInfo.width = 1 + GetLE24(vp8xHeader);
    decodeInfo.height = 1 + GetLE24(vp8xHeader + 3);
    free(vp8xHeader);
  }
  if (chunk_size_padded <= webpFile._end - webpFile._start) {
    Skip(&webpFile, chunk_size_padded + CHUNK_HEADER_SIZE);
  } else {
    return false;
  }
  return true;
}

DecodeInfo WebpUtility::getDecodeInfo(const std::string& filePath) {
  auto infile = fopen(filePath.c_str(), "rb");
  if (infile == nullptr) return {};
  if (fseek(infile, 0, SEEK_END)) {
    fclose(infile);
    return {};
  }
  DecodeInfo decodeInfo;
  size_t fileLength = ftell(infile);
  WebpFile webpFile = {infile, 0, fileLength, fileLength};
  webpFile._start = 12;
  auto chunkHeader = static_cast<uint8_t*>(malloc(RIFF_HEADER_SIZE));
  bool foundOrientation = false;
  do {
    fseek(infile, webpFile._start, SEEK_SET);
    if ((fileLength - webpFile._start) < RIFF_HEADER_SIZE) break;
    if (fread(chunkHeader, 1, RIFF_HEADER_SIZE, infile) != RIFF_HEADER_SIZE) break;
    const uint32_t fourcc = GetLE32(chunkHeader);
    const uint32_t chunk_size = GetLE32(chunkHeader + 4);
    const uint32_t chunk_size_padded = chunk_size + (chunk_size & 1);
    if (chunk_size > MAX_CHUNK_PAYLOAD) break;
    if (chunk_size_padded > webpFile._riff_end - webpFile._start) break;
    bool needBreak = false;
    switch (fourcc) {
      case MKFOURCC('V', 'P', '8', 'L'): {
        needBreak = !ParseVP8L(webpFile, decodeInfo, chunk_size_padded);
        break;
      }
      case MKFOURCC('V', 'P', '8', ' '): {
        needBreak = !ParseVP8(webpFile, decodeInfo, chunk_size_padded);
        break;
      }
      case MKFOURCC('V', 'P', '8', 'X'): {
        needBreak = !ParseVP8X(webpFile, decodeInfo, chunk_size_padded);
        break;
      }
      case MKFOURCC('E', 'X', 'I', 'F'): {
        foundOrientation = true;
        fseek(infile, webpFile._start + CHUNK_HEADER_SIZE, SEEK_SET);
        auto exifData = static_cast<uint8_t*>(malloc(chunk_size));
        if (fread(exifData, 1, chunk_size, infile) != chunk_size) {
          needBreak = true;
          break;
        }
        is_orientation_marker(exifData, chunk_size, &decodeInfo.orientation);
        if (chunk_size_padded <= webpFile._end - webpFile._start) {
          Skip(&webpFile, chunk_size_padded + CHUNK_HEADER_SIZE);
        } else {
          needBreak = true;
        }
        break;
      }
      default: {
        if (chunk_size_padded <= webpFile._end - webpFile._start) {
          Skip(&webpFile, chunk_size_padded + CHUNK_HEADER_SIZE);
        } else {
          needBreak = true;
          break;
        }
      }
    }
    if (needBreak) break;
  } while (!foundOrientation && (fileLength - webpFile._start) >= RIFF_HEADER_SIZE);
  free(chunkHeader);
  fclose(infile);
  return decodeInfo;
}

DecodeInfo WebpUtility::getDecodeInfo(const void* fileBytes, size_t byteLength) {
  if (fileBytes == nullptr || byteLength == 0) return {};
  WebPData webpData = {static_cast<const uint8_t*>(fileBytes), byteLength};
  WebPDemuxState state;
  auto demux = WebPDemuxPartial(&webpData, &state);
  switch (state) {
    case WEBP_DEMUX_PARSE_ERROR:
    case WEBP_DEMUX_PARSING_HEADER:
      return {};
    case WEBP_DEMUX_PARSED_HEADER:
    case WEBP_DEMUX_DONE:
      break;
  }
  const int width = WebPDemuxGetI(demux, WEBP_FF_CANVAS_WIDTH);
  const int height = WebPDemuxGetI(demux, WEBP_FF_CANVAS_HEIGHT);
  // Sanity check for image size that's about to be decoded.
  {
    const int64_t size = (int64_t)width * (int64_t)height;
    // now check that if we are 4-bytes per pixel, we also don't overflow
    if (!static_cast<int32_t>(size) || static_cast<int32_t>(size) > (0x7FFFFFFF >> 2)) {
      WebPDemuxDelete(demux);
      return {};
    }
  }

  Orientation origin = Orientation::TopLeft;
  {
    WebPChunkIterator chunkIterator;
    if (WebPDemuxGetChunk(demux, "EXIF", 1, &chunkIterator)) {
      is_orientation_marker(chunkIterator.chunk.bytes, chunkIterator.chunk.size, &origin);
    }
    WebPDemuxReleaseChunkIterator(&chunkIterator);
  }
  WebPDemuxDelete(demux);
  return {width, height, origin};
}

}  // namespace pag