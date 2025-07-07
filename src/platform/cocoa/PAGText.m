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

//
// Created by dom on 01/12/2017.
//

#import "PAGText.h"

@implementation PAGText {
}

@synthesize applyFill;
@synthesize applyStroke;
@synthesize baselineShift;
@synthesize boxText;
@synthesize boxTextRect;
@synthesize fauxBold;
@synthesize fauxItalic;
@synthesize fillColor;
@synthesize fontFamily;
@synthesize fontStyle;
@synthesize fontSize;
@synthesize strokeColor;
@synthesize strokeOverFill;
@synthesize strokeWidth;
@synthesize text;
@synthesize justification;
@synthesize leading;
@synthesize tracking;
@synthesize backgroundColor;
@synthesize backgroundAlpha;

- (void)dealloc {
  [fillColor release];
  [fontFamily release];
  [fontStyle release];
  [strokeColor release];
  [text release];
  [backgroundColor release];
  [super dealloc];
}

@end
