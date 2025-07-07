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

@interface PAGCompositionImpl : PAGLayerImpl

+ (PAGComposition*)Make:(CGSize)size;

- (NSInteger)width;

- (NSInteger)height;

- (void)setContentSize:(CGSize)size;

- (NSInteger)numChildren;

- (PAGLayer*)getLayerAt:(int)index;

- (NSInteger)getLayerIndex:(PAGLayer*)layer;

- (void)setLayerIndex:(NSInteger)index layer:(PAGLayer*)child;

- (CGRect)getBounds;

- (BOOL)addLayer:(PAGLayer*)pagLayer;

- (BOOL)addLayer:(PAGLayer*)pagLayer atIndex:(int)index;

- (BOOL)contains:(PAGLayer*)pagLayer;

- (PAGLayer*)removeLayer:(PAGLayer*)pagLayer;

- (PAGLayer*)removeLayerAt:(int)index;

- (void)removeAllLayers;

- (void)swapLayer:(PAGLayer*)pagLayer1 withLayer:(PAGLayer*)pagLayer2;

- (void)swapLayerAt:(int)index1 withIndex:(int)index2;

- (NSData*)audioBytes;

- (int64_t)audioStartTime;

- (NSArray<PAGMarker*>*)audioMarkers;

- (NSArray<PAGLayer*>*)getLayersByName:(NSString*)layerName;

- (NSArray<PAGLayer*>*)getLayersUnderPoint:(CGPoint)point;

@end
