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

#import "PAGTextImpl.h"
#import "PAGColorUtility.h"

using namespace pag;

inline bool IsUTF8StringEmpty(NSString* str) {
  return str == nil || str.UTF8String == nullptr;
}

PAGText* ToPAGText(pag::TextDocumentHandle textDocument) {
  if (textDocument == nullptr) {
    return nil;
  }
  PAGText* textData = [[[PAGText alloc] init] autorelease];
  textData.applyFill = textDocument->applyFill;
  textData.applyStroke = textDocument->applyStroke;
  textData.baselineShift = textDocument->baselineShift;
  textData.boxText = textDocument->boxText;
  textData.boxTextRect = CGRectMake(textDocument->boxTextPos.x, textDocument->boxTextPos.y,
                                    textDocument->boxTextSize.x, textDocument->boxTextSize.y);
  textData.firstBaseLine = textDocument->firstBaseLine;
  textData.fauxBold = textDocument->fauxBold;
  textData.fauxItalic = textDocument->fauxItalic;
  textData.fillColor = PAGColorUtility::ToCocoaColor(textDocument->fillColor);
  textData.fontFamily = [NSString stringWithUTF8String:textDocument->fontFamily.c_str()];
  textData.fontStyle = [NSString stringWithUTF8String:textDocument->fontStyle.c_str()];
  textData.fontSize = textDocument->fontSize;
  textData.strokeColor = PAGColorUtility::ToCocoaColor(textDocument->strokeColor);
  textData.strokeOverFill = textDocument->strokeOverFill;
  textData.strokeWidth = textDocument->strokeWidth;
  textData.text = [NSString stringWithUTF8String:textDocument->text.c_str()];
  textData.justification = static_cast<uint8_t>(textDocument->justification);
  textData.leading = textDocument->leading;
  textData.tracking = textDocument->tracking;
  textData.backgroundColor = PAGColorUtility::ToCocoaColor(textDocument->backgroundColor);
  textData.backgroundAlpha = textDocument->backgroundAlpha;
  return textData;
}

pag::TextDocumentHandle ToTextDocument(PAGText* value) {
  if (value == nil) {
    return nullptr;
  }
  auto textDocument = pag::TextDocumentHandle(new TextDocument());
  textDocument->applyFill = value.applyFill;
  textDocument->applyStroke = value.applyStroke;
  textDocument->baselineShift = value.baselineShift;
  textDocument->boxText = value.boxText;
  textDocument->boxTextPos = {static_cast<float>(value.boxTextRect.origin.x),
                              static_cast<float>(value.boxTextRect.origin.y)};
  textDocument->boxTextSize = {static_cast<float>(value.boxTextRect.size.width),
                               static_cast<float>(value.boxTextRect.size.height)};
  textDocument->firstBaseLine = value.firstBaseLine;
  textDocument->fauxBold = value.fauxBold;
  textDocument->fauxItalic = value.fauxItalic;
  textDocument->fillColor = PAGColorUtility::ToColor(value.fillColor);
  textDocument->fontFamily = IsUTF8StringEmpty(value.fontFamily) ? "" : value.fontFamily.UTF8String;
  textDocument->fontStyle = IsUTF8StringEmpty(value.fontStyle) ? "" : value.fontStyle.UTF8String;
  textDocument->fontSize = value.fontSize;
  textDocument->strokeColor = PAGColorUtility::ToColor(value.strokeColor);
  textDocument->strokeOverFill = value.strokeOverFill;
  textDocument->strokeWidth = value.strokeWidth;
  textDocument->text = IsUTF8StringEmpty(value.text) ? "" : value.text.UTF8String;
  textDocument->justification = static_cast<ParagraphJustification>(value.justification);
  textDocument->leading = value.leading;
  textDocument->tracking = value.tracking;
  textDocument->backgroundColor = PAGColorUtility::ToColor(value.backgroundColor);
  textDocument->backgroundAlpha = value.backgroundAlpha;
  return textDocument;
}
