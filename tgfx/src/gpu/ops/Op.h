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
#include "gpu/RenderTarget.h"
#include "gpu/TextureProxy.h"
#include "tgfx/core/Rect.h"
#include "tgfx/gpu/Context.h"

namespace tgfx {
class OpsRenderPass;

#define DEFINE_OP_CLASS_ID                   \
  static uint8_t ClassID() {                 \
    static uint8_t ClassID = GenOpClassID(); \
    return ClassID;                          \
  }

class Op {
 public:
  explicit Op(uint8_t classID) : _classID(classID) {
  }

  virtual ~Op() = default;

  virtual void visitProxies(const std::function<void(TextureProxy*)>&) const {
  }

  virtual void prepare(Gpu*) {
  }

  virtual void execute(OpsRenderPass* opsRenderPass) = 0;

  bool combineIfPossible(Op* op);

  const Rect& bounds() const {
    return _bounds;
  }

 protected:
  static uint8_t GenOpClassID();

  void setBounds(Rect bounds) {
    _bounds = bounds;
  }

  void setTransformedBounds(const Rect& srcBounds, const Matrix& matrix) {
    _bounds = matrix.mapRect(srcBounds);
  }

  virtual bool onCombineIfPossible(Op*) {
    return false;
  }

 private:
  uint8_t _classID = 0;
  Rect _bounds = Rect::MakeEmpty();
};
}  // namespace tgfx
