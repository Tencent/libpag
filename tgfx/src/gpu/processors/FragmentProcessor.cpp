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

#include "gpu/processors/FragmentProcessor.h"
#include "SeriesFragmentProcessor.h"
#include "gpu/Pipeline.h"
#include "gpu/processors/XfermodeFragmentProcessor.h"

namespace tgfx {
bool ComputeTotalInverse(const FPArgs& args, Matrix* totalInverse) {
  if (totalInverse == nullptr) {
    return false;
  }
  totalInverse->reset();
  if (!args.preLocalMatrix.isIdentity()) {
    totalInverse->preConcat(args.preLocalMatrix);
  }
  if (!args.postLocalMatrix.isIdentity()) {
    totalInverse->postConcat(args.postLocalMatrix);
  }
  return totalInverse->invert(totalInverse);
}

std::unique_ptr<FragmentProcessor> FragmentProcessor::MulChildByInputAlpha(
    std::unique_ptr<FragmentProcessor> child) {
  if (child == nullptr) {
    return nullptr;
  }
  return XfermodeFragmentProcessor::MakeFromDstProcessor(std::move(child), BlendMode::DstIn);
}

std::unique_ptr<FragmentProcessor> FragmentProcessor::MulInputByChildAlpha(
    std::unique_ptr<FragmentProcessor> child, bool inverted) {
  if (child == nullptr) {
    return nullptr;
  }
  return XfermodeFragmentProcessor::MakeFromDstProcessor(
      std::move(child), inverted ? BlendMode::SrcOut : BlendMode::SrcIn);
}

std::unique_ptr<FragmentProcessor> FragmentProcessor::RunInSeries(
    std::unique_ptr<FragmentProcessor>* series, int count) {
  return SeriesFragmentProcessor::Make(series, count);
}

void FragmentProcessor::computeProcessorKey(Context* context, BytesKey* bytesKey) const {
  bytesKey->write(classID());
  onComputeProcessorKey(bytesKey);
  auto textureSamplerCount = onCountTextureSamplers();
  for (size_t i = 0; i < textureSamplerCount; ++i) {
    textureSampler(i)->computeKey(context, bytesKey);
  }
  for (const auto& childProcessor : childProcessors) {
    childProcessor->computeProcessorKey(context, bytesKey);
  }
}

size_t FragmentProcessor::registerChildProcessor(std::unique_ptr<FragmentProcessor> child) {
  auto index = childProcessors.size();
  childProcessors.push_back(std::move(child));
  return index;
}

FragmentProcessor::Iter::Iter(const Pipeline* pipeline) {
  for (int i = static_cast<int>(pipeline->numFragmentProcessors()) - 1; i >= 0; --i) {
    fpStack.push_back(pipeline->getFragmentProcessor(i));
  }
}

const FragmentProcessor* FragmentProcessor::Iter::next() {
  if (fpStack.empty()) {
    return nullptr;
  }
  const FragmentProcessor* back = fpStack.back();
  fpStack.pop_back();
  for (size_t i = 0; i < back->numChildProcessors(); ++i) {
    fpStack.push_back(back->childProcessor(back->numChildProcessors() - i - 1));
  }
  return back;
}

FragmentProcessor::CoordTransformIter::CoordTransformIter(const Pipeline* pipeline)
    : fpIter(pipeline) {
  currFP = fpIter.next();
}

const CoordTransform* FragmentProcessor::CoordTransformIter::next() {
  if (!currFP) {
    return nullptr;
  }
  while (currentIndex == currFP->numCoordTransforms()) {
    currentIndex = 0;
    currFP = fpIter.next();
    if (!currFP) {
      return nullptr;
    }
  }
  return currFP->coordTransform(currentIndex++);
}

bool FragmentProcessor::isEqual(const FragmentProcessor& that) const {
  if (classID() != that.classID()) {
    return false;
  }
  if (this->numTextureSamplers() != that.numTextureSamplers()) {
    return false;
  }
  for (size_t i = 0; i < numTextureSamplers(); ++i) {
    if (textureSampler(i) != that.textureSampler(i)) {
      return false;
    }
  }
  if (numCoordTransforms() != that.numCoordTransforms()) {
    return false;
  }
  for (size_t i = 0; i < numCoordTransforms(); ++i) {
    if (coordTransform(i)->matrix != that.coordTransform(i)->matrix) {
      return false;
    }
  }
  if (!onIsEqual(that)) {
    return false;
  }
  if (numChildProcessors() != that.numChildProcessors()) {
    return false;
  }
  for (size_t i = 0; i < numChildProcessors(); ++i) {
    if (!childProcessor(i)->isEqual(*that.childProcessor(i))) {
      return false;
    }
  }
  return true;
}

void FragmentProcessor::visitProxies(const std::function<void(TextureProxy*)>& func) const {
  onVisitProxies(func);
  for (const auto& fp : childProcessors) {
    fp->visitProxies(func);
  }
}

void FragmentProcessor::setData(UniformBuffer* uniformBuffer) const {
  onSetData(uniformBuffer);
}

void FragmentProcessor::emitChild(size_t childIndex, const std::string& inputColor,
                                  std::string* outputColor, EmitArgs& args,
                                  std::function<std::string(std::string_view)> coordFunc) const {
  auto* fragBuilder = args.fragBuilder;
  outputColor->append(fragBuilder->mangleString());
  fragBuilder->codeAppendf("vec4 %s;", outputColor->c_str());
  internalEmitChild(childIndex, inputColor, *outputColor, args, std::move(coordFunc));
}

void FragmentProcessor::emitChild(size_t childIndex, const std::string& inputColor,
                                  EmitArgs& parentArgs) const {
  internalEmitChild(childIndex, inputColor, parentArgs.outputColor, parentArgs);
}

void FragmentProcessor::internalEmitChild(
    size_t childIndex, const std::string& inputColor, const std::string& outputColor,
    EmitArgs& args, std::function<std::string(std::string_view)> coordFunc) const {
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

  const auto* childProc = childProcessor(childIndex);

  // emit the code for the child in its own scope
  fragBuilder->codeAppend("{\n");
  fragBuilder->codeAppendf("// Child Index %d (mangle: %s): %s\n", childIndex,
                           fragBuilder->mangleString().c_str(), childProc->name().c_str());
  TransformedCoordVars coordVars = args.transformedCoords->childInputs(childIndex);
  TextureSamplers textureSamplers = args.textureSamplers->childInputs(childIndex);

  EmitArgs childArgs(fragBuilder, args.uniformHandler, outputColor,
                     inputName.empty() ? "vec4(1.0)" : inputName, &coordVars, &textureSamplers,
                     std::move(coordFunc));
  childProcessor(childIndex)->emitCode(childArgs);
  fragBuilder->codeAppend("}\n");

  fragBuilder->onAfterChildProcEmitCode();
}

template <typename T, size_t (FragmentProcessor::*COUNT)() const>
FragmentProcessor::BuilderInputProvider<T, COUNT>
FragmentProcessor::BuilderInputProvider<T, COUNT>::childInputs(size_t childIndex) const {
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
