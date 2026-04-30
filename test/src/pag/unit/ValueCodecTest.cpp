// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 1 unit tests for pagx::pag::ValueCodec — roundtrip for each primitive
// type + Color / Matrix variants + length-prefixed safe wrappers.
#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>
#include "codec/utils/DecodeStream.h"
#include "codec/utils/EncodeStream.h"
#include "codec/utils/StreamContext.h"
#include "gtest/gtest.h"
#include "pag/types.h"
#include "pagx/pag/ValueCodec.h"

namespace pagx::pag {

// Helper: finalize an EncodeStream to a ByteData, then build a DecodeStream on
// top so test bodies can assert read-after-write byte equality.
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

// ---------- Primitive scalars ----------

TEST(ValueCodec, BoolRoundTrip) {
  ::pag::StreamContext sc;
  ::pag::EncodeStream encoder(&sc);
  WriteBool(&encoder, true);
  WriteBool(&encoder, false);
  auto buf = Finalize(&encoder);
  EXPECT_TRUE(ReadBool(buf.reader.get()));
  EXPECT_FALSE(ReadBool(buf.reader.get()));
  EXPECT_EQ(buf.bytes->length(), 2u);
}

TEST(ValueCodec, IntRoundTrip) {
  ::pag::StreamContext sc;
  ::pag::EncodeStream encoder(&sc);
  WriteUint8(&encoder, 0xABu);
  WriteUint16(&encoder, 0xCAFEu);
  WriteUint32(&encoder, 0xDEADBEEFu);
  WriteInt32(&encoder, -12345);
  auto buf = Finalize(&encoder);
  EXPECT_EQ(ReadUint8(buf.reader.get()), 0xABu);
  EXPECT_EQ(ReadUint16(buf.reader.get()), 0xCAFEu);
  EXPECT_EQ(ReadUint32(buf.reader.get()), 0xDEADBEEFu);
  EXPECT_EQ(ReadInt32(buf.reader.get()), -12345);
}

TEST(ValueCodec, FloatRoundTrip) {
  ::pag::StreamContext sc;
  ::pag::EncodeStream encoder(&sc);
  WriteFloat(&encoder, 3.14159f);
  WriteFloat(&encoder, -0.0f);
  WriteFloat(&encoder, 1e10f);
  auto buf = Finalize(&encoder);
  EXPECT_FLOAT_EQ(ReadFloat(buf.reader.get()), 3.14159f);
  EXPECT_FLOAT_EQ(ReadFloat(buf.reader.get()), -0.0f);
  EXPECT_FLOAT_EQ(ReadFloat(buf.reader.get()), 1e10f);
}

// ---------- Color ----------

TEST(ValueCodec, ColorRoundTripU8) {
  ::pag::StreamContext sc;
  ::pag::EncodeStream encoder(&sc);
  tgfx::Color in{1.0f, 0.5f, 0.25f, 0.75f};
  WriteColor(&encoder, in);
  auto buf = Finalize(&encoder);
  // Exactly 4 bytes: R, G, B, A (u8 quantized).
  EXPECT_EQ(buf.bytes->length(), 4u);
  tgfx::Color out = ReadColor(buf.reader.get());
  // Quantization tolerance = 1/255.
  EXPECT_NEAR(out.red, 1.0f, 1.0f / 255.0f);
  EXPECT_NEAR(out.green, 0.5f, 1.0f / 255.0f);
  EXPECT_NEAR(out.blue, 0.25f, 1.0f / 255.0f);
  EXPECT_NEAR(out.alpha, 0.75f, 1.0f / 255.0f);
}

TEST(ValueCodec, ColorClampsNanAndOutOfRange) {
  // NaN → 0; > 1 → 255; < 0 → 0. FloatToU8 is the canonical clamp path.
  EXPECT_EQ(FloatToU8(std::numeric_limits<float>::quiet_NaN()), 0u);
  EXPECT_EQ(FloatToU8(2.0f), 255u);
  EXPECT_EQ(FloatToU8(-0.5f), 0u);
  EXPECT_EQ(FloatToU8(1.0f), 255u);
  EXPECT_EQ(FloatToU8(0.0f), 0u);
}

// ---------- Point / Rect ----------

TEST(ValueCodec, PointRect) {
  ::pag::StreamContext sc;
  ::pag::EncodeStream encoder(&sc);
  tgfx::Point p{3.5f, -1.25f};
  tgfx::Rect r{1.0f, 2.0f, 3.0f, 4.0f};
  WritePoint(&encoder, p);
  WriteRect(&encoder, r);
  auto buf = Finalize(&encoder);
  auto pOut = ReadPoint(buf.reader.get());
  auto rOut = ReadRect(buf.reader.get());
  EXPECT_FLOAT_EQ(pOut.x, 3.5f);
  EXPECT_FLOAT_EQ(pOut.y, -1.25f);
  EXPECT_FLOAT_EQ(rOut.left, 1.0f);
  EXPECT_FLOAT_EQ(rOut.top, 2.0f);
  EXPECT_FLOAT_EQ(rOut.right, 3.0f);
  EXPECT_FLOAT_EQ(rOut.bottom, 4.0f);
}

// ---------- Matrix variants ----------

TEST(ValueCodec, MatrixIdentityOneByte) {
  ::pag::StreamContext sc;
  ::pag::EncodeStream encoder(&sc);
  WriteMatrix(&encoder, tgfx::Matrix::I());
  auto buf = Finalize(&encoder);
  EXPECT_EQ(buf.bytes->length(), 1u);
  EXPECT_EQ(buf.bytes->data()[0], 0x01u);  // isIdentity bit set
  auto m = ReadMatrix(buf.reader.get());
  EXPECT_EQ(m, tgfx::Matrix::I());
}

TEST(ValueCodec, MatrixTranslateOnly) {
  ::pag::StreamContext sc;
  ::pag::EncodeStream encoder(&sc);
  tgfx::Matrix m = tgfx::Matrix::I();
  m.setTranslateX(10.0f);
  m.setTranslateY(-5.0f);
  WriteMatrix(&encoder, m);
  auto buf = Finalize(&encoder);
  EXPECT_EQ(buf.bytes->length(), 9u);      // 1 header + 2 floats
  EXPECT_EQ(buf.bytes->data()[0], 0x02u);  // hasTranslateOnly bit set
  auto out = ReadMatrix(buf.reader.get());
  EXPECT_FLOAT_EQ(out.getTranslateX(), 10.0f);
  EXPECT_FLOAT_EQ(out.getTranslateY(), -5.0f);
  EXPECT_FLOAT_EQ(out.getScaleX(), 1.0f);
  EXPECT_FLOAT_EQ(out.getScaleY(), 1.0f);
}

TEST(ValueCodec, MatrixFull) {
  ::pag::StreamContext sc;
  ::pag::EncodeStream encoder(&sc);
  tgfx::Matrix m{};
  m.setAll(2.0f, 0.3f, 10.0f, 0.1f, 1.5f, -4.0f);
  WriteMatrix(&encoder, m);
  auto buf = Finalize(&encoder);
  EXPECT_EQ(buf.bytes->length(), 25u);  // 1 header + 6 floats
  EXPECT_EQ(buf.bytes->data()[0], 0x00u);
  auto out = ReadMatrix(buf.reader.get());
  EXPECT_FLOAT_EQ(out.getScaleX(), 2.0f);
  EXPECT_FLOAT_EQ(out.getSkewX(), 0.3f);
  EXPECT_FLOAT_EQ(out.getTranslateX(), 10.0f);
  EXPECT_FLOAT_EQ(out.getSkewY(), 0.1f);
  EXPECT_FLOAT_EQ(out.getScaleY(), 1.5f);
  EXPECT_FLOAT_EQ(out.getTranslateY(), -4.0f);
}

TEST(ValueCodec, Matrix3DIdentityOneByte) {
  ::pag::StreamContext sc;
  ::pag::EncodeStream encoder(&sc);
  WriteMatrix3D(&encoder, tgfx::Matrix3D::I());
  auto buf = Finalize(&encoder);
  EXPECT_EQ(buf.bytes->length(), 1u);
  EXPECT_EQ(buf.bytes->data()[0], 0x01u);
  auto m = ReadMatrix3D(buf.reader.get());
  EXPECT_EQ(m, tgfx::Matrix3D::I());
}

TEST(ValueCodec, Matrix3DFull) {
  ::pag::StreamContext sc;
  ::pag::EncodeStream encoder(&sc);
  tgfx::Matrix3D m{};
  m.setAll(1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f,
           15.0f, 16.0f);
  WriteMatrix3D(&encoder, m);
  auto buf = Finalize(&encoder);
  EXPECT_EQ(buf.bytes->length(), 65u);  // 1 header + 16 floats
  auto out = ReadMatrix3D(buf.reader.get());
  EXPECT_EQ(out, m);
}

// ---------- Length-prefixed safe wrappers ----------

TEST(ValueCodec, ReadUtf8StringOk) {
  ::pag::StreamContext sc;
  ::pag::EncodeStream encoder(&sc);
  const std::string hello = u8"héllo, 世界";  // mixed ASCII + multibyte
  encoder.writeEncodedUint32(static_cast<uint32_t>(hello.size()));
  // v1 writeBytes takes a non-const uint8_t* — safe here because the encoder
  // only reads the input buffer; const_cast is the minimal expedient.
  encoder.writeBytes(reinterpret_cast<uint8_t*>(const_cast<char*>(hello.data())),
                     static_cast<uint32_t>(hello.size()));
  auto buf = Finalize(&encoder);
  DiagnosticCollectorGuard guard;  // no collector — nulls allowed
  std::string out = ReadUtf8String(buf.reader.get(), &guard, 1024);
  EXPECT_EQ(out, hello);
}

TEST(ValueCodec, ReadUtf8StringRejectsOverlongLength) {
  // length prefix larger than maxBytes → warn + return empty without resize.
  ::pag::StreamContext sc;
  ::pag::EncodeStream encoder(&sc);
  encoder.writeEncodedUint32(10000);  // way beyond maxBytes=4
  for (int i = 0; i < 10; ++i) {
    encoder.writeUint8(static_cast<uint8_t>('A'));
  }
  auto buf = Finalize(&encoder);

  struct Spy : DiagnosticCollector {
    using DiagnosticCollector::pushWarning;
  } spy;
  DiagnosticCollectorGuard guard;
  guard.collector = &spy;

  std::string out = ReadUtf8String(buf.reader.get(), &guard, /*maxBytes=*/4);
  EXPECT_TRUE(out.empty());
  ASSERT_EQ(spy.warnings.size(), 1u);
  EXPECT_EQ(spy.warnings[0].code, DiagnosticCode::StringInvalidUtf8);
}

TEST(ValueCodec, ReadUtf8StringRejectsInvalidBytes) {
  // Valid varU32 length, but the bytes contain a lone 0xFF (illegal in UTF-8).
  ::pag::StreamContext sc;
  ::pag::EncodeStream encoder(&sc);
  encoder.writeEncodedUint32(3);
  encoder.writeUint8(0xFFu);
  encoder.writeUint8(0xFEu);
  encoder.writeUint8(0x80u);
  auto buf = Finalize(&encoder);

  struct Spy : DiagnosticCollector {
    using DiagnosticCollector::pushWarning;
  } spy;
  DiagnosticCollectorGuard guard;
  guard.collector = &spy;

  std::string out = ReadUtf8String(buf.reader.get(), &guard, 1024);
  EXPECT_TRUE(out.empty());
  ASSERT_EQ(spy.warnings.size(), 1u);
  EXPECT_EQ(spy.warnings[0].code, DiagnosticCode::StringInvalidUtf8);
}

TEST(ValueCodec, ReadLengthPrefixedBytesOk) {
  ::pag::StreamContext sc;
  ::pag::EncodeStream encoder(&sc);
  const std::array<uint8_t, 5> payload{0xDE, 0xAD, 0xBE, 0xEF, 0x42};
  encoder.writeEncodedUint32(static_cast<uint32_t>(payload.size()));
  encoder.writeBytes(const_cast<uint8_t*>(payload.data()), static_cast<uint32_t>(payload.size()));
  auto buf = Finalize(&encoder);
  DiagnosticCollectorGuard guard;
  auto data = ReadLengthPrefixedBytes(buf.reader.get(), &guard, limits::MAX_IMAGE_BYTES,
                                      ErrorCode::ImageResourceSizeExceeded);
  ASSERT_NE(data, nullptr);
  EXPECT_EQ(data->size(), payload.size());
  EXPECT_EQ(std::memcmp(data->data(), payload.data(), payload.size()), 0);
}

TEST(ValueCodec, ReadLengthPrefixedBytesRejectsTooLarge) {
  ::pag::StreamContext sc;
  ::pag::EncodeStream encoder(&sc);
  encoder.writeEncodedUint32(static_cast<uint32_t>(limits::MAX_IMAGE_BYTES + 1));
  encoder.writeUint8(0x00u);
  auto buf = Finalize(&encoder);

  struct Spy : DiagnosticCollector {
    using DiagnosticCollector::pushWarning;
  } spy;
  DiagnosticCollectorGuard guard;
  guard.collector = &spy;

  auto data = ReadLengthPrefixedBytes(buf.reader.get(), &guard, limits::MAX_IMAGE_BYTES,
                                      ErrorCode::ImageResourceSizeExceeded);
  EXPECT_EQ(data, nullptr);
  ASSERT_EQ(spy.warnings.size(), 1u);
  EXPECT_EQ(spy.warnings[0].code, DiagnosticCode::ImageResourceSizeExceeded);
}

TEST(ValueCodec, ReadLengthPrefixedBytesEmpty) {
  ::pag::StreamContext sc;
  ::pag::EncodeStream encoder(&sc);
  encoder.writeEncodedUint32(0);
  auto buf = Finalize(&encoder);
  DiagnosticCollectorGuard guard;
  auto data = ReadLengthPrefixedBytes(buf.reader.get(), &guard, limits::MAX_IMAGE_BYTES,
                                      ErrorCode::ImageResourceSizeExceeded);
  ASSERT_NE(data, nullptr);
  EXPECT_EQ(data->size(), 0u);
}

}  // namespace pagx::pag
