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

#include "ColorShader.h"
#include "gpu/ConstColorProcessor.h"

namespace pag {
std::shared_ptr<Shader> Shader::MakeColorShader(Color color, Opacity opacity) {
  return std::make_shared<ColorShader>(
      Color4f{static_cast<float>(color.red) / 255.0f, static_cast<float>(color.green) / 255.0f,
              static_cast<float>(color.blue) / 255.0f, static_cast<float>(opacity) / 255.0f});
}

bool ColorShader::isOpaque() const {
  return color.isOpaque();
}

std::unique_ptr<FragmentProcessor> ColorShader::asFragmentProcessor(const FPArgs&) const {
  return ConstColorProcessor::Make(color);
}

}  // namespace pag
