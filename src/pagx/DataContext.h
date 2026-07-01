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
#include <vector>

namespace pagx {

class PAGViewModel;
class PAGViewModelValue;

/**
 * DataContext holds a ViewModel instance and an optional parent pointer, forming a linked list
 * that enables DataBind source path resolution to traverse up through nested compositions.
 *
 * Resolution: "resolve(path)" walks the path segments through the local ViewModel first,
 * descending into PAGViewModelValueViewModel references for nested paths. If a segment is not
 * found, the chain falls back to parentContext and retries.
 */
class DataContext {
 public:
  explicit DataContext(std::shared_ptr<PAGViewModel> vm);

  void parent(std::shared_ptr<DataContext> parentContext);

  std::shared_ptr<PAGViewModel> viewModelInstance() const;

  /**
   * Resolves a dot-separated path (e.g. {"$vm", "theme"} or {"$vm", "card", "price"}) to a
   * PAGViewModelValue within the DataContext chain. Returns nullptr if not found.
   */
  PAGViewModelValue* resolve(const std::vector<std::string>& path) const;

 private:
  std::shared_ptr<PAGViewModel> vm;
  std::shared_ptr<DataContext> parentContext;
};

}  // namespace pagx
