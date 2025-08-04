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

#import "PAGLayerImpl.h"
#include "platform/cocoa/CocoaUtils.h"

@class PAGFont;

@interface PAGTextLayerImpl : PAGLayerImpl

- (CocoaColor*)fillColor;

- (void)setFillColor:(CocoaColor*)color;

- (PAGFont*)font;

- (void)setFont:(PAGFont*)font;

- (CGFloat)fontSize;

- (void)setFontSize:(CGFloat)size;

- (CocoaColor*)strokeColor;

- (void)setStrokeColor:(CocoaColor*)color;

- (NSString*)text;

- (void)setText:(NSString*)text;

- (void)reset;

@end
