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

#import "PAGComposition.h"
#import "PAGCompositionImpl.h"
#import "platform/cocoa/private/PAGLayer+Internal.h"

@implementation PAGComposition

+ (PAGComposition*)Make:(CGSize)size {
  return [PAGCompositionImpl Make:size];
}

- (NSInteger)width {
  return [(PAGCompositionImpl*)self.impl width];
}

- (NSInteger)height {
  return [(PAGCompositionImpl*)self.impl height];
}

- (void)setContentSize:(CGSize)size {
  [(PAGCompositionImpl*)self.impl setContentSize:size];
}

- (NSInteger)numChildren {
  return [(PAGCompositionImpl*)self.impl numChildren];
}

- (PAGLayer*)getLayerAt:(int)index {
  return [(PAGCompositionImpl*)self.impl getLayerAt:index];
}

- (NSInteger)getLayerIndex:(PAGLayer*)child {
  return [(PAGCompositionImpl*)self.impl getLayerIndex:child];
}

- (void)setLayerIndex:(NSInteger)index layer:(PAGLayer*)child {
  [(PAGCompositionImpl*)self.impl setLayerIndex:index layer:child];
}

- (NSData*)audioBytes {
  return [(PAGCompositionImpl*)self.impl audioBytes];
}

- (int64_t)audioStartTime {
  return [(PAGCompositionImpl*)self.impl audioStartTime];
}

- (CGRect)getBounds {
  return [(PAGCompositionImpl*)self.impl getBounds];
}

- (BOOL)addLayer:(PAGLayer*)pagLayer {
  return [(PAGCompositionImpl*)self.impl addLayer:pagLayer];
}

- (BOOL)addLayer:(PAGLayer*)pagLayer atIndex:(int)index {
  return [(PAGCompositionImpl*)self.impl addLayer:pagLayer atIndex:index];
}

- (BOOL)contains:(PAGLayer*)pagLayer {
  return [(PAGCompositionImpl*)self.impl contains:pagLayer];
}

- (PAGLayer*)removeLayer:(PAGLayer*)pagLayer {
  return [(PAGCompositionImpl*)self.impl removeLayer:pagLayer];
}

- (PAGLayer*)removeLayerAt:(int)index {
  return [(PAGCompositionImpl*)self.impl removeLayerAt:index];
}

- (void)removeAllLayers {
  [(PAGCompositionImpl*)self.impl removeAllLayers];
}

- (void)swapLayer:(PAGLayer*)pagLayer1 withLayer:(PAGLayer*)pagLayer2 {
  [(PAGCompositionImpl*)self.impl swapLayer:pagLayer1 withLayer:pagLayer2];
}

- (void)swapLayerAt:(int)index1 withIndex:(int)index2 {
  [(PAGCompositionImpl*)self.impl swapLayerAt:index1 withIndex:index2];
}

- (NSArray<PAGMarker*>*)audioMarkers {
  return [(PAGCompositionImpl*)self.impl audioMarkers];
}

- (NSArray<PAGLayer*>*)getLayersByName:(NSString*)layerName {
  return [(PAGCompositionImpl*)self.impl getLayersByName:layerName];
}

- (NSArray<PAGLayer*>*)getLayersUnderPoint:(CGPoint)point {
  return [(PAGCompositionImpl*)self.impl getLayersUnderPoint:point];
}

@end
