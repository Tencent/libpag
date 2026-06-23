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
#include <string>
#include <unordered_map>
#include <vector>
#include "pagx/PAGViewModelValue.h"
#include "pagx/PAGViewModelValueBoolean.h"
#include "pagx/PAGViewModelValueColor.h"
#include "pagx/PAGViewModelValueImage.h"
#include "pagx/PAGViewModelValueNumber.h"
#include "pagx/PAGViewModelValueString.h"
#include "pagx/PAGViewModelValueViewModel.h"

namespace pagx {

class DataContext;
class ViewModel;

/**
 * PAGViewModel holds the runtime property values for one ViewModel instance. It provides
 * name-based typed access to individual properties (propertyNumber, propertyString, etc.)
 * and supports reflection via properties().
 *
 * Instances are created by PAGScene from the document's ViewModel schema. Each scene creates
 * independent instances; nested Composition ViewModels are also independently instantiated.
 *
 * Business-side usage:
 *   auto vm = scene->viewModel();
 *   vm->propertyNumber("speed")->value(2.0f);
 *   vm->propertyColor("theme")->value(pagx::Color{1, 0, 0, 1});
 *   vm->propertyViewModel("logo")->referenceViewModelInstance();
 */
class PAGViewModel {
 public:
  /**
   * Returns the number-typed property with the given name. Returns nullptr if no such property
   * exists or its type does not match.
   */
  std::shared_ptr<PAGViewModelValueNumber> propertyNumber(const std::string& name);

  /**
   * Returns the string-typed property with the given name. Returns nullptr if no such property
   * exists or its type does not match.
   */
  std::shared_ptr<PAGViewModelValueString> propertyString(const std::string& name);

  /**
   * Returns the boolean-typed property with the given name. Returns nullptr if no such property
   * exists or its type does not match.
   */
  std::shared_ptr<PAGViewModelValueBoolean> propertyBoolean(const std::string& name);

  /**
   * Returns the color-typed property with the given name. Returns nullptr if no such property
   * exists or its type does not match.
   */
  std::shared_ptr<PAGViewModelValueColor> propertyColor(const std::string& name);

  /**
   * Returns the image-typed property with the given name. Returns nullptr if no such property
   * exists or its type does not match.
   */
  std::shared_ptr<PAGViewModelValueImage> propertyImage(const std::string& name);

  /**
   * Returns the nested-ViewModel-typed property with the given name. Returns nullptr if no such
   * property exists or its type does not match.
   */
  std::shared_ptr<PAGViewModelValueViewModel> propertyViewModel(const std::string& name);

  /**
   * Returns all properties in declaration order. Used for reflection-driven UI generation or
   * iterating over the full property set.
   */
  std::vector<std::shared_ptr<PAGViewModelValue>> properties() const;

  /**
   * Returns the ViewModel id as declared in the schema, or an empty string if none.
   */
  const std::string& viewModelId() const;

  /**
   * Resets the dirty flag of every property in this ViewModel, and for nested ViewModel-typed
   * properties recursively resets the referenced child ViewModel as well. Intended to be called
   * once per frame after all DataBinds have been applied, so that hasChanged() reflects only
   * changes that occur within the next frame.
   */
  void advancedAll();

 private:
  PAGViewModel() = default;

  /**
   * Internal lookup: returns the raw PAGViewModelValue pointer for the given property name,
   * or nullptr if not found. Used by DataContext for path resolution.
   */
  PAGViewModelValue* findProperty(const std::string& name) const;

  std::string id = {};
  std::unordered_map<std::string, std::shared_ptr<PAGViewModelValue>> propertyMap = {};
  std::vector<std::shared_ptr<PAGViewModelValue>> propertyList = {};

  friend class PAGScene;
  friend class DataContext;
};

}  // namespace pagx
