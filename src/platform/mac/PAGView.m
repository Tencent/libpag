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

#import "PAGView.h"
#import "PAGPlayer.h"
#import "platform/cocoa/private/PAGAnimator.h"

@implementation PAGView {
  PAGPlayer* pagPlayer;
  PAGSurface* pagSurface;
  PAGFile* pagFile;
  NSString* filePath;
  PAGAnimator* animator;
  BOOL _isVisible;
}

- (instancetype)initWithFrame:(CGRect)frame {
  if (self = [super initWithFrame:frame]) {
    [self initPAG];
  }
  return self;
}

- (void)initPAG {
  _isVisible = FALSE;
  pagFile = nil;
  filePath = nil;
  self.layer.backgroundColor = [NSColor clearColor].CGColor;
  pagPlayer = [[PAGPlayer alloc] init];
  animator = [[PAGAnimator alloc] initWithUpdater:(id<PAGAnimatorUpdater>)self];
  // The animator must be set to sync mode. Otherwise, the internal surface in the PAGSurface could
  // not be created.
  [animator setSync:YES];
}

- (void)dealloc {
  [animator cancel];
  [animator release];
  [pagPlayer release];
  [pagSurface release];
  [pagFile release];
  [filePath release];
  [super dealloc];
}

- (void)setBounds:(CGRect)bounds {
  CGRect oldBounds = self.bounds;
  [super setBounds:bounds];
  if (pagSurface != nil &&
      (oldBounds.size.width != bounds.size.width || oldBounds.size.height != bounds.size.height)) {
    [pagSurface updateSize];
    if (oldBounds.size.width == 0 || oldBounds.size.height == 0) {
      [animator update];
    }
  }
}

- (void)setFrame:(CGRect)frame {
  CGRect oldRect = self.frame;
  [super setFrame:frame];
  if (pagSurface != nil &&
      (oldRect.size.width != frame.size.width || oldRect.size.height != frame.size.height)) {
    [pagSurface updateSize];
    if (oldRect.size.width == 0 || oldRect.size.height == 0) {
      [animator update];
    }
  }
}

- (void)viewDidMoveToWindow {
  [super viewDidMoveToWindow];
  [self checkVisible];
}

- (void)setAlphaValue:(CGFloat)alphaValue {
  [super setAlphaValue:alphaValue];
  [self checkVisible];
}

- (void)setHidden:(BOOL)hidden {
  [super setHidden:hidden];
  [self checkVisible];
}

- (void)checkVisible {
  BOOL visible = self.window && !self.isHidden && self.alphaValue > 0.0;
  if (_isVisible == visible) {
    return;
  }
  _isVisible = visible;
  if (_isVisible) {
    [animator setDuration:[pagPlayer duration]];
    if (pagSurface == nil) {
      [self initPAGSurface];
    }
  } else {
    [animator setDuration:0];
  }
}

- (void)initPAGSurface {
  pagSurface = [[PAGSurface FromView:self] retain];
  [pagPlayer setSurface:pagSurface];
  [animator update];
}

- (void)addListener:(id<PAGViewListener>)listener {
  [animator addListener:(id<PAGAnimatorListener>)listener];
}

- (void)removeListener:(id<PAGViewListener>)listener {
  [animator removeListener:(id<PAGAnimatorListener>)listener];
}

- (void)onAnimationFlush:(double)progress {
  [pagPlayer setProgress:progress];
  [pagPlayer flush];
}

- (int)repeatCount {
  return [animator repeatCount];
}

- (void)setRepeatCount:(int)repeatCount {
  [animator setRepeatCount:repeatCount];
}

- (BOOL)isPlaying {
  return [animator isRunning];
}

- (void)play {
  [pagPlayer prepare];
  [animator start];
}

- (void)pause {
  [animator cancel];
}

- (void)stop {
  [animator cancel];
}

- (NSString*)getPath {
  return filePath == nil ? nil : [[filePath retain] autorelease];
}

- (BOOL)setPath:(NSString*)path {
  if (filePath != nil) {
    [filePath release];
    filePath = nil;
  }
  PAGFile* file = [PAGFile Load:path];
  [self setComposition:file];
  filePath = [path retain];
  return file != nil;
}

- (PAGComposition*)getComposition {
  return [pagPlayer getComposition];
}

- (void)setComposition:(PAGComposition*)newComposition {
  if (filePath != nil) {
    [filePath release];
    filePath = nil;
  }
  if (pagFile != nil) {
    [pagFile release];
    pagFile = nil;
  }
  [pagPlayer setComposition:newComposition];
  [animator setProgress:[pagPlayer getProgress]];
  if (_isVisible) {
    [animator setDuration:[pagPlayer duration]];
  }
}

- (BOOL)videoEnabled {
  return [pagPlayer videoEnabled];
}

- (void)setVideoEnabled:(BOOL)enable {
  [pagPlayer setVideoEnabled:enable];
}

- (BOOL)cacheEnabled {
  return [pagPlayer cacheEnabled];
}

- (void)setCacheEnabled:(BOOL)value {
  [pagPlayer setCacheEnabled:value];
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
  return [pagPlayer scaleMode];
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
  return [animator progress];
}

- (void)setProgress:(double)value {
  [pagPlayer setProgress:value];
  [animator setProgress:[pagPlayer getProgress]];
}

- (BOOL)flush {
  return [pagPlayer flush];
}

- (NSArray<PAGLayer*>*)getLayersUnderPoint:(CGPoint)point {
  return [pagPlayer getLayersUnderPoint:point];
}

- (void)freeCache {
  if (pagSurface != nil) {
    [pagSurface freeCache];
  }
}

- (CVPixelBufferRef)makeSnapshot {
  if (pagSurface != nil) {
    return [pagSurface makeSnapshot];
  }
  return nil;
}

- (CGRect)getBounds:(PAGLayer*)pagLayer {
  if (pagLayer != nil) {
    return [pagPlayer getBounds:pagLayer];
  }
  return CGRectNull;
}
@end
