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

namespace tgfx {
class AlphaFragmentProcessor : public FragmentProcessor {
 public:
  static std::unique_ptr<AlphaFragmentProcessor> Make(float alpha);

  std::string name() const override {
    return "AlphaFragmentProcessor";
  }

 private:
  explicit AlphaFragmentProcessor(float alpha) : alpha(alpha) {
  }

  void onComputeProcessorKey(BytesKey* bytesKey) const override;

  std::unique_ptr<GLFragmentProcessor> onCreateGLInstance() const override;

  float alpha = 1;

  friend class GLAlphaFragmentProcessor;
};
}  // namespace tgfx
