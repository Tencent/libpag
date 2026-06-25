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

#include "pagx/DataBindRuntime.h"
#include <cmath>
#include <sstream>
#include "base/utils/Log.h"
#include "pagx/DataContext.h"
#include "pagx/DataConverterRegistry.h"
#include "pagx/PAGViewModelValue.h"
#include "pagx/PAGViewModelValueBoolean.h"
#include "pagx/PAGViewModelValueColor.h"
#include "pagx/PAGViewModelValueEnum.h"
#include "pagx/PAGViewModelValueImage.h"
#include "pagx/PAGViewModelValueNumber.h"
#include "pagx/PAGViewModelValueString.h"
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Channel.h"
#include "pagx/nodes/DataBind.h"
#include "pagx/nodes/DataConverter.h"
#include "pagx/types/Color.h"
#include "renderer/LayerBuilder.h"

namespace pagx {

// ---- Destructor — cleanup dependents ---------------------------------------

DataBindRuntime::~DataBindRuntime() {
  for (auto& entry : entries) {
    if (entry.source != nullptr && entry.sourceGuard.lock() != nullptr) {
      entry.source->removeDependent(this);
    }
  }
}

// ---- Source path parsing -----------------------------------------------------

static std::vector<std::string> ParseSourcePath(const std::string& source) {
  std::vector<std::string> parts;
  std::istringstream stream(source);
  std::string token;
  while (std::getline(stream, token, '.')) {
    if (!token.empty()) {
      parts.push_back(token);
    }
  }
  return parts;
}

// ---- bind --------------------------------------------------------------------

void DataBindRuntime::bind(const std::vector<DataBind*>& binds, DataContext* context,
                           PAGXDocument* doc) {
  for (auto* db : binds) {
    if (db == nullptr || context == nullptr || doc == nullptr) {
      continue;
    }
    auto path = ParseSourcePath(db->source);
    auto* sourceValue = context->resolve(path);
    if (sourceValue == nullptr) {
      LOGE("DataBind skipped: source '%s' did not resolve to a ViewModel property.",
           db->source.c_str());
      continue;
    }
    const Node* targetNode = nullptr;
    if (!db->target.empty() && db->target[0] == '@') {
      targetNode = doc->findNode(db->target.substr(1));
    }
    if (targetNode == nullptr) {
      LOGE("DataBind skipped: target '%s' was not found in the document.", db->target.c_str());
      continue;
    }
    sourceValue->addDependent(this);

    BindingEntry entry;
    entry.dataBind = db;
    entry.source = sourceValue;
    entry.sourceGuard = sourceValue->weak_from_this();
    entry.targetNode = targetNode;
    entry.channel = db->channel;
    entries.push_back(entry);

    // Mark dirty so the first update() applies the ViewModel's configured default to the target.
    // Only ToTarget/TwoWay bindings drive the target, so a pure ToSource binding is not dirtied
    // here (it only reads the target back into the ViewModel during syncBack).
    if (entry.toTarget()) {
      markDirty(db);
    }
  }
}

// ---- dirty tracking ----------------------------------------------------------

void DataBindRuntime::markDirty(DataBind* bind) {
  if (bind == nullptr) {
    return;
  }
  for (auto* existing : dirtyBinds) {
    if (existing == bind) {
      return;
    }
  }
  dirtyBinds.push_back(bind);
}

void DataBindRuntime::markDirtyForValue(PAGViewModelValue* value) {
  if (value == nullptr) {
    return;
  }
  for (auto& entry : entries) {
    if (entry.source == value) {
      markDirty(entry.dataBind);
    }
  }
}

void DataBindRuntime::markAllDirty() {
  // Re-mark every binding so update() re-applies the ViewModel value to the rebuilt target. A Once
  // binding keeps onceApplied==true and is therefore skipped in update(): it is fire-and-forget, so
  // a refresh does not restore its value (only ToTarget/TwoWay follow the ViewModel continuously).
  for (auto& entry : entries) {
    markDirty(entry.dataBind);
  }
}

// ---- update (ViewModel → render node) ----------------------------------------

static KeyValue ValueToKeyValue(PAGViewModelValue* value) {
  if (value == nullptr) {
    return KeyValue{0.0f};
  }
  switch (value->valueType()) {
    case ViewModelValueType::Number:
      return KeyValue{static_cast<PAGViewModelValueNumber*>(value)->value()};
    case ViewModelValueType::String:
      return KeyValue{static_cast<PAGViewModelValueString*>(value)->value()};
    case ViewModelValueType::Boolean:
      return KeyValue{static_cast<PAGViewModelValueBoolean*>(value)->value()};
    case ViewModelValueType::Color: {
      auto c = static_cast<PAGViewModelValueColor*>(value)->value();
      return KeyValue{c};
    }
    case ViewModelValueType::Image:
      return KeyValue{static_cast<PAGViewModelValueImage*>(value)->value()};
    case ViewModelValueType::Enum:
      return KeyValue{static_cast<PAGViewModelValueEnum*>(value)->value()};
    case ViewModelValueType::Trigger:
      return KeyValue{static_cast<PAGViewModelValueBoolean*>(value)->value()};
    default:
      return KeyValue{0.0f};
  }
}

void DataBindRuntime::update(RuntimeBinding* binding, float mix) {
  if (dirtyBinds.empty() || binding == nullptr) {
    return;
  }
  auto pending = std::move(dirtyBinds);
  dirtyBinds.clear();

  for (auto* db : pending) {
    if (db == nullptr) {
      continue;
    }
    BindingEntry* entry = nullptr;
    for (auto& e : entries) {
      if (e.dataBind == db) {
        entry = &e;
        break;
      }
    }
    if (entry == nullptr || entry->source == nullptr || entry->sourceGuard.lock() == nullptr ||
        entry->targetNode == nullptr || entry->dataBind == nullptr) {
      continue;
    }
    // Pure ToSource bindings never drive the target.
    if (!entry->toTarget()) {
      continue;
    }
    // Once: apply the ViewModel value to the target a single time, then skip.
    if (entry->dataBind->flags == DataBindDirection::Once && entry->onceApplied) {
      continue;
    }
    auto keyValue = ValueToKeyValue(entry->source);
    if (entry->source->converter != nullptr) {
      keyValue = DataConverterRegistry::instance().apply(entry->source->converter, keyValue);
    }
    binding->apply(entry->targetNode, entry->channel, keyValue, mix);
    if (entry->dataBind->flags == DataBindDirection::Once) {
      entry->onceApplied = true;
    }
  }
}

// ---- syncBack (render node → ViewModel) --------------------------------------

static float KeyValueToFloat(const KeyValue& kv) {
  if (auto* f = std::get_if<float>(&kv)) {
    return *f;
  }
  if (auto* i = std::get_if<int>(&kv)) {
    return static_cast<float>(*i);
  }
  // bool is a distinct KeyValue alternative; a Boolean/Trigger channel reads back as a bool, so
  // map it explicitly to 1.0f/0.0f instead of falling through to the zero default.
  if (auto* b = std::get_if<bool>(&kv)) {
    return *b ? 1.0f : 0.0f;
  }
  return 0.0f;
}

static int KeyValueToInt(const KeyValue& kv) {
  if (auto* i = std::get_if<int>(&kv)) {
    return *i;
  }
  // A converter that outputs a float (or a writeback with no converter) is rounded to the nearest
  // option index, since an enum value is an index into the option list.
  if (auto* f = std::get_if<float>(&kv)) {
    return static_cast<int>(std::lround(*f));
  }
  return 0;
}

void DataBindRuntime::WriteVmValue(PAGViewModelValue* value, const KeyValue& kv) {
  // syncBack writeback: route through setValueInternal(v, false) so observers are notified
  // (two-way binding semantics) but dirty/dependents are not triggered, preventing a shared
  // source from causing other bindings to be wrongly skipped in the same syncBack pass. The
  // converter (if any) decides the value type; this picks the matching alternative per VM type.
  if (value == nullptr) {
    return;
  }
  switch (value->valueType()) {
    case ViewModelValueType::Number:
      static_cast<PAGViewModelValueNumber*>(value)->setValueInternal(KeyValueToFloat(kv), false);
      break;
    case ViewModelValueType::Boolean:
    case ViewModelValueType::Trigger:
      static_cast<PAGViewModelValueBoolean*>(value)->setValueInternal(KeyValueToFloat(kv) != 0.0f,
                                                                      false);
      break;
    case ViewModelValueType::Enum:
      static_cast<PAGViewModelValueEnum*>(value)->setValueInternal(KeyValueToInt(kv), false);
      break;
    case ViewModelValueType::String:
      if (auto* s = std::get_if<std::string>(&kv)) {
        static_cast<PAGViewModelValueString*>(value)->setValueInternal(*s, false);
      }
      break;
    case ViewModelValueType::Color:
      if (auto* c = std::get_if<Color>(&kv)) {
        static_cast<PAGViewModelValueColor*>(value)->setValueInternal(*c, false);
      }
      break;
    case ViewModelValueType::Image:
      if (auto* img = std::get_if<std::shared_ptr<PAGImage>>(&kv)) {
        static_cast<PAGViewModelValueImage*>(value)->setValueInternal(*img, false);
      }
      break;
    default:
      break;
  }
}

void DataBindRuntime::syncBack(RuntimeBinding* binding) {
  if (binding == nullptr) {
    return;
  }
  for (auto& entry : entries) {
    if (entry.source == nullptr || entry.sourceGuard.lock() == nullptr ||
        entry.targetNode == nullptr) {
      continue;
    }
    // Only ToSource/TwoWay bindings flow target changes back to the ViewModel.
    if (!entry.toSource()) {
      continue;
    }
    // Skip dirty bindings — VM has pending changes that update() will apply.
    bool isDirty = false;
    for (auto* dirty : dirtyBinds) {
      if (dirty == entry.dataBind) {
        isDirty = true;
        break;
      }
    }
    if (isDirty) {
      continue;
    }
    // Read the current channel value back from the target's runtime object. A channel with no
    // registered reader (forward-only) returns false and is skipped, so the VM is not polluted.
    KeyValue kv{};
    if (!binding->read(entry.targetNode, entry.channel, &kv)) {
      continue;
    }
    // Change detection: only write back when the target value actually changed since the previous
    // pass (or on the first pass, to establish a baseline). This keeps repeated syncBacks
    // idempotent and, when one ViewModel property is bound to several targets, lets only the
    // target that actually changed write back instead of every binding overwriting the source.
    //
    // Image values are special: the reader wraps the target's tgfx::Image in a fresh PAGImage every
    // call, so wrapper-pointer equality on KeyValue would never match. Compare the underlying
    // tgfx::Image against the ViewModel's current image instead, so an unchanged image neither
    // overwrites the source URI nor fires observers, while a genuine image change still syncs back.
    if (entry.source->valueType() == ViewModelValueType::Image) {
      auto* imageValue = static_cast<PAGViewModelValueImage*>(entry.source);
      auto* targetImage = std::get_if<std::shared_ptr<PAGImage>>(&kv);
      auto targetTGFX = targetImage != nullptr ? LayerBuilder::GetTGFXImage(*targetImage) : nullptr;
      if (LayerBuilder::GetTGFXImage(imageValue->value()) == targetTGFX) {
        continue;
      }
    } else if (entry.hasLastTarget && entry.lastTargetValue == kv) {
      continue;
    }
    entry.lastTargetValue = kv;
    entry.hasLastTarget = true;
    // Apply inverse converter for syncBack direction (layer value → VM domain).
    auto vmValue = DataConverterRegistry::instance().applyInverse(entry.source->converter, kv);
    WriteVmValue(entry.source, vmValue);
  }
}

}  // namespace pagx