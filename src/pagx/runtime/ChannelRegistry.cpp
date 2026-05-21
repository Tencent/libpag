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

#include "pagx/runtime/ChannelRegistry.h"

namespace pagx {

// Channel registration entry points implemented in channels/*.cpp.
void RegisterLayerChannels(ChannelRegistry::Registrar& reg);
void RegisterSolidColorChannels(ChannelRegistry::Registrar& reg);
void RegisterColorStopChannels(ChannelRegistry::Registrar& reg);
void RegisterBlurFilterChannels(ChannelRegistry::Registrar& reg);
void RegisterDropShadowFilterChannels(ChannelRegistry::Registrar& reg);
void RegisterDropShadowStyleChannels(ChannelRegistry::Registrar& reg);

const ChannelRegistry& ChannelRegistry::Get() {
  static const ChannelRegistry instance;
  return instance;
}

ChannelRegistry::ChannelRegistry() {
  Registrar reg(this);
  RegisterLayerChannels(reg);
  RegisterSolidColorChannels(reg);
  RegisterColorStopChannels(reg);
  RegisterBlurFilterChannels(reg);
  RegisterDropShadowFilterChannels(reg);
  RegisterDropShadowStyleChannels(reg);
}

const ChannelDescriptor* ChannelRegistry::find(NodeType targetType,
                                               const std::string& name) const {
  auto key = std::to_string(static_cast<int>(targetType)) + ":" + name;
  auto it = channels.find(key);
  return it != channels.end() ? &it->second : nullptr;
}

void ChannelRegistry::registerChannel(ChannelDescriptor desc) {
  auto key = std::to_string(static_cast<int>(desc.targetType)) + ":" + desc.name;
  channels.emplace(std::move(key), std::move(desc));
}

ChannelRegistry::Registrar::Registrar(ChannelRegistry* registry) : registry(registry) {
}

void ChannelRegistry::Registrar::add(ChannelDescriptor desc) const {
  registry->registerChannel(std::move(desc));
}

}  // namespace pagx
