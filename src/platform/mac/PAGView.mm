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

#if defined(TGFX_USE_OPENGL) || defined(TGFX_USE_METAL)

#import "PAGPlayer.h"
#import "platform/cocoa/private/PAGAnimator.h"

#if defined(TGFX_USE_OPENGL)
#import "platform/mac/private/GPUDrawable.h"
#endif

#if defined(TGFX_USE_METAL)
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>
#endif

@interface PAGView () <PAGAnimatorUpdater, PAGAnimatorListener>
@end

@implementation PAGView {
  PAGPlayer* pagPlayer;
  PAGSurface* pagSurface;
  PAGFile* pagFile;
  NSString* filePath;
  PAGAnimator* animator;
  BOOL _isVisible;
  NSHashTable* listeners;
  NSLock* listenerLock;
  int _metalInitRetries;
}

- (instancetype)initWithFrame:(CGRect)frame {
  if (self = [super initWithFrame:frame]) {
    [self initPAG];
  }
  return self;
}

#if defined(TGFX_USE_METAL)
- (CALayer*)makeBackingLayer {
  return [CAMetalLayer layer];
}

- (void)updateLayerDrawableSize {
  CAMetalLayer* layer = (CAMetalLayer*)self.layer;
  CGSize size = self.bounds.size;
  // NSView's layer.contentsScale does not track window.backingScaleFactor automatically, so sync it
  // explicitly and derive the drawableSize in pixels (bounds × scale).
  CGFloat scale = self.window.backingScaleFactor > 0 ? self.window.backingScaleFactor : 1.0;
  layer.contentsScale = scale;
  layer.drawableSize = CGSizeMake(size.width * scale, size.height * scale);
}
#endif

- (void)initPAG {
#if defined(TGFX_USE_METAL)
  // NSView is not layer-backed by default; enable it so makeBackingLayer creates the CAMetalLayer
  // and self.layer returns a valid CAMetalLayer to render into.
  self.wantsLayer = YES;
#endif
  _isVisible = FALSE;
  pagFile = nil;
  filePath = nil;
  self.layer.backgroundColor = [NSColor clearColor].CGColor;
  pagPlayer = [[PAGPlayer alloc] init];
  animator = [[PAGAnimator alloc] initWithUpdater:(id<PAGAnimatorUpdater>)self];
  listeners = [[NSHashTable weakObjectsHashTable] retain];
  listenerLock = [[NSLock alloc] init];
  [animator addListener:self];
  // The animator must be set to sync mode. Otherwise, the internal surface in the PAGSurface could
  // not be created.
  [animator setSync:YES];
#if defined(TGFX_USE_OPENGL)
  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(onAsyncSurfacePrepared:)
                                               name:pag::AsyncSurfacePreparedNotification
                                             object:self];
#endif
}

- (void)dealloc {
  [animator cancel];
  [animator release];
  [pagPlayer release];
  [pagSurface release];
  [pagFile release];
  [filePath release];
  [listeners release];
  [listenerLock release];
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)setBounds:(CGRect)bounds {
  CGRect oldBounds = self.bounds;
  [super setBounds:bounds];
  if (pagSurface != nil &&
      (oldBounds.size.width != bounds.size.width || oldBounds.size.height != bounds.size.height)) {
#if defined(TGFX_USE_METAL)
    [self updateLayerDrawableSize];
#endif
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
#if defined(TGFX_USE_METAL)
    [self updateLayerDrawableSize];
#endif
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
#if defined(TGFX_USE_METAL)
  CAMetalLayer* layer = (CAMetalLayer*)self.layer;
  id<MTLDevice> device = MTLCreateSystemDefaultDevice();
  if (device == nil) {
    // Metal device isn't available yet on macOS: in an AppKit app, viewDidMoveToWindow can fire
    // before applicationDidFinishLaunching finishes setting up GPU access, so the very first
    // attempt here may return nil. Retry on the next runloop iteration until it succeeds.
    if (_metalInitRetries++ < 20) {
      dispatch_async(dispatch_get_main_queue(), ^{
        [self initPAGSurface];
      });
    } else {
      NSLog(@"[PAGView] Metal device unavailable after %d retries; giving up", _metalInitRetries);
    }
    return;
  }
  _metalInitRetries = 0;
  layer.device = device;
  layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
  layer.framebufferOnly = YES;
  // CAMetalLayer does not auto-derive drawableSize from bounds * contentsScale, so set it
  // explicitly here; otherwise the drawable is 0x0 and Metal rendering stays invisible.
  [self updateLayerDrawableSize];
  pagSurface = [[PAGSurface FromMetalLayer:layer] retain];
#else
  pagSurface = [[PAGSurface FromView:self] retain];
#endif
  [pagPlayer setSurface:pagSurface];
  [animator update];
}

- (void)addListener:(id<PAGViewListener>)listener {
  if (listener == nil) {
    return;
  }
  [listenerLock lock];
  [listeners addObject:listener];
  [listenerLock unlock];
}

- (void)removeListener:(id<PAGViewListener>)listener {
  if (listener == nil) {
    return;
  }
  [listenerLock lock];
  [listeners removeObject:listener];
  [listenerLock unlock];
}

#pragma mark - PAGAnimatorListener

- (void)onAnimationStart:(id<PAGAnimatorUpdater>)updater {
  [self dispatchListenerEvent:@selector(onAnimationStart:)];
}

- (void)onAnimationEnd:(id<PAGAnimatorUpdater>)updater {
  [self dispatchListenerEvent:@selector(onAnimationEnd:)];
}

- (void)onAnimationCancel:(id<PAGAnimatorUpdater>)updater {
  [self dispatchListenerEvent:@selector(onAnimationCancel:)];
}

- (void)onAnimationRepeat:(id<PAGAnimatorUpdater>)updater {
  [self dispatchListenerEvent:@selector(onAnimationRepeat:)];
}

- (void)onAnimationUpdate:(id<PAGAnimatorUpdater>)updater {
  [self dispatchListenerEvent:@selector(onAnimationUpdate:)];
}

- (void)dispatchListenerEvent:(SEL)selector {
  if ([NSThread isMainThread]) {
    [self performListenerEventOnMainThread:selector];
    return;
  }
  // Retain self before crossing threads to keep the receiver alive until the
  // dispatched block finishes notifying listeners on the main thread.
  [self retain];
  dispatch_async(dispatch_get_main_queue(), ^{
    [self performListenerEventOnMainThread:selector];
    [self release];
  });
}

- (void)performListenerEventOnMainThread:(SEL)selector {
  [listenerLock lock];
  NSArray* copiedListeners = [[listeners allObjects] retain];
  [listenerLock unlock];
  for (id<PAGViewListener> listener in copiedListeners) {
    if ([listener respondsToSelector:selector]) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
      [listener performSelector:selector withObject:self];
#pragma clang diagnostic pop
    }
  }
  [copiedListeners release];
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

#if defined(TGFX_USE_OPENGL)
- (void)onAsyncSurfacePrepared:(NSNotification*)notification {
  [animator update];
}
#endif
@end

#endif  // TGFX_USE_OPENGL || TGFX_USE_METAL
