// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 1 unit tests for Property<T> + propHeader bit layout.
#include <cstdint>
#include <memory>
#include "codec/utils/DecodeStream.h"
#include "codec/utils/EncodeStream.h"
#include "codec/utils/StreamContext.h"
#include "gtest/gtest.h"
#include "pag/types.h"
#include "pagx/pag/PropertyEncoding.h"

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

// ---------- isDefault short-circuit (1 byte total) ----------

TEST(PropertyEncoding, IsDefaultSkipValue) {
  ::pag::StreamContext sc;
  ::pag::EncodeStream encoder(&sc);
  // prop.value == defaultValue(1.0f) → isDefault=1 → 1 byte on the wire.
  WriteProperty<float>(&encoder, MakeProp(1.0f), 1.0f);
  auto buf = Finalize(&encoder);
  ASSERT_EQ(buf.bytes->length(), 1u);
  EXPECT_EQ(buf.bytes->data()[0],
            prop_header::kIsDefaultBit);  // bit 4 set, bits 0-3 = 0 (Constant)
}

TEST(PropertyEncoding, IsDefaultRoundTrip) {
  ::pag::StreamContext sc;
  ::pag::EncodeStream encoder(&sc);
  WriteProperty<float>(&encoder, MakeProp(1.0f), 1.0f);
  auto buf = Finalize(&encoder);
  // Reader returns MakeProp(defaultValue) without consuming value bytes.
  auto prop =
      ReadProperty<float>(buf.reader.get(), 1.0f, static_cast<uint32_t>(buf.bytes->length()));
  EXPECT_EQ(prop.encoding, PropertyEncoding::Constant);
  EXPECT_FLOAT_EQ(prop.value, 1.0f);
  EXPECT_EQ(buf.reader->position(), 1u);  // only the header was consumed
}

// ---------- non-default payload (1 + sizeof(T)) ----------

TEST(PropertyEncoding, NonDefaultWritesFullPayload) {
  ::pag::StreamContext sc;
  ::pag::EncodeStream encoder(&sc);
  WriteProperty<float>(&encoder, MakeProp(0.5f), 1.0f);
  auto buf = Finalize(&encoder);
  // 1 header + 4 float bytes.
  ASSERT_EQ(buf.bytes->length(), 5u);
  EXPECT_EQ(buf.bytes->data()[0], uint8_t{0x00});  // encoding=0, isDefault=0
  auto prop =
      ReadProperty<float>(buf.reader.get(), 1.0f, static_cast<uint32_t>(buf.bytes->length()));
  EXPECT_FLOAT_EQ(prop.value, 0.5f);
}

TEST(PropertyEncoding, BoolRoundTrip) {
  ::pag::StreamContext sc;
  ::pag::EncodeStream encoder(&sc);
  WriteProperty<bool>(&encoder, MakeProp(false), true);
  WriteProperty<bool>(&encoder, MakeProp(true), true);
  auto buf = Finalize(&encoder);

  const uint32_t end = static_cast<uint32_t>(buf.bytes->length());
  auto a = ReadProperty<bool>(buf.reader.get(), true, end);
  auto b = ReadProperty<bool>(buf.reader.get(), true, end);
  EXPECT_FALSE(a.value);  // explicit non-default
  EXPECT_TRUE(b.value);   // default-short-circuit
}

// ---------- reserved bits 5-7 must be ignored on read ----------

TEST(PropertyEncoding, ReservedBitsIgnored) {
  // Craft a byte-stream where the Writer accidentally (or maliciously) sets
  // reserved bits. Reader must still decode as Constant + isDefault=0.
  ::pag::StreamContext sc;
  ::pag::EncodeStream encoder(&sc);
  encoder.writeUint8(static_cast<uint8_t>(0xE0u));  // bits 5-7 = 0b111
  encoder.writeFloat(0.25f);
  auto buf = Finalize(&encoder);
  auto prop =
      ReadProperty<float>(buf.reader.get(), 1.0f, static_cast<uint32_t>(buf.bytes->length()));
  EXPECT_FLOAT_EQ(prop.value, 0.25f);
}

// ---------- Unknown encoding → seek to enclosing tag end ----------

TEST(PropertyEncoding, UnknownEncodingSeeksToEnd) {
  ::pag::StreamContext sc;
  ::pag::EncodeStream encoder(&sc);
  encoder.writeUint8(static_cast<uint8_t>(0x0Fu));  // encoding=15 (unknown)
  encoder.writeUint8(0x00u);                        // filler body bytes
  encoder.writeUint8(0x00u);
  auto buf = Finalize(&encoder);

  const uint32_t tagEnd = static_cast<uint32_t>(buf.bytes->length());
  auto prop = ReadProperty<float>(buf.reader.get(), 42.0f, tagEnd);
  EXPECT_FLOAT_EQ(prop.value, 42.0f);         // default returned
  EXPECT_EQ(buf.reader->position(), tagEnd);  // seeked to tag end
}

// ---------- tgfx::Color default short-circuit ----------

TEST(PropertyEncoding, ColorIsDefaultSkipValue) {
  ::pag::StreamContext sc;
  ::pag::EncodeStream encoder(&sc);
  tgfx::Color transparent{0.0f, 0.0f, 0.0f, 0.0f};
  WriteProperty<tgfx::Color>(&encoder, MakeProp(transparent), transparent);
  auto buf = Finalize(&encoder);
  EXPECT_EQ(buf.bytes->length(), 1u);
  EXPECT_EQ(buf.bytes->data()[0], prop_header::kIsDefaultBit);
}

TEST(PropertyEncoding, MatrixDefaultRoundTrip) {
  ::pag::StreamContext sc;
  ::pag::EncodeStream encoder(&sc);
  WriteProperty<tgfx::Matrix>(&encoder, MakeProp(tgfx::Matrix::I()), tgfx::Matrix::I());
  auto buf = Finalize(&encoder);
  EXPECT_EQ(buf.bytes->length(), 1u);
  EXPECT_EQ(buf.bytes->data()[0], prop_header::kIsDefaultBit);
  auto prop = ReadProperty<tgfx::Matrix>(buf.reader.get(), tgfx::Matrix::I(),
                                         static_cast<uint32_t>(buf.bytes->length()));
  EXPECT_EQ(prop.value, tgfx::Matrix::I());
}

}  // namespace pagx::pag
