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

#pragma once

#include "AttributeHelper.h"

namespace pag {
template <typename T>
class AttributeConfigBase {
 public:
  virtual ~AttributeConfigBase() {
  }

  virtual int dimensionality() const {
    return 1;
  }

  virtual Keyframe<T>* newKeyframe(const AttributeFlag&) const {
    return new Keyframe<T>();
  }
};

template <>
class AttributeConfig<float> : public AttributeBase, public AttributeConfigBase<float> {
 public:
  float defaultValue;

  AttributeConfig(AttributeType attributeType, float defaultValue)
      : AttributeBase(attributeType), defaultValue(defaultValue) {
  }

  void readAttribute(DecodeStream* stream, const AttributeFlag& flag, void* target) const override {
    ReadAttribute(stream, flag, target, *this);
  }

  void writeAttribute(EncodeStream* flagBytes, EncodeStream* stream, void* target) const override {
    WriteAttribute(flagBytes, stream, target, *this);
  }

  float readValue(DecodeStream* stream) const {
    return stream->readFloat();
  }

  void writeValue(EncodeStream* stream, const float& value) const {
    stream->writeFloat(value);
  }

  void readValueList(DecodeStream* stream, float* list, uint32_t count) const {
    for (uint32_t i = 0; i < count; i++) {
      list[i] = readValue(stream);
    }
  }

  void writeValueList(EncodeStream* stream, const float* list, uint32_t count) const {
    for (uint32_t i = 0; i < count; i++) {
      writeValue(stream, list[i]);
    }
  }

  Keyframe<float>* newKeyframe(const AttributeFlag&) const override {
    return new SingleEaseKeyframe<float>();
  }
};

template <>
class AttributeConfig<bool> : public AttributeBase, public AttributeConfigBase<bool> {
 public:
  bool defaultValue;

  AttributeConfig(AttributeType attributeType, bool defaultValue)
      : AttributeBase(attributeType), defaultValue(defaultValue) {
  }

  void readAttribute(DecodeStream* stream, const AttributeFlag& flag, void* target) const override {
    ReadAttribute(stream, flag, target, *this);
  }

  void writeAttribute(EncodeStream* flagBytes, EncodeStream* stream, void* target) const override {
    WriteAttribute(flagBytes, stream, target, *this);
  }

  bool readValue(DecodeStream* stream) const {
    return stream->readBoolean();
  }

  void writeValue(EncodeStream* stream, const bool& value) const {
    stream->writeBoolean(value);
  }

  void readValueList(DecodeStream* stream, bool* list, uint32_t count) const {
    for (uint32_t i = 0; i < count; i++) {
      list[i] = stream->readBitBoolean();
    }
  }

  void writeValueList(EncodeStream* stream, const bool* list, uint32_t count) const {
    for (uint32_t i = 0; i < count; i++) {
      stream->writeBitBoolean(list[i]);
    }
  }
};

template <>
class AttributeConfig<uint8_t> : public AttributeBase, public AttributeConfigBase<uint8_t> {
 public:
  uint8_t defaultValue;

  AttributeConfig(AttributeType attributeType, uint8_t defaultValue)
      : AttributeBase(attributeType), defaultValue(defaultValue) {
  }

  void readAttribute(DecodeStream* stream, const AttributeFlag& flag, void* target) const override {
    ReadAttribute(stream, flag, target, *this);
  }

  void writeAttribute(EncodeStream* flagBytes, EncodeStream* stream, void* target) const override {
    WriteAttribute(flagBytes, stream, target, *this);
  }

  uint8_t readValue(DecodeStream* stream) const {
    return stream->readUint8();
  }

  void writeValue(EncodeStream* stream, const uint8_t& value) const {
    stream->writeUint8(value);
  }

  void readValueList(DecodeStream* stream, uint8_t* list, uint32_t count) const {
    auto valueList = new uint32_t[count];
    stream->readUint32List(valueList, count);
    for (uint32_t i = 0; i < count; i++) {
      list[i] = static_cast<uint8_t>(valueList[i]);
    }
    delete[] valueList;
  }

