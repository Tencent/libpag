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

#include "pag/c/pag_text_document.h"
#include "pag_types_priv.h"

bool pag_text_document_get_apply_fill(pag_text_document* document) {
  if (document == nullptr) {
    return false;
  }
  return document->p->applyFill;
}

void pag_text_document_set_apply_fill(pag_text_document* document, bool applyFill) {
  if (document == nullptr) {
    return;
  }
  document->p->applyFill = applyFill;
}

bool pag_text_document_get_apply_stroke(pag_text_document* document) {
  if (document == nullptr) {
    return false;
  }
  return document->p->applyStroke;
}

void pag_text_document_set_apply_stroke(pag_text_document* document, bool applyStroke) {
  if (document == nullptr) {
    return;
  }
  document->p->applyStroke = applyStroke;
}

float pag_text_document_get_baseline_shift(pag_text_document* document) {
  if (document == nullptr) {
    return 0.0f;
  }
  return document->p->baselineShift;
}

void pag_text_document_set_baseline_shift(pag_text_document* document, float baselineShift) {
  if (document == nullptr) {
    return;
  }
  document->p->baselineShift = baselineShift;
}

bool pag_text_document_get_box_text(pag_text_document* document) {
  if (document == nullptr) {
    return false;
  }
  return document->p->boxText;
}

void pag_text_document_set_box_text(pag_text_document* document, bool boxText) {
  if (document == nullptr) {
    return;
  }
  document->p->boxText = boxText;
}

pag_point pag_text_document_get_box_text_pos(pag_text_document* document) {
  if (document == nullptr) {
    return {0.0f, 0.0f};
  }
  auto point = document->p->boxTextPos;
  return pag_point{point.x, point.y};
}

void pag_text_document_set_box_text_pos(pag_text_document* document, pag_point boxTextPos) {
  if (document == nullptr) {
    return;
  }
  document->p->boxTextPos.x = boxTextPos.x;
  document->p->boxTextPos.y = boxTextPos.y;
}

pag_point pag_text_document_get_box_text_size(pag_text_document* document) {
  if (document == nullptr) {
    return {0.0f, 0.0f};
  }
  auto point = document->p->boxTextSize;
  return pag_point{point.x, point.y};
}

void pag_text_document_set_box_text_size(pag_text_document* document, pag_point boxTextSize) {
  if (document == nullptr) {
    return;
  }
  document->p->boxTextSize.x = boxTextSize.x;
  document->p->boxTextSize.y = boxTextSize.y;
}

float pag_text_document_get_first_baseline(pag_text_document* document) {
  if (document == nullptr) {
    return 0.0f;
  }
  return document->p->firstBaseLine;
}

void pag_text_document_set_first_baseline(pag_text_document* document, float firstBaseline) {
  if (document == nullptr) {
    return;
  }
  document->p->firstBaseLine = firstBaseline;
}

bool pag_text_document_get_faux_bold(pag_text_document* document) {
  if (document == nullptr) {
    return false;
  }
  return document->p->fauxBold;
}

void pag_text_document_set_faux_bold(pag_text_document* document, bool fauxBold) {
  if (document == nullptr) {
    return;
  }
  document->p->fauxBold = fauxBold;
}

bool pag_text_document_get_faux_italic(pag_text_document* document) {
  if (document == nullptr) {
    return false;
  }
  return document->p->fauxItalic;
}

void pag_text_document_set_faux_italic(pag_text_document* document, bool fauxItalic) {
  if (document == nullptr) {
    return;
  }
  document->p->fauxItalic = fauxItalic;
}

pag_color pag_text_document_get_fill_color(pag_text_document* document) {
  if (document == nullptr) {
    return {0, 0, 0};
  }
  auto color = document->p->fillColor;
  return pag_color{color.red, color.green, color.blue};
}

void pag_text_document_set_fill_color(pag_text_document* document, pag_color fillColor) {
  if (document == nullptr) {
    return;
  }
  document->p->fillColor.red = fillColor.red;
  document->p->fillColor.green = fillColor.green;
  document->p->fillColor.blue = fillColor.blue;
}

const char* pag_text_document_get_font_family(pag_text_document* document) {
  if (document == nullptr) {
    return nullptr;
  }
  return document->p->fontFamily.c_str();
}

void pag_text_document_set_font_family(pag_text_document* document, const char* fontFamily) {
  if (document == nullptr) {
    return;
  }
  document->p->fontFamily = fontFamily;
}

