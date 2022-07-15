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

#include <string>
#include <vector>
#include "BitmaskOperators.h"

namespace tgfx {
enum class ShaderFlags : unsigned {
  None = 0,
  Vertex = 1 << 0,
  Fragment = 1 << 1,
  TGFX_MARK_AS_BITMASK_ENUM(Fragment)
};

class ShaderVar {
 public:
  enum class Type {
    Void,
    Float,
    Float2,
    Float3,
    Float4,
    Float3x3,
    Float4x4,
    Texture2DSampler,
    TextureExternalSampler,
    Texture2DRectSampler,
  };

  enum class TypeModifier {
    None,
    Attribute,
    Varying,
    Uniform,
    Out,
  };

  ShaderVar() = default;

  ShaderVar(std::string name, Type type) : _type(type), _name(std::move(name)) {
  }

  ShaderVar(std::string name, Type type, TypeModifier typeModifier)
      : _type(type), _typeModifier(typeModifier), _name(std::move(name)) {
  }

  void setName(const std::string& name) {
    _name = name;
  }

  const std::string& name() const {
    return _name;
  }

  void setType(Type type) {
    _type = type;
  }

  Type type() const {
    return _type;
  }

  void setTypeModifier(TypeModifier type) {
    _typeModifier = type;
  }

  TypeModifier typeModifier() const {
    return _typeModifier;
  }

 private:
  Type _type = Type::Void;
  TypeModifier _typeModifier = TypeModifier::None;
  std::string _name;
};
}  // namespace tgfx
