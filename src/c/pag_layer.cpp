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

#include "pag/c/pag_layer.h"
#include "pag_types_priv.h"

pag_layer_type pag_layer_get_layer_type(pag_layer* layer) {
  if (layer == nullptr) {
    return pag_layer_type_unknown;
  }
  pag_layer_type type = pag_layer_type_unknown;
  if (TocLayerType(layer->p->layerType(), &type)) {
    return type;
  }
  return pag_layer_type_unknown;
}

const char* pag_layer_get_layer_name(pag_layer* layer) {
  if (layer == nullptr) {
    return nullptr;
  }
  auto layerName = layer->p->layerName();
  auto length = layerName.length();
  auto name = malloc(sizeof(char) * (length + 1));
  memset(name, 0, length + 1);
  memcpy(name, layerName.c_str(), length);
  return static_cast<const char*>(name);
}

int64_t pag_layer_get_duration(pag_layer* layer) {
  if (layer == nullptr) {
    return 0;
  }
  return layer->p->duration();
}

float pag_layer_get_frame_rate(pag_layer* layer) {
  if (layer == nullptr) {
    return 0;
  }
  return layer->p->frameRate();
}

float pag_layer_get_alpha(pag_layer* layer) {
  if (layer == nullptr) {
    return 1.0f;
  }
  return layer->p->alpha();
}

void pag_layer_set_alpha(pag_layer* layer, float alpha) {
  if (layer == nullptr) {
    return;
  }
  layer->p->setAlpha(alpha);
}
