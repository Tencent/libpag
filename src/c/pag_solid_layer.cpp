/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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

#include "pag/c/pag_solid_layer.h"
#include "pag_types_priv.h"

pag_color pag_solid_layer_get_solid_color(pag_solid_layer* layer) {
  if (layer == nullptr) {
    return {0, 0, 0};
  }
  auto color = std::static_pointer_cast<pag::PAGSolidLayer>(layer->p)->solidColor();
  return pag_color{color.red, color.green, color.blue};
}

void pag_solid_layer_set_solid_color(pag_solid_layer* layer, pag_color color) {
  if (layer == nullptr) {
    return;
  }
  auto c = pag::Color{color.red, color.green, color.blue};
  std::static_pointer_cast<pag::PAGSolidLayer>(layer->p)->setSolidColor(c);
}
