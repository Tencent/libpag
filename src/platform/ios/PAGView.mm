/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#import "platform/ios/private/GPUDrawable.h"
#import "platform/ios/private/PAGSurface+Internal.h"
#import "platform/ios/private/PAGValueAnimator.h"

namespace pag {
static NSOperationQueue* flushQueue;
void DestoryFlushQueue() {
  NSOperationQueue* queue = flushQueue;
  [queue release];
  flushQueue = nil;
  [queue cancelAllOperations];
  [queue waitUntilAllOperationsAreFinished];
}
}

@interface PAGView ()
@property(atomic, assign) BOOL isAsyncFlushing;
@property(atomic, assign) BOOL bufferPrepared;
@property(atomic, assign) BOOL isInBackground;
@end

@implementation PAGView {
  PAGPlayer* pagPlayer;
  PAGSurface* pagSurface;
  PAGFile* pagFile;
  NSString* filePath;
  PAGValueAnimator* valueAnimator;
  BOOL _isPlaying;
  BOOL _isVisible;
  NSMutableDictionary* textReplacementMap;
  NSMutableDictionary* imageReplacementMap;
  NSHashTable* listeners;
}

+ (NSOperationQueue*)FlushQueue {
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    pag::flushQueue = [[[NSOperationQueue alloc] init] retain];
    pag::flushQueue.maxConcurrentOperationCount = 1;
    pag::flushQueue.name = @"pag.io.PAGView";
  });
  return pag::flushQueue;
}

/// 函数用于在执行 exit() 函数时把渲染任务全部完成，防止 PAG 的全局函数被析构，导致 PAG 野指针
/// crash。 注意这里注册需要等待 PAG 执行一次后再进行注册。因此需要等到 bufferPerpared 并再执行一次
/// flush 后, 否则 PAG 的 static 对象仍然会先析构
+ (void)RegisterFlushQueueDestoryMethod {
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    atexit(pag::DestoryFlushQueue);
  });
}

- (instancetype)initWithFrame:(CGRect)frame {
  if (self = [super initWithFrame:frame]) {
    [self initPAG];
  }
  return self;
}

- (void)initPAG {
  listeners = [[NSHashTable weakObjectsHashTable] retain];
  textReplacementMap = [[NSMutableDictionary dictionary] retain];
  imageReplacementMap = [[NSMutableDictionary dictionary] retain];
  _isPlaying = FALSE;
  _isVisible = FALSE;
  self.isInBackground =
      [UIApplication sharedApplication].applicationState == UIApplicationStateBackground;
  self.isAsyncFlushing = FALSE;
  pagFile = nil;
  filePath = nil;
  self.contentScaleFactor = [UIScreen mainScreen].scale;
  self.backgroundColor = [UIColor clearColor];
  pagPlayer = [[PAGPlayer alloc] init];
  valueAnimator = [[PAGValueAnimator alloc] init];
  [valueAnimator setListener:self];
  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(applicationDidBecomeActive:)
                                               name:UIApplicationDidBecomeActiveNotification
                                             object:nil];
  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(renderTargetBufferPrepared:)
                                               name:pag::kGPURenderTargetBufferPreparedNotification
                                             object:self.layer];
  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(applicationWillResignActive:)
                                               name:UIApplicationWillResignActiveNotification
                                             object:nil];
}

- (void)dealloc {
  [listeners release];
  [textReplacementMap release];
  [imageReplacementMap release];
  [valueAnimator stop:false];  // must stop the animator, or it will not dealloc since it is
                               // referenced by global displayLink.
  [valueAnimator release];
  [pagPlayer release];
  if (pagSurface != nil) {
    [pagSurface release];
  }
  if (pagFile != nil) {
    [pagFile release];
  }
  if (filePath != nil) {
    [filePath release];
  }
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
      [self updateView];
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
      [self updateView];
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
    ///先调用doPlay·doPlay会获取composition的duration，如果先初始化PAGSurface会让composition被锁，导致异步失效
    if (_isPlaying) {
      [self doPlay];
    }
    if (pagSurface == nil) {
      [self initPAGSurface];
    }
  } else {
    if (_isPlaying) {
      [valueAnimator stop:false];
    }
  }
}

- (void)initPAGSurface {
  CAEAGLLayer* layer = (CAEAGLLayer*)[self layer];
  pagSurface = [[PAGSurface FromLayer:layer] retain];
  [pagPlayer setSurface:pagSurface];
  [self updateView];
}

