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
//  license is distributed on an "AS IS" basis, without WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "pagx/pag/PAGKeyframeIO.h"
#include "pagx/pag/PAGDataTypes.h"
#include "pagx/pag/PAGPathIO.h"

namespace pagx {
static constexpr float kBezierPrecision = 0.005f;

void PAGKeyframeBlock::clear() {
  times.clear();
  interpolations.clear();
  bezierOut.clear();
  bezierIn.clear();
  floatValues.clear();
  intValues.clear();
  boolValues.clear();
  stringIndices.clear();
  colorValues.clear();
  matrixValues.clear();
  imageIndices.clear();
  pathValues.clear();
  pathStepExponent = 0;
}

static bool ValidBlock(const PAGKeyframeBlock& block, ChannelValueType type) {
  auto count = block.times.size();
  if (block.interpolations.size() != count) {
    return false;
  }
  if (block.bezierOut.size() != count || block.bezierIn.size() != count) {
    return false;
  }
  switch (type) {
    case ChannelValueType::Float:
      return block.floatValues.size() == count;
    case ChannelValueType::Bool:
      return block.boolValues.size() == count;
    case ChannelValueType::Int:
      return block.intValues.size() == count;
    case ChannelValueType::String:
      return block.stringIndices.size() == count;
    case ChannelValueType::Color:
      return block.colorValues.size() == count;
    case ChannelValueType::Matrix:
      return block.matrixValues.size() == count;
    case ChannelValueType::ImageRef:
      return block.imageIndices.size() == count;
    case ChannelValueType::Path:
      return block.pathValues.size() == count;
    default:
      return false;
  }
}

static size_t ValueCount(const PAGKeyframeBlock& block, ChannelValueType type) {
  switch (type) {
    case ChannelValueType::Float:
      return block.floatValues.size();
    case ChannelValueType::Bool:
      return block.boolValues.size();
    case ChannelValueType::Int:
      return block.intValues.size();
    case ChannelValueType::String:
      return block.stringIndices.size();
    case ChannelValueType::Color:
      return block.colorValues.size();
    case ChannelValueType::Matrix:
      return block.matrixValues.size();
    case ChannelValueType::ImageRef:
      return block.imageIndices.size();
    case ChannelValueType::Path:
      return block.pathValues.size();
    default:
      return 0;
  }
}

void WritePAGKeyframeBlock(pag::EncodeStream* stream, ChannelValueType type,
                           const PAGKeyframeBlock& block) {
  auto count = ValueCount(block, type);
  stream->writeEncodedUint32(static_cast<uint32_t>(count));
  for (auto interpolation : block.interpolations) {
    stream->writeUBits(static_cast<uint32_t>(interpolation), 2);
  }
  stream->alignWithBytes();
  for (auto time : block.times) {
    WritePAGFrame(stream, time);
  }
  switch (type) {
    case ChannelValueType::Float:
      for (auto value : block.floatValues) {
        stream->writeFloat(value);
      }
      break;
    case ChannelValueType::Bool:
      for (auto value : block.boolValues) {
        stream->writeUBits(value != 0 ? 1 : 0, 1);
      }
      stream->alignWithBytes();
      break;
    case ChannelValueType::Int:
      stream->writeInt32List(block.intValues.data(), static_cast<uint32_t>(count));
      stream->alignWithBytes();
      break;
    case ChannelValueType::String:
      for (auto index : block.stringIndices) {
        stream->writeEncodedUint32(index);
      }
      break;
    case ChannelValueType::Color:
      for (const auto& value : block.colorValues) {
        WritePAGColor(stream, value);
      }
      break;
    case ChannelValueType::Matrix:
      for (const auto& value : block.matrixValues) {
        WritePAGMatrix(stream, value);
      }
      break;
    case ChannelValueType::ImageRef:
      for (auto index : block.imageIndices) {
        stream->writeEncodedUint32(index);
      }
      break;
    case ChannelValueType::Path:
      for (auto value : block.pathValues) {
        WritePAGPath(stream, value, block.pathStepExponent);
      }
      break;
    default:
      break;
  }
  if (type == ChannelValueType::Matrix) {
    stream->alignWithBytes();
  }
  std::vector<float> bezierList = {};
  for (size_t i = 0; i < count; i++) {
    if (block.interpolations[i] != KeyframeInterpolationType::Bezier) {
      continue;
    }
    bezierList.push_back(block.bezierOut[i].x);
    bezierList.push_back(block.bezierOut[i].y);
    bezierList.push_back(block.bezierIn[i].x);
    bezierList.push_back(block.bezierIn[i].y);
  }
  stream->writeFloatList(bezierList.data(), static_cast<uint32_t>(bezierList.size()),
                         kBezierPrecision);
  stream->alignWithBytes();
}

bool ReadPAGKeyframeBlock(pag::DecodeStream* stream, ChannelValueType type,
                          PAGKeyframeBlock* block) {
  if (type != ChannelValueType::Float && type != ChannelValueType::Bool &&
      type != ChannelValueType::Int && type != ChannelValueType::String &&
      type != ChannelValueType::Color && type != ChannelValueType::Matrix &&
      type != ChannelValueType::ImageRef && type != ChannelValueType::Path) {
    // PAGImage values only exist at runtime (spec §8.1) and never appear in a file.
    stream->context->throwException("invalid channel value type");
    return false;
  }
  auto count = stream->readEncodedUint32();
  // PathData nodes are document-owned and cannot be created here; keep the caller's
  // pre-created nodes alive across the reset (spec §8.1 path column).
  std::vector<PathData*> providedPaths = std::move(block->pathValues);
  block->clear();
  block->times.resize(count);
  block->interpolations.resize(count);
  block->bezierOut.resize(count);
  block->bezierIn.resize(count);
  for (uint32_t i = 0; i < count; i++) {
    block->interpolations[i] = static_cast<KeyframeInterpolationType>(stream->readUBits(2));
  }
  stream->alignWithBytes();
  Frame lastTime = 0;
  for (uint32_t i = 0; i < count; i++) {
    auto time = ReadPAGFrame(stream);
    if (time < lastTime) {
      // Non-decreasing times are a format invariant (spec §8); the caller drops the channel.
      stream->context->throwException("keyframe times must be non-decreasing");
      return false;
    }
    lastTime = time;
    block->times[i] = time;
  }
  switch (type) {
    case ChannelValueType::Float:
      block->floatValues.resize(count);
      for (uint32_t i = 0; i < count; i++) {
        block->floatValues[i] = stream->readFloat();
      }
      break;
    case ChannelValueType::Bool:
      block->boolValues.resize(count);
      for (uint32_t i = 0; i < count; i++) {
        block->boolValues[i] = static_cast<uint8_t>(stream->readUBits(1));
      }
      stream->alignWithBytes();
      break;
    case ChannelValueType::Int:
      block->intValues.resize(count);
      stream->readInt32List(block->intValues.data(), count);
      stream->alignWithBytes();
      break;
    case ChannelValueType::String:
      block->stringIndices.resize(count);
      for (uint32_t i = 0; i < count; i++) {
        block->stringIndices[i] = stream->readEncodedUint32();
      }
      break;
    case ChannelValueType::Color:
      block->colorValues.resize(count);
      for (uint32_t i = 0; i < count; i++) {
        block->colorValues[i] = ReadPAGColor(stream);
      }
      break;
    case ChannelValueType::Matrix:
      block->matrixValues.resize(count);
      for (uint32_t i = 0; i < count; i++) {
        block->matrixValues[i] = ReadPAGMatrix(stream);
      }
      break;
    case ChannelValueType::ImageRef:
      block->imageIndices.resize(count);
      for (uint32_t i = 0; i < count; i++) {
        block->imageIndices[i] = stream->readEncodedUint32();
      }
      break;
    case ChannelValueType::Path:
      // PathData nodes are document-owned (private constructors); the caller must
      // provide one node per keyframe before reading (spec §8.1).
      if (providedPaths.size() != count) {
        stream->context->throwException("path column needs preallocated PathData nodes");
        return false;
      }
      block->pathValues = std::move(providedPaths);
      for (uint32_t i = 0; i < count; i++) {
        if (block->pathValues[i] == nullptr || !ReadPAGPath(stream, block->pathValues[i])) {
          return false;
        }
      }
      break;
    default:
      break;
  }
  if (type == ChannelValueType::Matrix) {
    stream->alignWithBytes();
  }
  uint32_t bezierCount = 0;
  for (auto interpolation : block->interpolations) {
    if (interpolation == KeyframeInterpolationType::Bezier) {
      bezierCount++;
    }
  }
  std::vector<float> bezierList(bezierCount * 4, 0.0f);
  stream->readFloatList(bezierList.data(), bezierCount * 4, kBezierPrecision);
  stream->alignWithBytes();
  uint32_t bezierIndex = 0;
  for (uint32_t i = 0; i < count; i++) {
    if (block->interpolations[i] != KeyframeInterpolationType::Bezier) {
      continue;
    }
    block->bezierOut[i] = {bezierList[bezierIndex * 4], bezierList[bezierIndex * 4 + 1]};
    block->bezierIn[i] = {bezierList[bezierIndex * 4 + 2], bezierList[bezierIndex * 4 + 3]};
    bezierIndex++;
  }
  return ValidBlock(*block, type) && !stream->context->hasException();
}
}  // namespace pagx
