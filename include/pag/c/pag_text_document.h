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

PAG_EXPORT bool pag_text_document_get_apply_fill(pag_text_document* document);

PAG_EXPORT void pag_text_document_set_apply_fill(pag_text_document* document, bool applyFill);

PAG_EXPORT bool pag_text_document_get_apply_stroke(pag_text_document* document);

PAG_EXPORT void pag_text_document_set_apply_stroke(pag_text_document* document, bool applyStroke);

PAG_EXPORT float pag_text_document_get_baseline_shift(pag_text_document* document);

PAG_EXPORT void pag_text_document_set_baseline_shift(pag_text_document* document, float baselineShift);

PAG_EXPORT bool pag_text_document_get_box_text(pag_text_document* document);

PAG_EXPORT void pag_text_document_set_box_text(pag_text_document* document, bool boxText);

PAG_EXPORT pag_point pag_text_document_get_box_text_pos(pag_text_document* document);

PAG_EXPORT void pag_text_document_set_box_text_pos(pag_text_document* document, pag_point boxTextPos);

PAG_EXPORT pag_point pag_text_document_get_box_text_size(pag_text_document* document);

PAG_EXPORT void pag_text_document_set_box_text_size(pag_text_document* document,
                                                 pag_point boxTextSize);

PAG_EXPORT float pag_text_document_get_first_baseline(pag_text_document* document);

PAG_EXPORT void pag_text_document_set_first_baseline(pag_text_document* document, float firstBaseline);

PAG_EXPORT bool pag_text_document_get_faux_bold(pag_text_document* document);

PAG_EXPORT void pag_text_document_set_faux_bold(pag_text_document* document, bool fauxBold);

PAG_EXPORT bool pag_text_document_get_faux_italic(pag_text_document* document);

PAG_EXPORT void pag_text_document_set_faux_italic(pag_text_document* document, bool fauxItalic);

PAG_EXPORT pag_color pag_text_document_get_fill_color(pag_text_document* document);

PAG_EXPORT void pag_text_document_set_fill_color(pag_text_document* document, pag_color fillColor);

PAG_EXPORT const char* pag_text_document_get_font_family(pag_text_document* document);

PAG_EXPORT void pag_text_document_set_font_family(pag_text_document* document, const char* fontFamily);

PAG_EXPORT const char* pag_text_document_get_font_style(pag_text_document* document);

PAG_EXPORT void pag_text_document_set_font_style(pag_text_document* document, const char* fontStyle);

PAG_EXPORT float pag_text_document_get_font_size(pag_text_document* document);

PAG_EXPORT void pag_text_document_set_font_size(pag_text_document* document, float fontSize);

PAG_EXPORT pag_color pag_text_document_get_stroke_color(pag_text_document* document);

PAG_EXPORT void pag_text_document_set_stroke_color(pag_text_document* document, pag_color strokeColor);

PAG_EXPORT bool pag_text_document_get_stroke_over_fill(pag_text_document* document);

PAG_EXPORT void pag_text_document_set_stroke_over_fill(pag_text_document* document,
                                                    bool strokeOverFill);

PAG_EXPORT float pag_text_document_get_stroke_width(pag_text_document* document);

PAG_EXPORT void pag_text_document_set_stroke_width(pag_text_document* document, float strokeWidth);

PAG_EXPORT const char* pag_text_document_get_text(pag_text_document* document);

PAG_EXPORT void pag_text_document_set_text(pag_text_document* document, const char* text);

PAG_EXPORT pag_paragraph_justification
pag_text_document_get_justification(pag_text_document* document);

PAG_EXPORT void pag_text_document_set_justification(pag_text_document* document,
                                                 pag_paragraph_justification justification);

PAG_EXPORT float pag_text_document_get_leading(pag_text_document* document);

PAG_EXPORT void pag_text_document_set_leading(pag_text_document* document, float leading);

PAG_EXPORT float pag_text_document_get_tracking(pag_text_document* document);

PAG_EXPORT void pag_text_document_set_tracking(pag_text_document* document, float tracking);

PAG_EXPORT pag_color pag_text_document_get_background_color(pag_text_document* document);

PAG_EXPORT void pag_text_document_set_background_color(pag_text_document* document,
                                                    pag_color backgroundColor);

PAG_EXPORT uint8_t pag_text_document_get_background_alpha(pag_text_document* document);

PAG_EXPORT void pag_text_document_set_background_alpha(pag_text_document* document,
                                                    uint8_t backgroundAlpha);

PAG_C_PLUS_PLUS_END_GUARD
