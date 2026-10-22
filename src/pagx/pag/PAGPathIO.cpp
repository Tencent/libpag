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

#include "pagx/pag/PAGPathIO.h"
#include <cmath>
#include <vector>

namespace pagx {
// Legacy quantization grid (0.05) shared with the old codec's spatial precision.
static constexpr float kPathBaseStep = 0.05f;

// Path records (spec §7.1), 4 bits each. Values 9-15 are reserved; readers reject them.
enum class PathRecord : uint8_t {
  Close = 0,
  Move = 1,
  Line = 2,
  HLine = 3,
  VLine = 4,
  Quad = 5,
  Curve01 = 6,  // control1 equals the current point
  Curve10 = 7,  // control2 equals the end point
  Curve11 = 8,
};

// Mirrors the legacy signed bit-length helper: two's complement width for a value,
// including the sign bit, capped so the +1 for the sign still fits 32 bits.
static uint8_t SignedBitLength(int32_t value) {
  auto data = static_cast<uint32_t>(value < 0 ? -static_cast<int64_t>(value) : value);
  uint8_t length = 32;
  static const uint32_t mask = 1u << 31;
  while (length > 1) {
    if ((data & mask) != 0) {
      break;
    }
    data <<= 1;
    length--;
  }
  if (length >= 32) {
    length = 31;
  }
  return static_cast<uint8_t>(length + 1);
}

uint8_t ComputeStepExponent(float maxDisplayScale) {
  if (maxDisplayScale <= 1.0f || std::isnan(maxDisplayScale)) {
    return 0;
  }
  auto exponent = static_cast<int>(std::ceil(std::log2(maxDisplayScale)));
  if (exponent < 0) {
    exponent = 0;
  }
  if (exponent > 255) {
    exponent = 255;
  }
  return static_cast<uint8_t>(exponent);
}

// Collects the record list and the absolute coordinate list for a path, mirroring the
// legacy degenerate-record selection. Coordinates land in the list in record order.
static void CollectPathRecords(const PathData* path, std::vector<uint8_t>* records,
                               std::vector<float>* coordinates) {
  const auto& verbs = path->verbs();
  const auto& points = path->points();
  size_t pointIndex = 0;
  Point lastPoint = {};
  for (auto verb : verbs) {
    switch (verb) {
      case PathVerb::Close:
        records->push_back(static_cast<uint8_t>(PathRecord::Close));
        break;
      case PathVerb::Move: {
        auto point = points[pointIndex++];
        records->push_back(static_cast<uint8_t>(PathRecord::Move));
        coordinates->push_back(point.x);
        coordinates->push_back(point.y);
        lastPoint = point;
        break;
      }
      case PathVerb::Line: {
        auto point = points[pointIndex++];
        if (point.x == lastPoint.x) {
          records->push_back(static_cast<uint8_t>(PathRecord::VLine));
          coordinates->push_back(point.y);
        } else if (point.y == lastPoint.y) {
          records->push_back(static_cast<uint8_t>(PathRecord::HLine));
          coordinates->push_back(point.x);
        } else {
          records->push_back(static_cast<uint8_t>(PathRecord::Line));
          coordinates->push_back(point.x);
          coordinates->push_back(point.y);
        }
        lastPoint = point;
        break;
      }
      case PathVerb::Quad: {
        auto control = points[pointIndex++];
        auto end = points[pointIndex++];
        records->push_back(static_cast<uint8_t>(PathRecord::Quad));
        coordinates->push_back(control.x);
        coordinates->push_back(control.y);
        coordinates->push_back(end.x);
        coordinates->push_back(end.y);
        lastPoint = end;
        break;
      }
      case PathVerb::Cubic: {
        auto control1 = points[pointIndex++];
        auto control2 = points[pointIndex++];
        auto end = points[pointIndex++];
        if (control1.x == lastPoint.x && control1.y == lastPoint.y) {
          records->push_back(static_cast<uint8_t>(PathRecord::Curve01));
          coordinates->push_back(control2.x);
          coordinates->push_back(control2.y);
        } else if (control2.x == end.x && control2.y == end.y) {
          records->push_back(static_cast<uint8_t>(PathRecord::Curve10));
          coordinates->push_back(control1.x);
          coordinates->push_back(control1.y);
        } else {
          records->push_back(static_cast<uint8_t>(PathRecord::Curve11));
          coordinates->push_back(control1.x);
          coordinates->push_back(control1.y);
          coordinates->push_back(control2.x);
          coordinates->push_back(control2.y);
        }
        coordinates->push_back(end.x);
        coordinates->push_back(end.y);
        lastPoint = end;
        break;
      }
    }
  }
}

void WritePAGPath(pag::EncodeStream* stream, const PathData* path, uint8_t stepExponent) {
  const auto& verbs = path->verbs();
  stream->writeEncodedUint32(static_cast<uint32_t>(verbs.size()));
  if (verbs.empty()) {
    return;
  }
  std::vector<uint8_t> records = {};
  std::vector<float> coordinates = {};
  CollectPathRecords(path, &records, &coordinates);

  stream->writeUint8(stepExponent);
  for (auto record : records) {
    stream->writeUBits(record, 4);
  }
  stream->alignWithBytes();

  auto scale = 1.0f / (kPathBaseStep * std::pow(2.0f, -static_cast<float>(stepExponent)));
  std::vector<int32_t> quantized(coordinates.size());
  uint8_t bitLength = 1;
  for (size_t i = 0; i < coordinates.size(); i++) {
    quantized[i] = static_cast<int32_t>(lroundf(coordinates[i] * scale));
    auto length = SignedBitLength(quantized[i]);
    if (bitLength < length) {
      bitLength = length;
    }
  }
  stream->writeUBits(static_cast<uint32_t>(bitLength - 1), 5);
  for (auto value : quantized) {
    stream->writeBits(value, bitLength);
  }
}

static float ReadCoordinate(pag::DecodeStream* stream, uint8_t numBits, float step) {
  return static_cast<float>(stream->readBits(numBits)) * step;
}

bool ReadPAGPath(pag::DecodeStream* stream, PathData* path) {
  auto verbCount = stream->readEncodedUint32();
  if (stream->context->hasException()) {
    return false;
  }
  if (verbCount == 0) {
    return true;
  }
  auto stepExponent = stream->readUint8();
  auto step = kPathBaseStep * std::pow(2.0f, -static_cast<float>(stepExponent));
  std::vector<uint8_t> records(verbCount, 0);
  for (uint32_t i = 0; i < verbCount; i++) {
    records[i] = static_cast<uint8_t>(stream->readUBits(4));
  }
  stream->alignWithBytes();

  auto numBits = stream->readUBits(5) + 1;
  Point lastPoint = {};
  for (auto record : records) {
    if (stream->context->hasException()) {
      return false;
    }
    switch (static_cast<PathRecord>(record)) {
      case PathRecord::Close:
        path->close();
        break;
      case PathRecord::Move:
        lastPoint = {ReadCoordinate(stream, numBits, step), ReadCoordinate(stream, numBits, step)};
        path->moveTo(lastPoint.x, lastPoint.y);
        break;
      case PathRecord::Line:
        lastPoint = {ReadCoordinate(stream, numBits, step), ReadCoordinate(stream, numBits, step)};
        path->lineTo(lastPoint.x, lastPoint.y);
        break;
      case PathRecord::HLine:
        lastPoint.x = ReadCoordinate(stream, numBits, step);
        path->lineTo(lastPoint.x, lastPoint.y);
        break;
      case PathRecord::VLine:
        lastPoint.y = ReadCoordinate(stream, numBits, step);
        path->lineTo(lastPoint.x, lastPoint.y);
        break;
      case PathRecord::Quad: {
        auto controlX = ReadCoordinate(stream, numBits, step);
        auto controlY = ReadCoordinate(stream, numBits, step);
        auto endX = ReadCoordinate(stream, numBits, step);
        auto endY = ReadCoordinate(stream, numBits, step);
        path->quadTo(controlX, controlY, endX, endY);
        lastPoint = {endX, endY};
        break;
      }
      case PathRecord::Curve01: {
        auto previous = lastPoint;
        auto c2x = ReadCoordinate(stream, numBits, step);
        auto c2y = ReadCoordinate(stream, numBits, step);
        auto endX = ReadCoordinate(stream, numBits, step);
        auto endY = ReadCoordinate(stream, numBits, step);
        path->cubicTo(previous.x, previous.y, c2x, c2y, endX, endY);
        lastPoint = {endX, endY};
        break;
      }
      case PathRecord::Curve10: {
        auto c1x = ReadCoordinate(stream, numBits, step);
        auto c1y = ReadCoordinate(stream, numBits, step);
        auto endX = ReadCoordinate(stream, numBits, step);
        auto endY = ReadCoordinate(stream, numBits, step);
        path->cubicTo(c1x, c1y, endX, endY, endX, endY);
        lastPoint = {endX, endY};
        break;
      }
      case PathRecord::Curve11: {
        auto c1x = ReadCoordinate(stream, numBits, step);
        auto c1y = ReadCoordinate(stream, numBits, step);
        auto c2x = ReadCoordinate(stream, numBits, step);
        auto c2y = ReadCoordinate(stream, numBits, step);
        auto endX = ReadCoordinate(stream, numBits, step);
        auto endY = ReadCoordinate(stream, numBits, step);
        path->cubicTo(c1x, c1y, c2x, c2y, endX, endY);
        lastPoint = {endX, endY};
        break;
      }
      default:
        // Reserved record value (spec §7.1): the path is malformed.
        stream->context->throwException("unknown path record");
        return false;
    }
  }
  return !stream->context->hasException();
}
}  // namespace pagx
