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

#include "pagx/PAGViewModel.h"

namespace pagx {

// ---- Typed property accessors ------------------------------------------------

std::shared_ptr<PAGViewModelValueNumber> PAGViewModel::propertyNumber(const std::string& name) {
  auto it = propertyMap.find(name);
  if (it == propertyMap.end()) {
    return nullptr;
  }
  const auto type = it->second->valueType();
  if (type != ViewModelValueType::Number && type != ViewModelValueType::Enum) {
    return nullptr;
  }
  return std::static_pointer_cast<PAGViewModelValueNumber>(it->second);
}

std::shared_ptr<PAGViewModelValueString> PAGViewModel::propertyString(const std::string& name) {
  auto it = propertyMap.find(name);
  if (it == propertyMap.end()) {
    return nullptr;
  }
  if (it->second->valueType() != ViewModelValueType::String) {
    return nullptr;
  }
  return std::static_pointer_cast<PAGViewModelValueString>(it->second);
}

std::shared_ptr<PAGViewModelValueBoolean> PAGViewModel::propertyBoolean(const std::string& name) {
  auto it = propertyMap.find(name);
  if (it == propertyMap.end()) {
    return nullptr;
  }
  const auto type = it->second->valueType();
  if (type != ViewModelValueType::Boolean && type != ViewModelValueType::Trigger) {
    return nullptr;
  }
  return std::static_pointer_cast<PAGViewModelValueBoolean>(it->second);
}

std::shared_ptr<PAGViewModelValueColor> PAGViewModel::propertyColor(const std::string& name) {
  auto it = propertyMap.find(name);
  if (it == propertyMap.end()) {
    return nullptr;
  }
  if (it->second->valueType() != ViewModelValueType::Color) {
    return nullptr;
  }
  return std::static_pointer_cast<PAGViewModelValueColor>(it->second);
}

std::shared_ptr<PAGViewModelValueImage> PAGViewModel::propertyImage(const std::string& name) {
  auto it = propertyMap.find(name);
  if (it == propertyMap.end()) {
    return nullptr;
  }
  if (it->second->valueType() != ViewModelValueType::Image) {
    return nullptr;
  }
  return std::static_pointer_cast<PAGViewModelValueImage>(it->second);
}

std::shared_ptr<PAGViewModelValueViewModel> PAGViewModel::propertyViewModel(
    const std::string& name) {
  auto it = propertyMap.find(name);
  if (it == propertyMap.end()) {
    return nullptr;
  }
  if (it->second->valueType() != ViewModelValueType::ViewModel) {
    return nullptr;
  }
  return std::static_pointer_cast<PAGViewModelValueViewModel>(it->second);
}

// ---- Reflection --------------------------------------------------------------

std::vector<std::shared_ptr<PAGViewModelValue>> PAGViewModel::properties() const {
  return propertyList;
}

const std::string& PAGViewModel::viewModelId() const {
  return id;
}

void PAGViewModel::advancedAll() {
  for (const auto& value : propertyList) {
    value->advanced();
    if (value->valueType() == ViewModelValueType::ViewModel) {
      const auto& vmValue = std::static_pointer_cast<PAGViewModelValueViewModel>(value);
      const auto& child = vmValue->referenceViewModelInstance();
      if (child != nullptr) {
        child->advancedAll();
      }
    }
  }
}

PAGViewModelValue* PAGViewModel::findProperty(const std::string& name) const {
  auto it = propertyMap.find(name);
  return it != propertyMap.end() ? it->second.get() : nullptr;
}

}  // namespace pagx
