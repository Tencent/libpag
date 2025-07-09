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

#import "PAGPlayer.h"
#import "platform/cocoa/private/PAGPlayerImpl.h"

@interface PAGSurface ()

@property(nonatomic, strong) PAGSurfaceImpl* surface;

@end

@implementation PAGPlayer {
  PAGPlayerImpl* pagPlayer;
  PAGSurface* pagSurface;
}

- (instancetype)init {
  if (self = [super init]) {
    pagPlayer = [[PAGPlayerImpl alloc] init];
    pagSurface = nil;
  }
  return self;
}

- (void)dealloc {
  [pagPlayer release];
  [pagSurface release];
  [super dealloc];
}

- (PAGComposition*)getComposition {
  return [pagPlayer getComposition];
}

- (void)setComposition:(PAGComposition*)newComposition {
  [pagPlayer setComposition:newComposition];
}

- (PAGSurface*)getSurface {
  return pagSurface == nil ? nil : [[pagSurface retain] autorelease];
}

- (void)setSurface:(PAGSurface*)surface {
  if (pagSurface == surface) {
    return;
  }
  if (pagSurface != nil) {
    [pagSurface release];
    pagSurface = nil;
  }
  if (surface != nil) {
    pagSurface = [surface retain];
    [pagPlayer setSurface:[surface surface]];
  } else {
    [pagPlayer setSurface:nil];
  }
}

- (BOOL)videoEnabled {
  return [pagPlayer videoEnabled];
}

- (void)setVideoEnabled:(BOOL)value {
  [pagPlayer setVideoEnabled:value];
}

- (BOOL)cacheEnabled {
  return [pagPlayer cacheEnabled];
}

- (void)setCacheEnabled:(BOOL)value {
  [pagPlayer setCacheEnabled:value];
}

- (BOOL)useDiskCache {
  return [pagPlayer useDiskCache];
}

- (void)setUseDiskCache:(BOOL)value {
  [pagPlayer setUseDiskCache:value];
}

- (float)cacheScale {
  return [pagPlayer cacheScale];
}

- (void)setCacheScale:(float)value {
  [pagPlayer setCacheScale:value];
}

- (float)maxFrameRate {
  return [pagPlayer maxFrameRate];
}

- (void)setMaxFrameRate:(float)value {
  [pagPlayer setMaxFrameRate:value];
}

- (PAGScaleMode)scaleMode {
  return (PAGScaleMode)[pagPlayer scaleMode];
}

- (void)setScaleMode:(PAGScaleMode)value {
  [pagPlayer setScaleMode:value];
}

- (CGAffineTransform)matrix {
  return [pagPlayer matrix];
}

- (void)setMatrix:(CGAffineTransform)value {
  [pagPlayer setMatrix:value];
}

- (int64_t)duration {
  return [pagPlayer duration];
}

- (double)getProgress {
  return [pagPlayer getProgress];
}

- (void)setProgress:(double)value {
  [pagPlayer setProgress:value];
}

- (int64_t)currentFrame {
  return [pagPlayer currentFrame];
}

- (void)prepare {
  [pagPlayer prepare];
}

- (BOOL)flush {
  return [pagPlayer flush];
}

- (CGRect)getBounds:(PAGLayer*)pagLayer {
  return [pagPlayer getBounds:pagLayer];
}

- (NSArray<PAGLayer*>*)getLayersUnderPoint:(CGPoint)point {
  return [pagPlayer getLayersUnderPoint:point];
}

- (BOOL)hitTestPoint:(PAGLayer*)layer point:(CGPoint)point {
  return [pagPlayer hitTestPoint:layer point:point pixelHitTest:FALSE];
}

- (BOOL)hitTestPoint:(PAGLayer*)layer point:(CGPoint)point pixelHitTest:(BOOL)pixelHitTest {
  return [pagPlayer hitTestPoint:layer point:point pixelHitTest:pixelHitTest];
}
@end
