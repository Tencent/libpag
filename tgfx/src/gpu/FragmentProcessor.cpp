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

#include "FragmentProcessor.h"
#include "GLFragmentProcessor.h"
#include "Pipeline.h"
#include "SeriesFragmentProcessor.h"
#include "XfermodeFragmentProcessor.h"

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
  onComputeProcessorKey(bytesKey);
  for (size_t i = 0; i < textureSamplerCount; ++i) {
    textureSampler(i)->computeKey(context, bytesKey);
  }
  for (const auto& childProcessor : childProcessors) {
    childProcessor->computeProcessorKey(context, bytesKey);
  }
}

std::unique_ptr<GLFragmentProcessor> FragmentProcessor::createGLInstance() const {
  auto glFragProc = onCreateGLInstance();
  for (const auto& fChildProcessor : childProcessors) {
    glFragProc->childProcessors.push_back(fChildProcessor->createGLInstance());
  }
  return glFragProc;
}

size_t FragmentProcessor::registerChildProcessor(std::unique_ptr<FragmentProcessor> child) {
  auto index = childProcessors.size();
  childProcessors.push_back(std::move(child));
  return index;
}

FragmentProcessor::Iter::Iter(const Pipeline& pipeline) {
  for (int i = static_cast<int>(pipeline.numFragmentProcessors()) - 1; i >= 0; --i) {
    fpStack.push_back(pipeline.getFragmentProcessor(i));
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

FragmentProcessor::CoordTransformIter::CoordTransformIter(const Pipeline& pipeline)
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
}  // namespace tgfx
