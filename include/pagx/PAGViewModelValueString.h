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

#include <string>
#include "pagx/PAGViewModelValue.h"

namespace pagx {

/**
 * PAGViewModelValueString holds a string ViewModel property value.
 */
class PAGViewModelValueString : public PAGViewModelValue {
 public:
  /**
   * Returns the current string value.
   */
  const std::string& value() const {
    return propertyValue;
  }

  /**
   * Sets the string value. Setting the same value is a no-op.
   */
  void value(const std::string& v);

 private:
  std::string propertyValue = {};

  friend class PAGViewModel;
  friend class PAGScene;
};

}  // namespace pagx
