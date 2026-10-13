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

#include "utils/TestUtils.h"

namespace pag {

// A video sequence declares its own width, height, alphaStartX and alphaStartY, and
// getVideoWidth()/getVideoHeight() add the offsets to the sizes. That declared size is what the
// decoder is configured with and what tgfx copies out of the decoder's frame buffer, so a file that
// declares more than the encoded stream contains must be rejected at load time. The bytes below are
// the offsets of the video sequence header inside the two assets, where an encoded int32 is stored
// for each field.
struct SequenceHeader {
  const char* path;
  size_t heightOffset;
  int32_t height;
  size_t alphaOffset;
  int32_t alphaValue;
};

// wz_mvp.pag packs the alpha plane below the color plane, so it sets alphaStartY.
// RootLayerVideoOffset.pag packs it side by side, so it sets alphaStartX. Both fields patched below
// are stored in two bytes in these assets.
static constexpr SequenceHeader SEQUENCE_HEADERS[] = {
    {"resources/apitest/wz_mvp.pag", 103, 253, 110, 258},
    {"assets/RootLayerVideoOffset.pag", 92, 1080, 98, 720},
};

// Encodes value the way the PAG writer does and fails when it does not fit in capacity.
static bool EncodeInt32(uint8_t* out, size_t capacity, int32_t value, size_t* encodedLength) {
  auto encoded = static_cast<uint32_t>(value < 0 ? -value : value) << 1;
  if (value < 0) {
    encoded |= 1;
  }
  size_t count = 0;
  while (true) {
    if (count >= capacity) {
      return false;
    }
    auto group = static_cast<uint8_t>(encoded & 0x7F);
    encoded >>= 7;
    if (encoded != 0) {
      out[count++] = static_cast<uint8_t>(group | 0x80);
    } else {
      out[count++] = group;
      break;
    }
  }
  *encodedLength = count;
  return true;
}

// Decodes the value stored in fieldLength bytes, to check a sample still starts from the value the
// asset declares.
static bool DecodeInt32(const uint8_t* data, size_t fieldLength, int32_t* value) {
  uint32_t encoded = 0;
  for (size_t i = 0; i < fieldLength; i++) {
    encoded |= static_cast<uint32_t>(data[i] & 0x7F) << (7 * i);
    if ((data[i] & 0x80) == 0) {
      if (i + 1 != fieldLength) {
        return false;
      }
      auto magnitude = static_cast<int32_t>(encoded >> 1);
      *value = (encoded & 1) != 0 ? -magnitude : magnitude;
      return true;
    }
  }
  return false;
}

// Returns a copy of the asset with the given field of its video sequence header replaced by
// newValue. The field currently has to hold originalValue and the new value has to need the same
// number of bytes, so the rest of the file stays readable. Returns nullptr if the asset does not
// have the expected layout.
static std::shared_ptr<tgfx::Data> MakeSample(const SequenceHeader& header, size_t offset,
                                              int32_t originalValue, int32_t newValue) {
  auto bytes = ReadFile(header.path);
  constexpr size_t fieldLength = 2;
  if (bytes == nullptr || bytes->size() <= offset + fieldLength) {
    return nullptr;
  }
  auto raw = static_cast<const uint8_t*>(bytes->data());
  int32_t value = 0;
  uint8_t encoded[4] = {};
  size_t encodedLength = 0;
  if (!DecodeInt32(raw + offset, fieldLength, &value) || value != originalValue ||
      !EncodeInt32(encoded, sizeof(encoded), newValue, &encodedLength) ||
      encodedLength != fieldLength) {
    return nullptr;
  }
  auto sample = tgfx::Data::MakeWithCopy(bytes->data(), bytes->size());
  auto writable = const_cast<uint8_t*>(static_cast<const uint8_t*>(sample->data()));
  memcpy(writable + offset, encoded, fieldLength);
  return sample;
}

static void ExpectLoadResult(const std::shared_ptr<tgfx::Data>& sample, bool expected) {
  if (sample == nullptr) {
    FAIL() << "failed to build the sample";
    return;
  }
  auto pagFile = PAGFile::Load(sample->data(), sample->size());
  if (expected) {
    EXPECT_NE(pagFile, nullptr);
  } else {
    EXPECT_EQ(pagFile, nullptr);
  }
}

// The unmodified assets declare a video size that matches the encoded stream, so they still load.
PAG_TEST(PAGVideoSequenceTest, AcceptsDeclaredSizeMatchingStream) {
  for (auto& header : SEQUENCE_HEADERS) {
    ExpectLoadResult(ReadFile(header.path), true);
  }
}

// Raising the declared height makes the container claim a video that the encoded stream does not
// contain, which used to make the upload read past the decoder's frame buffer.
PAG_TEST(PAGVideoSequenceTest, RejectsDeclaredHeightLargerThanStream) {
  for (auto& header : SEQUENCE_HEADERS) {
    ExpectLoadResult(MakeSample(header, header.heightOffset, header.height, 4095), false);
  }
}

// The alpha offsets are added to the declared width and height, so a large one inflates the
// declared video size in the same way.
PAG_TEST(PAGVideoSequenceTest, RejectsAlphaStartBeyondImageSize) {
  for (auto& header : SEQUENCE_HEADERS) {
    ExpectLoadResult(MakeSample(header, header.alphaOffset, header.alphaValue, 4000), false);
  }
}

}  // namespace pag
