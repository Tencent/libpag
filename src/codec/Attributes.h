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

#pragma once

#include "AttributeHelper.h"

namespace pag {
template <typename T, template <typename TT> class K = Keyframe>
class AttributeConfigBase : public AttributeBase {
 public:
  T defaultValue;

  AttributeConfigBase(AttributeType attributeType, T defaultValue)
      : AttributeBase(attributeType), defaultValue(defaultValue) {
  }

  virtual ~AttributeConfigBase() {
  }

  virtual int dimensionality() const {
    return 1;
  }

  virtual Keyframe<T>* newKeyframe(const AttributeFlag&) const {
    return new K<T>();
  }

  void readAttribute(DecodeStream* stream, const AttributeFlag& flag, void* target) const override {
    ReadAttribute(stream, flag, target, reinterpret_cast<const AttributeConfig<T>&>(*this));
  }

  void writeAttribute(EncodeStream* flagBytes, EncodeStream* stream, void* target) const override {
    WriteAttribute(flagBytes, stream, target, reinterpret_cast<const AttributeConfig<T>&>(*this));
  }

  virtual void readValueList(DecodeStream* stream, T* list, uint32_t count) const {
    for (uint32_t i = 0; i < count; i++) {
      list[i] = readValue(stream);
    }
  }

  virtual void writeValueList(EncodeStream* stream, const T* list, uint32_t count) const {
    for (uint32_t i = 0; i < count; i++) {
      writeValue(stream, list[i]);
    }
  }

  virtual T readValue(DecodeStream* stream) const = 0;

  virtual void writeValue(EncodeStream* stream, const T& value) const = 0;
};

template <>
class AttributeConfig<float> : public AttributeConfigBase<float, SingleEaseKeyframe> {
 public:
  using AttributeConfigBase::AttributeConfigBase;

  float readValue(DecodeStream* stream) const override {
    return stream->readFloat();
  }

  void writeValue(EncodeStream* stream, const float& value) const override {
    stream->writeFloat(value);
  }
};

template <>
class AttributeConfig<bool> : public AttributeConfigBase<bool> {
 public:
  using AttributeConfigBase::AttributeConfigBase;

  bool readValue(DecodeStream* stream) const override {
    return stream->readBoolean();
  }

  void writeValue(EncodeStream* stream, const bool& value) const override {
    stream->writeBoolean(value);
  }

  void readValueList(DecodeStream* stream, bool* list, uint32_t count) const override {
    for (uint32_t i = 0; i < count; i++) {
      list[i] = stream->readBitBoolean();
    }
  }

  void writeValueList(EncodeStream* stream, const bool* list, uint32_t count) const override {
    for (uint32_t i = 0; i < count; i++) {
      stream->writeBitBoolean(list[i]);
    }
  }
};

template <>
class AttributeConfig<uint8_t> : public AttributeConfigBase<uint8_t, SingleEaseKeyframe> {
 public:
  using AttributeConfigBase::AttributeConfigBase;

  uint8_t readValue(DecodeStream* stream) const override {
    return stream->readUint8();
  }

  void writeValue(EncodeStream* stream, const uint8_t& value) const override {
    stream->writeUint8(value);
  }

  void readValueList(DecodeStream* stream, uint8_t* list, uint32_t count) const override {
    auto valueList = new uint32_t[count];
    stream->readUint32List(valueList, count);
    for (uint32_t i = 0; i < count; i++) {
      list[i] = static_cast<uint8_t>(valueList[i]);
    }
    delete[] valueList;
  }

  void writeValueList(EncodeStream* stream, const uint8_t* list, uint32_t count) const override {
    auto valueList = new uint32_t[count];
    for (uint32_t i = 0; i < count; i++) {
      valueList[i] = list[i];
    }
    stream->writeUint32List(valueList, count);
    delete[] valueList;
  }
};

template <>
class AttributeConfig<uint16_t> : public AttributeConfigBase<uint16_t, SingleEaseKeyframe> {
 public:
  using AttributeConfigBase::AttributeConfigBase;

  uint16_t readValue(DecodeStream* stream) const override {
    auto value = stream->readEncodedUint32();
    return static_cast<uint16_t>(value);
  }

  void writeValue(EncodeStream* stream, const uint16_t& value) const override {
    stream->writeEncodedUint32(static_cast<uint32_t>(value));
  }

