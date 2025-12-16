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

/**
 * Creates a new pag_decoder with a pag_composition, a frame rate limit, and a scale factor for the
 * decoded image size. Release it by pag_release.
 */
PAG_EXPORT pag_decoder* pag_decoder_create(pag_composition* composition, float maxFrameRate,
                                        float scale);

PAG_EXPORT int pag_decoder_get_width(pag_decoder* decoder);

PAG_EXPORT int pag_decoder_get_height(pag_decoder* decoder);

PAG_EXPORT int pag_decoder_get_num_frames(pag_decoder* decoder);

PAG_EXPORT float pag_decoder_get_frame_rate(pag_decoder* decoder);

PAG_EXPORT bool pag_decoder_check_frame_changed(pag_decoder* decoder, int index);

PAG_EXPORT bool pag_decoder_read_frame(pag_decoder* decoder, int index, void* pixels, size_t rowBytes,
                                    pag_color_type colorType, pag_alpha_type alphaType);

PAG_EXPORT bool pag_decoder_read_frame_to_hardware_buffer(pag_decoder* decoder, int index,
                                                       void* buffer);

PAG_C_PLUS_PLUS_END_GUARD
