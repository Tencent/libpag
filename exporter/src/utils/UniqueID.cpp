/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 Tencent. All rights reserved.
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

#include "UniqueID.h"

namespace exporter {
pag::ID GetLayerUniqueID(const std::vector<pag::Composition*>& compositions) {
  pag::ID id = 2000;
  for (const auto& composition : compositions) {
    if (composition->type() == pag::CompositionType::Vector) {
      for (const auto& layer : static_cast<pag::VectorComposition*>(composition)->layers) {
        if (id <= layer->id) {
          id = layer->id + 1;
        }
      }
    }
  }
  return id;
}

pag::ID GetCompositionUniqueID(const std::vector<pag::Composition*>& compositions) {
  pag::ID id = 1000;
  for (const auto& composition : compositions) {
    if (id <= composition->id) {
      id = composition->id + 1;
    }
  }
  return id;
}

}  // namespace exporter