  void writeValueList(EncodeStream* stream, const uint8_t* list, uint32_t count) const {
    auto valueList = new uint32_t[count];
    for (uint32_t i = 0; i < count; i++) {
      valueList[i] = list[i];
    }
    stream->writeUint32List(valueList, count);
    delete[] valueList;
  }

  Keyframe<uint8_t>* newKeyframe(const AttributeFlag&) const override {
    return new SingleEaseKeyframe<uint8_t>();
  }
};

template <>
class AttributeConfig<uint16_t> : public AttributeBase, public AttributeConfigBase<uint16_t> {
 public:
  uint16_t defaultValue;

  AttributeConfig(AttributeType attributeType, uint16_t defaultValue)
      : AttributeBase(attributeType), defaultValue(defaultValue) {
  }

  void readAttribute(DecodeStream* stream, const AttributeFlag& flag, void* target) const override {
    ReadAttribute(stream, flag, target, *this);
  }

  void writeAttribute(EncodeStream* flagBytes, EncodeStream* stream, void* target) const override {
    WriteAttribute(flagBytes, stream, target, *this);
  }

  uint16_t readValue(DecodeStream* stream) const {
    auto value = stream->readEncodedUint32();
    return static_cast<uint16_t>(value);
  }

  void writeValue(EncodeStream* stream, const uint16_t& value) const {
    stream->writeEncodedUint32(static_cast<uint32_t>(value));
  }

  void readValueList(DecodeStream* stream, uint16_t* list, uint32_t count) const {
    auto valueList = new uint32_t[count];
    stream->readUint32List(valueList, count);
    for (uint32_t i = 0; i < count; i++) {
      list[i] = static_cast<uint16_t>(valueList[i]);
    }
    delete[] valueList;
  }

  void writeValueList(EncodeStream* stream, const uint16_t* list, uint32_t count) const {
    auto valueList = new uint32_t[count];
    for (uint32_t i = 0; i < count; i++) {
      valueList[i] = list[i];
    }
    stream->writeUint32List(valueList, count);
    delete[] valueList;
  }

  Keyframe<uint16_t>* newKeyframe(const AttributeFlag&) const override {
    return new SingleEaseKeyframe<uint16_t>();
  }
};

template <>
class AttributeConfig<uint32_t> : public AttributeBase, public AttributeConfigBase<uint32_t> {
 public:
  uint32_t defaultValue;

  AttributeConfig(AttributeType attributeType, uint32_t defaultValue)
      : AttributeBase(attributeType), defaultValue(defaultValue) {
  }

  void readAttribute(DecodeStream* stream, const AttributeFlag& flag, void* target) const override {
    ReadAttribute(stream, flag, target, *this);
  }

  void writeAttribute(EncodeStream* flagBytes, EncodeStream* stream, void* target) const override {
    WriteAttribute(flagBytes, stream, target, *this);
  }

  uint32_t readValue(DecodeStream* stream) const {
    return stream->readEncodedUint32();
  }

  void writeValue(EncodeStream* stream, const uint32_t& value) const {
    stream->writeEncodedUint32(value);
  }

  void readValueList(DecodeStream* stream, uint32_t* list, uint32_t count) const {
    stream->readUint32List(list, count);
  }

  void writeValueList(EncodeStream* stream, const uint32_t* list, uint32_t count) const {
    stream->writeUint32List(list, count);
  }

  Keyframe<uint32_t>* newKeyframe(const AttributeFlag&) const override {
    return new SingleEaseKeyframe<uint32_t>();
  }
};

template <>
class AttributeConfig<int32_t> : public AttributeBase, public AttributeConfigBase<int32_t> {
 public:
  int32_t defaultValue;

  AttributeConfig(AttributeType attributeType, int32_t defaultValue)
      : AttributeBase(attributeType), defaultValue(defaultValue) {
  }

