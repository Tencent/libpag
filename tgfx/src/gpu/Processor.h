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
#include "utils/UniqueID.h"

namespace tgfx {
#define DEFINE_PROCESSOR_CLASS_ID               \
  static uint32_t ClassID() {                   \
    static uint32_t ClassID = UniqueID::Next(); \
    return ClassID;                             \
  }

class Processor {
 public:
  explicit Processor(uint32_t classID) : _classID(classID) {
  }

  virtual ~Processor() = default;

  /**
   * Human-meaningful string to identify this processor.
   */
  virtual std::string name() const = 0;

  uint32_t classID() const {
    return _classID;
  }

  virtual void computeProcessorKey(Context* context, BytesKey* bytesKey) const = 0;

 private:
  uint32_t _classID = 0;
};
}  // namespace tgfx
