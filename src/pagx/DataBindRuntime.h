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
#include "pagx/nodes/Channel.h"
#include "pagx/nodes/DataBind.h"

namespace pagx {

class DataContext;
class PAGViewModelValue;
class PAGXDocument;
struct RuntimeBinding;
class Node;
}  // namespace pagx

namespace pagx {

class DataBindRuntime {
 public:
  DataBindRuntime() = default;
  ~DataBindRuntime();

  void bind(const std::vector<DataBind*>& binds, DataContext* context, PAGXDocument* doc);

  void markDirtyForValue(PAGViewModelValue* value);

  // Re-marks every binding dirty so the next update() re-applies the ViewModel value to the target.
  // Called after a layer is refreshed in place: the rebuilt runtime targets reset their channels to
  // the node defaults, so the VM-driven values must be re-applied or they would be lost.
  void markAllDirty();

  void update(RuntimeBinding* binding, float mix = 1.0f);
  void syncBack(RuntimeBinding* binding);

 private:
  void markDirty(DataBind* bind);

  struct BindingEntry {
    DataBind* dataBind = nullptr;
    PAGViewModelValue* source = nullptr;
    std::weak_ptr<PAGViewModelValue> sourceGuard = {};
    const Node* targetNode = nullptr;
    std::string channel = {};
    bool onceApplied = false;       // Once: true after the single ViewModel → target application
    KeyValue lastTargetValue = {};  // Cached target value from the previous syncBack pass
    bool hasLastTarget = false;     // Whether lastTargetValue has been captured yet

    // ToSource/TwoWay flow target changes back to the ViewModel.
    bool toSource() const {
      return dataBind->flags == DataBindDirection::ToSource ||
             dataBind->flags == DataBindDirection::TwoWay;
    }
    // Everything except pure ToSource drives the target from the ViewModel.
    bool toTarget() const {
      return dataBind->flags != DataBindDirection::ToSource;
    }
  };

  std::vector<DataBind*> dirtyBinds = {};
  std::vector<BindingEntry> entries = {};

  static void WriteVmValue(PAGViewModelValue* value, const KeyValue& kv);
};

}  // namespace pagx
