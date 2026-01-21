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
#include <string>
#include <vector>
#include "pagx/model/Resource.h"

namespace pagx {

class Layer;

/**
 * Composition resource.
 */
class Composition : public Resource {
 public:
  std::string id = {};
  float width = 0;
  float height = 0;
  std::vector<std::unique_ptr<Layer>> layers = {};

  ResourceType type() const override {
    return ResourceType::Composition;
  }

  const std::string& resourceId() const override {
    return id;
  }
};

}  // namespace pagx
