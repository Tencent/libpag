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

#include "tgfx/core/BytesKey.h"
#include "tgfx/gpu/Context.h"

namespace tgfx {
/**
 * The base class for GPU program. Overrides the onReleaseGPU() method to to free all GPU resources.
 * No backend API calls should be made during destructuring since there may be no GPU context which
 * is current on the calling thread.
 */
class Program {
 public:
  explicit Program(Context* context) : context(context) {
  }

  virtual ~Program() = default;

  /**
   * Retrieves the context associated with this Program.
   */
  Context* getContext() const {
    return context;
  }

 protected:
  Context* context = nullptr;

 private:
  BytesKey uniqueKey = {};

  /**
   * Overridden to free GPU resources in the backend API.
   */
  virtual void onReleaseGPU() = 0;

  friend class ProgramCache;
};

class ProgramCreator {
 public:
  virtual ~ProgramCreator() = default;
  /**
   * Overridden to compute a unique key for the program.
   */
  virtual void computeUniqueKey(Context* context, BytesKey* uniqueKey) const = 0;

  /**
   * Overridden to create a new program.
   */
  virtual std::unique_ptr<Program> createProgram(Context* context) const = 0;
};
}  // namespace tgfx