  void readAttribute(DecodeStream* stream, const AttributeFlag& flag, void* target) const override {
    ReadAttribute(stream, flag, target, *this);
  }

  void writeAttribute(EncodeStream* flagBytes, EncodeStream* stream, void* target) const override {
    WriteAttribute(flagBytes, stream, target, *this);
  }

  int32_t readValue(DecodeStream* stream) const {
    return stream->readEncodedInt32();
  }

  void writeValue(EncodeStream* stream, const int32_t& value) const {
    stream->writeEncodedInt32(value);
  }

  void readValueList(DecodeStream* stream, int32_t* list, uint32_t count) const {
    stream->readInt32List(list, count);
  }

  void writeValueList(EncodeStream* stream, const int32_t* list, uint32_t count) const {
    stream->writeInt32List(list, count);
  }

  Keyframe<int32_t>* newKeyframe(const AttributeFlag&) const override {
    return new SingleEaseKeyframe<int32_t>();
  }
};

template <>
class AttributeConfig<Frame> : public AttributeBase, public AttributeConfigBase<Frame> {
 public:
  Frame defaultValue;

  AttributeConfig(AttributeType attributeType, Frame defaultValue)
      : AttributeBase(attributeType), defaultValue(defaultValue) {
  }

  void readAttribute(DecodeStream* stream, const AttributeFlag& flag, void* target) const override {
    ReadAttribute(stream, flag, target, *this);
  }

  void writeAttribute(EncodeStream* flagBytes, EncodeStream* stream, void* target) const override {
    WriteAttribute(flagBytes, stream, target, *this);
  }

  Frame readValue(DecodeStream* stream) const {
    return ReadTime(stream);
  }

  void writeValue(EncodeStream* stream, const Frame& value) const {
    WriteTime(stream, value);
  }

  void readValueList(DecodeStream* stream, Frame* list, uint32_t count) const {
    for (uint32_t i = 0; i < count; i++) {
      list[i] = readValue(stream);
    }
  }

  void writeValueList(EncodeStream* stream, const Frame* list, uint32_t count) const {
    for (uint32_t i = 0; i < count; i++) {
      writeValue(stream, list[i]);
    }
  }

  Keyframe<Frame>* newKeyframe(const AttributeFlag&) const override {
    return new SingleEaseKeyframe<Frame>();
  }
};

template <>
class AttributeConfig<Point> : public AttributeBase, public AttributeConfigBase<Point> {
 public:
  Point defaultValue;

  AttributeConfig(AttributeType attributeType, Point defaultValue)
      : AttributeBase(attributeType), defaultValue(defaultValue) {
  }

  void readAttribute(DecodeStream* stream, const AttributeFlag& flag, void* target) const override {
    ReadAttribute(stream, flag, target, *this);
  }

  void writeAttribute(EncodeStream* flagBytes, EncodeStream* stream, void* target) const override {
    WriteAttribute(flagBytes, stream, target, *this);
  }

  int dimensionality() const override {
    return 2;
  }

  Point readValue(DecodeStream* stream) const {
    return ReadPoint(stream);
  }

  void writeValue(EncodeStream* stream, const Point& value) const {
    WritePoint(stream, value);
  }

  void readValueList(DecodeStream* stream, Point* list, uint32_t count) const {
    if (attributeType == AttributeType::SpatialProperty) {
      stream->readFloatList(&(list[0].x), count * 2, SPATIAL_PRECISION);
    } else {
      for (uint32_t i = 0; i < count; i++) {
        list[i] = ReadPoint(stream);
      }
    }
  }

  void writeValueList(EncodeStream* stream, const Point* list, uint32_t count) const {
    if (attributeType == AttributeType::SpatialProperty) {
      stream->writeFloatList(&(list[0].x), count * 2, SPATIAL_PRECISION);
    } else {
      for (uint32_t i = 0; i < count; i++) {
        WritePoint(stream, list[i]);
      }
    }
  }

  Keyframe<Point>* newKeyframe(const AttributeFlag& flag) const override {
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
};

template <>
class AttributeConfig<Color> : public AttributeBase, public AttributeConfigBase<Color> {
 public:
  Color defaultValue;

