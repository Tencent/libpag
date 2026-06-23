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

#include <memory>
#include "pagx/PAGViewModelValue.h"

namespace pagx {

class PAGViewModel;

/**
 * PAGViewModelValueViewModel holds a reference to a nested PAGViewModel instance. When a ViewModel
 * property has type "ViewModel", this value wraps the child PAGViewModel that was independently
 * instantiated from the referenced ViewModel schema.
 */
class PAGViewModelValueViewModel : public PAGViewModelValue {
 public:
  /**
   * Returns the referenced nested PAGViewModel instance, or nullptr if none is set.
   */
  std::shared_ptr<PAGViewModel> referenceViewModel() const {
    return referenceInstance;
  }

 private:
  std::shared_ptr<PAGViewModel> referenceInstance = nullptr;

  friend class PAGViewModel;
  friend class PAGScene;
};

}  // namespace pagx