const char* pag_text_document_get_font_style(pag_text_document* document) {
  if (document == nullptr) {
    return nullptr;
  }
  return document->p->fontStyle.c_str();
}

void pag_text_document_set_font_style(pag_text_document* document, const char* fontStyle) {
  if (document == nullptr) {
    return;
  }
  document->p->fontStyle = fontStyle;
}

float pag_text_document_get_font_size(pag_text_document* document) {
  if (document == nullptr) {
    return 0.0f;
  }
  return document->p->fontSize;
}

void pag_text_document_set_font_size(pag_text_document* document, float fontSize) {
  if (document == nullptr) {
    return;
  }
  document->p->fontSize = fontSize;
}

pag_color pag_text_document_get_stroke_color(pag_text_document* document) {
  if (document == nullptr) {
    return {0, 0, 0};
  }
  auto color = document->p->strokeColor;
  return pag_color{color.red, color.green, color.blue};
}

void pag_text_document_set_stroke_color(pag_text_document* document, pag_color strokeColor) {
  if (document == nullptr) {
    return;
  }
  document->p->strokeColor.red = strokeColor.red;
  document->p->strokeColor.green = strokeColor.green;
  document->p->strokeColor.blue = strokeColor.blue;
}

bool pag_text_document_get_stroke_over_fill(pag_text_document* document) {
  if (document == nullptr) {
    return false;
  }
  return document->p->strokeOverFill;
}

void pag_text_document_set_stroke_over_fill(pag_text_document* document, bool strokeOverFill) {
  if (document == nullptr) {
    return;
  }
  document->p->strokeOverFill = strokeOverFill;
}

float pag_text_document_get_stroke_width(pag_text_document* document) {
  if (document == nullptr) {
    return 0.0f;
  }
  return document->p->strokeWidth;
}

void pag_text_document_set_stroke_width(pag_text_document* document, float strokeWidth) {
  if (document == nullptr) {
    return;
  }
  document->p->strokeWidth = strokeWidth;
}

const char* pag_text_document_get_text(pag_text_document* document) {
  if (document == nullptr) {
    return nullptr;
  }
  return document->p->text.c_str();
}

void pag_text_document_set_text(pag_text_document* document, const char* text) {
  if (document == nullptr) {
    return;
  }
  document->p->text = text;
}

pag_paragraph_justification pag_text_document_get_justification(pag_text_document* document) {
  if (document == nullptr) {
    return pag_paragraph_justification_left_justify;
  }
  pag_paragraph_justification justification;
  if (ToCParagraphJustification(document->p->justification, &justification)) {
    return justification;
  }
  return pag_paragraph_justification_left_justify;
}

void pag_text_document_set_justification(pag_text_document* document,
                                         pag_paragraph_justification justification) {
  if (document == nullptr) {
    return;
  }
  pag::ParagraphJustification pagJustification;
  if (FromCParagraphJustification(justification, &pagJustification)) {
    document->p->justification = pagJustification;
  }
}

float pag_text_document_get_leading(pag_text_document* document) {
  if (document == nullptr) {
    return 0.0f;
  }
  return document->p->leading;
}

void pag_text_document_set_leading(pag_text_document* document, float leading) {
  if (document == nullptr) {
    return;
  }
  document->p->leading = leading;
}

float pag_text_document_get_tracking(pag_text_document* document) {
  if (document == nullptr) {
    return 0.0f;
  }
  return document->p->tracking;
}

void pag_text_document_set_tracking(pag_text_document* document, float tracking) {
  if (document == nullptr) {
    return;
  }
  document->p->tracking = tracking;
}

pag_color pag_text_document_get_background_color(pag_text_document* document) {
  if (document == nullptr) {
    return {0, 0, 0};
  }
  auto color = document->p->backgroundColor;
  return pag_color{color.red, color.green, color.blue};
}

void pag_text_document_set_background_color(pag_text_document* document,
                                            pag_color backgroundColor) {
  if (document == nullptr) {
    return;
  }
  document->p->backgroundColor.red = backgroundColor.red;
  document->p->backgroundColor.green = backgroundColor.green;
  document->p->backgroundColor.blue = backgroundColor.blue;
}

uint8_t pag_text_document_get_background_alpha(pag_text_document* document) {
  if (document == nullptr) {
    return 0;
  }
  return document->p->backgroundAlpha;
}

void pag_text_document_set_background_alpha(pag_text_document* document, uint8_t backgroundAlpha) {
  if (document == nullptr) {
    return;
  }
  document->p->backgroundAlpha = backgroundAlpha;
}