  void readValueList(DecodeStream* stream, uint16_t* list, uint32_t count) const override {
    auto valueList = new uint32_t[count];
    stream->readUint32List(valueList, count);
    for (uint32_t i = 0; i < count; i++) {
      list[i] = static_cast<uint16_t>(valueList[i]);
    }
    delete[] valueList;
  }

  void writeValueList(EncodeStream* stream, const uint16_t* list, uint32_t count) const override {
    auto valueList = new uint32_t[count];
    for (uint32_t i = 0; i < count; i++) {
      valueList[i] = list[i];
    }
    stream->writeUint32List(valueList, count);
    delete[] valueList;
  }
};

template <>
class AttributeConfig<uint32_t> : public AttributeConfigBase<uint32_t, SingleEaseKeyframe> {
 public:
  using AttributeConfigBase::AttributeConfigBase;

  uint32_t readValue(DecodeStream* stream) const override {
    return stream->readEncodedUint32();
  }

  void writeValue(EncodeStream* stream, const uint32_t& value) const override {
    stream->writeEncodedUint32(value);
  }

  void readValueList(DecodeStream* stream, uint32_t* list, uint32_t count) const override {
    stream->readUint32List(list, count);
  }

  void writeValueList(EncodeStream* stream, const uint32_t* list, uint32_t count) const override {
    stream->writeUint32List(list, count);
  }
};

template <>
class AttributeConfig<int32_t> : public AttributeConfigBase<int32_t, SingleEaseKeyframe> {
 public:
  using AttributeConfigBase::AttributeConfigBase;

  int32_t readValue(DecodeStream* stream) const override {
    return stream->readEncodedInt32();
  }

  void writeValue(EncodeStream* stream, const int32_t& value) const override {
    stream->writeEncodedInt32(value);
  }

  void readValueList(DecodeStream* stream, int32_t* list, uint32_t count) const override {
    stream->readInt32List(list, count);
  }

  void writeValueList(EncodeStream* stream, const int32_t* list, uint32_t count) const override {
    stream->writeInt32List(list, count);
  }
};

template <>
class AttributeConfig<Frame> : public AttributeConfigBase<Frame, SingleEaseKeyframe> {
 public:
  using AttributeConfigBase::AttributeConfigBase;

  Frame readValue(DecodeStream* stream) const override {
    return ReadTime(stream);
  }

  void writeValue(EncodeStream* stream, const Frame& value) const override {
    WriteTime(stream, value);
  }
};

template <>
class AttributeConfig<Point> : public AttributeConfigBase<Point> {
 public:
  using AttributeConfigBase::AttributeConfigBase;

  int dimensionality() const override {
    return 2;
  }

  Point readValue(DecodeStream* stream) const override {
    return ReadPoint(stream);
  }

  void writeValue(EncodeStream* stream, const Point& value) const override {
    WritePoint(stream, value);
  }

  void readValueList(DecodeStream* stream, Point* list, uint32_t count) const override {
    if (attributeType == AttributeType::SpatialProperty) {
      stream->readFloatList(&(list[0].x), count * 2, SPATIAL_PRECISION);
    } else {
      for (uint32_t i = 0; i < count; i++) {
        list[i] = ReadPoint(stream);
      }
    }
  }

