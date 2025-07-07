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

#include "WebpDecoder.h"
#include <assert.h>
#include <string.h>

namespace pag {

// Read 16, 24 or 32 bits stored in little-endian order.
static WEBP_INLINE int GetLE16(const uint8_t* const data) {
  return (int)(data[0] << 0) | (data[1] << 8);
}

static WEBP_INLINE int GetLE24(const uint8_t* const data) {
  return GetLE16(data) | (data[2] << 16);
}

static WEBP_INLINE uint32_t GetLE32(const uint8_t* const data) {
  return GetLE16(data) | ((uint32_t)GetLE16(data + 2) << 16);
}

static VP8StatusCode ParseVP8X(const uint8_t** const data, size_t* const data_size,
                               int* const found_vp8x, int* const width_ptr, int* const height_ptr,
                               uint32_t* const flags_ptr) {
  const uint32_t vp8x_size = CHUNK_HEADER_SIZE + VP8X_CHUNK_SIZE;
  assert(data != nullptr);
  assert(data_size != nullptr);
  assert(found_vp8x != nullptr);

  *found_vp8x = 0;

  if (*data_size < CHUNK_HEADER_SIZE) {
    return VP8_STATUS_NOT_ENOUGH_DATA;  // Insufficient data.
  }

  if (!memcmp(*data, "VP8X", TAG_SIZE)) {
    int width, height;
    uint32_t flags;
    const uint32_t chunk_size = GetLE32(*data + TAG_SIZE);
    if (chunk_size != VP8X_CHUNK_SIZE) {
      return VP8_STATUS_BITSTREAM_ERROR;  // Wrong chunk size.
    }

    // Verify if enough data is available to validate the VP8X chunk.
    if (*data_size < vp8x_size) {
      return VP8_STATUS_NOT_ENOUGH_DATA;  // Insufficient data.
    }
    flags = GetLE32(*data + 8);
    width = 1 + GetLE24(*data + 12);
    height = 1 + GetLE24(*data + 15);
    if (width * (uint64_t)height >= MAX_IMAGE_AREA) {
      return VP8_STATUS_BITSTREAM_ERROR;  // image is too large
    }

    if (flags_ptr != nullptr) *flags_ptr = flags;
    if (width_ptr != nullptr) *width_ptr = width;
    if (height_ptr != nullptr) *height_ptr = height;
    // Skip over VP8X header bytes.
    *data += vp8x_size;
    *data_size -= vp8x_size;
    *found_vp8x = 1;
  }
  return VP8_STATUS_OK;
}

static VP8StatusCode ParseRIFF(const uint8_t** const data, size_t* const data_size,
                               int have_all_data, size_t* const riff_size) {
  assert(data != nullptr);
  assert(data_size != nullptr);
  assert(riff_size != nullptr);

  *riff_size = 0;  // Default: no RIFF present.
  if (*data_size >= RIFF_HEADER_SIZE && !memcmp(*data, "RIFF", TAG_SIZE)) {
    if (memcmp(*data + 8, "WEBP", TAG_SIZE)) {
      return VP8_STATUS_BITSTREAM_ERROR;  // Wrong image file signature.
    } else {
      const uint32_t size = GetLE32(*data + TAG_SIZE);
      // Check that we have at least one chunk (i.e "WEBP" + "VP8?nnnn").
      if (size < TAG_SIZE + CHUNK_HEADER_SIZE) {
        return VP8_STATUS_BITSTREAM_ERROR;
      }
      if (size > MAX_CHUNK_PAYLOAD) {
        return VP8_STATUS_BITSTREAM_ERROR;
      }
      if (have_all_data && (size > *data_size - CHUNK_HEADER_SIZE)) {
        return VP8_STATUS_NOT_ENOUGH_DATA;  // Truncated bitstream.
      }
      // We have a RIFF container. Skip it.
      *riff_size = size;
      *data += RIFF_HEADER_SIZE;
      *data_size -= RIFF_HEADER_SIZE;
    }
  }
  return VP8_STATUS_OK;
}

// Skips to the next VP8/VP8L chunk header in the data given the size of the
// RIFF chunk 'riff_size'.
// Returns VP8_STATUS_BITSTREAM_ERROR if any invalid chunk size is encountered,
//         VP8_STATUS_NOT_ENOUGH_DATA in case of insufficient data, and
//         VP8_STATUS_OK otherwise.
// If an alpha chunk is found, *alpha_data and *alpha_size are set
// appropriately.
static VP8StatusCode ParseOptionalChunks(const uint8_t** const data, size_t* const data_size,
                                         size_t const riff_size, const uint8_t** const alpha_data,
                                         size_t* const alpha_size) {
  const uint8_t* buf;
  size_t buf_size;
  uint32_t total_size = TAG_SIZE +           // "WEBP".
                        CHUNK_HEADER_SIZE +  // "VP8Xnnnn".
                        VP8X_CHUNK_SIZE;     // data.
  assert(data != nullptr);
  assert(data_size != nullptr);
  buf = *data;
  buf_size = *data_size;

  assert(alpha_data != nullptr);
  assert(alpha_size != nullptr);
  *alpha_data = nullptr;
  *alpha_size = 0;

  while (1) {
    uint32_t chunk_size;
    uint32_t disk_chunk_size;  // chunk_size with padding

    *data = buf;
    *data_size = buf_size;

    if (buf_size < CHUNK_HEADER_SIZE) {  // Insufficient data.
      return VP8_STATUS_NOT_ENOUGH_DATA;
    }

    chunk_size = GetLE32(buf + TAG_SIZE);
    if (chunk_size > MAX_CHUNK_PAYLOAD) {
      return VP8_STATUS_BITSTREAM_ERROR;  // Not a valid chunk size.
    }
    // For odd-sized chunk-payload, there's one byte padding at the end.
    disk_chunk_size = (CHUNK_HEADER_SIZE + chunk_size + 1) & ~1;
    total_size += disk_chunk_size;

    // Check that total bytes skipped so far does not exceed riff_size.
    if (riff_size > 0 && (total_size > riff_size)) {
      return VP8_STATUS_BITSTREAM_ERROR;  // Not a valid chunk size.
    }

    // Start of a (possibly incomplete) VP8/VP8L chunk implies that we have
    // parsed all the optional chunks.
    // Note: This check must occur before the check 'buf_size < disk_chunk_size'
    // below to allow incomplete VP8/VP8L chunks.
    if (!memcmp(buf, "VP8 ", TAG_SIZE) || !memcmp(buf, "VP8L", TAG_SIZE)) {
      return VP8_STATUS_OK;
    }

    if (buf_size < disk_chunk_size) {  // Insufficient data.
      return VP8_STATUS_NOT_ENOUGH_DATA;
    }

    if (!memcmp(buf, "ALPH", TAG_SIZE)) {  // A valid ALPH header.
      *alpha_data = buf + CHUNK_HEADER_SIZE;
      *alpha_size = chunk_size;
    }

    // We have a full and valid chunk; skip it.
    buf += disk_chunk_size;
    buf_size -= disk_chunk_size;
  }
}

int VP8LCheckSignature(const uint8_t* const data, size_t size) {
  return (size >= VP8L_FRAME_HEADER_SIZE && data[0] == VP8L_MAGIC_BYTE &&
          (data[4] >> 5) == 0);  // version
}

// Validates the VP8/VP8L Header ("VP8 nnnn" or "VP8L nnnn") and skips over it.
// Returns VP8_STATUS_BITSTREAM_ERROR for invalid (chunk larger than
//         riff_size) VP8/VP8L header,
//         VP8_STATUS_NOT_ENOUGH_DATA in case of insufficient data, and
//         VP8_STATUS_OK otherwise.
// If a VP8/VP8L chunk is found, *chunk_size is set to the total number of bytes
// extracted from the VP8/VP8L chunk header.
// The flag '*is_lossless' is set to 1 in case of VP8L chunk / raw VP8L data.
static VP8StatusCode ParseVP8Header(const uint8_t** const data_ptr, size_t* const data_size,
                                    int have_all_data, size_t riff_size, size_t* const chunk_size,
                                    int* const is_lossless) {
  const uint8_t* const data = *data_ptr;
  const int is_vp8 = !memcmp(data, "VP8 ", TAG_SIZE);
  const int is_vp8l = !memcmp(data, "VP8L", TAG_SIZE);
  const uint32_t minimal_size = TAG_SIZE + CHUNK_HEADER_SIZE;  // "WEBP" + "VP8 nnnn" OR
  // "WEBP" + "VP8Lnnnn"
  assert(data != nullptr);
  assert(data_size != nullptr);
  assert(chunk_size != nullptr);
  assert(is_lossless != nullptr);

  if (*data_size < CHUNK_HEADER_SIZE) {
    return VP8_STATUS_NOT_ENOUGH_DATA;  // Insufficient data.
  }

  if (is_vp8 || is_vp8l) {
    // Bitstream contains VP8/VP8L header.
    const uint32_t size = GetLE32(data + TAG_SIZE);
    if ((riff_size >= minimal_size) && (size > riff_size - minimal_size)) {
      return VP8_STATUS_BITSTREAM_ERROR;  // Inconsistent size information.
    }
    if (have_all_data && (size > *data_size - CHUNK_HEADER_SIZE)) {
      return VP8_STATUS_NOT_ENOUGH_DATA;  // Truncated bitstream.
    }
    // Skip over CHUNK_HEADER_SIZE bytes from VP8/VP8L Header.
    *chunk_size = size;
    *data_ptr += CHUNK_HEADER_SIZE;
    *data_size -= CHUNK_HEADER_SIZE;
    *is_lossless = is_vp8l;
  } else {
    // Raw VP8/VP8L bitstream (no header).
    *is_lossless = VP8LCheckSignature(data, *data_size);
    *chunk_size = *data_size;
  }

  return VP8_STATUS_OK;
}

int VP8CheckSignature(const uint8_t* const data, size_t data_size) {
  return (data_size >= 3 && data[0] == 0x9d && data[1] == 0x01 && data[2] == 0x2a);
}

int VP8GetInfo(const uint8_t* data, size_t data_size, size_t chunk_size, int* const width,
               int* const height) {
  if (data == nullptr || data_size < VP8_FRAME_HEADER_SIZE) {
    return 0;  // not enough data
  }
  // check signature
  if (!VP8CheckSignature(data + 3, data_size - 3)) {
    return 0;  // Wrong signature.
  } else {
    const uint32_t bits = data[0] | (data[1] << 8) | (data[2] << 16);
    const int key_frame = !(bits & 1);
    const int w = ((data[7] << 8) | data[6]) & 0x3fff;
    const int h = ((data[9] << 8) | data[8]) & 0x3fff;

    if (!key_frame) {  // Not a keyframe.
      return 0;
    }

    if (((bits >> 1) & 7) > 3) {
      return 0;  // unknown profile
    }
    if (!((bits >> 4) & 1)) {
      return 0;  // first frame is invisible!
    }
    if (((bits >> 5)) >= chunk_size) {  // partition_length
      return 0;                         // inconsistent size information.
    }
    if (w == 0 || h == 0) {
      return 0;  // We don't support both width and height to be zero.
    }

    if (width) {
      *width = w;
    }
    if (height) {
      *height = h;
    }

    return 1;
  }
}

void VP8LInitBitReader(VP8LBitReader* const br, const uint8_t* const start, size_t length) {
  size_t i;
  vp8l_val_t value = 0;
  assert(br != nullptr);
  assert(start != nullptr);
  assert(length < 0xfffffff8u);  // can't happen with a RIFF chunk.

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

static WEBP_INLINE uint32_t VP8LPrefetchBits(VP8LBitReader* const br) {
  return (uint32_t)(br->val_ >> (br->bit_pos_ & (VP8L_LBITS - 1)));
}

static WEBP_INLINE int VP8LIsEndOfStream(const VP8LBitReader* const br) {
  assert(br->pos_ <= br->len_);
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

uint32_t VP8LReadBits(VP8LBitReader* const br, int n_bits) {
  assert(n_bits >= 0);
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

int VP8LGetInfo(const uint8_t* data, size_t data_size, int* const width, int* const height,
                int* const has_alpha) {
  if (data == nullptr || data_size < VP8L_FRAME_HEADER_SIZE) {
    return 0;  // not enough data
  } else if (!VP8LCheckSignature(data, data_size)) {
    return 0;  // bad signature
  } else {
    int w, h, a;
    VP8LBitReader br;
    VP8LInitBitReader(&br, data, data_size);
    if (!ReadImageInfo(&br, &w, &h, &a)) {
      return 0;
    }
    if (width != nullptr) *width = w;
    if (height != nullptr) *height = h;
    if (has_alpha != nullptr) *has_alpha = a;
    return 1;
  }
}

static VP8StatusCode ParseHeadersInternal(const uint8_t* data, size_t data_size, int* const width,
                                          int* const height, int* const has_alpha,
                                          int* const has_animation, int* const format,
                                          WebPHeaderStructure* const headers) {
  int canvas_width = 0;
  int canvas_height = 0;
  int image_width = 0;
  int image_height = 0;
  int found_riff = 0;
  int found_vp8x = 0;
  int animation_present = 0;
  const int have_all_data = (headers != nullptr) ? headers->have_all_data : 0;

  VP8StatusCode status;
  WebPHeaderStructure hdrs;

  if (data == nullptr || data_size < RIFF_HEADER_SIZE) {
    return VP8_STATUS_NOT_ENOUGH_DATA;
  }
  memset(&hdrs, 0, sizeof(hdrs));
  hdrs.data = data;
  hdrs.data_size = data_size;

  // Skip over RIFF header.
  status = ParseRIFF(&data, &data_size, have_all_data, &hdrs.riff_size);
  if (status != VP8_STATUS_OK) {
    return status;  // Wrong RIFF header / insufficient data.
  }
  found_riff = (hdrs.riff_size > 0);

  // Skip over VP8X.
  {
    uint32_t flags = 0;
    status = ParseVP8X(&data, &data_size, &found_vp8x, &canvas_width, &canvas_height, &flags);
    if (status != VP8_STATUS_OK) {
      return status;  // Wrong VP8X / insufficient data.
    }
    animation_present = !!(flags & ANIMATION_FLAG);
    if (!found_riff && found_vp8x) {
      // Note: This restriction may be removed in the future, if it becomes
      // necessary to send VP8X chunk to the decoder.
      return VP8_STATUS_BITSTREAM_ERROR;
    }
    if (has_alpha != nullptr) *has_alpha = !!(flags & ALPHA_FLAG);
    if (has_animation != nullptr) *has_animation = animation_present;
    if (format != nullptr) *format = 0;  // default = undefined

    image_width = canvas_width;
    image_height = canvas_height;
    if (found_vp8x && animation_present && headers == nullptr) {
      status = VP8_STATUS_OK;
      goto ReturnWidthHeight;  // Just return features from VP8X header.
    }
  }

  if (data_size < TAG_SIZE) {
    status = VP8_STATUS_NOT_ENOUGH_DATA;
    goto ReturnWidthHeight;
  }

  // Skip over optional chunks if data started with "RIFF + VP8X" or "ALPH".
  if ((found_riff && found_vp8x) ||
      (!found_riff && !found_vp8x && !memcmp(data, "ALPH", TAG_SIZE))) {
    status = ParseOptionalChunks(&data, &data_size, hdrs.riff_size, &hdrs.alpha_data,
                                 &hdrs.alpha_data_size);
    if (status != VP8_STATUS_OK) {
      goto ReturnWidthHeight;  // Invalid chunk size / insufficient data.
    }
  }

  // Skip over VP8/VP8L header.
  status = ParseVP8Header(&data, &data_size, have_all_data, hdrs.riff_size, &hdrs.compressed_size,
                          &hdrs.is_lossless);
  if (status != VP8_STATUS_OK) {
    goto ReturnWidthHeight;  // Wrong VP8/VP8L chunk-header / insufficient data.
  }
  if (hdrs.compressed_size > MAX_CHUNK_PAYLOAD) {
    return VP8_STATUS_BITSTREAM_ERROR;
  }

  if (format != nullptr && !animation_present) {
    *format = hdrs.is_lossless ? 2 : 1;
  }

  if (!hdrs.is_lossless) {
    if (data_size < VP8_FRAME_HEADER_SIZE) {
      status = VP8_STATUS_NOT_ENOUGH_DATA;
      goto ReturnWidthHeight;
    }
    // Validates raw VP8 data.
    if (!VP8GetInfo(data, data_size, (uint32_t)hdrs.compressed_size, &image_width, &image_height)) {
      return VP8_STATUS_BITSTREAM_ERROR;
    }
  } else {
    if (data_size < VP8L_FRAME_HEADER_SIZE) {
      status = VP8_STATUS_NOT_ENOUGH_DATA;
      goto ReturnWidthHeight;
    }
    // Validates raw VP8L data.
    if (!VP8LGetInfo(data, data_size, &image_width, &image_height, has_alpha)) {
      return VP8_STATUS_BITSTREAM_ERROR;
    }
  }
  // Validates image size coherency.
  if (found_vp8x) {
    if (canvas_width != image_width || canvas_height != image_height) {
      return VP8_STATUS_BITSTREAM_ERROR;
    }
  }
  if (headers != nullptr) {
    *headers = hdrs;
    headers->offset = data - headers->data;
    assert((uint64_t)(data - headers->data) < MAX_CHUNK_PAYLOAD);
    assert(headers->offset == headers->data_size - data_size);
  }
ReturnWidthHeight:
  if (status == VP8_STATUS_OK ||
      (status == VP8_STATUS_NOT_ENOUGH_DATA && found_vp8x && headers == nullptr)) {
    if (has_alpha != nullptr) {
      // If the data did not contain a VP8X/VP8L chunk the only definitive way
      // to set this is by looking for alpha data (from an ALPH chunk).
      *has_alpha |= (hdrs.alpha_data != nullptr);
    }
    if (width != nullptr) *width = image_width;
    if (height != nullptr) *height = image_height;
    return VP8_STATUS_OK;
  } else {
    return status;
  }
}

static void DefaultFeatures(WebPBitstreamFeatures* const features) {
  assert(features != nullptr);
  memset(features, 0, sizeof(*features));
}

static VP8StatusCode GetFeatures(const uint8_t* const data, size_t data_size,
                                 WebPBitstreamFeatures* const features) {
  if (features == nullptr || data == nullptr) {
    return VP8_STATUS_INVALID_PARAM;
  }
  DefaultFeatures(features);

  // Only parse enough of the data to retrieve the features.
  return ParseHeadersInternal(data, data_size, &features->width, &features->height,
                              &features->has_alpha, &features->has_animation, &features->format,
                              nullptr);
}

int WebPGetInfo(const uint8_t* data, size_t data_size, int* width, int* height) {
  WebPBitstreamFeatures features = {};

  if (data == nullptr) {
    return 0;
  }

  if (GetFeatures(data, data_size, &features) != VP8_STATUS_OK) {
    return 0;
  }

  if (width != nullptr) {
    *width = features.width;
  }
  if (height != nullptr) {
    *height = features.height;
  }

  return 1;
}
}  // namespace pag
