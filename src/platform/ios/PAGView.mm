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
#import "PAGSurface.h"
#import "platform/cocoa/private/PAGAnimator.h"
#import "platform/ios/private/GPUDrawable.h"

@implementation PAGView {
  PAGPlayer* pagPlayer;
  PAGSurface* pagSurface;
  NSString* filePath;
  PAGAnimator* animator;
  BOOL _isVisible;
  std::mutex lock;
}

- (instancetype)initWithFrame:(CGRect)frame {
  if (self = [super initWithFrame:frame]) {
    [self initPAG];
  }
  return self;
}

- (void)initPAG {
  _isVisible = FALSE;
  filePath = nil;
  self.contentScaleFactor = [UIScreen mainScreen].scale;
  self.backgroundColor = [UIColor clearColor];
  pagPlayer = [[PAGPlayer alloc] init];
  animator = [[PAGAnimator alloc] initWithUpdater:(id<PAGAnimatorUpdater>)self];
  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(applicationDidBecomeActive:)
                                               name:UIApplicationDidBecomeActiveNotification
                                             object:nil];
  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(applicationDidReceiveMemoryWarning:)
                                               name:UIApplicationDidReceiveMemoryWarningNotification
                                             object:nil];
  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(AsyncSurfacePrepared:)
                                               name:pag::kAsyncSurfacePreparedNotification
                                             object:self.layer];
}

- (void)dealloc {
  [animator cancel];
  [animator release];
  [pagPlayer release];
  [pagSurface release];
  [filePath release];
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

+ (Class)layerClass {
  return [CAEAGLLayer class];
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

- (void)setContentScaleFactor:(CGFloat)scaleFactor {
  CGFloat oldScaleFactor = self.contentScaleFactor;
  [super setContentScaleFactor:scaleFactor];
  if (pagSurface != nil && oldScaleFactor != scaleFactor) {
    [pagSurface updateSize];
  }
}

- (void)didMoveToWindow {
  [super didMoveToWindow];
  [self checkVisible];
}

- (void)setAlpha:(CGFloat)alpha {
  [super setAlpha:alpha];
  [self checkVisible];
}

- (void)setHidden:(BOOL)hidden {
  [super setHidden:hidden];
  [self checkVisible];
}

- (void)checkVisible {
  BOOL visible = self.window && !self.isHidden && self.alpha > 0.0;
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
  CAEAGLLayer* layer = (CAEAGLLayer*)[self layer];
  pagSurface = [[PAGSurface FromLayer:layer] retain];
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
  if (_isVisible) {
    [animator setDuration:[pagPlayer duration]];
  }
  [pagPlayer flush];
}

- (BOOL)sync {
  return [animator isSync];
}

- (void)setSync:(BOOL)value {
  [animator setSync:value];
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
  std::lock_guard<std::mutex> autoLock(lock);
  return filePath == nil ? nil : [[filePath retain] autorelease];
}

- (BOOL)setPath:(NSString*)path {
  std::lock_guard<std::mutex> autoLock(lock);
  if (filePath != nil) {
    [filePath release];
    filePath = nil;
  }
  filePath = [path retain];
  PAGFile* file = [PAGFile Load:path];
  [self setCompositionInternal:file];
  return file != nil;
}

- (void)setPathAsync:(NSString*)path completionBlock:(void (^)(PAGFile*))callback {
  std::lock_guard<std::mutex> autoLock(lock);
  if (filePath != nil) {
    [filePath release];
    filePath = nil;
  }
  filePath = [path retain];
  [self retain];
  [PAGFile LoadAsync:path
      completionBlock:^(PAGFile* file) {
        [self setCompositionInternal:file];
        callback(file);
        [self release];
      }];
}

- (PAGComposition*)getComposition {
  return [pagPlayer getComposition];
}

- (void)setComposition:(PAGComposition*)newComposition {
  std::lock_guard<std::mutex> autoLock(lock);
  if (filePath != nil) {
    [filePath release];
    filePath = nil;
  }
  [self setCompositionInternal:newComposition];
}

- (void)setCompositionInternal:(PAGComposition*)newComposition {
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
  // TODO(domchen): Remove the next line. All pending changes should be applied in flush().
  [animator update];
}

- (int64_t)currentFrame {
  return [pagPlayer currentFrame];
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

- (void)applicationDidBecomeActive:(NSNotification*)notification {
  if (_isVisible) {
    [animator update];
  }
}

- (void)applicationDidReceiveMemoryWarning:(NSNotification*)notification {
  [self freeCache];
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

- (void)AsyncSurfacePrepared:(NSNotification*)notification {
  [animator update];
}
@end
