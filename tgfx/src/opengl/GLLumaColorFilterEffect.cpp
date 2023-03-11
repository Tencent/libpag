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

#include "GLLumaColorFilterEffect.h"

namespace tgfx {
void GLLumaColorFilterEffect::emitCode(EmitArgs& args) {
  auto* fragBuilder = args.fragBuilder;
  fragBuilder->codeAppendf(
      "%s = vec4(0.0, 0.0, 0.0, clamp(dot(vec3(0.21260000000000001, 0.71519999999999995, 0.0722), "
      "%s.rgb), 0.0, 1.0));",
      args.outputColor.c_str(), args.inputColor.c_str());
}
}  // namespace tgfx
