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
#include "pagx/ObserverHandle.h"
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

  // Resolves each DataBind's source path against the DataContext and its target node against the
  // document, then registers this runtime as a dependent of every resolved ViewModel value. A
  // binding that drives the target is marked dirty so the first update() applies the configured
  // default. Bindings whose source or target cannot be resolved are skipped. binding is the owning
  // composition's RuntimeBinding, borrowed so that a later ViewModel change can apply to the target
  // immediately; the composition owns it and outlives this runtime, so the pointer stays valid.
  void bind(const std::vector<DataBind*>& binds, DataContext* context, PAGXDocument* doc,
            RuntimeBinding* binding);

  // Applies the change of a ViewModel value to its bound targets immediately through the borrowed
  // RuntimeBinding so queries that read render geometry (bounds, hit testing) observe the new value
  // without waiting for the next draw. Only bindings that drive the target are applied; pure
  // ToSource bindings are left untouched.
  void markDirtyForValue(PAGViewModelValue* value);

  // Re-marks every binding dirty so the next update() re-applies the ViewModel value to the target.
  // Called after a layer is refreshed in place: the rebuilt runtime targets reset their channels to
  // the node defaults, so the VM-driven values must be re-applied or they would be lost.
  void markAllDirty();

  // Applies pending dirty bindings to their target nodes through the RuntimeBinding (ViewModel →
  // render node). Each dirty source value is run through its converter and written to the target
  // channel with the given mix weight. Once bindings apply a single time and are then skipped.
  void update(RuntimeBinding* binding, float mix = 1.0f);

  // Reads bound target channels back into their ViewModel sources through the RuntimeBinding
  // (render node → ViewModel). Only ToSource/TwoWay bindings flow back, dirty bindings are skipped,
  // and a write occurs only when the target value actually changed since the previous pass.
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
      return dataBind->direction == DataBindDirection::ToSource ||
             dataBind->direction == DataBindDirection::TwoWay;
    }
    // Everything except pure ToSource drives the target from the ViewModel.
    bool toTarget() const {
      return dataBind->direction != DataBindDirection::ToSource;
    }
  };

  std::vector<DataBind*> dirtyBinds = {};
  std::vector<BindingEntry> entries = {};
  std::unordered_map<DataBind*, ObserverHandle> triggerHandles = {};
  // Borrowed from the owning composition (not owned). The composition holds both the RuntimeBinding
  // and this runtime and destroys them together, so the pointer is valid for this runtime's life.
  RuntimeBinding* boundBinding = nullptr;

  // Applies a single binding entry's ViewModel value to its target channel through binding, running
  // the source through its converter. Handles the Once direction's single-application guard. Shared
  // by update() and the immediate-apply path in markDirtyForValue().
  void applyEntry(BindingEntry& entry, RuntimeBinding* binding, float mix);

  static void WriteVmValue(PAGViewModelValue* value, const KeyValue& kv);
};

}  // namespace pagx
