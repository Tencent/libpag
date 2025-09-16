/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2024 Tencent. All rights reserved.
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

#include "JPAGText.h"
#include "base/utils/Log.h"
#include "platform/ohos/JsHelper.h"

#define JPAGTEXT_APPLY_FILL "applyFill"
#define JPAGTEXT_APPLY_STROKE "applyStroke"
#define JPAGTEXT_BASELINE_SHIFT "baselineShift"
#define JPAGTEXT_BOX_TEXT "boxText"
#define JPAGTEXT_BOX_TEXT_RECT "boxTextRect"
#define JPAGTEXT_FIRST_BASE_LINE "firstBaseLine"
#define JPAGTEXT_FAUX_BOLD "fauxBold"
#define JPAGTEXT_FAUX_ITALIC "fauxItalic"
#define JPAGTEXT_FILL_COLOR "fillColor"
#define JPAGTEXT_FONT_FAMILY "fontFamily"
#define JPAGTEXT_FONT_STYLE "fontStyle"
#define JPAGTEXT_FONT_SIZE "fontSize"
#define JPAGTEXT_STROKE_COLOR "strokeColor"
#define JPAGTEXT_STROKE_OVER_FILL "strokeOverFill"
#define JPAGTEXT_STROKE_WIDTH "strokeWidth"
#define JPAGTEXT_TEXT "text"
#define JPAGTEXT_JUSTIFICATION "justification"
#define JPAGTEXT_LEADING "leading"
#define JPAGTEXT_TRACKING "tracking"
#define JPAGTEXT_BACKGROUND_COLOR "backgroundColor"
#define JPAGTEXT_BACKGROUND_ALPHA "backgroundAlpha"