  AttributeConfig(AttributeType attributeType, Color defaultValue)
      : AttributeBase(attributeType), defaultValue(defaultValue) {
  }

  void readAttribute(DecodeStream* stream, const AttributeFlag& flag, void* target) const override {
    ReadAttribute(stream, flag, target, *this);
  }

  void writeAttribute(EncodeStream* flagBytes, EncodeStream* stream, void* target) const override {
    WriteAttribute(flagBytes, stream, target, *this);
  }

  int dimensionality() const override {
    return 3;
  }

  Color readValue(DecodeStream* stream) const {
    return ReadColor(stream);
  }

  void writeValue(EncodeStream* stream, const Color& value) const {
    WriteColor(stream, value);
  }

  void readValueList(DecodeStream* stream, Color* list, uint32_t count) const {
    for (uint32_t i = 0; i < count; i++) {
      list[i] = readValue(stream);
    }
  }

  void writeValueList(EncodeStream* stream, const Color* list, uint32_t count) const {
    for (uint32_t i = 0; i < count; i++) {
      writeValue(stream, list[i]);
    }
  }

  Keyframe<Color>* newKeyframe(const AttributeFlag&) const override {
    return new SingleEaseKeyframe<Color>();
  }
};

template <>
class AttributeConfig<Ratio> : public AttributeBase, public AttributeConfigBase<Ratio> {
 public:
  Ratio defaultValue;

  AttributeConfig(AttributeType attributeType, Ratio defaultValue)
      : AttributeBase(attributeType), defaultValue(defaultValue) {
  }

  void readAttribute(DecodeStream* stream, const AttributeFlag& flag, void* target) const override {
    ReadAttribute(stream, flag, target, *this);
  }

  void writeAttribute(EncodeStream* flagBytes, EncodeStream* stream, void* target) const override {
    WriteAttribute(flagBytes, stream, target, *this);
  }

  Ratio readValue(DecodeStream* stream) const {
    return ReadRatio(stream);
  }

  void writeValue(EncodeStream* stream, const Ratio& value) const {
    WriteRatio(stream, value);
  }

  void readValueList(DecodeStream* stream, Ratio* list, uint32_t count) const {
    for (uint32_t i = 0; i < count; i++) {
      list[i] = readValue(stream);
    }
  }

  void writeValueList(EncodeStream* stream, const Ratio* list, uint32_t count) const {
    for (uint32_t i = 0; i < count; i++) {
      writeValue(stream, list[i]);
    }
  }
};

template <>
class AttributeConfig<std::string> : public AttributeBase, public AttributeConfigBase<std::string> {
 public:
  std::string defaultValue;

  AttributeConfig(AttributeType attributeType, std::string defaultValue)
      : AttributeBase(attributeType), defaultValue(defaultValue) {
  }

  void readAttribute(DecodeStream* stream, const AttributeFlag& flag, void* target) const override {
    ReadAttribute(stream, flag, target, *this);
  }

  void writeAttribute(EncodeStream* flagBytes, EncodeStream* stream, void* target) const override {
    WriteAttribute(flagBytes, stream, target, *this);
  }

  std::string readValue(DecodeStream* stream) const {
    return stream->readUTF8String();
  }

  void writeValue(EncodeStream* stream, const std::string& value) const {
    stream->writeUTF8String(value);
  }

  void readValueList(DecodeStream* stream, std::string* list, uint32_t count) const {
    for (uint32_t i = 0; i < count; i++) {
      list[i] = readValue(stream);
    }
  }

  void writeValueList(EncodeStream* stream, const std::string* list, uint32_t count) const {
    for (uint32_t i = 0; i < count; i++) {
      writeValue(stream, list[i]);
    }
  }

  Keyframe<std::string>* newKeyframe(const AttributeFlag&) const override {
    return new Keyframe<std::string>();
  }
};

template <>
class AttributeConfig<PathHandle> : public AttributeBase, public AttributeConfigBase<PathHandle> {
 public:
  PathHandle defaultValue;

