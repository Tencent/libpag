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

#include "CoordTransform.h"
#include "Processor.h"
#include "tgfx/gpu/Texture.h"

namespace tgfx {
struct FPArgs {
  explicit FPArgs(const Context* context) : context(context) {
  }

  const Context* context = nullptr;
  Matrix preLocalMatrix = Matrix::I();
  Matrix postLocalMatrix = Matrix::I();
};

bool ComputeTotalInverse(const FPArgs& args, Matrix* totalInverse);

class Pipeline;
class GLFragmentProcessor;

class FragmentProcessor : public Processor {
 public:
  /**
   *  In many instances (e.g. Shader::asFragmentProcessor() implementations) it is desirable to
   *  only consider the input color's alpha. However, there is a competing desire to have reusable
   *  FragmentProcessor subclasses that can be used in other scenarios where the entire input
   *  color is considered. This function exists to filter the input color and pass it to a FP. It
   *  does so by returning a parent FP that multiplies the passed in FPs output by the parent's
   *  input alpha. The passed in FP will not receive an input color.
   */
  static std::unique_ptr<FragmentProcessor> MulChildByInputAlpha(
      std::unique_ptr<FragmentProcessor> child);

  /**
   * Returns the input modulated by the child's alpha. The passed in FP will not receive an input
   * color.
   *
   * @param inverted false: output = input * child.a; true: output = input * (1 - child.a)
   */
  static std::unique_ptr<FragmentProcessor> MulInputByChildAlpha(
      std::unique_ptr<FragmentProcessor> child, bool inverted = false);

  /**
   * Returns a fragment processor that runs the passed in array of fragment processors in a
   * series. The original input is passed to the first, the first's output is passed to the
   * second, etc. The output of the returned processor is the output of the last processor of the
   * series.
   *
   * The array elements with be moved.
   */
  static std::unique_ptr<FragmentProcessor> RunInSeries(std::unique_ptr<FragmentProcessor>* series,
                                                        int count);

  std::unique_ptr<GLFragmentProcessor> createGLInstance() const;

  size_t numTextureSamplers() const {
    return textureSamplerCount;
  }

  const TextureSampler* textureSampler(size_t i) const {
    return onTextureSampler(i);
  }

  void computeProcessorKey(Context* context, BytesKey* bytesKey) const override;

  size_t numChildProcessors() const {
    return childProcessors.size();
  }

  const FragmentProcessor* childProcessor(size_t index) const {
    return childProcessors[index].get();
  }

  size_t numCoordTransforms() const {
    return coordTransforms.size();
  }

  /**
   * Returns the coordinate transformation at index. index must be valid according to
   * numCoordTransforms().
   */
  const CoordTransform* coordTransform(size_t index) const {
    return coordTransforms[index];
  }

  /**
   * Pre-order traversal of a FP hierarchy, or of the forest of FPs in a Pipeline. In the latter
   * case the tree rooted at each FP in the Pipeline is visited successively.
   */
  class Iter {
   public:
    explicit Iter(const FragmentProcessor* fp) {
      fpStack.push_back(fp);
    }

    explicit Iter(const Pipeline& pipeline);

    const FragmentProcessor* next();

   private:
    std::vector<const FragmentProcessor*> fpStack;
  };

  /**
   * Iterates over all the CoordTransforms owned by the forest of FragmentProcessors in a Pipeline.
   * FPs are visited in the same order as Iter and each of an FP's CoordTransforms are visited in
   * order.
   */
  class CoordTransformIter {
   public:
    explicit CoordTransformIter(const Pipeline& pipeline);

    const CoordTransform* next();

   private:
    const FragmentProcessor* currFP = nullptr;
    size_t currentIndex = 0;
    FragmentProcessor::Iter fpIter;
  };

 protected:
  /**
   * FragmentProcessor subclasses call this from their constructor to register any child
   * FragmentProcessors they have. This must be called AFTER all texture accesses and coord
   * transforms have been added.
   * This is for processors whose shader code will be composed of nested processors whose output
   * colors will be combined somehow to produce its output color.  Registering these child
   * processors will allow the ProgramBuilder to automatically handle their transformed coords and
   * texture accesses and mangle their uniform and output color names.
   */
  size_t registerChildProcessor(std::unique_ptr<FragmentProcessor> child);

  /**
   * Fragment Processor subclasses call this from their constructor to register coordinate
   * transformations. Coord transforms provide a mechanism for a processor to receive coordinates
   * in their FS code. The matrix expresses a transformation from local space. For a given
   * fragment the matrix will be applied to the local coordinate that maps to the fragment.
   *
   * This must only be called from the constructor because Processors are immutable. The
   * processor subclass manages the lifetime of the transformations (this function only stores a
   * pointer). The CoordTransform is typically a member field of the Processor subclass.
   *
   * A processor subclass that has multiple methods of construction should always add its coord
   * transforms in a consistent order.
   */
  void addCoordTransform(const CoordTransform* transform) {
    coordTransforms.push_back(transform);
  }

  void setTextureSamplerCnt(size_t count) {
    textureSamplerCount = count;
  }

 private:
  virtual void onComputeProcessorKey(BytesKey* bytesKey) const = 0;

  virtual std::unique_ptr<GLFragmentProcessor> onCreateGLInstance() const = 0;

  virtual const TextureSampler* onTextureSampler(size_t) const {
    return nullptr;
  }

  size_t textureSamplerCount = 0;

  std::vector<const CoordTransform*> coordTransforms;

  std::vector<std::unique_ptr<FragmentProcessor>> childProcessors;
};
}  // namespace tgfx
