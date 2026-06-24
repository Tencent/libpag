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
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include "pagx/PAGImage.h"
#include "pagx/PAGViewModelValue.h"

namespace pagx {

/**
 * PAGViewModelValueImage holds an image ViewModel property value. The value is a PAGImage object
 * wrapping a decoded image, so DataBind writers receive a ready-to-render image without per-frame
 * decoding.
 */
class PAGViewModelValueImage : public PAGViewModelValue {
 public:
  /**
   * Returns the current image value, or nullptr if none is set.
   */
  std::shared_ptr<PAGImage> value() const {
    return propertyValue;
  }

  /**
   * Sets the image value. Setting the same value is a no-op. Triggers observer callbacks
   * (unless suppressed) and marks dependent DataBinds dirty.
   */
  void value(std::shared_ptr<PAGImage> v);

 protected:
  /**
   * Internal write entry point. When fromVM is true, behaves exactly like value(v). When fromVM
   * is false, notifies observers but does not mark dirty or notify dependents.
   */
  void setValueInternal(std::shared_ptr<PAGImage> v, bool fromVM);

 private:
  std::shared_ptr<PAGImage> propertyValue = nullptr;

  friend class PAGViewModel;
  friend class PAGScene;
  friend class DataBindRuntime;
};

}  // namespace pagx
