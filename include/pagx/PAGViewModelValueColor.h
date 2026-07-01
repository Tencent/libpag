/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
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

#include "pagx/PAGViewModelValue.h"
#include "pagx/types/Color.h"

namespace pagx {

/**
 * PAGViewModelValueColor holds a color ViewModel property value.
 */
class PAGViewModelValueColor : public PAGViewModelValue {
 public:
  /**
   * Returns the current color value.
   */
  const Color& value() const {
    return propertyValue;
  }

  /**
   * Sets the color value. Setting the same value is a no-op. Triggers observer callbacks
   * (unless suppressed) and marks dependent DataBinds dirty.
   */
  void value(const Color& v);

 protected:
  /**
   * Internal write entry point. When fromVM is true, behaves exactly like value(v). When fromVM
   * is false, notifies observers but does not mark dirty or notify dependents.
   */
  void setValueInternal(const Color& v, bool fromVM);

 private:
  Color propertyValue = {};

  friend class PAGViewModel;
  friend class PAGScene;
  friend class DataBindRuntime;
};

}  // namespace pagx
