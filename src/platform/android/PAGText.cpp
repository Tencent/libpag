/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include "PAGText.h"

using namespace pag;

namespace pag {
static Global<jclass> PAGTextClass;
static jmethodID PAGTextConstructID;
static jfieldID PAGText_applyFill;
static jfieldID PAGText_applyStroke;
static jfieldID PAGText_baselineShift;
static jfieldID PAGText_boxText;
static jfieldID PAGText_boxTextRect;
static jfieldID PAGText_firstBaseLine;
static jfieldID PAGText_fauxBold;
static jfieldID PAGText_fauxItalic;
static jfieldID PAGText_fillColor;
static jfieldID PAGText_fontFamily;
static jfieldID PAGText_fontStyle;
static jfieldID PAGText_fontSize;
static jfieldID PAGText_strokeColor;
static jfieldID PAGText_strokeOverFill;
static jfieldID PAGText_strokeWidth;
static jfieldID PAGText_text;
static jfieldID PAGText_justification;
static jfieldID PAGText_leading;
static jfieldID PAGText_tracking;
static jfieldID PAGText_backgroundColor;
static jfieldID PAGText_backgroundAlpha;

void InitPAGTextJNI(JNIEnv* env) {
  PAGTextClass = env->FindClass("org/libpag/PAGText");
  if (PAGTextClass.get() == nullptr) {
    LOGE("Could not run InitPAGTextJNI, PAGTextClass is not found!");
    return;
  }
  PAGTextConstructID = env->GetMethodID(PAGTextClass.get(), "<init>", "()V");
  PAGText_applyFill = env->GetFieldID(PAGTextClass.get(), "applyFill", "Z");
  PAGText_applyStroke = env->GetFieldID(PAGTextClass.get(), "applyStroke", "Z");
  PAGText_baselineShift = env->GetFieldID(PAGTextClass.get(), "baselineShift", "F");
  PAGText_boxText = env->GetFieldID(PAGTextClass.get(), "boxText", "Z");
  PAGText_boxTextRect =
      env->GetFieldID(PAGTextClass.get(), "boxTextRect", "Landroid/graphics/RectF;");
  PAGText_firstBaseLine = env->GetFieldID(PAGTextClass.get(), "firstBaseLine", "F");
  PAGText_fauxBold = env->GetFieldID(PAGTextClass.get(), "fauxBold", "Z");
  PAGText_fauxItalic = env->GetFieldID(PAGTextClass.get(), "fauxItalic", "Z");
  PAGText_fillColor = env->GetFieldID(PAGTextClass.get(), "fillColor", "I");
  PAGText_fontFamily = env->GetFieldID(PAGTextClass.get(), "fontFamily", "Ljava/lang/String;");
  PAGText_fontStyle = env->GetFieldID(PAGTextClass.get(), "fontStyle", "Ljava/lang/String;");
  PAGText_fontSize = env->GetFieldID(PAGTextClass.get(), "fontSize", "F");
  PAGText_strokeColor = env->GetFieldID(PAGTextClass.get(), "strokeColor", "I");
  PAGText_strokeOverFill = env->GetFieldID(PAGTextClass.get(), "strokeOverFill", "Z");
  PAGText_strokeWidth = env->GetFieldID(PAGTextClass.get(), "strokeWidth", "F");
  PAGText_text = env->GetFieldID(PAGTextClass.get(), "text", "Ljava/lang/String;");
  PAGText_justification = env->GetFieldID(PAGTextClass.get(), "justification", "I");
  PAGText_leading = env->GetFieldID(PAGTextClass.get(), "leading", "F");
  PAGText_tracking = env->GetFieldID(PAGTextClass.get(), "tracking", "F");
  PAGText_backgroundColor = env->GetFieldID(PAGTextClass.get(), "backgroundColor", "I");
  PAGText_backgroundAlpha = env->GetFieldID(PAGTextClass.get(), "backgroundAlpha", "I");
}

jobject ToPAGTextObject(JNIEnv* env, pag::TextDocumentHandle textDocument) {
  if (textDocument == nullptr) {
    return nullptr;
  }
  if (PAGTextClass.get() == nullptr) {
    LOGE("Could not run ToPAGTextObject, PAGTextClass is not found!");
    return nullptr;
  }
  auto textData = env->NewObject(PAGTextClass.get(), PAGTextConstructID);
  env->SetBooleanField(textData, PAGText_applyFill, static_cast<jboolean>(textDocument->applyFill));
  env->SetBooleanField(textData, PAGText_applyStroke,
                       static_cast<jboolean>(textDocument->applyStroke));
  env->SetFloatField(textData, PAGText_baselineShift, textDocument->baselineShift);
  env->SetBooleanField(textData, PAGText_boxText, static_cast<jboolean>(textDocument->boxText));
  auto boxTextRect = MakeRectFObject(env, textDocument->boxTextPos.x, textDocument->boxTextPos.y,
                                     textDocument->boxTextSize.x, textDocument->boxTextSize.y);
  env->SetObjectField(textData, PAGText_boxTextRect, boxTextRect);
  env->SetFloatField(textData, PAGText_firstBaseLine, textDocument->firstBaseLine);
  env->SetBooleanField(textData, PAGText_fauxBold, static_cast<jboolean>(textDocument->fauxBold));
  env->SetBooleanField(textData, PAGText_fauxItalic,
                       static_cast<jboolean>(textDocument->fauxItalic));
  jint fillColor = MakeColorInt(env, textDocument->fillColor.red, textDocument->fillColor.green,
                                textDocument->fillColor.blue);
  env->SetIntField(textData, PAGText_fillColor, fillColor);
  auto fontFamily = SafeConvertToJString(env, textDocument->fontFamily);
  env->SetObjectField(textData, PAGText_fontFamily, fontFamily);
  auto fontStyle = SafeConvertToJString(env, textDocument->fontStyle);
  env->SetObjectField(textData, PAGText_fontStyle, fontStyle);
  env->SetFloatField(textData, PAGText_fontSize, textDocument->fontSize);
  jint strokeColor = MakeColorInt(env, textDocument->strokeColor.red,
                                  textDocument->strokeColor.green, textDocument->strokeColor.blue);
  env->SetIntField(textData, PAGText_strokeColor, strokeColor);
  env->SetBooleanField(textData, PAGText_strokeOverFill,
                       static_cast<jboolean>(textDocument->strokeOverFill));
  env->SetFloatField(textData, PAGText_strokeWidth, textDocument->strokeWidth);
  auto text = SafeConvertToJString(env, textDocument->text);
  env->SetObjectField(textData, PAGText_text, text);
  env->SetIntField(textData, PAGText_justification, static_cast<jint>(textDocument->justification));
  env->SetFloatField(textData, PAGText_leading, textDocument->leading);
  env->SetFloatField(textData, PAGText_tracking, textDocument->tracking);
  jint backgroundColor =
      MakeColorInt(env, textDocument->backgroundColor.red, textDocument->backgroundColor.green,
                   textDocument->backgroundColor.blue);
  env->SetIntField(textData, PAGText_backgroundColor, backgroundColor);
  env->SetIntField(textData, PAGText_backgroundAlpha, textDocument->backgroundAlpha);
  return textData;
}

TextDocumentHandle ToTextDocument(JNIEnv* env, jobject textData) {
  if (PAGTextClass.get() == nullptr) {
    LOGE("Could not run ToTextDocument, PAGTextClass is not found!");
    return nullptr;
  }
  if (textData == nullptr) {
    return nullptr;
  }
  auto textDocument = pag::TextDocumentHandle(new TextDocument());
  textDocument->applyFill = env->GetBooleanField(textData, PAGText_applyFill);
  textDocument->applyStroke = env->GetBooleanField(textData, PAGText_applyStroke);
  textDocument->baselineShift = env->GetFloatField(textData, PAGText_baselineShift);
  textDocument->boxText = env->GetBooleanField(textData, PAGText_boxText);
  auto boxTextRect = env->GetObjectField(textData, PAGText_boxTextRect);
  auto boxRect = ToRect(env, boxTextRect);
  textDocument->boxTextPos = {boxRect.x(), boxRect.y()};
  textDocument->boxTextSize = {boxRect.width(), boxRect.height()};
  textDocument->firstBaseLine = env->GetFloatField(textData, PAGText_firstBaseLine);
  textDocument->fauxBold = env->GetBooleanField(textData, PAGText_fauxBold);
  textDocument->fauxItalic = env->GetBooleanField(textData, PAGText_fauxItalic);
  jint fillColorObject = env->GetIntField(textData, PAGText_fillColor);
  textDocument->fillColor = ToColor(env, fillColorObject);
  auto fontFamilyObject = (jstring)env->GetObjectField(textData, PAGText_fontFamily);
  textDocument->fontFamily = SafeConvertToStdString(env, fontFamilyObject);
  auto fontStyleObject = (jstring)env->GetObjectField(textData, PAGText_fontStyle);
  textDocument->fontStyle = SafeConvertToStdString(env, fontStyleObject);
  textDocument->fontSize = env->GetFloatField(textData, PAGText_fontSize);
  jint strokeColorObject = env->GetIntField(textData, PAGText_strokeColor);
  textDocument->strokeColor = ToColor(env, strokeColorObject);
  textDocument->strokeOverFill = env->GetBooleanField(textData, PAGText_strokeOverFill);
  textDocument->strokeWidth = env->GetFloatField(textData, PAGText_strokeWidth);
  auto textObject = (jstring)env->GetObjectField(textData, PAGText_text);
  textDocument->text = SafeConvertToStdString(env, textObject);
  textDocument->justification =
      static_cast<ParagraphJustification>(env->GetIntField(textData, PAGText_justification));
  textDocument->leading = env->GetFloatField(textData, PAGText_leading);
  textDocument->tracking = env->GetFloatField(textData, PAGText_tracking);
  jint backgroundColorObject = env->GetIntField(textData, PAGText_backgroundColor);
  textDocument->backgroundColor = ToColor(env, backgroundColorObject);
  jint backgroundAlphaObject = env->GetIntField(textData, PAGText_backgroundAlpha);
  textDocument->backgroundAlpha = static_cast<uint8_t>(backgroundAlphaObject);
  return textDocument;
}
}  // namespace pag
