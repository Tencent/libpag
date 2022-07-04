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

#include "FragmentProcessor.h"
#include "FragmentShaderBuilder.h"
#include "ProgramDataManager.h"
#include "UniformHandler.h"

namespace tgfx {
class GLFragmentProcessor {
 public:
  virtual ~GLFragmentProcessor() = default;

  /**
   * This class allows the shader builder to provide each GLFragmentProcessor with an array of
   * generated variables where each generated variable corresponds to an element of an array on
   * the FragmentProcessor that generated the GLFragmentProcessor.
   */
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
             const FragmentProcessor* fp, std::string outputColor, std::string inputColor,
             const TransformedCoordVars* transformedCoords, const TextureSamplers* textureSamplers)
        : fragBuilder(fragBuilder),
          uniformHandler(uniformHandler),
          fragmentProcessor(fp),
          outputColor(std::move(outputColor)),
          inputColor(std::move(inputColor)),
          transformedCoords(transformedCoords),
          textureSamplers(textureSamplers) {
    }
    /**
     * Interface used to emit code in the shaders.
     */
    FragmentShaderBuilder* fragBuilder;
    UniformHandler* uniformHandler;
    /**
     * The processor that generated this program stage.
     */
    const FragmentProcessor* fragmentProcessor;
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
  };

  /**
   * Called when the program stage should insert its code into the shaders. The code in each shader
   * will be in its own block ({}) and so locally scoped names will not collide across stages.
   */
  virtual void emitCode(EmitArgs&) = 0;

  void setData(const ProgramDataManager& programDataManager, const FragmentProcessor& processor);

  size_t numChildProcessors() const {
    return childProcessors.size();
  }

  GLFragmentProcessor* childProcessor(size_t index) {
    return childProcessors[index].get();
  }

  /**
   * Emit the child with the default input color (solid white)
   */
  void emitChild(size_t childIndex, std::string* outputColor, EmitArgs& parentArgs) {
    emitChild(childIndex, "", outputColor, parentArgs);
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
                 EmitArgs& parentArgs);

  /**
   * Variation that uses the parent's output color variable to hold the child's output.
   */
  void emitChild(size_t childIndex, const std::string& inputColor, EmitArgs& parentArgs);

  /**
   * Pre-order traversal of a GLFP hierarchy, or of multiple trees with roots in an array of
   * GLFPs. This agrees with the traversal order of FragmentProcessor::Iter
   */
  class Iter {
   public:
    explicit Iter(const std::vector<std::unique_ptr<GLFragmentProcessor>>& fragmentProcessors) {
      for (auto iter = fragmentProcessors.rbegin(); iter != fragmentProcessors.rend(); iter++) {
        fpStack.push_back(iter->get());
      }
    }
    GLFragmentProcessor* next();

   private:
    std::vector<GLFragmentProcessor*> fpStack;
  };

 protected:
  /**
   * A GLFragmentProcessor instance can be reused with any FragmentProcessor that produces
   * the same stage key; this function reads data from a FragmentProcessor and uploads any
   * uniform variables required by the shaders created in emitCode(). The FragmentProcessor
   * parameter is guaranteed to be of the same type that created this GLFragmentProcessor and
   * to have an identical processor key as the one that created this GLFragmentProcessor.
   */
  virtual void onSetData(const ProgramDataManager&, const FragmentProcessor&) {
  }

 private:
  void internalEmitChild(size_t, const std::string&, const std::string&, EmitArgs&);

  std::vector<std::unique_ptr<GLFragmentProcessor>> childProcessors;

  friend class FragmentProcessor;
};
}  // namespace tgfx
