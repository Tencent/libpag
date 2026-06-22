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

namespace pagx {

class DataBind;
class DataContext;
class PAGViewModelValue;
class PAGXDocument;
struct RuntimeBinding;
class Node;
}

namespace tgfx {
class Layer;
}

namespace pagx {

class DataBindRuntime {
 public:
  DataBindRuntime() = default;
  ~DataBindRuntime();

  void bind(const std::vector<DataBind*>& binds, DataContext* context,
            RuntimeBinding* binding, PAGXDocument* doc);

  void markDirty(DataBind* bind);
  void markDirtyForValue(PAGViewModelValue* value);

  void update(RuntimeBinding* binding, float mix = 1.0f);
  void syncBack(RuntimeBinding* binding);
  void clearDirty();

 private:
  struct BindingEntry {
    DataBind* dataBind = nullptr;
    PAGViewModelValue* source = nullptr;
    const Node* targetNode = nullptr;
    std::string channel = {};
    bool isToSource = false;   // ToSource or TwoWay
    bool isToTarget = true;    // false for pure ToSource
    bool onceApplied = false;  // Once flag: applied once, skip thereafter
  };

  std::vector<DataBind*> dirtyBinds = {};
  std::vector<BindingEntry> entries = {};

  static float readTargetFloat(tgfx::Layer* layer, const std::string& channel,
                               float fallback);
};

}  // namespace pagx
