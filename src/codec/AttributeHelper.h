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

#include "DataTypes.h"
#include "base/Keyframes.h"

namespace pag {
static constexpr int MAX_KEYFRAMES = 5184000;  // 60 frames per second, 24 hours

enum class AttributeType {
  Value,
  FixedValue,  // always exists, no need to store a flag.
  SimpleProperty,
  DiscreteProperty,
  MultiDimensionProperty,
  SpatialProperty,
  BitFlag,  // save bool value as a flag
  Custom    // save a flag to indicate whether it should trigger a custom reading/writing action.
};

struct AttributeFlag {
  /**
   * Indicates whether or not this value is exist.
   */
  bool exist = false;
  /**
   * Indicates whether or not the size of this property's keyframes is greater than zero.
   */
  bool animatable = false;
  /**
   * Indicates whether or not this property has spatial values.
   */
  bool hasSpatial = false;
};

class AttributeBase {
 public:
  AttributeType attributeType;

  explicit AttributeBase(AttributeType attributeType) : attributeType(attributeType) {
  }

  virtual ~AttributeBase() = default;

  virtual void readAttribute(DecodeStream* stream, const AttributeFlag& flag,
                             void* target) const = 0;
  virtual void writeAttribute(EncodeStream* flagBytes, EncodeStream* stream,
                              void* target) const = 0;
};

class CustomAttribute : public AttributeBase {
 public:
  CustomAttribute(const std::function<void(DecodeStream*, void*)> reader,
                  const std::function<bool(EncodeStream*, void*)> writer)
      : AttributeBase(AttributeType::Custom), reader(reader), writer(writer) {
  }

  const std::function<void(DecodeStream*, void*)> reader;
  const std::function<bool(EncodeStream*, void*)> writer;

  void readAttribute(DecodeStream* stream, const AttributeFlag& flag, void* target) const override {
    if (flag.exist) {
      reader(stream, target);
    }
  }

  void writeAttribute(EncodeStream* flagBytes, EncodeStream* stream, void* target) const override {
    auto flag = writer(stream, target);
    flagBytes->writeBitBoolean(flag);
  }
};

template <typename T>
class AttributeConfig;

class BlockConfig {
 public:
  BlockConfig() : tagCode(TagCode::End) {
  }

  explicit BlockConfig(TagCode tagCode) : tagCode(tagCode) {
  }

  ~BlockConfig() {
    for (auto& config : configs) {
      delete config;
    }
  }

  TagCode tagCode;
  std::vector<void*> targets = {};
  std::vector<AttributeBase*> configs = {};
};

template <typename T>
void AddAttribute(BlockConfig* blockConfig, void* target, AttributeType attributeType,
                  T defaultValue) {
  blockConfig->targets.push_back(target);
  blockConfig->configs.push_back(new AttributeConfig<T>(attributeType, defaultValue));
}

void AddCustomAttribute(BlockConfig* blockConfig, void* target,
                        const std::function<void(DecodeStream*, void*)> reader,
                        const std::function<bool(EncodeStream*, void*)> writer);

AttributeFlag ReadAttributeFlag(DecodeStream* stream, const AttributeBase* config);

template <typename T>
T ReadValue(DecodeStream* stream, const AttributeConfig<T>& config, const AttributeFlag& flag) {
  if (flag.exist) {
    return config.readValue(stream);
  }
  return config.defaultValue;
}

template <typename T>
std::vector<Keyframe<T>*> ReadKeyframes(DecodeStream* stream, const AttributeConfig<T>& config,
                                        const AttributeFlag& flag) {
  std::vector<Keyframe<T>*> keyframes;
  auto numFrames = stream->readEncodedUint32();
  if (numFrames > MAX_KEYFRAMES) {
    PAGThrowError(stream->context, "number of keyframes is too large");
    return keyframes;
  }
  for (uint32_t i = 0; i < numFrames; i++) {
    Keyframe<T>* keyframe;
    if (config.attributeType == AttributeType::DiscreteProperty) {
      // There is no need to read any bits here.
      keyframe = new Keyframe<T>();
    } else {
      auto interpolationType = static_cast<KeyframeInterpolationType>(stream->readUBits(2));
      if (interpolationType == KeyframeInterpolationType::Hold) {
        keyframe = new Keyframe<T>();
      } else {
        keyframe = config.newKeyframe(flag);
        keyframe->interpolationType = interpolationType;
      }
    }
    keyframes.push_back(keyframe);
  }
  return keyframes;
}

template <typename T>
void ReadTimeAndValue(DecodeStream* stream, const std::vector<Keyframe<T>*>& keyframes,
                      const AttributeConfig<T>& config) {
  auto numFrames = static_cast<uint32_t>(keyframes.size());
  keyframes[0]->startTime = ReadTime(stream);
  for (uint32_t i = 0; i < numFrames; i++) {
    auto time = ReadTime(stream);
    keyframes[i]->endTime = time;
    if (i < numFrames - 1) {
      keyframes[i + 1]->startTime = time;
    }
  }
  auto list = new T[numFrames + 1];
  config.readValueList(stream, list, numFrames + 1);
  int index = 0;
  keyframes[0]->startValue = list[index++];
  for (uint32_t i = 0; i < numFrames; i++) {
    auto value = list[index++];
    keyframes[i]->endValue = value;
    if (i < numFrames - 1) {
      keyframes[i + 1]->startValue = value;
    }
  }
  delete[] list;
}

template <typename T>
void ReadTimeEase(DecodeStream* stream, const std::vector<Keyframe<T>*>& keyframes,
                  const AttributeConfig<T>& config) {
  int dimensionality =
      config.attributeType == AttributeType::MultiDimensionProperty ? config.dimensionality() : 1;
  auto numBits = stream->readNumBits();
  for (auto& keyframe : keyframes) {
    if (keyframe->interpolationType != KeyframeInterpolationType::Bezier) {
      continue;
    }
    float x, y;
    for (int i = 0; i < dimensionality; i++) {
      x = stream->readBits(numBits) * BEZIER_PRECISION;
      y = stream->readBits(numBits) * BEZIER_PRECISION;
      keyframe->bezierOut.push_back({x, y});
      x = stream->readBits(numBits) * BEZIER_PRECISION;
      y = stream->readBits(numBits) * BEZIER_PRECISION;
      keyframe->bezierIn.push_back({x, y});
    }
  }
}

template <typename T>
class SpatialOperater {
 public:
  static float ReadZ(pag::DecodeStream*, unsigned char) {
    return 0.0f;
  }

