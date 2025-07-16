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

#import "PAGTextLayer.h"
#import "PAGColorUtility.h"
#import "PAGFont.h"
#import "platform/cocoa/private/PAGLayer+Internal.h"

@implementation PAGTextLayer

- (CocoaColor*)fillColor {
  auto pagTextLayer = std::static_pointer_cast<pag::PAGTextLayer>([self pagLayer]);
  auto color = pagTextLayer->fillColor();
  return PAGColorUtility::ToCocoaColor(color);
}

- (void)setFillColor:(CocoaColor*)color {
  auto pagTextLayer = std::static_pointer_cast<pag::PAGTextLayer>([self pagLayer]);
  pagTextLayer->setFillColor(PAGColorUtility::ToColor(color));
}

- (PAGFont*)font {
  auto pagTextLayer = std::static_pointer_cast<pag::PAGTextLayer>([self pagLayer]);
  auto font = pagTextLayer->font();
  PAGFont* pagFont = [PAGFont new];
  pagFont.fontFamily = [NSString stringWithUTF8String:font.fontFamily.c_str()];
  pagFont.fontStyle = [NSString stringWithUTF8String:font.fontStyle.c_str()];
  ;
  return [pagFont autorelease];
}

- (void)setFont:(PAGFont*)font {
  auto pagTextLayer = std::static_pointer_cast<pag::PAGTextLayer>([self pagLayer]);
  if (font.fontFamily) {
    std::string familyName =
        std::string([font.fontFamily cStringUsingEncoding:NSUTF8StringEncoding]);
    std::string fontStyle = "";
    if (font.fontStyle) {
      fontStyle = std::string([font.fontStyle cStringUsingEncoding:NSUTF8StringEncoding]);
    }
    pagTextLayer->setFont(pag::PAGFont(familyName, fontStyle));
  }
}

- (CGFloat)fontSize {
  auto pagTextLayer = std::static_pointer_cast<pag::PAGTextLayer>([self pagLayer]);
  return pagTextLayer->fontSize();
}

- (void)setFontSize:(CGFloat)size {
  auto pagTextLayer = std::static_pointer_cast<pag::PAGTextLayer>([self pagLayer]);
  return pagTextLayer->setFontSize(size);
}

- (CocoaColor*)strokeColor {
  auto pagTextLayer = std::static_pointer_cast<pag::PAGTextLayer>([self pagLayer]);
  auto color = pagTextLayer->strokeColor();
  return PAGColorUtility::ToCocoaColor(color);
}

- (void)setStrokeColor:(CocoaColor*)color {
  auto pagTextLayer = std::static_pointer_cast<pag::PAGTextLayer>([self pagLayer]);
  pagTextLayer->setStrokeColor(PAGColorUtility::ToColor(color));
}

- (NSString*)text {
  auto pagTextLayer = std::static_pointer_cast<pag::PAGTextLayer>([self pagLayer]);
  auto text = pagTextLayer->text();
  return [NSString stringWithUTF8String:text.c_str()];
}

- (void)setText:(NSString*)text {
  auto pagTextLayer = std::static_pointer_cast<pag::PAGTextLayer>([self pagLayer]);
  auto newText = std::string([text cStringUsingEncoding:NSUTF8StringEncoding]);
  pagTextLayer->setText(newText);
}

- (void)reset {
  auto pagTextLayer = std::static_pointer_cast<pag::PAGTextLayer>([self pagLayer]);
  pagTextLayer->reset();
}

@end