- (void)onAnimationUpdate {
  [self updateView];
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

- (void)onAnimationStart {
  NSHashTable* copiedListeners = listeners.copy;
  for (id item in copiedListeners) {
    id<PAGViewListener> listener = (id<PAGViewListener>)item;
    if ([listener respondsToSelector:@selector(onAnimationStart:)]) {
      [listener onAnimationStart:self];
    }
    if ([listener respondsToSelector:@selector(onAnimationStart)]) {
      [listener onAnimationStart];
    }
  }
  [copiedListeners release];
}

- (void)onAnimationEnd {
  _isPlaying = false;
  NSHashTable* copiedListeners = listeners.copy;
  for (id item in copiedListeners) {
    id<PAGViewListener> listener = (id<PAGViewListener>)item;
    if ([listener respondsToSelector:@selector(onAnimationEnd:)]) {
      [listener onAnimationEnd:self];
    }
    if ([listener respondsToSelector:@selector(onAnimationEnd)]) {
      [listener onAnimationEnd];
    }
  }
  [copiedListeners release];
}

- (void)onAnimationCancel {
  NSHashTable* copiedListeners = listeners.copy;
  for (id item in copiedListeners) {
    id<PAGViewListener> listener = (id<PAGViewListener>)item;
    if ([listener respondsToSelector:@selector(onAnimationCancel:)]) {
      [listener onAnimationCancel:self];
    }
    if ([listener respondsToSelector:@selector(onAnimationCancel)]) {
      [listener onAnimationCancel];
    }
  }
  [copiedListeners release];
}

- (void)onAnimationRepeat {
  NSHashTable* copiedListeners = listeners.copy;
  for (id item in copiedListeners) {
    id<PAGViewListener> listener = (id<PAGViewListener>)item;
    if ([listener respondsToSelector:@selector(onAnimationRepeat:)]) {
      [listener onAnimationRepeat:self];
    }
    if ([listener respondsToSelector:@selector(onAnimationRepeat)]) {
      [listener onAnimationRepeat];
    }
  }
  [copiedListeners release];
}

#pragma clang diagnostic pop

- (void)addListener:(id<PAGViewListener>)listener {
  [listeners addObject:listener];
}

- (void)removeListener:(id<PAGViewListener>)listener {
  [listeners removeObject:listener];
}

- (BOOL)isPlaying {
  return _isPlaying;
}

- (void)play {
  _isPlaying = true;
  if ([valueAnimator getAnimatedFraction] == 1.0) {
    [self setProgress:0];
  }
  [self doPlay];
}

- (void)doPlay {
  if (!_isVisible) {
    return;
  }
  int64_t playTime = (int64_t)([valueAnimator getAnimatedFraction] * [valueAnimator duration]);
  [valueAnimator setCurrentPlayTime:playTime];
  [valueAnimator start];
}

- (void)stop {
  _isPlaying = false;
  [valueAnimator stop];
}

- (void)setRepeatCount:(int)repeatCount {
  if (repeatCount < 0) {
    repeatCount = 0;
  }
  [valueAnimator setRepeatCount:(repeatCount - 1)];
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
  [valueAnimator setDuration:[pagPlayer duration]];
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
  return [valueAnimator getAnimatedFraction];
}

- (void)setProgress:(double)value {
  [valueAnimator setCurrentPlayTime:(int64_t)(value * valueAnimator.duration)];
  [valueAnimator setRepeatedTimes:0];
}

- (BOOL)flush {
  if (self.isInBackground) {
    return false;
  }
  [pagPlayer setProgress:[valueAnimator getAnimatedFraction]];
  auto result = [pagPlayer flush];
  NSHashTable* copiedListeners = listeners.copy;
  for (id item in copiedListeners) {
    id<PAGViewListener> listener = (id<PAGViewListener>)item;
    if ([listener respondsToSelector:@selector(onAnimationUpdate:)]) {
      [listener onAnimationUpdate:self];
    }
  }
  [copiedListeners release];
  if (self.bufferPrepared) {
    [PAGView RegisterFlushQueueDestoryMethod];
  }
  return result;
}

- (NSArray<PAGLayer*>*)getLayersUnderPoint:(CGPoint)point {
  return [pagPlayer getLayersUnderPoint:point];
}

- (void)freeCache {
  if (pagSurface != nil) {
    [pagSurface freeCache];
  }
}

- (void)updateView {
  if (_sync) {
    [self flush];
  } else {
    [self updateViewAsync];
  }
}

- (void)updateViewAsync {
  if (self.isAsyncFlushing) {
    return;
  }
  self.isAsyncFlushing = TRUE;
  NSOperationQueue* flushQueue = [PAGView FlushQueue];
  [self retain];
  NSBlockOperation* operation = [NSBlockOperation blockOperationWithBlock:^{
    [self flush];
    self.isAsyncFlushing = FALSE;
    dispatch_async(dispatch_get_main_queue(), ^{
      [self release];
    });
  }];
  [flushQueue addOperation:operation];
}

- (void)applicationDidBecomeActive:(NSNotification*)notification {
  self.isInBackground = FALSE;
  if (_isVisible) {
    [self updateView];
  }
}

- (void)renderTargetBufferPrepared:(NSNotification*)notification {
  self.bufferPrepared = TRUE;
  if ([notification.userInfo[pag::kPreparedAsync] boolValue]) {
    [self updateView];
  }
}

- (void)applicationWillResignActive:(NSNotification*)notification {
  self.isInBackground = TRUE;
}
@end
