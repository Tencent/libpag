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

#include <vector>

#include "GeometryProcessor.h"
#include "ShaderVar.h"

namespace tgfx {
class ProgramBuilder;

class Varying {
 public:
  const std::string& vsOut() const {
    return _name;
  }
  const std::string& fsIn() const {
    return _name;
  }
  const std::string& name() const {
    return _name;
  }
  ShaderVar::Type type() const {
    return _type;
  }

 private:
  ShaderVar::Type _type = ShaderVar::Type::Void;
  std::string _name;

  friend class VaryingHandler;
};

class VaryingHandler {
 public:
  explicit VaryingHandler(ProgramBuilder* program) : programBuilder(program) {
  }

  virtual ~VaryingHandler() = default;

  Varying addVarying(const std::string& name, ShaderVar::Type type);

  void emitAttributes(const GeometryProcessor& processor);

  /**
   * This should be called once all attributes and varyings have been added to the VaryingHandler
   * and before getting/adding any of the declarations to the shaders.
   */
  void finalize();

  void getVertexDecls(std::string* inputDecls, std::string* outputDecls) const;

  void getFragDecls(std::string* inputDecls) const;

 private:
  void appendDecls(const std::vector<ShaderVar>& vars, std::string* out, ShaderFlags flag) const;

  void addAttribute(const ShaderVar& var);

  std::vector<Varying> varyings;
  std::vector<ShaderVar> vertexInputs;
  std::vector<ShaderVar> vertexOutputs;
  std::vector<ShaderVar> fragInputs;

  // This is not owned by the class
  ProgramBuilder* programBuilder;
};
}  // namespace tgfx