  void writeValueList(EncodeStream* stream, const Point* list, uint32_t count) const override {
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
class AttributeConfig<Point3D> : public AttributeConfigBase<Point3D> {
 public:
  using AttributeConfigBase::AttributeConfigBase;

  int dimensionality() const override {
    return 3;
  }

  Point3D readValue(DecodeStream* stream) const override {
    return ReadPoint3D(stream);
  }

  void writeValue(EncodeStream* stream, const Point3D& value) const override {
    WritePoint3D(stream, value);
  }

  void readValueList(DecodeStream* stream, Point3D* list, uint32_t count) const override {
    if (attributeType == AttributeType::SpatialProperty) {
      stream->readPoint3DList(list, count, SPATIAL_PRECISION);
    } else {
      for (uint32_t i = 0; i < count; i++) {
        list[i] = ReadPoint3D(stream);
      }
    }
  }

  void writeValueList(EncodeStream* stream, const Point3D* list, uint32_t count) const override {
    if (attributeType == AttributeType::SpatialProperty) {
      stream->writePoint3DList(list, count, SPATIAL_PRECISION);
    } else {
      for (uint32_t i = 0; i < count; i++) {
        WritePoint3D(stream, list[i]);
      }
    }
  }

  Keyframe<Point3D>* newKeyframe(const AttributeFlag& flag) const override {
    switch (attributeType) {
      case AttributeType::MultiDimensionProperty:
        return new MultiDimensionPoint3DKeyframe();
      case AttributeType::SpatialProperty:
        if (flag.hasSpatial) {
          return new SpatialPoint3DKeyframe();
        }
      default:
        return new SingleEaseKeyframe<Point3D>();
    }
  }
};

template <>
class AttributeConfig<Color> : public AttributeConfigBase<Color, SingleEaseKeyframe> {
 public:
  using AttributeConfigBase::AttributeConfigBase;

  int dimensionality() const override {
    return 3;
  }

  Color readValue(DecodeStream* stream) const override {
    return ReadColor(stream);
  }

  void writeValue(EncodeStream* stream, const Color& value) const override {
    WriteColor(stream, value);
  }
};

template <>
class AttributeConfig<Ratio> : public AttributeConfigBase<Ratio> {
 public:
  using AttributeConfigBase::AttributeConfigBase;

  Ratio readValue(DecodeStream* stream) const override {
    return ReadRatio(stream);
  }

  void writeValue(EncodeStream* stream, const Ratio& value) const override {
    WriteRatio(stream, value);
  }
};

template <>
class AttributeConfig<std::string> : public AttributeConfigBase<std::string> {
 public:
  using AttributeConfigBase::AttributeConfigBase;

  std::string readValue(DecodeStream* stream) const override {
    return stream->readUTF8String();
  }

  void writeValue(EncodeStream* stream, const std::string& value) const override {
    stream->writeUTF8String(value);
  }
};

template <>
class AttributeConfig<PathHandle> : public AttributeConfigBase<PathHandle, SingleEaseKeyframe> {
 public:
  using AttributeConfigBase::AttributeConfigBase;

  PathHandle readValue(DecodeStream* stream) const override {
    return ReadPath(stream);
  }

  void writeValue(EncodeStream* stream, const PathHandle& value) const override {
    WritePath(stream, value);
  }
};

template <>
class AttributeConfig<TextDocumentHandle> : public AttributeConfigBase<TextDocumentHandle> {
 public:
  using AttributeConfigBase::AttributeConfigBase;

  TextDocumentHandle readValue(DecodeStream* stream) const override {
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

  void writeValue(EncodeStream* stream, const TextDocumentHandle& value) const override {
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
};

template <>
class AttributeConfig<GradientColorHandle>
    : public AttributeConfigBase<GradientColorHandle, SingleEaseKeyframe> {
 public:
  using AttributeConfigBase::AttributeConfigBase;

  GradientColorHandle readValue(DecodeStream* stream) const override {
    return ReadGradientColor(stream);
  }

  void writeValue(EncodeStream* stream, const GradientColorHandle& value) const override {
    WriteGradientColor(stream, value);
  }
};

template <>
class AttributeConfig<Layer*> : public AttributeConfigBase<Layer*> {
 public:
  using AttributeConfigBase::AttributeConfigBase;

  Layer* readValue(DecodeStream* stream) const override {
    return ReadLayerID(stream);
  }

  void writeValue(EncodeStream* stream, Layer* const& value) const override {
    WriteLayerID(stream, const_cast<Layer*>(value));
  }
};

template <>
class AttributeConfig<MaskData*> : public AttributeConfigBase<MaskData*> {
 public:
  using AttributeConfigBase::AttributeConfigBase;

  MaskData* readValue(DecodeStream* stream) const override {
    return ReadMaskID(stream);
  }

  void writeValue(EncodeStream* stream, MaskData* const& value) const override {
    WriteMaskID(stream, const_cast<MaskData*>(value));
  }
};

template <>
class AttributeConfig<Composition*> : public AttributeConfigBase<Composition*> {
 public:
  using AttributeConfigBase::AttributeConfigBase;

  Composition* readValue(DecodeStream* stream) const override {
    return ReadCompositionID(stream);
  }

  void writeValue(EncodeStream* stream, Composition* const& value) const override {
    WriteCompositionID(stream, const_cast<Composition*>(value));
  }
};

}  // namespace pag
