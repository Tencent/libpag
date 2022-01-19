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
class AttributeConfig : public AttributeBase {
 public:
  T defaultValue;

  AttributeConfig(AttributeType attributeType, T defaultValue)
      : AttributeBase(attributeType), defaultValue(defaultValue) {
  }

  virtual ~AttributeConfig() {
  }

  int dimensionality() const {
    return 1;
  }

  Keyframe<T>* newKeyframe(const AttributeFlag&) const {
    return new Keyframe<T>();
  }

  void readValueList(DecodeStream* stream, T* list, uint32_t count) const {
    for (uint32_t i = 0; i < count; i++) {
      list[i] = readValue(stream);
    }
  }

  void writeValueList(EncodeStream* stream, const T* list, uint32_t count) const {
    for (uint32_t i = 0; i < count; i++) {
      writeValue(stream, list[i]);
    }
  }

  void readAttribute(DecodeStream* stream, const AttributeFlag& flag, void* target) const override {
    ReadAttribute(stream, flag, target, *this);
  }

  void writeAttribute(EncodeStream* flagBytes, EncodeStream* stream, void* target) const override {
    WriteAttribute(flagBytes, stream, target, *this);
  }

  // partial specialization functions
  T readValue(DecodeStream* stream) const;
  void writeValue(EncodeStream* stream, const T& value) const;
};

}  // namespace pag
