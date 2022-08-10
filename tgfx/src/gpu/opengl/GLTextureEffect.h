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

#include <optional>
#include "gpu/GLFragmentProcessor.h"
#include "gpu/TextureEffect.h"

namespace tgfx {
class GLTextureEffect : public GLFragmentProcessor {
 public:
  void emitCode(EmitArgs& args) override;

 private:
  void onSetData(const ProgramDataManager& programDataManager,
                 const FragmentProcessor& fragmentProcessor) override;

  static bool ShaderModeRequiresUnormCoord(TextureEffect::ShaderMode m);

  static bool ShaderModeUsesSubset(TextureEffect::ShaderMode m);

  static bool ShaderModeUsesClamp(TextureEffect::ShaderMode m);

  void readColor(EmitArgs& args, const std::string& coord, const char* out);

  void subsetCoord(EmitArgs& args, TextureEffect::ShaderMode mode, const char* coordSwizzle,
                   const char* subsetStartSwizzle, const char* subsetStopSwizzle);

  void clampCoord(EmitArgs& args, bool clamp, const char* coordSwizzle,
                  const char* clampStartSwizzle, const char* clampStopSwizzle);

  void clampCoord(EmitArgs& args, const bool useClamp[2]);

  void initUniform(EmitArgs& args, const bool useSubset[2], const bool useClamp[2]);

  UniformHandle dimensionsUniform;
  UniformHandle subsetUniform;
  UniformHandle clampUniform;

  std::string subsetName;
  std::string clampName;
  std::string dimensionsName;

  std::optional<Point> alphaStartPrev;
  std::optional<Point> dimensionsPrev;
  std::optional<Rect> subsetPrev;
  std::optional<Rect> clampPrev;
};
}  // namespace tgfx
