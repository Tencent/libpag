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
 * Registers a font required by the text layers in pag files, so that they can be rendered as
 * designed. Release it by pag_release.
 */
PAG_EXPORT pag_font* pag_font_register_from_path(const char* filePath, int ttcIndex,
                                              const char* fontFamily, const char* fontStyle);
/**
 * Registers a font required by the text layers in pag files, so that they can be rendered as
 * designed. Release it by pag_release.
 */
PAG_EXPORT pag_font* pag_font_register_from_data(const void* data, size_t length, int ttcIndex,
                                              const char* fontFamily, const char* fontStyle);

PAG_EXPORT const char* pag_font_get_family(pag_font* font);

PAG_EXPORT const char* pag_font_get_style(pag_font* font);

PAG_C_PLUS_PLUS_END_GUARD
