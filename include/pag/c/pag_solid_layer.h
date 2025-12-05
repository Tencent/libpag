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

PAG_EXPORT pag_color pag_solid_layer_get_solid_color(pag_solid_layer* layer);

PAG_EXPORT void pag_solid_layer_set_solid_color(pag_solid_layer* layer, pag_color color);

PAG_C_PLUS_PLUS_END_GUARD
