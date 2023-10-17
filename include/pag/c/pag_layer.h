/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

PAG_API pag_layer_type pag_layer_get_layer_type(pag_layer* layer);

PAG_API const char* pag_layer_get_layer_name(pag_layer* layer);

PAG_API int64_t pag_layer_get_duration(pag_layer* layer);

PAG_API float pag_layer_get_frame_rate(pag_layer* layer);

PAG_API float pag_layer_get_alpha(pag_layer* layer);

PAG_API void pag_layer_set_alpha(pag_layer* layer, float alpha);

PAG_C_PLUS_PLUS_END_GUARD
