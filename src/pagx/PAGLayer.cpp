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

#include "pagx/PAGLayer.h"
#include "pagx/nodes/Layer.h"

namespace pagx {

struct PAGLayer::Impl {
  const Layer* node = nullptr;
};

PAGLayer::PAGLayer() : impl(std::make_unique<Impl>()) {
}

PAGLayer::~PAGLayer() = default;

std::shared_ptr<PAGLayer> PAGLayer::Wrap(const Layer* node) {
  if (node == nullptr) {
    return nullptr;
  }
  auto layer = std::shared_ptr<PAGLayer>(new PAGLayer());
  layer->impl->node = node;
  return layer;
}

std::string PAGLayer::getName() const {
  return impl->node != nullptr ? impl->node->name : std::string();
}

}  // namespace pagx
