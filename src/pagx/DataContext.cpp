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

#include "pagx/DataContext.h"
#include "pagx/PAGViewModel.h"
#include "pagx/PAGViewModelValue.h"
#include "pagx/PAGViewModelValueViewModel.h"

namespace pagx {

DataContext::DataContext(std::shared_ptr<PAGViewModel> viewModel) : vm(std::move(viewModel)) {
}

void DataContext::parent(std::shared_ptr<DataContext> parentCtx) {
  parentContext = std::move(parentCtx);
}

std::shared_ptr<PAGViewModel> DataContext::viewModelInstance() const {
  return vm;
}

PAGViewModelValue* DataContext::resolve(const std::vector<std::string>& path) const {
  // ViewModel-typed values are containers for nested properties, not bindable leaf
  // values. Descending into the reference and returning nullptr at the end of a
  // ViewModel-only path is intentional: only concrete leaf values are bindable.
  if (path.empty() || vm == nullptr) {
    return nullptr;
  }
  // The first segment is expected to be "$vm". Skip it and resolve the rest.
  size_t start = 0;
  if (path[0] == "$vm") {
    start = 1;
  }
  if (start >= path.size()) {
    return nullptr;
  }

  // Walk the path segments through the local VM first.
  std::shared_ptr<PAGViewModel> currentVM = vm;
  for (size_t i = start; i < path.size(); i++) {
    if (currentVM == nullptr) {
      if (parentContext != nullptr) {
        return parentContext->resolve(path);
      }
      break;
    }
    auto* value = currentVM->findProperty(path[i]);
    if (value == nullptr) {
      // Not found in current VM — try the parent chain.
      if (parentContext != nullptr) {
        return parentContext->resolve(path);
      }
      return nullptr;
    }
    // Check if value is a nested ViewModel reference. If so, descend into it.
    if (value->valueType() == ViewModelValueType::ViewModel) {
      auto* vmValue = static_cast<PAGViewModelValueViewModel*>(value);
      currentVM = vmValue->referenceViewModelInstance();
    } else if (i == path.size() - 1) {
      // Last segment and a leaf value — found.
      return value;
    } else {
      // Mid-path and not a ViewModel — can't descend further.
      if (parentContext != nullptr) {
        return parentContext->resolve(path);
      }
      return nullptr;
    }
  }
  return nullptr;
}

}  // namespace pagx
