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
#import "private/PAGLayer+Internal.h"

@implementation PAGLayer

- (instancetype)initWithImpl:(id)impl {
  self = [super init];
  if (self) {
    _impl = [impl retain];
  }
  return self;
}

- (PAGLayerType)layerType {
  return (PAGLayerType)[_impl layerType];
}

- (NSString*)layerName {
  return [(PAGLayerImpl*)_impl layerName];
}

- (id)impl {
  return _impl;
}

- (CGAffineTransform)matrix {
  return [(PAGLayerImpl*)_impl matrix];
}

- (void)setMatrix:(CGAffineTransform)matrix {
  [(PAGLayerImpl*)_impl setMatrix:matrix];
}

- (void)resetMatrix {
  [(PAGLayerImpl*)_impl resetMatrix];
}

- (CGAffineTransform)getTotalMatrix {
  return [(PAGLayerImpl*)_impl getTotalMatrix];
}

- (BOOL)visible {
  return [(PAGLayerImpl*)_impl visible];
}

- (void)setVisible:(BOOL)visible {
  [(PAGLayerImpl*)_impl setVisible:visible];
}

- (NSInteger)editableIndex {
  return [(PAGLayerImpl*)_impl editableIndex];
}

- (PAGComposition*)parent {
  return [(PAGLayerImpl*)_impl parent];
}

- (NSArray<PAGMarker*>*)markers {
  return [(PAGLayerImpl*)_impl markers];
}

- (int64_t)localTimeToGlobal:(int64_t)localTime {
  return [(PAGLayerImpl*)_impl localTimeToGlobal:localTime];
}

- (int64_t)globalToLocalTime:(int64_t)globalTime {
  return [(PAGLayerImpl*)_impl globalToLocalTime:globalTime];
}

- (int64_t)duration {
  return [(PAGLayerImpl*)_impl duration];
}

- (float)frameRate {
  return [(PAGLayerImpl*)_impl frameRate];
}

- (int64_t)startTime {
  return [(PAGLayerImpl*)_impl startTime];
}

- (void)setStartTime:(int64_t)time {
  [(PAGLayerImpl*)_impl setStartTime:time];
}

- (int64_t)currentTime {
  return [(PAGLayerImpl*)_impl currentTime];
}

- (void)setCurrentTime:(int64_t)time {
  [(PAGLayerImpl*)_impl setCurrentTime:time];
}

- (double)getProgress {
  return [(PAGLayerImpl*)_impl getProgress];
}

- (void)setProgress:(double)value {
  [(PAGLayerImpl*)_impl setProgress:value];
}

- (PAGLayer*)trackMatteLayer {
  return [(PAGLayerImpl*)_impl trackMatteLayer];
}

- (CGRect)getBounds {
  return [(PAGLayerImpl*)_impl getBounds];
}

- (BOOL)excludedFromTimeline {
  return [(PAGLayerImpl*)_impl excludedFromTimeline];
}

- (void)setExcludedFromTimeline:(BOOL)value {
  [(PAGLayerImpl*)_impl setExcludedFromTimeline:value];
}

- (float)alpha {
  return [(PAGLayerImpl*)_impl alpha];
}

- (void)setAlpha:(float)value {
  [(PAGLayerImpl*)_impl setAlpha:value];
}

- (void)dealloc {
  [_impl release];
  [super dealloc];
}
@end
