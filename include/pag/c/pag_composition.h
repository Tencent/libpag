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

#pragma once

#include "pag_types.h"

PAG_C_PLUS_PLUS_BEGIN_GUARD

PAG_EXPORT int pag_composition_get_width(pag_composition* composition);

PAG_EXPORT int pag_composition_get_height(pag_composition* composition);

/**
 * Returns an array of layers that match the specified layer name. Release it by pag_release.
 */
PAG_EXPORT pag_layer** pag_composition_get_layers_by_name(pag_composition* composition,
                                                       const char* layerName, size_t* count);

/**
 * Returns an array of layers that match the specified layer type. Release it by pag_release.
 */
PAG_EXPORT pag_layer** pag_composition_get_layers_by_type(pag_composition* composition,
                                                       pag_layer_type layerType, size_t* count);

PAG_C_PLUS_PLUS_END_GUARD