  static void WriteZ(std::vector<float>&, float) {
  }
};

template <>
class SpatialOperater<Point3D> {
 public:
  static float ReadZ(pag::DecodeStream* stream, unsigned char numBits) {
    return stream->readBits(numBits) * SPATIAL_PRECISION;
  }

  static void WriteZ(std::vector<float>& spatialList, float z) {
    spatialList.push_back(z);
  }
};

template <typename T>
void ReadSpatialEase(DecodeStream* stream, const std::vector<Keyframe<T>*>& keyframes) {
  auto spatialFlagList = new bool[keyframes.size() * 2];
  auto count = keyframes.size() * 2;
  for (size_t i = 0; i < count; i++) {
    spatialFlagList[i] = stream->readBitBoolean();
  }
  auto numBits = stream->readNumBits();
  int index = 0;
  for (auto& keyframe : keyframes) {
    auto hasSpatialIn = spatialFlagList[index++];
    auto hasSpatialOut = spatialFlagList[index++];
    if (hasSpatialIn || hasSpatialOut) {
      if (hasSpatialIn) {
        keyframe->spatialIn.x = stream->readBits(numBits) * SPATIAL_PRECISION;
        keyframe->spatialIn.y = stream->readBits(numBits) * SPATIAL_PRECISION;
        keyframe->spatialIn.z = SpatialOperater<T>::ReadZ(stream, numBits);
      }
      if (hasSpatialOut) {
        keyframe->spatialOut.x = stream->readBits(numBits) * SPATIAL_PRECISION;
        keyframe->spatialOut.y = stream->readBits(numBits) * SPATIAL_PRECISION;
        keyframe->spatialOut.z = SpatialOperater<T>::ReadZ(stream, numBits);
      }
    }
  }
  delete[] spatialFlagList;
}

template <typename T>
Property<T>* ReadProperty(DecodeStream* stream, const AttributeConfig<T>& config,
                          const AttributeFlag& flag) {
  Property<T>* property = nullptr;
  if (flag.exist) {
    if (flag.animatable) {
      auto keyframes = ReadKeyframes(stream, config, flag);
      if (keyframes.empty()) {
        PAGThrowError(stream->context, "Wrong number of keyframes.");
        return property;
      }
      ReadTimeAndValue(stream, keyframes, config);
      ReadTimeEase(stream, keyframes, config);
      if (flag.hasSpatial) {
        ReadSpatialEase(stream, keyframes);
      }
      property = new AnimatableProperty<T>(keyframes);
    } else {
      property = new Property<T>();
      property->value = ReadValue(stream, config, flag);
    }
  } else {
    property = new Property<T>();
    property->value = config.defaultValue;
  }
  return property;
}

template <typename T>
void ReadAttribute(DecodeStream* stream, const AttributeFlag& flag, void* target,
                   const AttributeConfig<T>& config) {
  if (config.attributeType == AttributeType::BitFlag) {
    auto valueTarget = reinterpret_cast<bool*>(target);
    *valueTarget = flag.exist;
  } else if (config.attributeType == AttributeType::FixedValue) {
    auto valueTarget = reinterpret_cast<T*>(target);
    *valueTarget = config.readValue(stream);
  } else if (config.attributeType == AttributeType::Value) {
    auto valueTarget = reinterpret_cast<T*>(target);
    *valueTarget = ReadValue(stream, config, flag);
  } else {
    auto propertyTarget = reinterpret_cast<Property<T>**>(target);
    *propertyTarget = ReadProperty(stream, config, flag);
  }
}

template <class T>
bool ReadTagBlock(DecodeStream* stream, T* parameter,
                  std::unique_ptr<BlockConfig> (*ConfigMaker)(T*)) {
  auto tagConfig = ConfigMaker(parameter);

  std::vector<AttributeFlag> flags;
  for (auto& config : tagConfig->configs) {
    auto flag = ReadAttributeFlag(stream, config);
    flags.push_back(flag);
  }
  stream->alignWithBytes();
  int index = 0;
  for (auto& config : tagConfig->configs) {
    auto flag = flags[index];
    auto target = tagConfig->targets[index];
    config->readAttribute(stream, flag, target);
    index++;
  }
  return !stream->context->hasException();
}

template <class T>
T* ReadTagBlock(DecodeStream* stream, std::unique_ptr<BlockConfig> (*ConfigMaker)(T*)) {
  auto parameter = std::make_unique<T>();
  if (!ReadTagBlock(stream, parameter.get(), ConfigMaker)) {
    return nullptr;
  }
  return parameter.release();
}

template <class T>
bool ReadBlock(DecodeStream* stream, T* parameter,
               std::unique_ptr<BlockConfig> (*ConfigMaker)(T*)) {
  stream->alignWithBytes();
  return ReadTagBlock(stream, parameter, ConfigMaker);
}

void WriteAttributeFlag(EncodeStream* stream, const AttributeFlag& flag,
                        const AttributeBase* config);

template <typename T>
AttributeFlag WriteValue(EncodeStream* stream, const AttributeConfig<T>& config, const T& value) {
  AttributeFlag flag = {};
  if (value == config.defaultValue) {
    flag.exist = false;
  } else {
    flag.exist = true;
    config.writeValue(stream, value);
  }
  return flag;
}

template <typename T>
void WriteKeyframes(EncodeStream* stream, const std::vector<Keyframe<T>*>& keyframes,
                    const AttributeConfig<T>& config) {
  auto numFrames = keyframes.size();
  stream->writeEncodedUint32(static_cast<uint32_t>(numFrames));
  if (config.attributeType == AttributeType::DiscreteProperty) {
    return;
  }
  for (auto& keyframe : keyframes) {
    stream->writeUBits(static_cast<uint32_t>(keyframe->interpolationType), 2);
  }
}

template <typename T>
void WriteTimeAndValue(EncodeStream* stream, const std::vector<Keyframe<T>*>& keyframes,
                       const AttributeConfig<T>& config) {
  WriteTime(stream, keyframes[0]->startTime);
  for (auto& keyframe : keyframes) {
    WriteTime(stream, keyframe->endTime);
  }
  auto list = new T[keyframes.size() + 1];
  int index = 0;
  list[index++] = keyframes[0]->startValue;
  for (auto& keyframe : keyframes) {
    list[index++] = keyframe->endValue;
  }
  config.writeValueList(stream, list, static_cast<uint32_t>(keyframes.size()) + 1);
  delete[] list;
}

template <typename T>
void WriteTimeEase(EncodeStream* stream, const std::vector<Keyframe<T>*>& keyframes,
                   const AttributeConfig<T>& config) {
  int dimensionality =
      config.attributeType == AttributeType::MultiDimensionProperty ? config.dimensionality() : 1;
  std::vector<float> bezierList;
  for (auto& keyframe : keyframes) {
    if (keyframe->interpolationType != KeyframeInterpolationType::Bezier) {
      continue;
    }
    for (int j = 0; j < dimensionality; j++) {
      auto& bezierOut = keyframe->bezierOut[j];
      bezierList.push_back(bezierOut.x);
      bezierList.push_back(bezierOut.y);
      auto& bezierIn = keyframe->bezierIn[j];
      bezierList.push_back(bezierIn.x);
      bezierList.push_back(bezierIn.y);
    }
  }
  auto count = static_cast<uint32_t>(bezierList.size());
  stream->writeFloatList(bezierList.data(), count, BEZIER_PRECISION);
}

template <typename T>
void WriteSpatialEase(EncodeStream* stream, const std::vector<Keyframe<T>*>& keyframes) {
  std::vector<float> spatialList;
  for (auto& keyframe : keyframes) {
    stream->writeBitBoolean(keyframe->spatialIn != Point3D::Zero());
    stream->writeBitBoolean(keyframe->spatialOut != Point3D::Zero());
    if (keyframe->spatialIn != Point3D::Zero()) {
      spatialList.push_back(keyframe->spatialIn.x);
      spatialList.push_back(keyframe->spatialIn.y);
      SpatialOperater<T>::WriteZ(spatialList, keyframe->spatialIn.z);
    }
    if (keyframe->spatialOut != Point3D::Zero()) {
      spatialList.push_back(keyframe->spatialOut.x);
      spatialList.push_back(keyframe->spatialOut.y);
      SpatialOperater<T>::WriteZ(spatialList, keyframe->spatialOut.z);
    }
  }
  auto count = static_cast<uint32_t>(spatialList.size());
  stream->writeFloatList(spatialList.data(), count, SPATIAL_PRECISION);
}

template <typename T>
AttributeFlag WriteProperty(EncodeStream* stream, const AttributeConfig<T>& config,
                            Property<T>* property) {
  AttributeFlag flag = {};
  if (property == nullptr) {
    return flag;
  }
  if (!property->animatable()) {
    return WriteValue(stream, config, property->getValueAt(0));
  }
  flag.exist = true;
  flag.animatable = true;
  auto& keyframes = static_cast<AnimatableProperty<T>*>(property)->keyframes;
  bool hasSpatial = false;
  if (config.attributeType == AttributeType::SpatialProperty) {
    for (auto keyframe : keyframes) {
      if (keyframe->spatialIn != Point3D::Zero() || keyframe->spatialOut != Point3D::Zero()) {
        hasSpatial = true;
        break;
      }
    }
  }
  flag.hasSpatial = hasSpatial;
  WriteKeyframes(stream, keyframes, config);
  WriteTimeAndValue(stream, keyframes, config);
  WriteTimeEase(stream, keyframes, config);
  if (hasSpatial) {
    WriteSpatialEase(stream, keyframes);
  }
  return flag;
}

template <typename T>
void WriteAttribute(EncodeStream* flagBytes, EncodeStream* stream, void* target,
                    const AttributeConfig<T>& config) {
  AttributeFlag flag = {};
  if (config.attributeType == AttributeType::BitFlag) {
    flag.exist = *reinterpret_cast<bool*>(target);
  } else if (config.attributeType == AttributeType::FixedValue) {
    flag.exist = true;
    auto valueTarget = reinterpret_cast<T*>(target);
    config.writeValue(stream, *valueTarget);
  } else if (config.attributeType == AttributeType::Value) {
    auto valueTarget = reinterpret_cast<T*>(target);
    flag = WriteValue(stream, config, *valueTarget);
  } else {
    auto propertyTarget = reinterpret_cast<Property<T>**>(target);
    flag = WriteProperty(stream, config, *propertyTarget);
  }
  WriteAttributeFlag(flagBytes, flag, &config);
}

template <class T>
void WriteTagBlock(EncodeStream* stream, T* parameter,
                   std::unique_ptr<BlockConfig> (*ConfigMaker)(T*)) {
  EncodeStream flagBytes(stream->context);
  EncodeStream bytes(stream->context);
  auto tagConfig = ConfigMaker(parameter);
  int index = 0;
  for (auto& config : tagConfig->configs) {
    auto target = tagConfig->targets[index];
    config->writeAttribute(&flagBytes, &bytes, target);
    index++;
  }
  // Actually the alignWithBytes() here has no effect,
  // it is just for reminding us that we need to call
  // alignWithBytes() when start reading this block.
  flagBytes.alignWithBytes();
  flagBytes.writeBytes(&bytes);
  WriteTagHeader(stream, &flagBytes, tagConfig->tagCode);
}

template <class T>
void WriteBlock(EncodeStream* stream, T* parameter,
                std::unique_ptr<BlockConfig> (*ConfigMaker)(T*)) {
  // Must call alignWithBytes() here in case
  // we have already written some bit values in stream.
  stream->alignWithBytes();
  EncodeStream contentBytes(stream->context);
  auto tagConfig = ConfigMaker(parameter);
  int index = 0;
  for (auto& config : tagConfig->configs) {
    auto target = tagConfig->targets[index];
    config->writeAttribute(stream, &contentBytes, target);
    index++;
  }
  // Actually the alignWithBytes() here has no effect,
  // it is just for reminding us that we need to call
  // alignWithBytes() when start reading this block.
  stream->alignWithBytes();
  stream->writeBytes(&contentBytes);
}
}  // namespace pag
