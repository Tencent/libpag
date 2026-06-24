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

#include "pagx/SuppressDelegation.h"
#include <unordered_set>
#include "pagx/PAGScene.h"
#include "pagx/PAGViewModelValue.h"

namespace pagx {

SuppressDelegation::SuppressDelegation(const std::shared_ptr<PAGScene>& scn) : scene(scn) {
  auto s = scn;
  if (s != nullptr) {
    wasAlreadySuppressed = s->suppressNotify;
    if (!wasAlreadySuppressed) {
      s->suppressNotify = true;
    }
  }
}

SuppressDelegation::~SuppressDelegation() {
  auto s = scene.lock();
  if (s == nullptr || wasAlreadySuppressed) {
    return;
  }
  s->suppressNotify = false;
  auto& pending = s->pendingNotifications;
  // Deduplicate with a set while preserving the original pending order for notify.
  std::unordered_set<PAGViewModelValue*> seen = {};
  seen.reserve(pending.size());
  for (auto* value : pending) {
    if (value == nullptr) {
      continue;
    }
    if (seen.insert(value).second) {
      value->notifyObservers();
    }
  }
  pending.clear();
}

bool SuppressDelegation::IsSuppressed(const std::shared_ptr<PAGScene>& scene) {
  return scene != nullptr && scene->suppressNotify;
}

void SuppressDelegation::AddPendingNotification(const std::shared_ptr<PAGScene>& scene,
                                                PAGViewModelValue* value) {
  if (scene != nullptr && value != nullptr) {
    scene->pendingNotifications.push_back(value);
  }
}

}  // namespace pagx
