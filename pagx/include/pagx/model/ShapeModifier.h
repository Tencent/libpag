/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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
#include "pagx/model/VectorElement.h"
#include "pagx/model/enums/MergePathMode.h"
#include "pagx/model/enums/TrimType.h"

namespace pagx {

/**
 * Trim path modifier.
 */
struct TrimPath : public VectorElement {
  float start = 0;
  float end = 1;
  float offset = 0;
  TrimType trimType = TrimType::Separate;

  NodeType type() const override {
    return NodeType::TrimPath;
  }

  std::unique_ptr<Node> clone() const override {
    return std::make_unique<TrimPath>(*this);
  }
};

/**
 * Round corner modifier.
 */
struct RoundCorner : public VectorElement {
  float radius = 10;

  NodeType type() const override {
    return NodeType::RoundCorner;
  }

  std::unique_ptr<Node> clone() const override {
    return std::make_unique<RoundCorner>(*this);
  }
};

/**
 * Merge path modifier.
 */
struct MergePath : public VectorElement {
  MergePathMode mode = MergePathMode::Append;

  NodeType type() const override {
    return NodeType::MergePath;
  }

  std::unique_ptr<Node> clone() const override {
    return std::make_unique<MergePath>(*this);
  }
};

}  // namespace pagx
