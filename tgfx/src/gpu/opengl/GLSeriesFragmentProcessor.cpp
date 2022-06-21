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

#include "GLSeriesFragmentProcessor.h"

namespace tgfx {
void GLSeriesFragmentProcessor::emitCode(EmitArgs& args) {
  // First guy's input might be nil.
  std::string temp = "out0";
  emitChild(0, args.inputColor, &temp, args);
  std::string input = temp;
  for (size_t i = 1; i < numChildProcessors() - 1; ++i) {
    temp = "out" + std::to_string(i);
    emitChild(i, input, &temp, args);
    input = temp;
  }
  // Last guy writes to our output variable.
  emitChild(numChildProcessors() - 1, input, args);
}
}  // namespace tgfx
