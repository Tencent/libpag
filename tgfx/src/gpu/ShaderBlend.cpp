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

#include "ShaderBlend.h"
#include "FragmentProcessor.h"
#include "XfermodeFragmentProcessor.h"

namespace tgfx {
std::shared_ptr<Shader> Shader::MakeBlend(BlendMode mode, std::shared_ptr<Shader> dst,
                                          std::shared_ptr<Shader> src) {
  switch (mode) {
    case BlendMode::Clear:
      return MakeColorShader(Color::Transparent());
    case BlendMode::Dst:
      return dst;
    case BlendMode::Src:
      return src;
    default:
      break;
  }
  if (dst == nullptr || src == nullptr) {
    return nullptr;
  }
  auto shader = std::make_shared<ShaderBlend>(mode, std::move(dst), std::move(src));
  shader->weakThis = shader;
  return shader;
}

std::unique_ptr<FragmentProcessor> ShaderBlend::asFragmentProcessor(const FPArgs& args) const {
  auto fpA = dst->asFragmentProcessor(args);
  if (fpA == nullptr) {
    return nullptr;
  }
  auto fpB = src->asFragmentProcessor(args);
  if (fpB == nullptr) {
    return nullptr;
  }
  return XfermodeFragmentProcessor::MakeFromTwoProcessors(std::move(fpB), std::move(fpA), mode);
}
}  // namespace tgfx
