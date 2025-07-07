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

#import <Foundation/Foundation.h>
#include "CocoaUtils.h"
#import "PAGLayer.h"

@class PAGFont;

PAG_API @interface PAGTextLayer : PAGLayer

/**
 * Returns the TextLayer’s fill color.
 */
- (CocoaColor*)fillColor;

/**
 * Sets the TextLayer’s fill color.
 */
- (void)setFillColor:(CocoaColor*)color;

/**
 * Returns the TextLayer's font.
 */
- (PAGFont*)font;

/**
 * Sets the TextLayer's font.
 */
- (void)setFont:(PAGFont*)font;

/**
 * Returns the TextLayer's font size.
 */
- (CGFloat)fontSize;

/**
 * Sets the TextLayer's font size.
 */
- (void)setFontSize:(CGFloat)size;

/**
 * Returns the TextLayer's stroke color.
 */
- (CocoaColor*)strokeColor;

/**
 * Sets the TextLayer's stroke color.
 */
- (void)setStrokeColor:(CocoaColor*)color;

/**
 * Returns the TextLayer's text.
 */
- (NSString*)text;

/**
 * Set the TextLayer's text.
 */
- (void)setText:(NSString*)text;

/**
 * Reset the text layer to its default text data.
 */
- (void)reset;

@end
