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
#include "tgfx/core/BlendMode.h"

namespace tgfx {
class XfermodeFragmentProcessor : public FragmentProcessor {
 public:
  /**
   * The color input to the returned processor is treated as the src and the passed in processor is
   * the dst.
   */
  static std::unique_ptr<FragmentProcessor> MakeFromDstProcessor(
      std::unique_ptr<FragmentProcessor> dst, BlendMode mode);

  /**
   * The color input to the returned processor is treated as the dst and the passed in processor is
   * the src.
   */
  static std::unique_ptr<FragmentProcessor> MakeFromSrcProcessor(
      std::unique_ptr<FragmentProcessor> src, BlendMode mode);

  /**
   * Takes the input color, which is assumed to be unpremultiplied, passes it as an opaque color
   * to both src and dst. The outputs of a src and dst are blended using mode and the original
   * input's alpha is applied to the blended color to produce a premul output.
   */
  static std::unique_ptr<FragmentProcessor> MakeFromTwoProcessors(
      std::unique_ptr<FragmentProcessor> src, std::unique_ptr<FragmentProcessor> dst,
      BlendMode mode);

  std::string name() const override;

  void onComputeProcessorKey(BytesKey* bytesKey) const override;

  std::unique_ptr<GLFragmentProcessor> onCreateGLInstance() const override;

 private:
  enum class Child { DstChild, SrcChild, TwoChild };

  XfermodeFragmentProcessor(std::unique_ptr<FragmentProcessor> src,
                            std::unique_ptr<FragmentProcessor> dst, BlendMode mode);

  Child child;
  BlendMode mode;

  friend class GLXfermodeFragmentProcessor;
};
}  // namespace tgfx
