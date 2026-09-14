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
  size_t alphaStartXOffset;
  size_t alphaStartYOffset;
};

static constexpr SequenceHeader SEQUENCE_HEADERS[] = {
    {"resources/apitest/wz_mvp.pag", 103, 109, 110},
    {"assets/RootLayerVideoOffset.pag", 92, 98, 100},
};

static void EncodeInt32(uint8_t* out, int32_t value) {
  auto encoded = static_cast<uint32_t>(value < 0 ? -value : value) << 1;
  if (value < 0) {
    encoded |= 1;
  }
  int count = 0;
  while (true) {
    auto group = static_cast<uint8_t>(encoded & 0x7F);
    encoded >>= 7;
    if (encoded != 0) {
      out[count++] = static_cast<uint8_t>(group | 0x80);
    } else {
      out[count++] = group;
      break;
    }
  }
}

// Returns a copy of the asset with the given field of its video sequence header replaced by value.
// The value is chosen to need the same number of bytes as the original, so the rest of the file
// stays readable.
static std::shared_ptr<tgfx::Data> MakeSample(const SequenceHeader& header, size_t offset,
                                              int32_t value, size_t fieldLength) {
  auto bytes = ReadFile(header.path);
  if (bytes == nullptr || bytes->size() <= offset + fieldLength) {
    return nullptr;
  }
  auto sample = tgfx::Data::MakeWithCopy(bytes->data(), bytes->size());
  auto writable = const_cast<uint8_t*>(static_cast<const uint8_t*>(sample->data()));
  uint8_t encoded[4] = {};
  EncodeInt32(encoded, value);
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
    ExpectLoadResult(MakeSample(header, header.heightOffset, 4095, 2), false);
  }
}

// The alpha offsets are added to the declared width and height, so a large one inflates the
// declared video size in the same way.
PAG_TEST(PAGVideoSequenceTest, RejectsAlphaStartBeyondImageSize) {
  // wz_mvp.pag packs alpha vertically, RootLayerVideoOffset.pag packs it side by side.
  ExpectLoadResult(MakeSample(SEQUENCE_HEADERS[0], SEQUENCE_HEADERS[0].alphaStartYOffset, 4000, 2),
                   false);
  ExpectLoadResult(MakeSample(SEQUENCE_HEADERS[1], SEQUENCE_HEADERS[1].alphaStartXOffset, 4000, 2),
                   false);
}

}  // namespace pag
