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
#import "platform/cocoa/private/PAGAnimatorListenerProxy.h"
#include "platform/cocoa/private/PAGViewSurface.h"

@interface PAGView () <PAGAnimatorUpdater, PAGViewAnimatorForwarder>
@end

@implementation PAGView {
  PAGPlayer* pagPlayer;
  PAGSurface* pagSurface;
  PAGFile* pagFile;
  NSString* filePath;
  PAGAnimator* animator;
  PAGAnimatorListenerProxy* animatorListenerProxy;
  BOOL _isVisible;
  NSHashTable* listeners;
  NSLock* listenerLock;
  std::mutex renderLock;
}

- (instancetype)initWithFrame:(CGRect)frame {
  if (self = [super initWithFrame:frame]) {
    [self initPAG];
  }
  return self;
}

- (CALayer*)makeBackingLayer {
  Class layerClass = pag::cocoa::PAGViewSurface::LayerClass();
  return layerClass == Nil ? [super makeBackingLayer] : [layerClass layer];
}

- (void)initPAG {
  pag::cocoa::PAGViewSurface::Configure(self);
  _isVisible = FALSE;
  pagFile = nil;
  filePath = nil;
  self.layer.backgroundColor = [NSColor clearColor].CGColor;
  pagPlayer = [[PAGPlayer alloc] init];
  animator = [[PAGAnimator alloc] initWithUpdater:(id<PAGAnimatorUpdater>)self];
  listeners = [[NSHashTable weakObjectsHashTable] retain];
  listenerLock = [[NSLock alloc] init];
  animatorListenerProxy = [[PAGAnimatorListenerProxy alloc] initWithForwarder:self];
  [animator addListener:animatorListenerProxy];
  // The animator must be set to sync mode. Otherwise, the internal surface in the PAGSurface could
  // not be created.
  [animator setSync:YES];
  pag::cocoa::PAGViewSurface::ObserveSurfacePrepared(self, @selector(onAsyncSurfacePrepared:));
}

- (void)dealloc {
  [animator cancel];
  [animator release];
  [animatorListenerProxy release];
  {
    auto autoLock = pag::cocoa::PAGViewSurface::Lock(renderLock);
    [pagPlayer release];
    pagPlayer = nil;
    [pagSurface release];
    pagSurface = nil;
  }
  [pagFile release];
  [filePath release];
  [listeners release];
  [listenerLock release];
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)refreshSurfaceSize {
  auto autoLock = pag::cocoa::PAGViewSurface::Lock(renderLock);
  if (pagSurface == nil) {
    return;
  }
  pag::cocoa::PAGViewSurface::UpdateSize(self, pagSurface);
}

- (void)setBounds:(CGRect)bounds {
  CGRect oldBounds = self.bounds;
  [super setBounds:bounds];
  // Resizing the view's coordinate system directly does not necessarily go through
  // setFrameSize:, and AppKit does not document which internal paths call which setter. Both
  // overrides therefore call the same helper, and the size comparison here keeps an
  // already-applied change from being applied twice.
  if (pagSurface != nil &&
      (oldBounds.size.width != bounds.size.width || oldBounds.size.height != bounds.size.height)) {
    [self refreshSurfaceSize];
    if (oldBounds.size.width == 0 || oldBounds.size.height == 0) {
      [animator update];
    }
  }
}

- (void)setFrameSize:(NSSize)newSize {
  NSSize oldSize = self.bounds.size;
  [super setFrameSize:newSize];
  // setFrame: calls this internally, and autoresizing / Auto Layout drive it directly, so this is
  // the main entry for frame-driven resizes. Handling the same change in setFrame: as well would
  // run updateSize twice per resize.
  if (pagSurface != nil && (oldSize.width != newSize.width || oldSize.height != newSize.height)) {
    [self refreshSurfaceSize];
    if (oldSize.width == 0 || oldSize.height == 0) {
      [animator update];
    }
  }
}

- (void)viewDidMoveToWindow {
  [super viewDidMoveToWindow];
  [self checkVisible];
}

- (void)viewDidChangeBackingProperties {
  [super viewDidChangeBackingProperties];
  auto autoLock = pag::cocoa::PAGViewSurface::Lock(renderLock);
  pag::cocoa::PAGViewSurface::UpdateWindowSize(self, pagSurface);
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
  {
    auto autoLock = pag::cocoa::PAGViewSurface::Lock(renderLock);
    pagSurface = [pag::cocoa::PAGViewSurface::CreateSurface(self) retain];
    [pagPlayer setSurface:pagSurface];
  }
  // Synchronous updates re-enter onAnimationFlush:, which acquires renderLock.
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

#pragma mark - Listener dispatch

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
  auto autoLock = pag::cocoa::PAGViewSurface::Lock(renderLock);
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
  auto autoLock = pag::cocoa::PAGViewSurface::Lock(renderLock);
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

- (void)onAsyncSurfacePrepared:(NSNotification*)notification {
  [animator update];
}
@end
