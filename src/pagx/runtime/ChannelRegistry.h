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

#include <functional>
#include <string>
#include <unordered_map>
#include "pagx/runtime/AnimContext.h"

namespace pagx {

enum class ChannelValueType { Float, Bool, Enum, String, ImageRef, Color };

struct ChannelDescriptor {
  NodeType targetType = NodeType::Document;
  std::string name = {};
  ChannelValueType valueType = ChannelValueType::Float;
  std::function<void(const AnimContext&, const KeyValue&)> writer;
};

// Registry of channel descriptors keyed by (NodeType, channel name). The registry is a process-
// wide singleton populated at first access; entries are immutable after registration.
class ChannelRegistry {
 public:
  // Helper passed to channel registration functions to avoid friending each translation unit.
  class Registrar {
   public:
    void add(ChannelDescriptor desc) const;

   private:
    explicit Registrar(ChannelRegistry* registry);

    ChannelRegistry* registry = nullptr;

    friend class ChannelRegistry;
  };

  static const ChannelRegistry& Get();

  // Returns the descriptor for the given target type and channel name, or nullptr if no descriptor
  // matches.
  const ChannelDescriptor* find(NodeType targetType, const std::string& name) const;

 private:
  ChannelRegistry();

  void registerChannel(ChannelDescriptor desc);

  // Key composed as "{targetType}:{name}". Channel names are short ASCII strings, so string keys
  // keep the registry simple and avoid a custom hasher.
  std::unordered_map<std::string, ChannelDescriptor> channels = {};
};

}  // namespace pagx
