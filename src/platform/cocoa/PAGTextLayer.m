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
#import "PAGLayer+Internal.h"
#import "PAGTextLayerImpl.h"

@implementation PAGTextLayer

- (CocoaColor*)fillColor {
  return [(PAGTextLayerImpl*)self.impl fillColor];
}

- (void)setFillColor:(CocoaColor*)color {
  [(PAGTextLayerImpl*)self.impl setFillColor:color];
}

- (PAGFont*)font {
  return [(PAGTextLayerImpl*)self.impl font];
}

- (void)setFont:(PAGFont*)font {
  [(PAGTextLayerImpl*)self.impl setFont:font];
}

- (CGFloat)fontSize {
  return [(PAGTextLayerImpl*)self.impl fontSize];
}

- (void)setFontSize:(CGFloat)size {
  [(PAGTextLayerImpl*)self.impl setFontSize:size];
}

- (CocoaColor*)strokeColor {
  return [(PAGTextLayerImpl*)self.impl strokeColor];
}

- (void)setStrokeColor:(CocoaColor*)color {
  [(PAGTextLayerImpl*)self.impl setStrokeColor:color];
}

- (NSString*)text {
  return [(PAGTextLayerImpl*)self.impl text];
}

- (void)setText:(NSString*)text {
  [(PAGTextLayerImpl*)self.impl setText:text];
}

- (void)reset {
  [(PAGTextLayerImpl*)self.impl reset];
}

@end
