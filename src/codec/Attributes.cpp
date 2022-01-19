/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "Attributes.h"

namespace pag {
// -----------------------------------------------------------------------------
// AttributeConfig<float>
// -----------------------------------------------------------------------------
template <>
float AttributeConfig<float>::readValue(DecodeStream* stream) const {
  return stream->readFloat();
}

template <>
void AttributeConfig<float>::writeValue(EncodeStream* stream, const float& value) const {
  stream->writeFloat(value);
}

template <>
Keyframe<float>* AttributeConfig<float>::newKeyframe(const AttributeFlag&) const {
  return new SingleEaseKeyframe<float>();
}

// -----------------------------------------------------------------------------
// AttributeConfig<bool>
// -----------------------------------------------------------------------------
template <>
bool AttributeConfig<bool>::readValue(DecodeStream* stream) const {
  return stream->readBoolean();
}

template <>
void AttributeConfig<bool>::writeValue(EncodeStream* stream, const bool& value) const {
  stream->writeBoolean(value);
}

template <>
void AttributeConfig<bool>::readValueList(DecodeStream* stream, bool* list, uint32_t count) const {
  for (uint32_t i = 0; i < count; i++) {
    list[i] = stream->readBitBoolean();
  }
}

template <>
void AttributeConfig<bool>::writeValueList(EncodeStream* stream, const bool* list,
                                           uint32_t count) const {
  for (uint32_t i = 0; i < count; i++) {
    stream->writeBitBoolean(list[i]);
  }
}

// -----------------------------------------------------------------------------
// AttributeConfig<uint8_t>
// -----------------------------------------------------------------------------
template <>
uint8_t AttributeConfig<uint8_t>::readValue(DecodeStream* stream) const {
  return stream->readUint8();
}

template <>
void AttributeConfig<uint8_t>::writeValue(EncodeStream* stream, const uint8_t& value) const {
  stream->writeUint8(value);
}

template <>
void AttributeConfig<uint8_t>::readValueList(DecodeStream* stream, uint8_t* list,
                                             uint32_t count) const {
  auto valueList = new uint32_t[count];
  stream->readUint32List(valueList, count);
  for (uint32_t i = 0; i < count; i++) {
    list[i] = static_cast<uint8_t>(valueList[i]);
  }
  delete[] valueList;
}

template <>
void AttributeConfig<uint8_t>::writeValueList(EncodeStream* stream, const uint8_t* list,
                                              uint32_t count) const {
  auto valueList = new uint32_t[count];
  for (uint32_t i = 0; i < count; i++) {
    valueList[i] = list[i];
  }
  stream->writeUint32List(valueList, count);
  delete[] valueList;
}

template <>
Keyframe<uint8_t>* AttributeConfig<uint8_t>::newKeyframe(const AttributeFlag&) const {
  return new SingleEaseKeyframe<uint8_t>();
}

// -----------------------------------------------------------------------------
// AttributeConfig<uint16_t>
// -----------------------------------------------------------------------------
template <>
uint16_t AttributeConfig<uint16_t>::readValue(DecodeStream* stream) const {
  auto value = stream->readEncodedUint32();
  return static_cast<uint16_t>(value);
}

template <>
void AttributeConfig<uint16_t>::writeValue(EncodeStream* stream, const uint16_t& value) const {
  stream->writeEncodedUint32(static_cast<uint32_t>(value));
}

template <>
void AttributeConfig<uint16_t>::readValueList(DecodeStream* stream, uint16_t* list,
                                              uint32_t count) const {
  auto valueList = new uint32_t[count];
  stream->readUint32List(valueList, count);
  for (uint32_t i = 0; i < count; i++) {
    list[i] = static_cast<uint16_t>(valueList[i]);
  }
  delete[] valueList;
}

template <>
void AttributeConfig<uint16_t>::writeValueList(EncodeStream* stream, const uint16_t* list,
                                               uint32_t count) const {
  auto valueList = new uint32_t[count];
  for (uint32_t i = 0; i < count; i++) {
    valueList[i] = list[i];
  }
  stream->writeUint32List(valueList, count);
  delete[] valueList;
}

template <>
Keyframe<uint16_t>* AttributeConfig<uint16_t>::newKeyframe(const AttributeFlag&) const {
  return new SingleEaseKeyframe<uint16_t>();
}

// -----------------------------------------------------------------------------
// AttributeConfig<uint32_t>
// -----------------------------------------------------------------------------
template <>
uint32_t AttributeConfig<uint32_t>::readValue(DecodeStream* stream) const {
  return stream->readEncodedUint32();
}

template <>
void AttributeConfig<uint32_t>::writeValue(EncodeStream* stream, const uint32_t& value) const {
  stream->writeEncodedUint32(value);
}

template <>
void AttributeConfig<uint32_t>::readValueList(DecodeStream* stream, uint32_t* list,
                                              uint32_t count) const {
  stream->readUint32List(list, count);
}

template <>
void AttributeConfig<uint32_t>::writeValueList(EncodeStream* stream, const uint32_t* list,
                                               uint32_t count) const {
  stream->writeUint32List(list, count);
}

template <>
Keyframe<uint32_t>* AttributeConfig<uint32_t>::newKeyframe(const AttributeFlag&) const {
  return new SingleEaseKeyframe<uint32_t>();
}

// -----------------------------------------------------------------------------
// AttributeConfig<int32_t>
// -----------------------------------------------------------------------------
template <>
int32_t AttributeConfig<int32_t>::readValue(DecodeStream* stream) const {
  return stream->readEncodedInt32();
}

template <>
void AttributeConfig<int32_t>::writeValue(EncodeStream* stream, const int32_t& value) const {
  stream->writeEncodedInt32(value);
}

template <>
void AttributeConfig<int32_t>::readValueList(DecodeStream* stream, int32_t* list,
                                             uint32_t count) const {
  stream->readInt32List(list, count);
}

template <>
void AttributeConfig<int32_t>::writeValueList(EncodeStream* stream, const int32_t* list,
                                              uint32_t count) const {
  stream->writeInt32List(list, count);
}

template <>
Keyframe<int32_t>* AttributeConfig<int32_t>::newKeyframe(const AttributeFlag&) const {
  return new SingleEaseKeyframe<int32_t>();
}

// -----------------------------------------------------------------------------
// AttributeConfig<Frame>
// -----------------------------------------------------------------------------
template <>
Frame AttributeConfig<Frame>::readValue(DecodeStream* stream) const {
  return ReadTime(stream);
}

template <>
void AttributeConfig<Frame>::writeValue(EncodeStream* stream, const Frame& value) const {
  WriteTime(stream, value);
}

template <>
Keyframe<Frame>* AttributeConfig<Frame>::newKeyframe(const AttributeFlag&) const {
  return new SingleEaseKeyframe<Frame>();
}

// -----------------------------------------------------------------------------
// AttributeConfig<Point>
// -----------------------------------------------------------------------------
template <>
int AttributeConfig<Point>::dimensionality() const {
  return 2;
}

template <>
Point AttributeConfig<Point>::readValue(DecodeStream* stream) const {
  return ReadPoint(stream);
}

template <>
void AttributeConfig<Point>::writeValue(EncodeStream* stream, const Point& value) const {
  WritePoint(stream, value);
}

template <>
void AttributeConfig<Point>::readValueList(DecodeStream* stream, Point* list,
                                           uint32_t count) const {
  if (attributeType == AttributeType::SpatialProperty) {
    stream->readFloatList(&(list[0].x), count * 2, SPATIAL_PRECISION);
  } else {
    for (uint32_t i = 0; i < count; i++) {
      list[i] = ReadPoint(stream);
    }
  }
}

template <>
void AttributeConfig<Point>::writeValueList(EncodeStream* stream, const Point* list,
                                            uint32_t count) const {
  if (attributeType == AttributeType::SpatialProperty) {
    stream->writeFloatList(&(list[0].x), count * 2, SPATIAL_PRECISION);
  } else {
    for (uint32_t i = 0; i < count; i++) {
      WritePoint(stream, list[i]);
    }
  }
}

template <>
Keyframe<Point>* AttributeConfig<Point>::newKeyframe(const AttributeFlag& flag) const {
  switch (attributeType) {
    case AttributeType::MultiDimensionProperty:
      return new MultiDimensionPointKeyframe();
    case AttributeType::SpatialProperty:
      if (flag.hasSpatial) {
        return new SpatialPointKeyframe();
      }
    default:
      return new SingleEaseKeyframe<Point>();
  }
}

// -----------------------------------------------------------------------------
// AttributeConfig<Color>
// -----------------------------------------------------------------------------
template <>
int AttributeConfig<Color>::dimensionality() const {
  return 3;
}

template <>
Color AttributeConfig<Color>::readValue(DecodeStream* stream) const {
  return ReadColor(stream);
}

template <>
void AttributeConfig<Color>::writeValue(EncodeStream* stream, const Color& value) const {
  WriteColor(stream, value);
}

template <>
Keyframe<Color>* AttributeConfig<Color>::newKeyframe(const AttributeFlag&) const {
  return new SingleEaseKeyframe<Color>();
}

// -----------------------------------------------------------------------------
// AttributeConfig<Ratio>
// -----------------------------------------------------------------------------
template <>
Ratio AttributeConfig<Ratio>::readValue(DecodeStream* stream) const {
  return ReadRatio(stream);
}

template <>
void AttributeConfig<Ratio>::writeValue(EncodeStream* stream, const Ratio& value) const {
  WriteRatio(stream, value);
}

// -----------------------------------------------------------------------------
// AttributeConfig<std::string>
// -----------------------------------------------------------------------------
template <>
std::string AttributeConfig<std::string>::readValue(DecodeStream* stream) const {
  return stream->readUTF8String();
}

template <>
void AttributeConfig<std::string>::writeValue(EncodeStream* stream,
                                              const std::string& value) const {
  stream->writeUTF8String(value);
}

// -----------------------------------------------------------------------------
// AttributeConfig<PathHandle>
// -----------------------------------------------------------------------------
template <>
PathHandle AttributeConfig<PathHandle>::readValue(DecodeStream* stream) const {
  return ReadPath(stream);
}

template <>
void AttributeConfig<PathHandle>::writeValue(EncodeStream* stream, const PathHandle& value) const {
  WritePath(stream, value);
}

template <>
Keyframe<PathHandle>* AttributeConfig<PathHandle>::newKeyframe(const AttributeFlag&) const {
  return new SingleEaseKeyframe<PathHandle>();
}

// -----------------------------------------------------------------------------
// AttributeConfig<TextDocumentHandle>
// -----------------------------------------------------------------------------
template <>
TextDocumentHandle AttributeConfig<TextDocumentHandle>::readValue(DecodeStream* stream) const {
  // 外面根据 tag V1/V2/V3 设置了 defaultValue，
  // 这里根据 defaultValue 决定使用哪个 TAG 来读
  if (defaultValue->direction != TextDirection::Default) {
    return ReadTextDocumentV3(stream);
  } else if (defaultValue->backgroundAlpha != 0) {
    return ReadTextDocumentV2(stream);
  } else {
    return ReadTextDocument(stream);
  }
}

template <>
void AttributeConfig<TextDocumentHandle>::writeValue(EncodeStream* stream,
                                                     const TextDocumentHandle& value) const {
  // 外面根据 tag V1/V2/V3 设置了 defaultValue，
  // 这里根据 defaultValue 决定使用哪个 TAG 来写
  if (defaultValue->direction != TextDirection::Default) {
    WriteTextDocumentV3(stream, value);
  } else if (defaultValue->backgroundAlpha != 0) {
    WriteTextDocumentV2(stream, value);
  } else {
    WriteTextDocument(stream, value);
  }
}

// -----------------------------------------------------------------------------
// AttributeConfig<GradientColorHandle>
// -----------------------------------------------------------------------------
template <>
GradientColorHandle AttributeConfig<GradientColorHandle>::readValue(DecodeStream* stream) const {
  return ReadGradientColor(stream);
}

template <>
void AttributeConfig<GradientColorHandle>::writeValue(EncodeStream* stream,
                                                      const GradientColorHandle& value) const {
  WriteGradientColor(stream, value);
}

template <>
Keyframe<GradientColorHandle>* AttributeConfig<GradientColorHandle>::newKeyframe(
    const AttributeFlag&) const {
  return new SingleEaseKeyframe<GradientColorHandle>();
}

// -----------------------------------------------------------------------------
// AttributeConfig<Layer*>
// -----------------------------------------------------------------------------
template <>
Layer* AttributeConfig<Layer*>::readValue(DecodeStream* stream) const {
  return ReadLayerID(stream);
}

template <>
void AttributeConfig<Layer*>::writeValue(EncodeStream* stream, Layer* const& value) const {
  WriteLayerID(stream, const_cast<Layer*>(value));
}

// -----------------------------------------------------------------------------
// AttributeConfig<MaskData*>
// -----------------------------------------------------------------------------
template <>
MaskData* AttributeConfig<MaskData*>::readValue(DecodeStream* stream) const {
  return ReadMaskID(stream);
}

template <>
void AttributeConfig<MaskData*>::writeValue(EncodeStream* stream, MaskData* const& value) const {
  WriteMaskID(stream, const_cast<MaskData*>(value));
}

// -----------------------------------------------------------------------------
// AttributeConfig<Composition*>
// -----------------------------------------------------------------------------
template <>
Composition* AttributeConfig<Composition*>::readValue(DecodeStream* stream) const {
  return ReadCompositionID(stream);
}

template <>
void AttributeConfig<Composition*>::writeValue(EncodeStream* stream,
                                               Composition* const& value) const {
  WriteCompositionID(stream, const_cast<Composition*>(value));
}

}  // namespace pag
