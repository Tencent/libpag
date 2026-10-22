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
//  or required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "AS IS" basis, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include <cstring>
#include "base/PAGTest.h"
#include "codec/utils/StreamContext.h"
#include "pagx/PAGImporter.h"
#include "pagx/PAGXDocument.h"
#include "pagx/pag/PAGAttributeIO.h"
#include "pagx/pag/PAGDataTypes.h"
#include "pagx/pag/PAGFileHeader.h"
#include "pagx/pag/PAGKeyframeIO.h"
#include "pagx/pag/PAGPathIO.h"
#include "pagx/pag/PAGTagHeader.h"

namespace pagx {
using pag::DecodeStream;
using pag::EncodeStream;
using pag::StreamContext;

static std::shared_ptr<pag::ByteData> WriteVaruint(uint32_t value) {
  StreamContext context = {};
  EncodeStream stream(&context);
  stream.writeEncodedUint32(value);
  return stream.release();
}

static bool BytesEqual(const pag::ByteData* data, const std::vector<uint8_t>& expected) {
  if (data->length() != expected.size()) {
    return false;
  }
  return std::memcmp(data->data(), expected.data(), expected.size()) == 0;
}

PAGX_TEST(PAGBinaryFormatTest, Varuint) {
  // spec §3.1: 300 -> AC 02.
  EXPECT_TRUE(BytesEqual(WriteVaruint(300).get(), {0xAC, 0x02}));
  // spec §3.2: -5 as a sign-in-LSB varint is |5|<<1|1 = 11 -> 0B.
  StreamContext context = {};
  EncodeStream stream(&context);
  stream.writeEncodedInt32(-5);
  EXPECT_TRUE(BytesEqual(stream.release().get(), {0x0B}));
}

PAGX_TEST(PAGBinaryFormatTest, FileHeaderAndDetect) {
  StreamContext context = {};
  EncodeStream stream(&context);
  WritePAGFileHeader(&stream, 0x12345678);
  auto bytes = stream.release();
  EXPECT_TRUE(BytesEqual(bytes.get(),
                         {'P', 'A', 'G', 0x04, 0x78, 0x56, 0x34, 0x12, 'U', 0, 0, 0, 0, 0, 0, 0}));

  PAGFileHeader header = {};
  EXPECT_TRUE(header.parse(bytes->data(), bytes->length()));
  EXPECT_EQ(header.version, 4);
  EXPECT_EQ(header.bodyLength, 0x12345678u);

  EXPECT_EQ(PAGImporter::DetectFormat(bytes->data(), 16), PAGFormat::PAGv4);
  uint8_t legacy[16] = {'P', 'A', 'G', 1};
  EXPECT_EQ(PAGImporter::DetectFormat(legacy, sizeof(legacy)), PAGFormat::PAGLegacy);
  uint8_t tooNew[16] = {'P', 'A', 'G', 5};
  EXPECT_EQ(PAGImporter::DetectFormat(tooNew, sizeof(tooNew)), PAGFormat::Unknown);
  uint8_t pagx[4] = {'<', 'p', 'a', 'g'};
  EXPECT_EQ(PAGImporter::DetectFormat(pagx, sizeof(pagx)), PAGFormat::PAGX);
  uint8_t bomPAGX[8] = {0xEF, 0xBB, 0xBF, '\n', ' ', '<', 'p', 'a'};
  EXPECT_EQ(PAGImporter::DetectFormat(bomPAGX, sizeof(bomPAGX)), PAGFormat::PAGX);
  uint8_t garbage[4] = {'P', 'K', 0x03, 0x04};
  EXPECT_EQ(PAGImporter::DetectFormat(garbage, sizeof(garbage)), PAGFormat::Unknown);
  EXPECT_EQ(PAGImporter::DetectFormat(nullptr, 0), PAGFormat::Unknown);
  uint8_t truncated[3] = {'P', 'A', 'G'};
  EXPECT_EQ(PAGImporter::DetectFormat(truncated, sizeof(truncated)), PAGFormat::Unknown);
}

PAGX_TEST(PAGBinaryFormatTest, TagHeader) {
  // spec §10.1: code 416 with a 21-byte content -> 15 68.
  StreamContext context = {};
  EncodeStream stream(&context);
  WritePAGTagHeader(&stream, TagCode::Rectangle, 21);
  EXPECT_TRUE(BytesEqual(stream.release().get(), {0x15, 0x68}));

  // Lengths 63 and above extend with a u32 (spec §2).
  EncodeStream extended(&context);
  WritePAGTagHeader(&extended, TagCode::PathData, 100);
  EXPECT_TRUE(BytesEqual(extended.release().get(), {0x7F, 0x48, 100, 0, 0, 0}));

  // Reader round-trip over the boundary values.
  StreamContext readContext = {};
  EncodeStream roundTrip(&readContext);
  WritePAGTagHeader(&roundTrip, TagCode::SubDocument, 62);
  WritePAGTagHeader(&roundTrip, TagCode::Channel, 63);
  WritePAGTagHeader(&roundTrip, TagCode::ImageTable, 0x10000);
  auto bytes = roundTrip.release();
  DecodeStream reader(&readContext, bytes->data(), static_cast<uint32_t>(bytes->length()));
  auto first = ReadPAGTagHeader(&reader);
  EXPECT_EQ(first.code, TagCode::SubDocument);
  EXPECT_EQ(first.length, 62u);
  auto second = ReadPAGTagHeader(&reader);
  EXPECT_EQ(second.code, TagCode::Channel);
  EXPECT_EQ(second.length, 63u);
  auto third = ReadPAGTagHeader(&reader);
  EXPECT_EQ(third.code, TagCode::ImageTable);
  EXPECT_EQ(third.length, 0x10000u);
}

PAGX_TEST(PAGBinaryFormatTest, StringTable) {
  // spec §10.2: "logo" + "标题文字".
  StreamContext context = {};
  EncodeStream stream(&context);
  std::vector<std::string> strings = {"logo", "标题文字"};
  WritePAGStringTable(&stream, strings);
  auto bytes = stream.release();
  std::vector<uint8_t> expected = {0x02, 0x04, 'l',  'o',  'g',  'o',  0x0C, 0xE6, 0xA0, 0x87,
                                   0xE9, 0xA2, 0x98, 0xE6, 0x96, 0x87, 0xE5, 0xAD, 0x97};
  EXPECT_TRUE(BytesEqual(bytes.get(), expected));

  StreamContext readContext = {};
  DecodeStream reader(&readContext, bytes->data(), static_cast<uint32_t>(bytes->length()));
  std::vector<std::string> decoded = {};
  EXPECT_TRUE(ReadPAGStringTable(&reader, &decoded));
  EXPECT_EQ(decoded, strings);
}

PAGX_TEST(PAGBinaryFormatTest, AttributeBlock) {
  // spec §10.1: Rectangle position(50,40) size(100,80) roundness 12.
  StreamContext context = {};
  PAGAttributeBlockWriter writer(&context);
  writer.writeFlag(true);   // position.x
  writer.writeFlag(true);   // position.y
  writer.writeFlag(true);   // size.width
  writer.writeFlag(true);   // size.height
  writer.writeFlag(true);   // roundness
  writer.writeFlag(false);  // reversed
  auto values = writer.valueStream();
  values->writeFloat(50.0f);
  values->writeFloat(40.0f);
  values->writeFloat(100.0f);
  values->writeFloat(80.0f);
  values->writeFloat(12.0f);
  auto bytes = writer.release();
  std::vector<uint8_t> expected = {0x1F, 0x00, 0x00, 0x48, 0x42, 0x00, 0x00, 0x20, 0x42, 0x00, 0x00,
                                   0xC8, 0x42, 0x00, 0x00, 0xA0, 0x42, 0x00, 0x00, 0x40, 0x41};
  EXPECT_TRUE(BytesEqual(bytes.get(), expected));

  StreamContext readContext = {};
  DecodeStream reader(&readContext, bytes->data(), static_cast<uint32_t>(bytes->length()));
  PAGAttributeBlockReader block(&reader, 6);
  EXPECT_TRUE(block.flag(0));
  EXPECT_TRUE(block.flag(4));
  EXPECT_FALSE(block.flag(5));
  EXPECT_EQ(reader.readFloat(), 50.0f);
  EXPECT_EQ(reader.readFloat(), 40.0f);
  EXPECT_EQ(reader.readFloat(), 100.0f);
  EXPECT_EQ(reader.readFloat(), 80.0f);
  EXPECT_EQ(reader.readFloat(), 12.0f);
}

PAGX_TEST(PAGBinaryFormatTest, Colors) {
  StreamContext context = {};
  EncodeStream stream(&context);
  Color srgb = {};
  srgb.red = 1.0f;
  WritePAGColor(&stream, srgb);
  Color wide = {};
  wide.red = 1.0f;
  wide.blue = 0.5f;
  wide.colorSpace = ColorSpace::DisplayP3;
  WritePAGColor(&stream, wide);
  auto bytes = stream.release();
  std::vector<uint8_t> expected = {0x00, 0xFF, 0x00, 0x00, 0xFF, 0x01, 0x01, 0x00,
                                   0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00,
                                   0x00, 0x00, 0x3F, 0x00, 0x00, 0x80, 0x3F};
  EXPECT_TRUE(BytesEqual(bytes.get(), expected));

  StreamContext readContext = {};
  DecodeStream reader(&readContext, bytes->data(), static_cast<uint32_t>(bytes->length()));
  auto decoded = ReadPAGColor(&reader);
  EXPECT_EQ(decoded.red, 1.0f);
  EXPECT_EQ(decoded.colorSpace, ColorSpace::SRGB);
  auto decodedWide = ReadPAGColor(&reader);
  EXPECT_EQ(decodedWide.colorSpace, ColorSpace::DisplayP3);
  EXPECT_EQ(decodedWide.blue, 0.5f);
}

PAGX_TEST(PAGBinaryFormatTest, PathQuadStepExponent) {
  // spec §10.5: Move(0,0), Quad control(3,1.5) end(6,0), Close, display scale 3.
  EXPECT_EQ(ComputeStepExponent(0.5f), 0);
  EXPECT_EQ(ComputeStepExponent(1.0f), 0);
  EXPECT_EQ(ComputeStepExponent(3.0f), 2);

  auto doc = PAGXDocument::Make(100, 100);
  auto path = doc->makeNode<PathData>();
  path->moveTo(0.0f, 0.0f);
  path->quadTo(3.0f, 1.5f, 6.0f, 0.0f);
  path->close();
  StreamContext context = {};
  EncodeStream stream(&context);
  WritePAGPath(&stream, path, ComputeStepExponent(3.0f));
  auto bytes = stream.release();
  // numVerbs=3, stepExponent=2, records Move/Quad/Close = 51 00.
  // Byte-exact expectation (spec §10.5, regenerated from this encoder): numVerbs 03,
  // stepExponent 02, records 51 00, packed prefix 09, then the six 10-bit coordinates.
  EXPECT_TRUE(BytesEqual(
      bytes.get(), {0x03, 0x02, 0x51, 0x00, 0x09, 0x00, 0x00, 0xE0, 0xC1, 0x03, 0x3C, 0x00, 0x00}));

  auto decoded = doc->makeNode<PathData>();
  StreamContext readContext = {};
  DecodeStream reader(&readContext, bytes->data(), static_cast<uint32_t>(bytes->length()));
  EXPECT_TRUE(ReadPAGPath(&reader, decoded));
  ASSERT_EQ(decoded->countPoints(), path->countPoints());
  const auto& expectedPoints = path->points();
  const auto& decodedPoints = decoded->points();
  for (size_t i = 0; i < expectedPoints.size(); i++) {
    EXPECT_NEAR(decodedPoints[i].x, expectedPoints[i].x, 0.05f);
    EXPECT_NEAR(decodedPoints[i].y, expectedPoints[i].y, 0.05f);
  }
  EXPECT_EQ(decoded->verbs(), path->verbs());
}

PAGX_TEST(PAGBinaryFormatTest, KeyframeBlock) {
  // spec §10.4 value block: 2 float keys, linear, times 0 and 30, values 1 and 0.5.
  StreamContext context = {};
  PAGKeyframeBlock block = {};
  block.times = {0, 30};
  block.interpolations = {KeyframeInterpolationType::Linear, KeyframeInterpolationType::Linear};
  block.bezierOut = {{}, {}};
  block.bezierIn = {{}, {}};
  block.floatValues = {1.0f, 0.5f};
  EncodeStream stream(&context);
  WritePAGKeyframeBlock(&stream, ChannelValueType::Float, block);
  auto bytes = stream.release();
  std::vector<uint8_t> expected = {0x02, 0x05, 0x00, 0x1E, 0x00, 0x00, 0x80,
                                   0x3F, 0x00, 0x00, 0x00, 0x3F, 0x00};
  EXPECT_TRUE(BytesEqual(bytes.get(), expected));

  StreamContext readContext = {};
  DecodeStream reader(&readContext, bytes->data(), static_cast<uint32_t>(bytes->length()));
  PAGKeyframeBlock decoded = {};
  EXPECT_TRUE(ReadPAGKeyframeBlock(&reader, ChannelValueType::Float, &decoded));
  EXPECT_EQ(decoded.times, block.times);
  EXPECT_EQ(decoded.interpolations, block.interpolations);
  EXPECT_EQ(decoded.floatValues, block.floatValues);
}

PAGX_TEST(PAGBinaryFormatTest, KeyframeBlockAllTypes) {
  auto doc = PAGXDocument::Make(100, 100);
  auto path = doc->makeNode<PathData>();
  path->moveTo(0.0f, 0.0f);
  path->lineTo(10.0f, 10.0f);

  PAGKeyframeBlock block = {};
  block.times = {0, 30};
  block.interpolations = {KeyframeInterpolationType::Bezier, KeyframeInterpolationType::Hold};
  block.bezierOut = {{0.3f, 0.0f}, {}};
  block.bezierIn = {{0.2f, 1.0f}, {}};
  block.boolValues = {1, 0};
  block.intValues = {7, -9};
  block.stringIndices = {3, 4};
  block.colorValues = {{}, {}};
  block.matrixValues = {Matrix::Identity(), Matrix::Identity()};
  block.imageIndices = {1, 0};
  block.pathValues = {path, path};

  const ChannelValueType types[] = {ChannelValueType::Bool,   ChannelValueType::Int,
                                    ChannelValueType::String, ChannelValueType::Color,
                                    ChannelValueType::Matrix, ChannelValueType::ImageRef,
                                    ChannelValueType::Path};
  for (auto type : types) {
    StreamContext context = {};
    EncodeStream stream(&context);
    WritePAGKeyframeBlock(&stream, type, block);
    auto bytes = stream.release();
    StreamContext readContext = {};
    DecodeStream reader(&readContext, bytes->data(), static_cast<uint32_t>(bytes->length()));
    PAGKeyframeBlock decoded = {};
    if (type == ChannelValueType::Path) {
      // PathData nodes are document-owned; pre-create one node per keyframe.
      decoded.pathValues.resize(block.times.size());
      for (size_t i = 0; i < decoded.pathValues.size(); i++) {
        decoded.pathValues[i] = doc->makeNode<PathData>();
      }
    }
    EXPECT_TRUE(ReadPAGKeyframeBlock(&reader, type, &decoded)) << "type " << static_cast<int>(type);
    EXPECT_EQ(decoded.times, block.times);
    EXPECT_EQ(decoded.interpolations, block.interpolations);
    if (decoded.bezierOut.size() >= 1) {
      EXPECT_NEAR(decoded.bezierOut[0].x, 0.3f, 0.005f);
      EXPECT_NEAR(decoded.bezierIn[0].y, 1.0f, 0.005f);
    }
    switch (type) {
      case ChannelValueType::Bool:
        EXPECT_EQ(decoded.boolValues, block.boolValues);
        break;
      case ChannelValueType::Int:
        EXPECT_EQ(decoded.intValues, block.intValues);
        break;
      case ChannelValueType::String:
        EXPECT_EQ(decoded.stringIndices, block.stringIndices);
        break;
      case ChannelValueType::Color:
        EXPECT_EQ(decoded.colorValues.size(), 2u);
        break;
      case ChannelValueType::Matrix:
        EXPECT_EQ(decoded.matrixValues.size(), 2u);
        break;
      case ChannelValueType::ImageRef:
        EXPECT_EQ(decoded.imageIndices, block.imageIndices);
        break;
      case ChannelValueType::Path:
        ASSERT_EQ(decoded.pathValues.size(), 2u);
        EXPECT_EQ(decoded.pathValues[0]->countPoints(), path->countPoints());
        break;
      default:
        break;
    }
  }
}

PAGX_TEST(PAGBinaryFormatTest, KeyframeBlockRejectsInvalidInput) {
  StreamContext context = {};
  PAGKeyframeBlock block = {};
  block.times = {10, 5};
  block.interpolations = {KeyframeInterpolationType::Linear, KeyframeInterpolationType::Linear};
  block.floatValues = {1.0f, 0.5f};
  EncodeStream stream(&context);
  WritePAGKeyframeBlock(&stream, ChannelValueType::Float, block);
  auto bytes = stream.release();
  StreamContext readContext = {};
  DecodeStream reader(&readContext, bytes->data(), static_cast<uint32_t>(bytes->length()));
  PAGKeyframeBlock decoded = {};
  // The reader validates non-decreasing times (spec §8); the writer trusts its input, so
  // feeding it decreasing times produces input the reader must reject.
  EXPECT_FALSE(ReadPAGKeyframeBlock(&reader, ChannelValueType::Float, &decoded));

  // PAGImage values never appear in files (spec §8.1).
  StreamContext other = {};
  DecodeStream emptyReader(&other);
  PAGKeyframeBlock otherBlock = {};
  EXPECT_FALSE(ReadPAGKeyframeBlock(&emptyReader, ChannelValueType::PAGImage, &otherBlock));
}
}  // namespace pagx
