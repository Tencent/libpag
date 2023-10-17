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

#include <functional>
#include "gpu/CoordTransform.h"
#include "gpu/FragmentShaderBuilder.h"
#include "gpu/SamplerState.h"
#include "gpu/Texture.h"
#include "gpu/TextureProxy.h"
#include "gpu/UniformBuffer.h"
#include "gpu/UniformHandler.h"
#include "gpu/processors/Processor.h"

namespace tgfx {
struct FPArgs {
  FPArgs(Context* context, uint32_t surfaceFlags) : context(context), surfaceFlags(surfaceFlags) {
  }

  Context* context = nullptr;
  uint32_t surfaceFlags = 0;
  Matrix preLocalMatrix = Matrix::I();
  Matrix postLocalMatrix = Matrix::I();
};

bool ComputeTotalInverse(const FPArgs& args, Matrix* totalInverse);

class Pipeline;

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

  size_t numTextureSamplers() const {
    return onCountTextureSamplers();
  }

  const TextureSampler* textureSampler(size_t i) const {
    return onTextureSampler(i);
  }

  SamplerState samplerState(size_t i) const {
    return onSamplerState(i);
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

  void visitProxies(const std::function<void(TextureProxy*)>& func) const;

  /**
   * Returns the coordinate transformation at index. index must be valid according to
   * numCoordTransforms().
   */
  const CoordTransform* coordTransform(size_t index) const {
    return coordTransforms[index];
  }

  bool isEqual(const FragmentProcessor& that) const;

  /**
   * Pre-order traversal of a FP hierarchy, or of the forest of FPs in a Pipeline. In the latter
   * case the tree rooted at each FP in the Pipeline is visited successively.
   */
  class Iter {
   public:
    explicit Iter(const FragmentProcessor* fp) {
      fpStack.push_back(fp);
    }

    explicit Iter(const Pipeline* pipeline);

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
    explicit CoordTransformIter(const Pipeline* pipeline);

    const CoordTransform* next();

   private:
    const FragmentProcessor* currFP = nullptr;
    size_t currentIndex = 0;
    FragmentProcessor::Iter fpIter;
  };

  template <typename T, size_t (FragmentProcessor::*COUNT)() const>
  class BuilderInputProvider {
   public:
    BuilderInputProvider(const FragmentProcessor* fp, const T* t) : fragmentProcessor(fp), t(t) {
    }

    const T& operator[](size_t i) const {
      return t[i];
    }

    size_t count() const {
      return (fragmentProcessor->*COUNT)();
    }

    BuilderInputProvider childInputs(size_t childIndex) const;

   private:
    const FragmentProcessor* fragmentProcessor;
    const T* t;
  };

  using TransformedCoordVars =
      BuilderInputProvider<ShaderVar, &FragmentProcessor::numCoordTransforms>;
  using TextureSamplers =
      BuilderInputProvider<SamplerHandle, &FragmentProcessor::numTextureSamplers>;

  struct EmitArgs {
    EmitArgs(FragmentShaderBuilder* fragBuilder, UniformHandler* uniformHandler,
             std::string outputColor, std::string inputColor,
             const TransformedCoordVars* transformedCoords, const TextureSamplers* textureSamplers,
             std::function<std::string(std::string_view)> coordFunc = {})
        : fragBuilder(fragBuilder),
          uniformHandler(uniformHandler),
          outputColor(std::move(outputColor)),
          inputColor(std::move(inputColor)),
          transformedCoords(transformedCoords),
          textureSamplers(textureSamplers),
          coordFunc(std::move(coordFunc)) {
    }
    /**
     * Interface used to emit code in the shaders.
     */
    FragmentShaderBuilder* fragBuilder;
    UniformHandler* uniformHandler;
    /**
     * A predefined vec4 in the FS in which the stage should place its output color (or coverage).
     */
    const std::string outputColor;
    /**
     * A vec4 that holds the input color to the stage in the FS.
     */
    const std::string inputColor;
    /**
     * Fragment shader variables containing the coords computed using each of the
     * FragmentProcessor's CoordTransforms.
     */
    const TransformedCoordVars* transformedCoords;
    /**
     * Contains one entry for each TextureSampler of the Processor. These can be passed to the
     * builder to emit texture reads in the generated code.
     */
    const TextureSamplers* textureSamplers;
    const std::function<std::string(std::string_view)> coordFunc;
  };

  /**
   * Called when the program stage should insert its code into the shaders. The code in each shader
   * will be in its own block ({}) and so locally scoped names will not collide across stages.
   */
  virtual void emitCode(EmitArgs& args) const = 0;

  void setData(UniformBuffer* uniformBuffer) const;

  /**
   * Emit the child with the default input color (solid white)
   */
  void emitChild(size_t childIndex, std::string* outputColor, EmitArgs& parentArgs,
                 std::function<std::string(std::string_view)> coordFunc = {}) const {
    emitChild(childIndex, "", outputColor, parentArgs, std::move(coordFunc));
  }

  /**
   * Will emit the code of a child proc in its own scope. Pass in the parent's EmitArgs and
   * emitChild will automatically extract the coords and samplers of that child and pass them
   * on to the child's emitCode(). Also, any uniforms or functions emitted by the child will
   * have their names mangled to prevent redefinitions. The output color name is also mangled
   * therefore in an in/out param. It will be declared in mangled form by emitChild(). It is
   * legal to pass empty string as inputColor, since all fragment processors are required to work
   * without an input color.
   */
  void emitChild(size_t childIndex, const std::string& inputColor, std::string* outputColor,
                 EmitArgs& parentArgs,
                 std::function<std::string(std::string_view)> coordFunc = {}) const;

  /**
   * Variation that uses the parent's output color variable to hold the child's output.
   */
  void emitChild(size_t childIndex, const std::string& inputColor, EmitArgs& parentArgs) const;

 protected:
  explicit FragmentProcessor(uint32_t classID) : Processor(classID) {
  }

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

  virtual void onSetData(UniformBuffer*) const {
  }

 private:
  virtual void onComputeProcessorKey(BytesKey*) const {
  }

  virtual size_t onCountTextureSamplers() const {
    return 0;
  }

  virtual const TextureSampler* onTextureSampler(size_t) const {
    return nullptr;
  }

  virtual SamplerState onSamplerState(size_t) const {
    return {};
  }

  virtual bool onIsEqual(const FragmentProcessor&) const {
    return true;
  }

  virtual void onVisitProxies(const std::function<void(TextureProxy*)>&) const {
  }

  void internalEmitChild(size_t, const std::string&, const std::string&, EmitArgs&,
                         std::function<std::string(std::string_view)> = {}) const;

  std::vector<const CoordTransform*> coordTransforms;
  std::vector<std::unique_ptr<FragmentProcessor>> childProcessors;
};
}  // namespace tgfx
