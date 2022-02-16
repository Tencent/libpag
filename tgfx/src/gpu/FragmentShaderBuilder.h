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

#include "ShaderBuilder.h"

namespace tgfx {
class FragmentShaderBuilder : public ShaderBuilder {
 public:
  explicit FragmentShaderBuilder(ProgramBuilder* program);

  const std::string& mangleString() const {
    return _mangleString;
  }

  virtual std::string dstColor() = 0;

  void onBeforeChildProcEmitCode();

  void onAfterChildProcEmitCode();

  void declareCustomOutputColor();

 protected:
  virtual std::string colorOutputName() = 0;

  static std::string CustomColorOutputName() {
    return "tgfx_FragColor";
  }

  void onFinalize() override;

  /**
   * State that tracks which child proc in the proc tree is currently emitting code.  This is
   * used to update the _mangleString, which is used to mangle the names of uniforms and functions
   * emitted by the proc. subStageIndices is a stack: its count indicates how many levels deep
   * we are in the tree, and its second-to-last value is the index of the child proc at that
   * level which is currently emitting code. For example, if subStageIndices = [3, 1, 2, 0], that
   * means we're currently emitting code for the base proc's 3rd child's 1st child's 2nd child.
   */
  std::vector<int> subStageIndices;

  /**
   * The mangle string is used to mangle the names of uniforms/functions emitted by the child
   * procs so no duplicate uniforms/functions appear in the generated shader program. The mangle
   * string is simply based on subStageIndices. For example, if subStageIndices = [3, 1, 2, 0],
   * then the _mangleString will be "_c3_c1_c2", and any uniform/function emitted by that proc will
   * have "_c3_c1_c2" appended to its name, which can be interpreted as "base proc's 3rd child's
   * 1st child's 2nd child".
   */
  std::string _mangleString;

  friend class ProgramBuilder;
};
}  // namespace tgfx
