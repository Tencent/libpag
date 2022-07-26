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

#include "GLFragmentProcessor.h"

namespace tgfx {
void GLFragmentProcessor::setData(const ProgramDataManager& programDataManager,
                                  const FragmentProcessor& processor) {
  onSetData(programDataManager, processor);
}

void GLFragmentProcessor::emitChild(size_t childIndex, const std::string& inputColor,
                                    std::string* outputColor, EmitArgs& args,
                                    std::function<std::string(std::string_view)> coordFunc) {
  auto* fragBuilder = args.fragBuilder;
  outputColor->append(fragBuilder->mangleString());
  fragBuilder->codeAppendf("vec4 %s;", outputColor->c_str());
  internalEmitChild(childIndex, inputColor, *outputColor, args, std::move(coordFunc));
}

void GLFragmentProcessor::emitChild(size_t childIndex, const std::string& inputColor,
                                    EmitArgs& parentArgs) {
  internalEmitChild(childIndex, inputColor, parentArgs.outputColor, parentArgs);
}

void GLFragmentProcessor::internalEmitChild(
    size_t childIndex, const std::string& inputColor, const std::string& outputColor,
    EmitArgs& args, std::function<std::string(std::string_view)> coordFunc) {
  auto* fragBuilder = args.fragBuilder;
  fragBuilder->onBeforeChildProcEmitCode();  // call first so mangleString is updated
  // Prepare a mangled input color variable if the default is not used,
  // inputName remains the empty string if no variable is needed.
  std::string inputName;
  if (!inputColor.empty() && inputColor != "vec4(1.0)" && inputColor != "vec4(1)") {
    // The input name is based off of the current mangle string, and
    // since this is called after onBeforeChildProcEmitCode(), it will be
    // unique to the child processor (exactly what we want for its input).
    inputName += "_childInput";
    inputName += fragBuilder->mangleString();
    fragBuilder->codeAppendf("vec4 %s = %s;", inputName.c_str(), inputColor.c_str());
  }

  const auto* childProc = args.fragmentProcessor->childProcessor(childIndex);

  // emit the code for the child in its own scope
  fragBuilder->codeAppend("{\n");
  fragBuilder->codeAppendf("// Child Index %d (mangle: %s): %s\n", childIndex,
                           fragBuilder->mangleString().c_str(), childProc->name().c_str());
  TransformedCoordVars coordVars = args.transformedCoords->childInputs(childIndex);
  TextureSamplers textureSamplers = args.textureSamplers->childInputs(childIndex);

  EmitArgs childArgs(fragBuilder, args.uniformHandler, childProc, outputColor,
                     inputName.empty() ? "vec4(1.0)" : inputName, &coordVars, &textureSamplers,
                     std::move(coordFunc));
  childProcessor(childIndex)->emitCode(childArgs);
  fragBuilder->codeAppend("}\n");

  fragBuilder->onAfterChildProcEmitCode();
}

GLFragmentProcessor* GLFragmentProcessor::Iter::next() {
  if (fpStack.empty()) {
    return nullptr;
  }
  GLFragmentProcessor* back = fpStack.back();
  fpStack.pop_back();
  for (size_t i = 0; i < back->numChildProcessors(); ++i) {
    fpStack.push_back(back->childProcessor(back->numChildProcessors() - 1 - i));
  }
  return back;
}

template <typename T, size_t (FragmentProcessor::*COUNT)() const>
GLFragmentProcessor::BuilderInputProvider<T, COUNT>
GLFragmentProcessor::BuilderInputProvider<T, COUNT>::childInputs(size_t childIndex) const {
  const FragmentProcessor* child = fragmentProcessor->childProcessor(childIndex);
  FragmentProcessor::Iter iter(fragmentProcessor);
  int numToSkip = 0;
  while (true) {
    const FragmentProcessor* processor = iter.next();
    if (processor == child) {
      return BuilderInputProvider(child, t + numToSkip);
    }
    numToSkip += (processor->*COUNT)();
  }
}
}  // namespace tgfx
