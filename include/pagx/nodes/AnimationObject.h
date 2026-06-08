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

#include <string>
#include <vector>
#include "pagx/nodes/Node.h"

namespace pagx {

class Channel;

/**
 * AnimationObject groups the animated channels that target a single node within an Animation.
 * Each AnimationObject binds to one node (identified by target) and carries the set of channels
 * whose keyframes drive that node over time.
 */
class AnimationObject : public Node {
 public:
  /**
   * The id of the target node this AnimationObject drives. Resolved against the owning document's
   * id table when the runtime timeline is built.
   */
  std::string target = {};

  /**
   * The animated channels applied to the target node. Each Channel carries its own channel name
   * and keyframes.
   */
  std::vector<Channel*> channels = {};

  NodeType nodeType() const override {
    return NodeType::AnimationObject;
  }

 private:
  AnimationObject() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