  AttributeConfig(AttributeType attributeType, PathHandle defaultValue)
      : AttributeBase(attributeType), defaultValue(defaultValue) {
  }

  void readAttribute(DecodeStream* stream, const AttributeFlag& flag, void* target) const override {
    ReadAttribute(stream, flag, target, *this);
  }

  void writeAttribute(EncodeStream* flagBytes, EncodeStream* stream, void* target) const override {
    WriteAttribute(flagBytes, stream, target, *this);
  }

  PathHandle readValue(DecodeStream* stream) const {
    return ReadPath(stream);
  }

  void writeValue(EncodeStream* stream, const PathHandle& value) const {
    WritePath(stream, value);
  }

  void readValueList(DecodeStream* stream, PathHandle* list, uint32_t count) const {
    for (uint32_t i = 0; i < count; i++) {
      list[i] = readValue(stream);
    }
  }

  void writeValueList(EncodeStream* stream, const PathHandle* list, uint32_t count) const {
    for (uint32_t i = 0; i < count; i++) {
      writeValue(stream, list[i]);
    }
  }

  Keyframe<PathHandle>* newKeyframe(const AttributeFlag&) const override {
    return new SingleEaseKeyframe<PathHandle>();
  }
};

template <>
class AttributeConfig<TextDocumentHandle> : public AttributeBase,
                                            public AttributeConfigBase<TextDocumentHandle> {
 public:
  TextDocumentHandle defaultValue;

  AttributeConfig(AttributeType attributeType, TextDocumentHandle defaultValue)
      : AttributeBase(attributeType), defaultValue(defaultValue) {
  }

  void readAttribute(DecodeStream* stream, const AttributeFlag& flag, void* target) const override {
    ReadAttribute(stream, flag, target, *this);
  }

  void writeAttribute(EncodeStream* flagBytes, EncodeStream* stream, void* target) const override {
    WriteAttribute(flagBytes, stream, target, *this);
  }

  TextDocumentHandle readValue(DecodeStream* stream) const {
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

  void writeValue(EncodeStream* stream, const TextDocumentHandle& value) const {
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

  void readValueList(DecodeStream* stream, TextDocumentHandle* list, uint32_t count) const {
    for (uint32_t i = 0; i < count; i++) {
      list[i] = readValue(stream);
    }
  }

  void writeValueList(EncodeStream* stream, const TextDocumentHandle* list, uint32_t count) const {
    for (uint32_t i = 0; i < count; i++) {
      writeValue(stream, list[i]);
    }
  }
};

template <>
class AttributeConfig<GradientColorHandle> : public AttributeBase,
                                             public AttributeConfigBase<GradientColorHandle> {
 public:
  GradientColorHandle defaultValue;

  AttributeConfig(AttributeType attributeType, GradientColorHandle defaultValue)
      : AttributeBase(attributeType), defaultValue(defaultValue) {
  }

  void readAttribute(DecodeStream* stream, const AttributeFlag& flag, void* target) const override {
    ReadAttribute(stream, flag, target, *this);
  }

  void writeAttribute(EncodeStream* flagBytes, EncodeStream* stream, void* target) const override {
    WriteAttribute(flagBytes, stream, target, *this);
  }

  GradientColorHandle readValue(DecodeStream* stream) const {
    return ReadGradientColor(stream);
  }

  void writeValue(EncodeStream* stream, const GradientColorHandle& value) const {
    WriteGradientColor(stream, value);
  }

  void readValueList(DecodeStream* stream, GradientColorHandle* list, uint32_t count) const {
    for (uint32_t i = 0; i < count; i++) {
      list[i] = readValue(stream);
    }
  }

  void writeValueList(EncodeStream* stream, const GradientColorHandle* list, uint32_t count) const {
    for (uint32_t i = 0; i < count; i++) {
      writeValue(stream, list[i]);
    }
  }

  Keyframe<GradientColorHandle>* newKeyframe(const AttributeFlag&) const override {
    return new SingleEaseKeyframe<GradientColorHandle>();
  }
};

template <>
class AttributeConfig<Layer*> : public AttributeBase, public AttributeConfigBase<Layer*> {
 public:
  Layer* defaultValue;

  AttributeConfig(AttributeType attributeType, Layer* defaultValue)
      : AttributeBase(attributeType), defaultValue(defaultValue) {
  }

  void readAttribute(DecodeStream* stream, const AttributeFlag& flag, void* target) const override {
    ReadAttribute(stream, flag, target, *this);
  }

  void writeAttribute(EncodeStream* flagBytes, EncodeStream* stream, void* target) const override {
    WriteAttribute(flagBytes, stream, target, *this);
  }

  Layer* readValue(DecodeStream* stream) const {
    return ReadLayerID(stream);
  }

  void writeValue(EncodeStream* stream, const Layer* value) const {
    WriteLayerID(stream, const_cast<Layer*>(value));
  }

  void readValueList(DecodeStream* stream, Layer** list, uint32_t count) const {
    for (uint32_t i = 0; i < count; i++) {
      list[i] = readValue(stream);
    }
  }

  void writeValueList(EncodeStream* stream, Layer** list, uint32_t count) const {
    for (uint32_t i = 0; i < count; i++) {
      writeValue(stream, list[i]);
    }
  }
};

template <>
class AttributeConfig<MaskData*> : public AttributeBase, public AttributeConfigBase<MaskData*> {
 public:
  MaskData* defaultValue;

  AttributeConfig(AttributeType attributeType, MaskData* defaultValue)
      : AttributeBase(attributeType), defaultValue(defaultValue) {
  }

  void readAttribute(DecodeStream* stream, const AttributeFlag& flag, void* target) const override {
    ReadAttribute(stream, flag, target, *this);
  }

  void writeAttribute(EncodeStream* flagBytes, EncodeStream* stream, void* target) const override {
    WriteAttribute(flagBytes, stream, target, *this);
  }

  MaskData* readValue(DecodeStream* stream) const {
    return ReadMaskID(stream);
  }

  void writeValue(EncodeStream* stream, const MaskData* value) const {
    WriteMaskID(stream, const_cast<MaskData*>(value));
  }

  void readValueList(DecodeStream* stream, MaskData** list, uint32_t count) const {
    for (uint32_t i = 0; i < count; i++) {
      list[i] = readValue(stream);
    }
  }

  void writeValueList(EncodeStream* stream, MaskData** list, uint32_t count) const {
    for (uint32_t i = 0; i < count; i++) {
      writeValue(stream, list[i]);
    }
  }
};

template <>
class AttributeConfig<Composition*> : public AttributeBase,
                                      public AttributeConfigBase<Composition*> {
 public:
  Composition* defaultValue;

  AttributeConfig(AttributeType attributeType, Composition* defaultValue)
      : AttributeBase(attributeType), defaultValue(defaultValue) {
  }

  void readAttribute(DecodeStream* stream, const AttributeFlag& flag, void* target) const override {
    ReadAttribute(stream, flag, target, *this);
  }

  void writeAttribute(EncodeStream* flagBytes, EncodeStream* stream, void* target) const override {
    WriteAttribute(flagBytes, stream, target, *this);
  }

  Composition* readValue(DecodeStream* stream) const {
    return ReadCompositionID(stream);
  }

  void writeValue(EncodeStream* stream, const Composition* value) const {
    WriteCompositionID(stream, const_cast<Composition*>(value));
  }

  void readValueList(DecodeStream* stream, Composition** list, uint32_t count) const {
    for (uint32_t i = 0; i < count; i++) {
      list[i] = readValue(stream);
    }
  }

  void writeValueList(EncodeStream* stream, Composition** list, uint32_t count) const {
    for (uint32_t i = 0; i < count; i++) {
      writeValue(stream, list[i]);
    }
  }
};

}  // namespace pag
