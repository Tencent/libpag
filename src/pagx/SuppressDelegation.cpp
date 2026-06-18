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
#include "pagx/PAGScene.h"
#include "pagx/PAGViewModelValue.h"

namespace pagx {

SuppressDelegation::SuppressDelegation(const std::shared_ptr<PAGScene>& scn)
    : scene(scn) {
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
  // Deduplicate and notify each value once.
  bool duplicates = false;
  for (size_t i = 0; i < pending.size(); i++) {
    for (size_t j = i + 1; j < pending.size(); j++) {
      if (pending[i] == pending[j]) {
        duplicates = true;
        break;
      }
    }
  }
  if (!duplicates) {
    for (auto* value : pending) {
      if (value != nullptr) {
        value->notifyObservers();
      }
    }
  } else {
    // Simple dedup: use a set for the notify pass.
    std::vector<PAGViewModelValue*> unique = {};
    unique.reserve(pending.size());
    for (auto* value : pending) {
      if (value == nullptr) {
        continue;
      }
      bool found = false;
      for (auto* u : unique) {
        if (u == value) {
          found = true;
          break;
        }
      }
      if (!found) {
        unique.push_back(value);
        value->notifyObservers();
      }
    }
  }
  pending.clear();
}

bool SuppressDelegation::isSuppressed(const std::shared_ptr<PAGScene>& scene) {
  return scene != nullptr && scene->suppressNotify;
}

void SuppressDelegation::addPendingNotification(const std::shared_ptr<PAGScene>& scene,
                                                PAGViewModelValue* value) {
  if (scene != nullptr && value != nullptr) {
    scene->pendingNotifications.push_back(value);
  }
}

}  // namespace pagx
