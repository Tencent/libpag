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
#include <algorithm>
#include <sstream>
#include "pagx/DataContext.h"
#include "pagx/DataConverterRegistry.h"
#include "pagx/PAGViewModelValue.h"
#include "pagx/PAGViewModelValueBoolean.h"
#include "pagx/PAGViewModelValueColor.h"
#include "pagx/PAGViewModelValueImage.h"
#include "pagx/PAGViewModelValueNumber.h"
#include "pagx/PAGViewModelValueString.h"
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Channel.h"
#include "pagx/nodes/DataBind.h"
#include "pagx/nodes/DataConverter.h"
#include "pagx/types/Color.h"
#include "renderer/LayerBuilder.h"
#include "tgfx/layers/Layer.h"

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

static std::vector<std::string> parseSourcePath(const std::string& source) {
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
    auto path = parseSourcePath(db->source);
    auto* sourceValue = context->resolve(path);
    if (sourceValue == nullptr) {
      continue;
    }
    const Node* targetNode = nullptr;
    if (!db->target.empty() && db->target[0] == '@') {
      targetNode = doc->findNode(db->target.substr(1));
    }
    if (targetNode == nullptr) {
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

    // Mark dirty so the default value is applied on the first draw.
    markDirty(db);
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

// ---- update (ViewModel → render node) ----------------------------------------

static KeyValue valueToKeyValue(PAGViewModelValue* value) {
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
      return KeyValue{Color{c.red, c.green, c.blue, c.alpha}};
    }
    case ViewModelValueType::Image:
      return KeyValue{static_cast<PAGViewModelValueImage*>(value)->value()};
    case ViewModelValueType::Enum:
      return KeyValue{static_cast<PAGViewModelValueNumber*>(value)->value()};
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
    auto keyValue = valueToKeyValue(entry->source);
    if (entry->source->converter != nullptr) {
      keyValue = DataConverterRegistry::instance().apply(entry->source->converter, keyValue);
    }
    binding->apply(entry->targetNode, entry->channel, keyValue, mix);
  }
}

// ---- syncBack (render node → ViewModel) --------------------------------------

float DataBindRuntime::readTargetFloat(tgfx::Layer* layer, const std::string& channel,
                                       float fallback) {
  if (layer == nullptr) {
    return fallback;
  }
  if (channel == "alpha") {
    return layer->alpha();
  }
  if (channel == "x") {
    return layer->matrix().getTranslateX();
  }
  if (channel == "y") {
    return layer->matrix().getTranslateY();
  }
  return fallback;
}

static void writeVmFromFloat(PAGViewModelValue* value, float f) {
  // Use the normal setter to notify observers (consistent with Rive's TwoWay).
  // The same-value check in value() prevents unnecessary dirty/loop.
  if (value == nullptr) {
    return;
  }
  if (value->valueType() == ViewModelValueType::Number) {
    static_cast<PAGViewModelValueNumber*>(value)->value(f);
  } else if (value->valueType() == ViewModelValueType::Boolean) {
    static_cast<PAGViewModelValueBoolean*>(value)->value(f != 0.0f);
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
    auto layer = binding->get<tgfx::Layer>(entry.targetNode);
    if (layer == nullptr) {
      continue;
    }
    // syncBack only supports float-readable channels. Skipping unsupported channels
    // avoids writing the readTargetFloat fallback (0.0f) into the VM.
    if (entry.channel != "alpha" && entry.channel != "x" && entry.channel != "y") {
      continue;
    }
    float current = readTargetFloat(layer.get(), entry.channel, 0.0f);
    // Apply inverse converter for syncBack direction (layer value → VM domain).
    KeyValue kv{current};
    kv = DataConverterRegistry::instance().applyInverse(entry.source->converter, kv);
    writeVmFromFloat(entry.source, std::get<float>(kv));
  }
}

// ---- clearDirty --------------------------------------------------------------

void DataBindRuntime::clearDirty() {
  dirtyBinds.clear();
}

}  // namespace pagx