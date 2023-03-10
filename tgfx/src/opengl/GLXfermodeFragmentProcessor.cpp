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

#include "GLXfermodeFragmentProcessor.h"
#include "GLBlend.h"
#include "gpu/XfermodeFragmentProcessor.h"

namespace tgfx {
void GLXfermodeFragmentProcessor::emitCode(EmitArgs& args) {
  auto* fragBuilder = args.fragBuilder;
  const auto* fp = static_cast<const XfermodeFragmentProcessor*>(args.fragmentProcessor);
  if (fp->child == XfermodeFragmentProcessor::Child::TwoChild) {
    std::string inputColor;
    if (!args.inputColor.empty()) {
      inputColor = "inputColor";
      fragBuilder->codeAppendf("vec4 inputColor = vec4(%s.rgb, 1.0);", args.inputColor.c_str());
    }
    std::string srcColor = "xfer_src";
    emitChild(0, inputColor, &srcColor, args);
    std::string dstColor = "xfer_dst";
    emitChild(1, inputColor, &dstColor, args);
    fragBuilder->codeAppendf("// Compose Xfer Mode: %s\n", BlendModeName(fp->mode));
    AppendMode(fragBuilder, srcColor, dstColor, args.outputColor, fp->mode);
    // re-multiply the output color by the input color's alpha
    if (!args.inputColor.empty()) {
      fragBuilder->codeAppendf("%s *= %s.a;", args.outputColor.c_str(), args.inputColor.c_str());
    }
  } else {
    std::string childColor = "child";
    emitChild(0, &childColor, args);
    // emit blend code
    fragBuilder->codeAppendf("// Compose Xfer Mode: %s\n", BlendModeName(fp->mode));
    if (fp->child == XfermodeFragmentProcessor::Child::DstChild) {
      AppendMode(fragBuilder, args.inputColor, childColor, args.outputColor, fp->mode);
    } else {
      AppendMode(fragBuilder, childColor, args.inputColor, args.outputColor, fp->mode);
    }
  }
}
}  // namespace tgfx
