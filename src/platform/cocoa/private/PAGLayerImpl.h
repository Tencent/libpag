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

#import "platform/cocoa/PAGComposition.h"

@class PAGCompositionImpl;

@interface PAGLayerImpl : NSObject

- (PAGLayerType)layerType;

- (NSString*)layerName;

- (CGAffineTransform)matrix;

- (void)setMatrix:(CGAffineTransform)matrix;

- (void)resetMatrix;

- (CGAffineTransform)getTotalMatrix;

- (BOOL)visible;

- (void)setVisible:(BOOL)visible;

- (NSInteger)editableIndex;

- (PAGComposition*)parent;

- (NSArray<PAGMarker*>*)markers;

- (int64_t)localTimeToGlobal:(int64_t)localTime;

- (int64_t)globalToLocalTime:(int64_t)globalTime;

- (int64_t)duration;

- (float)frameRate;

- (int64_t)startTime;

- (void)setStartTime:(int64_t)time;

- (int64_t)currentTime;

- (void)setCurrentTime:(int64_t)time;

- (double)getProgress;

- (void)setProgress:(double)value;

- (PAGLayer*)trackMatteLayer;

- (CGRect)getBounds;

- (BOOL)excludedFromTimeline;

- (void)setExcludedFromTimeline:(BOOL)value;

- (float)alpha;

- (void)setAlpha:(float)value;

@end
