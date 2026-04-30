// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 1 unit tests for the v2 TagHeader writer / reader. Wire format must
// match v1 byte-for-byte (design doc §4.2).
#include <cstdint>
#include <memory>
#include "codec/utils/DecodeStream.h"
#include "codec/utils/EncodeStream.h"
#include "codec/utils/StreamContext.h"
#include "gtest/gtest.h"
#include "pag/types.h"
#include "pagx/pag/TagHeader.h"

namespace pagx::pag {

struct EncodedBuffer {
  std::unique_ptr<::pag::ByteData> bytes;
  ::pag::StreamContext ctx;
  std::unique_ptr<::pag::DecodeStream> reader;
};

static EncodedBuffer Finalize(::pag::EncodeStream* encoder) {
  EncodedBuffer out;
  out.bytes = encoder->release();
  out.reader = std::make_unique<::pag::DecodeStream>(&out.ctx, out.bytes->data(),
                                                     static_cast<uint32_t>(out.bytes->length()));
  return out;
}

TEST(TagHeader, ShortFormRoundTrip) {
  ::pag::StreamContext sc;
  ::pag::EncodeStream encoder(&sc);
  // length < 63 → 2-byte header.
  WriteTagHeader(&encoder, TagCode::FileHeader, /*length=*/10);
  auto buf = Finalize(&encoder);
  EXPECT_EQ(buf.bytes->length(), 2u);

  auto header = ReadTagHeader(buf.reader.get());
  EXPECT_EQ(header.code, TagCode::FileHeader);
  EXPECT_EQ(header.length, 10u);
}

TEST(TagHeader, LongFormRoundTrip) {
  ::pag::StreamContext sc;
  ::pag::EncodeStream encoder(&sc);
  // length >= 63 → 6-byte header (2 + 4 extended).
  const uint32_t kLargeLength = 1 << 20;
  WriteTagHeader(&encoder, TagCode::Composition, kLargeLength);
  auto buf = Finalize(&encoder);
  EXPECT_EQ(buf.bytes->length(), 6u);

  auto header = ReadTagHeader(buf.reader.get());
  EXPECT_EQ(header.code, TagCode::Composition);
  EXPECT_EQ(header.length, kLargeLength);
}

TEST(TagHeader, EndTagIsTwoZeroBytes) {
  ::pag::StreamContext sc;
  ::pag::EncodeStream encoder(&sc);
  WriteEndTag(&encoder);
  auto buf = Finalize(&encoder);
  ASSERT_EQ(buf.bytes->length(), 2u);
  EXPECT_EQ(buf.bytes->data()[0], 0x00u);
  EXPECT_EQ(buf.bytes->data()[1], 0x00u);
  auto header = ReadTagHeader(buf.reader.get());
  EXPECT_EQ(header.code, TagCode::End);
  EXPECT_EQ(header.length, 0u);
}

TEST(TagHeader, BoundaryAt63Bytes) {
  // length == 63 is the boundary that forces the long form.
  ::pag::StreamContext sc;
  ::pag::EncodeStream encoder(&sc);
  WriteTagHeader(&encoder, TagCode::LayerBlock, /*length=*/63);
  auto buf = Finalize(&encoder);
  EXPECT_EQ(buf.bytes->length(), 6u);  // forced to long form

  auto header = ReadTagHeader(buf.reader.get());
  EXPECT_EQ(header.code, TagCode::LayerBlock);
  EXPECT_EQ(header.length, 63u);
}

TEST(TagHeader, WriteTagCopiesBody) {
  ::pag::StreamContext sc;
  ::pag::EncodeStream body(&sc);
  body.writeUint8(0xAA);
  body.writeUint8(0xBB);
  body.writeUint8(0xCC);

  ::pag::EncodeStream encoder(&sc);
  WriteTag(&encoder, TagCode::ShapePayload, &body);
  auto buf = Finalize(&encoder);
  // 2-byte short header + 3 body bytes.
  ASSERT_EQ(buf.bytes->length(), 5u);

  auto header = ReadTagHeader(buf.reader.get());
  EXPECT_EQ(header.code, TagCode::ShapePayload);
  EXPECT_EQ(header.length, 3u);
  EXPECT_EQ(buf.reader->readUint8(), 0xAAu);
  EXPECT_EQ(buf.reader->readUint8(), 0xBBu);
  EXPECT_EQ(buf.reader->readUint8(), 0xCCu);
}

}  // namespace pagx::pag