namespace pag {
bool JPAGText::Init(napi_env env, napi_value exports) {
  auto status = DefineClass(env, exports, ClassName(), 0, nullptr, Constructor, "");
  return status == napi_ok;
}

std::shared_ptr<TextDocument> JPAGText::FromJs(napi_env env, napi_value value) {
  auto text = std::make_shared<TextDocument>();
  napi_value applyFill;
  napi_get_named_property(env, value, JPAGTEXT_APPLY_FILL, &applyFill);
  napi_get_value_bool(env, applyFill, &text->applyFill);

  napi_value applyStroke;
  napi_get_named_property(env, value, JPAGTEXT_APPLY_STROKE, &applyStroke);
  napi_get_value_bool(env, applyStroke, &text->applyStroke);

  napi_value baselineShift;
  napi_get_named_property(env, value, JPAGTEXT_BASELINE_SHIFT, &baselineShift);
  double temp = 0;
  napi_get_value_double(env, baselineShift, &temp);
  text->baselineShift = temp;

  napi_value boxText;
  napi_get_named_property(env, value, JPAGTEXT_BOX_TEXT, &boxText);
  napi_get_value_bool(env, boxText, &text->boxText);

  napi_value boxTextRect;
  napi_get_named_property(env, value, JPAGTEXT_BOX_TEXT_RECT, &boxTextRect);
  Rect rect = GetRect(env, boxTextRect);
  text->boxTextPos = {rect.x(), rect.y()};
  text->boxTextSize = {rect.width(), rect.height()};

  napi_value firstBaseLine;
  napi_get_named_property(env, value, JPAGTEXT_FIRST_BASE_LINE, &firstBaseLine);
  temp = 0;
  napi_get_value_double(env, firstBaseLine, &temp);
  text->firstBaseLine = temp;

  napi_value fauxBold;
  napi_get_named_property(env, value, JPAGTEXT_FAUX_BOLD, &fauxBold);
  napi_get_value_bool(env, fauxBold, &text->fauxBold);

  napi_value fauxItalic;
  napi_get_named_property(env, value, JPAGTEXT_FAUX_ITALIC, &fauxItalic);
  napi_get_value_bool(env, fauxItalic, &text->fauxItalic);

  napi_value fillColor;
  napi_get_named_property(env, value, JPAGTEXT_FILL_COLOR, &fillColor);
  int color = 0;
  napi_get_value_int32(env, fillColor, &color);
  text->fillColor = ToColor(color);

  napi_value fontFamily;
  napi_get_named_property(env, value, JPAGTEXT_FONT_FAMILY, &fontFamily);
  char fontFamilyBuf[1024];
  size_t familyLength = 0;
  napi_get_value_string_utf8(env, fontFamily, fontFamilyBuf, 1024, &familyLength);
  text->fontFamily = {fontFamilyBuf, familyLength};

  napi_value fontStyle;
  napi_get_named_property(env, value, JPAGTEXT_FONT_STYLE, &fontStyle);
  char fontStyleBuf[1024];
  size_t styleLength = 0;
  napi_get_value_string_utf8(env, fontStyle, fontStyleBuf, 1024, &styleLength);
  text->fontStyle = {fontStyleBuf, styleLength};

  napi_value fontSize;
  napi_get_named_property(env, value, JPAGTEXT_FONT_SIZE, &fontSize);
  napi_get_value_double(env, fontSize, &temp);
  text->fontSize = temp;

  napi_value strokeColor;
  napi_get_named_property(env, value, JPAGTEXT_STROKE_COLOR, &strokeColor);
  napi_get_value_int32(env, strokeColor, &color);
  text->strokeColor = ToColor(color);

  napi_value strokeOverFill;
  napi_get_named_property(env, value, JPAGTEXT_STROKE_OVER_FILL, &strokeOverFill);
  napi_get_value_bool(env, strokeOverFill, &text->strokeOverFill);

  napi_value strokeWidth;
  napi_get_named_property(env, value, JPAGTEXT_STROKE_WIDTH, &strokeWidth);
  napi_get_value_double(env, strokeWidth, &temp);
  text->strokeWidth = temp;

  napi_value content;
  napi_get_named_property(env, value, JPAGTEXT_TEXT, &content);
  char textBuf[1024];
  size_t textLength = 0;
  napi_get_value_string_utf8(env, content, textBuf, 1024, &textLength);
  text->text = {textBuf, textLength};

  napi_value justification;
  napi_get_named_property(env, value, JPAGTEXT_JUSTIFICATION, &justification);
  int intTemp = 0;
  napi_get_value_int32(env, justification, &intTemp);
  text->justification = static_cast<ParagraphJustification>(intTemp);

  napi_value leading;
  napi_get_named_property(env, value, JPAGTEXT_LEADING, &leading);
  napi_get_value_double(env, leading, &temp);
  text->leading = temp;

  napi_value tracking;
  napi_get_named_property(env, value, JPAGTEXT_TRACKING, &tracking);
  napi_get_value_double(env, tracking, &temp);
  text->tracking = temp;

  napi_value backgroundColor;
  napi_get_named_property(env, value, JPAGTEXT_BACKGROUND_COLOR, &backgroundColor);
  napi_get_value_int32(env, backgroundColor, &color);
  text->backgroundColor = ToColor(color);

  napi_value backgroundAlpha;
  napi_get_named_property(env, value, JPAGTEXT_BACKGROUND_ALPHA, &backgroundAlpha);
  napi_get_value_int32(env, backgroundAlpha, &intTemp);
  text->backgroundAlpha = intTemp;
  return text;
}

napi_value JPAGText::ToJs(napi_env env, const std::shared_ptr<TextDocument>& text) {
  napi_value result;
  auto status = napi_new_instance(env, GetConstructor(env, ClassName()), 0, nullptr, &result);
  if (status != napi_ok) {
    LOGE("JPAGText::ToJs napi_new_instance failed :%d", status);
    return nullptr;
  }
  if (text == nullptr) {
    return nullptr;
  }

  napi_value applyFill;
  napi_get_boolean(env, text->applyFill, &applyFill);
  napi_set_named_property(env, result, JPAGTEXT_APPLY_FILL, applyFill);

  napi_value applyStroke;
  napi_get_boolean(env, text->applyStroke, &applyStroke);
  napi_set_named_property(env, result, JPAGTEXT_APPLY_STROKE, applyStroke);

  napi_value baselineShift;
  napi_create_int32(env, text->baselineShift, &baselineShift);
  napi_set_named_property(env, result, JPAGTEXT_BASELINE_SHIFT, baselineShift);

  napi_value boxText;
  napi_get_boolean(env, text->boxText, &boxText);
  napi_set_named_property(env, result, JPAGTEXT_BOX_TEXT, boxText);

  auto boxTextPosition = text->boxTextPos;
  auto boxTextSize = text->boxTextSize;
  napi_value boxTextRect = CreateRect(
      env, Rect::MakeXYWH(boxTextPosition.x, boxTextPosition.y, boxTextSize.x, boxTextSize.y));
  napi_set_named_property(env, result, JPAGTEXT_BOX_TEXT_RECT, boxTextRect);

  napi_value firstBaseLine;
  napi_create_int32(env, text->firstBaseLine, &firstBaseLine);
  napi_set_named_property(env, result, JPAGTEXT_FIRST_BASE_LINE, firstBaseLine);

  napi_value fauxBold;
  napi_get_boolean(env, text->fauxBold, &fauxBold);
  napi_set_named_property(env, result, JPAGTEXT_FAUX_BOLD, fauxBold);

  napi_value fauxItalic;
  napi_get_boolean(env, text->fauxItalic, &fauxItalic);
  napi_set_named_property(env, result, JPAGTEXT_FAUX_ITALIC, fauxItalic);

  napi_value fillColor;
  napi_create_int32(env,
                    MakeColorInt(text->fillColor.red, text->fillColor.green, text->fillColor.blue),
                    &fillColor);
  napi_set_named_property(env, result, JPAGTEXT_FILL_COLOR, fillColor);

  napi_value fontFamily;
  napi_create_string_utf8(env, text->fontFamily.c_str(), text->fontFamily.length(), &fontFamily);
  napi_set_named_property(env, result, JPAGTEXT_FONT_FAMILY, fontFamily);

  napi_value fontStyle;
  napi_create_string_utf8(env, text->fontStyle.c_str(), text->fontStyle.length(), &fontStyle);
  napi_set_named_property(env, result, JPAGTEXT_FONT_STYLE, fontStyle);

  napi_value fontSize;
  napi_create_double(env, text->fontSize, &fontSize);
  napi_set_named_property(env, result, JPAGTEXT_FONT_SIZE, fontSize);

  napi_value strokeColor;
  napi_create_int32(
      env, MakeColorInt(text->strokeColor.red, text->strokeColor.green, text->strokeColor.blue),
      &strokeColor);
  napi_set_named_property(env, result, JPAGTEXT_STROKE_COLOR, strokeColor);

  napi_value strokeOverFill;
  napi_get_boolean(env, text->strokeOverFill, &strokeOverFill);
  napi_set_named_property(env, result, JPAGTEXT_STROKE_OVER_FILL, strokeOverFill);

  napi_value strokeWidth;
  napi_create_double(env, text->strokeWidth, &strokeWidth);
  napi_set_named_property(env, result, JPAGTEXT_STROKE_WIDTH, strokeWidth);

  napi_value content;
  napi_create_string_utf8(env, text->text.c_str(), text->text.length(), &content);
  napi_set_named_property(env, result, JPAGTEXT_TEXT, content);

  napi_value justification;
  napi_create_int32(env, static_cast<int32_t>(text->justification), &justification);
  napi_set_named_property(env, result, JPAGTEXT_JUSTIFICATION, justification);

  napi_value leading;
  napi_create_double(env, text->leading, &leading);
  napi_set_named_property(env, result, JPAGTEXT_LEADING, leading);

  napi_value tracking;
  napi_create_double(env, text->tracking, &tracking);
  napi_set_named_property(env, result, JPAGTEXT_TRACKING, tracking);

  napi_value backgroundColor;
  napi_create_int32(env,
                    MakeColorInt(text->backgroundColor.red, text->backgroundColor.green,
                                 text->backgroundColor.blue),
                    &backgroundColor);
  napi_set_named_property(env, result, JPAGTEXT_BACKGROUND_COLOR, backgroundColor);

  napi_value backgroundAlpha;
  napi_create_int32(env, text->backgroundAlpha, &backgroundAlpha);
  napi_set_named_property(env, result, JPAGTEXT_BACKGROUND_ALPHA, backgroundAlpha);

  return result;
}

napi_value JPAGText::Constructor(napi_env env, napi_callback_info info) {
  napi_value result = nullptr;
  size_t argc = 0;
  napi_value args[1];
  napi_get_cb_info(env, info, &argc, args, &result, nullptr);
  return result;
}
}  // namespace pag
