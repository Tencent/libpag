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

#include "core/Color4f.h"
#include "gpu/GLFragmentProcessor.h"

namespace pag {
class GLDualIntervalGradientColorizer : public GLFragmentProcessor {
 public:
  void emitCode(EmitArgs& args) override;

 private:
  void onSetData(const ProgramDataManager&, const FragmentProcessor&) override;

  UniformHandle scale01Uniform;
  UniformHandle bias01Uniform;
  UniformHandle scale23Uniform;
  UniformHandle bias23Uniform;
  UniformHandle thresholdUniform;

  Color4f scale01Prev = Color4f::Invalid();
  Color4f bias01Prev = Color4f::Invalid();
  Color4f scale23Prev = Color4f::Invalid();
  Color4f bias23Prev = Color4f::Invalid();
  float thresholdPrev = -1;
};
}  // namespace pag
