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

#include "DataTypes.h"
#include <algorithm>
#include <cstdint>
#include "Attributes.h"
#include "codec/tags/FontTables.h"

namespace pag {

enum class PathRecord : uint8_t {
  Close = 0,
  Move = 1,
  Line = 2,
  HLine = 3,
  VLine = 4,
  Curve01 = 5,  // has only control2 point
  Curve10 = 6,  // has only control1 point
  Curve11 = 7   // has both control points
};

Ratio ReadRatio(DecodeStream* stream) {
  Ratio ratio = {};
  ratio.numerator = stream->readEncodedInt32();
  ratio.denominator = stream->readEncodedUint32();
  return ratio;
}

Color ReadColor(DecodeStream* stream) {
  Color color = {};
  color.red = stream->readUint8();
  color.green = stream->readUint8();
  color.blue = stream->readUint8();
  return color;
}

Frame ReadTime(DecodeStream* stream) {
  return static_cast<Frame>(stream->readEncodedUint64());
}

float ReadFloat(DecodeStream* stream) {
  return stream->readFloat();
}

bool ReadBoolean(DecodeStream* stream) {
  return stream->readBoolean();
}

uint8_t ReadUint8(DecodeStream* stream) {
  return stream->readUint8();
}

ID ReadID(DecodeStream* stream) {
  return stream->readEncodedUint32();
}

Layer* ReadLayerID(DecodeStream* stream) {
  auto id = stream->readEncodedUint32();
  if (id > 0) {
    auto layer = new Layer();
    layer->id = id;
    return layer;
  }
  return nullptr;
}

MaskData* ReadMaskID(DecodeStream* stream) {
  auto id = stream->readEncodedUint32();
  if (id > 0) {
    auto mask = new MaskData();
    mask->id = id;
    return mask;
  }
  return nullptr;
}

Composition* ReadCompositionID(DecodeStream* stream) {
  auto id = stream->readEncodedUint32();
  if (id > 0) {
    auto composition = new Composition();
    composition->id = id;
    return composition;
  }
  return nullptr;
}

std::string ReadString(DecodeStream* stream) {
  return stream->readUTF8String();
}

Opacity ReadOpacity(DecodeStream* stream) {
  return stream->readUint8();
}

Point ReadPoint(DecodeStream* stream) {
  Point point = {};
  point.x = stream->readFloat();
  point.y = stream->readFloat();
  return point;
}

Point3D ReadPoint3D(DecodeStream* stream) {
  Point3D point = {};
  point.x = stream->readFloat();
  point.y = stream->readFloat();
  point.z = stream->readFloat();
  return point;
}

static void ReadPathInternal(DecodeStream* stream, PathData* value, const uint8_t records[],
                             int64_t numVerbs) {
  auto numBits = stream->readNumBits();
  float lastX = 0;
  float lastY = 0;
  auto& verbs = value->verbs;
  auto& points = value->points;
  for (uint32_t i = 0; i < numVerbs; i++) {
    if (stream->context->hasException()) {
      break;
    }
    auto pathRecord = records[i];
    switch (static_cast<PathRecord>(pathRecord)) {
      case PathRecord::Close:
        verbs.push_back(PathDataVerb::Close);
        break;
      case PathRecord::Move:
        verbs.push_back(PathDataVerb::MoveTo);
        lastX = stream->readBits(numBits) * SPATIAL_PRECISION;
        lastY = stream->readBits(numBits) * SPATIAL_PRECISION;
        points.push_back({lastX, lastY});
        break;
      case PathRecord::Line:
        verbs.push_back(PathDataVerb::LineTo);
        lastX = stream->readBits(numBits) * SPATIAL_PRECISION;
        lastY = stream->readBits(numBits) * SPATIAL_PRECISION;
        points.push_back({lastX, lastY});
        break;
      case PathRecord::HLine:
        verbs.push_back(PathDataVerb::LineTo);
        lastX = stream->readBits(numBits) * SPATIAL_PRECISION;
        points.push_back({lastX, lastY});
        break;
      case PathRecord::VLine:
        verbs.push_back(PathDataVerb::LineTo);
        lastY = stream->readBits(numBits) * SPATIAL_PRECISION;
        points.push_back({lastX, lastY});
        break;
      case PathRecord::Curve01:
        verbs.push_back(PathDataVerb::CurveTo);
        points.push_back({lastX, lastY});
        lastX = stream->readBits(numBits) * SPATIAL_PRECISION;
        lastY = stream->readBits(numBits) * SPATIAL_PRECISION;
        points.push_back({lastX, lastY});
        lastX = stream->readBits(numBits) * SPATIAL_PRECISION;
        lastY = stream->readBits(numBits) * SPATIAL_PRECISION;
        points.push_back({lastX, lastY});
        break;
      case PathRecord::Curve10:
        verbs.push_back(PathDataVerb::CurveTo);
        lastX = stream->readBits(numBits) * SPATIAL_PRECISION;
        lastY = stream->readBits(numBits) * SPATIAL_PRECISION;
        points.push_back({lastX, lastY});
        lastX = stream->readBits(numBits) * SPATIAL_PRECISION;
        lastY = stream->readBits(numBits) * SPATIAL_PRECISION;
        points.push_back({lastX, lastY});
        points.push_back({lastX, lastY});
        break;
      case PathRecord::Curve11:
        verbs.push_back(PathDataVerb::CurveTo);
        lastX = stream->readBits(numBits) * SPATIAL_PRECISION;
        lastY = stream->readBits(numBits) * SPATIAL_PRECISION;
        points.push_back({lastX, lastY});
        lastX = stream->readBits(numBits) * SPATIAL_PRECISION;
        lastY = stream->readBits(numBits) * SPATIAL_PRECISION;
        points.push_back({lastX, lastY});
        lastX = stream->readBits(numBits) * SPATIAL_PRECISION;
        lastY = stream->readBits(numBits) * SPATIAL_PRECISION;
        points.push_back({lastX, lastY});
        break;
      default:
        break;
    }
  }
}

PathHandle ReadPath(DecodeStream* stream) {
  auto value = new PathData();
  int64_t numVerbs = stream->readEncodedUint32();
  if (numVerbs == 0) {
    return PathHandle(value);
  }
  auto records = new uint8_t[static_cast<size_t>(numVerbs)];
  for (uint32_t i = 0; i < numVerbs; i++) {
    records[i] = static_cast<uint8_t>(stream->readUBits(3));
  }
  ReadPathInternal(stream, value, records, numVerbs);
  delete[] records;
  return PathHandle(value);
}

void ReadFontData(DecodeStream* stream, void* target) {
  auto textDocument = reinterpret_cast<TextDocument*>(target);
  auto fontID = stream->readEncodedUint32();
  auto font = static_cast<CodecContext*>(stream->context)->getFontData(fontID);
  textDocument->fontFamily = font.fontFamily;
  textDocument->fontStyle = font.fontStyle;
}

bool WriteFontData(EncodeStream* stream, void* target) {
  auto textDocument = reinterpret_cast<TextDocument*>(target);
  auto id = static_cast<CodecContext*>(stream->context)
                ->getFontID(textDocument->fontFamily, textDocument->fontStyle);
  stream->writeEncodedUint32(id);
  return true;
}

static std::unique_ptr<BlockConfig> TextDocumentBlockCore(TextDocument* textDocument,
                                                          TagCode tagCode) {
  auto blockConfig = new BlockConfig();
  AddAttribute(blockConfig, &textDocument->applyFill, AttributeType::BitFlag, true);
  AddAttribute(blockConfig, &textDocument->applyStroke, AttributeType::BitFlag, false);
  AddAttribute(blockConfig, &textDocument->boxText, AttributeType::BitFlag, false);
  AddAttribute(blockConfig, &textDocument->fauxBold, AttributeType::BitFlag, false);
  AddAttribute(blockConfig, &textDocument->fauxItalic, AttributeType::BitFlag, false);
  AddAttribute(blockConfig, &textDocument->strokeOverFill, AttributeType::BitFlag, true);
  AddAttribute(blockConfig, &textDocument->baselineShift, AttributeType::Value, 0.0f);
  AddAttribute(blockConfig, &textDocument->firstBaseLine, AttributeType::Value, 0.0f);
  AddAttribute(blockConfig, &textDocument->boxTextPos, AttributeType::Value, Point::Zero());
  AddAttribute(blockConfig, &textDocument->boxTextSize, AttributeType::Value, Point::Zero());
  AddAttribute(blockConfig, &textDocument->fillColor, AttributeType::Value, Black);
  AddAttribute(blockConfig, &textDocument->fontSize, AttributeType::Value, 24.0f);
  AddAttribute(blockConfig, &textDocument->strokeColor, AttributeType::Value, Black);
  AddAttribute(blockConfig, &textDocument->strokeWidth, AttributeType::Value, 1.0f);
  AddAttribute(blockConfig, &textDocument->text, AttributeType::Value, std::string());
  AddAttribute(blockConfig, &textDocument->justification, AttributeType::Value,
               static_cast<uint8_t>(ParagraphJustification::LeftJustify));
  AddAttribute(blockConfig, &textDocument->leading, AttributeType::Value, 0.0f);
  AddAttribute(blockConfig, &textDocument->tracking, AttributeType::Value, 0.0f);
  if (tagCode >= TagCode::TextSourceV2) {
    AddAttribute(blockConfig, &textDocument->backgroundColor, AttributeType::Value, White);
    AddAttribute(blockConfig, &textDocument->backgroundAlpha, AttributeType::Value, Opaque);
  }
  if (tagCode >= TagCode::TextSourceV3) {
    AddAttribute(blockConfig, &textDocument->direction, AttributeType::Value,
                 static_cast<uint8_t>(TextDirection::Vertical));
  }
  AddCustomAttribute(blockConfig, textDocument, ReadFontData, WriteFontData);
  return std::unique_ptr<BlockConfig>(blockConfig);
}

static std::unique_ptr<BlockConfig> TextDocumentBlock(TextDocument* textDocument) {
  return TextDocumentBlockCore(textDocument, TagCode::TextSource);
}

static std::unique_ptr<BlockConfig> TextDocumentBlockV2(TextDocument* textDocument) {
  return TextDocumentBlockCore(textDocument, TagCode::TextSourceV2);
}

static std::unique_ptr<BlockConfig> TextDocumentBlockV3(TextDocument* textDocument) {
  return TextDocumentBlockCore(textDocument, TagCode::TextSourceV3);
}

TextDocumentHandle ReadTextDocument(DecodeStream* stream,
                                    std::unique_ptr<BlockConfig> (*ConfigMaker)(TextDocument*)) {
  auto value = std::make_shared<TextDocument>();
  if (!ReadBlock(stream, value.get(), ConfigMaker)) {
    return nullptr;
  };
  return value;
}

TextDocumentHandle ReadTextDocument(DecodeStream* stream) {
  return ReadTextDocument(stream, TextDocumentBlock);
}

TextDocumentHandle ReadTextDocumentV2(DecodeStream* stream) {
  return ReadTextDocument(stream, TextDocumentBlockV2);
}

TextDocumentHandle ReadTextDocumentV3(DecodeStream* stream) {
  return ReadTextDocument(stream, TextDocumentBlockV3);
}

GradientColorHandle ReadGradientColor(DecodeStream* stream) {
  auto value = new GradientColor();
  auto& alphaStops = value->alphaStops;
  auto& colorStops = value->colorStops;
  auto alphaCount = stream->readEncodedUint32();
  auto colorCount = stream->readEncodedUint32();
  for (uint32_t i = 0; i < alphaCount; i++) {
    AlphaStop stop = {};
    stop.position = stream->readUint16() * GRADIENT_PRECISION;
    stop.midpoint = stream->readUint16() * GRADIENT_PRECISION;
    stop.opacity = stream->readUint8();
    alphaStops.push_back(stop);
  }
  for (uint32_t i = 0; i < colorCount; i++) {
    ColorStop stop = {};
    stop.position = stream->readUint16() * GRADIENT_PRECISION;
    stop.midpoint = stream->readUint16() * GRADIENT_PRECISION;
    stop.color = ReadColor(stream);
    colorStops.push_back(stop);
  }
  std::sort(alphaStops.begin(), alphaStops.end(),
            [](const AlphaStop& a, const AlphaStop& b) { return a.position < b.position; });
  std::sort(colorStops.begin(), colorStops.end(),
            [](const ColorStop& a, const ColorStop& b) { return a.position < b.position; });
  return GradientColorHandle(value);
}

void WriteRatio(EncodeStream* stream, const Ratio& ratio) {
  stream->writeEncodedInt32(ratio.numerator);
  stream->writeEncodedUint32(ratio.denominator);
}

void WriteTime(EncodeStream* stream, Frame time) {
  // Since most of the time values are positive (time in keyframes),
  // just write it as uint64_t to save space (no need to write the sign bit).
  // If it is negative time, it is also can be written and read as normal, but
  // requires more space.
  stream->writeEncodedUint64(static_cast<uint64_t>(time));
}

void WriteColor(EncodeStream* stream, const Color& color) {
  stream->writeUint8(color.red);
  stream->writeUint8(color.green);
  stream->writeUint8(color.blue);
}

void WriteFloat(EncodeStream* stream, float value) {
  stream->writeFloat(value);
}

void WriteBoolean(EncodeStream* stream, bool value) {
  stream->writeBoolean(value);
}

void WriteUint8(EncodeStream* stream, uint8_t value) {
  stream->writeUint8(value);
}

void WriteID(EncodeStream* stream, pag::ID value) {
  stream->writeEncodedUint32(value);
}

void WriteLayerID(EncodeStream* stream, Layer* layer) {
  if (layer != nullptr) {
    stream->writeEncodedUint32(layer->id);
  } else {
    stream->writeEncodedUint32(0);
  }
}

void WriteMaskID(EncodeStream* stream, MaskData* mask) {
  if (mask != nullptr) {
    stream->writeEncodedUint32(mask->id);
  } else {
    stream->writeEncodedUint32(0);
  }
}

void WriteCompositionID(EncodeStream* stream, Composition* composition) {
  if (composition != nullptr) {
    stream->writeEncodedUint32(composition->id);
  } else {
    stream->writeEncodedUint32(0);
  }
}

void WriteString(EncodeStream* stream, std::string value) {
  stream->writeUTF8String(value);
}

void WriteOpacity(EncodeStream* stream, pag::Opacity value) {
  stream->writeUint8(value);
}

void WritePoint(EncodeStream* stream, pag::Point value) {
  stream->writeFloat(value.x);
  stream->writeFloat(value.y);
}

void WritePoint3D(EncodeStream* stream, pag::Point3D value) {
  stream->writeFloat(value.x);
  stream->writeFloat(value.y);
  stream->writeFloat(value.z);
}

static void WritePathInternal(EncodeStream* stream, pag::PathHandle value) {
  auto& points = value->points;
  std::vector<float> pointList;
  uint32_t index = 0;
  auto lastPoint = Point::Make(0, 0);
  Point control1{}, control2{}, point{};
  for (auto& verb : value->verbs) {
    switch (verb) {
      case PathDataVerb::Close:
        stream->writeUBits(static_cast<uint32_t>(PathRecord::Close), 3);
        break;
      case PathDataVerb::MoveTo:
        lastPoint = points[index++];
        stream->writeUBits(static_cast<uint32_t>(PathRecord::Move), 3);
        pointList.push_back(lastPoint.x);
        pointList.push_back(lastPoint.y);
        break;
      case PathDataVerb::LineTo:
        point = points[index++];
        if (point.x == lastPoint.x) {
          stream->writeUBits(static_cast<uint32_t>(PathRecord::VLine), 3);
          pointList.push_back(point.y);
        } else if (point.y == lastPoint.y) {
          stream->writeUBits(static_cast<uint32_t>(PathRecord::HLine), 3);
          pointList.push_back(point.x);
        } else {
          stream->writeUBits(static_cast<uint32_t>(PathRecord::Line), 3);
          pointList.push_back(point.x);
          pointList.push_back(point.y);
        }
        lastPoint = point;
        break;
      case PathDataVerb::CurveTo:
        control1 = points[index++];
        control2 = points[index++];
        point = points[index++];
        if (control1 == lastPoint) {
          stream->writeUBits(static_cast<uint32_t>(PathRecord::Curve01), 3);
          pointList.push_back(control2.x);
          pointList.push_back(control2.y);
          pointList.push_back(point.x);
          pointList.push_back(point.y);
          lastPoint = point;
        } else if (control2 == point) {
          stream->writeUBits(static_cast<uint32_t>(PathRecord::Curve10), 3);
          pointList.push_back(control1.x);
          pointList.push_back(control1.y);
          pointList.push_back(point.x);
          pointList.push_back(point.y);
          lastPoint = point;
        } else {
          stream->writeUBits(static_cast<uint32_t>(PathRecord::Curve11), 3);
          pointList.push_back(control1.x);
          pointList.push_back(control1.y);
          pointList.push_back(control2.x);
          pointList.push_back(control2.y);
          pointList.push_back(point.x);
          pointList.push_back(point.y);
          lastPoint = point;
        }
        break;
    }
  }
  stream->writeFloatList(pointList.data(), static_cast<int32_t>(pointList.size()),
                         SPATIAL_PRECISION);
}

void WritePath(EncodeStream* stream, pag::PathHandle value) {
  stream->writeEncodedUint32(static_cast<uint32_t>(value->verbs.size()));
  if (value->verbs.empty()) {
    return;
  }
  WritePathInternal(stream, value);
}

void WriteTextDocument(EncodeStream* stream, pag::TextDocumentHandle value) {
  WriteBlock(stream, value.get(), TextDocumentBlock);
}

void WriteTextDocumentV2(EncodeStream* stream, pag::TextDocumentHandle value) {
  WriteBlock(stream, value.get(), TextDocumentBlockV2);
}

void WriteTextDocumentV3(EncodeStream* stream, pag::TextDocumentHandle value) {
  WriteBlock(stream, value.get(), TextDocumentBlockV3);
}

void WriteGradientColor(EncodeStream* stream, pag::GradientColorHandle value) {
  auto& alphaStops = value->alphaStops;
  auto& colorStops = value->colorStops;
  auto alphaCount = static_cast<uint32_t>(alphaStops.size());
  auto colorCount = static_cast<uint32_t>(colorStops.size());
  stream->writeEncodedUint32(alphaCount);
  stream->writeEncodedUint32(colorCount);
  for (uint32_t i = 0; i < alphaCount; i++) {
    auto& stop = alphaStops[i];
    stream->writeUint16(static_cast<uint16_t>(stop.position / GRADIENT_PRECISION));
    stream->writeUint16(static_cast<uint16_t>(stop.midpoint / GRADIENT_PRECISION));
    stream->writeUint8(stop.opacity);
  }
  for (uint32_t i = 0; i < colorCount; i++) {
    auto& stop = colorStops[i];
    stream->writeUint16(static_cast<uint16_t>(stop.position / GRADIENT_PRECISION));
    stream->writeUint16(static_cast<uint16_t>(stop.midpoint / GRADIENT_PRECISION));
    WriteColor(stream, stop.color);
  }
}
}  // namespace pag
