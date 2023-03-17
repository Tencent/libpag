/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

#import "PAGImageView.h"

#import <CommonCrypto/CommonCrypto.h>
#import <VideoToolbox/VideoToolbox.h>
#include <compression.h>

#import "PAGDecoder.h"
#import "PAGFile.h"
#import "platform/cocoa/private/PAGComposition+Internal.h"
#import "platform/cocoa/private/PixelBufferUtils.h"
#import "private/PAGCacheManager.h"
#import "private/PAGDiskCache.h"
#import "private/PAGValueAnimator.h"

namespace pag {
static NSOperationQueue* imageViewFlushQueue;
void DestoryImageViewFlushQueue() {
  NSOperationQueue* queue = imageViewFlushQueue;
  [queue release];
  imageViewFlushQueue = nil;
  [queue cancelAllOperations];
  [queue waitUntilAllOperationsAreFinished];
}
}  // namespace pag

static const float DEFAULT_MAX_FRAMERATE = 30.0;

@interface PAGImageView ()
@property(atomic, assign) BOOL isInBackground;
@property(atomic, assign) BOOL isVisible;
@property(nonatomic, assign) BOOL isPlaying;
@property(nonatomic, strong) NSLock* listenersLock;
@property(nonatomic, strong) PAGDecoder* pagDecoder;
@property(atomic, assign) float scaleFactor;
@property(atomic, assign) NSUInteger currentFrameIndex;
@property(nonatomic, strong) UIImage* currentImage;
@property(nonatomic, assign) NSInteger pagContentVersion;
@property(atomic, assign) float renderScale;
@property(atomic, assign) NSInteger currentFrameExplicitlySet;
@property(nonatomic, assign) float frameRate;
@property(nonatomic, assign) BOOL memoryCacheEnabled;

@end

@implementation PAGImageView {
  NSString* filePath;
  PAGValueAnimator* valueAnimator;
  NSHashTable* listeners;
  PAGComposition* pagComposition;
  NSUInteger numFrames;
  NSString* cacheKey;

  NSInteger cacheWidth;
  NSInteger cacheHeight;

  PAGDiskCache* imageViewCache;
  NSMutableDictionary<NSNumber*, UIImage*>* imagesMap;
  NSInteger _repeatCount;
  dispatch_queue_t flushQueue;
  CVPixelBufferRef pixelBuffer;
}

@synthesize isPlaying = _isPlaying;
@synthesize renderScale = _renderScale;
@synthesize currentImage = _currentImage;
@synthesize memoryCacheEnabled = _memoryCacheEnabled;

- (instancetype)initWithFrame:(CGRect)frame {
  if (self = [super initWithFrame:frame]) {
    [self initPAG];
  }
  return self;
}

- (instancetype)init {
  if (self = [super init]) {
    [self initPAG];
  }
  return self;
}

- (void)initPAG {
  _pagDecoder = nil;
  self.currentFrameIndex = -1;
  _renderScale = 1.0;
  self.memoryCacheEnabled = NO;
  listeners = [[NSHashTable weakObjectsHashTable] retain];
  self.isPlaying = FALSE;
  self.isVisible = FALSE;
  self.isInBackground =
      [UIApplication sharedApplication].applicationState == UIApplicationStateBackground;
  filePath = nil;
  _listenersLock = [[NSLock alloc] init];
  self.contentScaleFactor = [UIScreen mainScreen].scale;
  self.backgroundColor = [UIColor clearColor];
  valueAnimator = [[PAGValueAnimator alloc] init];
  [valueAnimator setRepeatCount:-1];
  [valueAnimator setListener:self];
  [self updateSize];

  flushQueue = dispatch_queue_create("pag.art.PAGImageView", DISPATCH_QUEUE_SERIAL);
  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(applicationDidBecomeActive:)
                                               name:UIApplicationDidBecomeActiveNotification
                                             object:nil];
  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(applicationWillResignActive:)
                                               name:UIApplicationWillResignActiveNotification
                                             object:nil];
}

- (void)dealloc {
  [listeners release];
  [_listenersLock release];
  [valueAnimator stop:false];
  [valueAnimator release];
  [self unloadAllFrames];
  if (_pagDecoder != nil) {
    [_pagDecoder release];
  }
  if (pixelBuffer) {
    CVPixelBufferRelease(pixelBuffer);
  }
  if (_currentImage) {
    [_currentImage release];
  }
  if (pagComposition) {
    [pagComposition release];
  }
  if (filePath != nil) {
    [filePath release];
  }
  if (imagesMap) {
    [imagesMap removeAllObjects];
    [imagesMap release];
    imagesMap = nil;
  }
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

#pragma mark - private
+ (NSOperationQueue*)ImageViewFlushQueue {
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    pag::imageViewFlushQueue = [[[NSOperationQueue alloc] init] retain];
    pag::imageViewFlushQueue.maxConcurrentOperationCount = 1;
    pag::imageViewFlushQueue.name = @"pag.art.PAGImageView";
  });
  return pag::imageViewFlushQueue;
}

/// 函数用于在执行 exit() 函数时把渲染任务全部完成，防止 PAG 的全局函数被析构，导致 PAG 野指针
/// crash。 注意这里注册需要等待 PAG 执行一次后再进行注册。因此需要等到 bufferPerpared 并再执行一次
/// flush 后, 否则 PAG 的 static 对象仍然会先析构
+ (void)RegisterFlushQueueDestoryMethod {
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    atexit(pag::DestoryImageViewFlushQueue);
  });
}

static NSString* NSStringMD5(NSString* string) {
  if (!string) {
    return nil;
  }
  NSData* data = [string dataUsingEncoding:NSUTF8StringEncoding];
  unsigned char result[CC_MD5_DIGEST_LENGTH];
  CC_MD5(data.bytes, (CC_LONG)data.length, result);
  return [NSString
      stringWithFormat:@"%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                       result[0], result[1], result[2], result[3], result[4], result[5], result[6],
                       result[7], result[8], result[9], result[10], result[11], result[12],
                       result[13], result[14], result[15]];
}

static NSString* RemovePathVariableComponent(NSString* original) {
  if (original.length == 0) {
    return original;
  }
  NSMutableArray* mutableArray = [[original pathComponents] mutableCopy];
  BOOL needRemove = NO;
  for (id item in mutableArray) {
    if (needRemove) {
      [mutableArray removeObject:item];
      break;
    }
    if ([item isEqualToString:@"Application"]) {
      needRemove = YES;
    }
  }
  NSString* path = [NSString pathWithComponents:mutableArray];
  [mutableArray release];
  return path;
}

- (CVPixelBufferRef)getCVPixelBuffer {
  if (self.memoryCacheEnabled) {
    return pag::PixelBufferUtils::Make((int)cacheWidth, (int)cacheHeight);
  }
  if (pixelBuffer == nil) {
    pixelBuffer = pag::PixelBufferUtils::Make((int)cacheWidth, (int)cacheHeight);
  }
  return pixelBuffer;
}

- (NSString*)generateCacheKey {
  NSString* cacheKey = nil;
  if ([pagComposition isMemberOfClass:[PAGFile class]] && [pagComposition getContentVersion] == 0) {
    filePath = [[(PAGFile*)pagComposition path] retain];
    cacheKey = RemovePathVariableComponent(filePath);
  } else {
    cacheKey = [NSString stringWithFormat:@"%@", pagComposition];
  }
  cacheKey = [cacheKey
      stringByAppendingFormat:@"_%u_%f", [pagComposition getContentVersion], self.scaleFactor];
  return NSStringMD5(cacheKey);
}

- (PAGDecoder*)pagDecoder {
  if (_pagDecoder == nullptr) {
    _pagDecoder = [PAGDecoder Make:pagComposition frameRate:self.frameRate scale:self.scaleFactor];
    [_pagDecoder retain];
  }
  return _pagDecoder;
}

- (NSString*)removePathVariableItem:(NSString*)original {
  if (original.length == 0) {
    return original;
  }
  NSMutableArray* mutableArray = [[original pathComponents] mutableCopy];
  BOOL needRemove = NO;
  for (NSString* item : mutableArray) {
    if (needRemove) {
      [mutableArray removeObject:item];
      break;
    }
    if ([item isEqualToString:@"Application"]) {
      needRemove = YES;
    }
  }
  NSString* path = [NSString pathWithComponents:mutableArray];
  [mutableArray release];
  return path;
}

- (void)onAnimationUpdate {
  [self updateView];
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

- (void)onAnimationStart {
  [_listenersLock lock];
  NSHashTable* copiedListeners = listeners.copy;
  [_listenersLock unlock];
  for (id item in copiedListeners) {
    id<PAGImageViewListener> listener = (id<PAGImageViewListener>)item;
    if ([listener respondsToSelector:@selector(onAnimationStart:)]) {
      [listener onAnimationStart:self];
    }
  }
  [copiedListeners release];
}

- (void)onAnimationEnd {
  self.isPlaying = FALSE;
  [_listenersLock lock];
  NSHashTable* copiedListeners = listeners.copy;
  [_listenersLock unlock];
  for (id item in copiedListeners) {
    id<PAGImageViewListener> listener = (id<PAGImageViewListener>)item;
    if ([listener respondsToSelector:@selector(onAnimationEnd:)]) {
      [listener onAnimationEnd:self];
    }
  }
  [copiedListeners release];
}

- (void)onAnimationCancel {
  [_listenersLock lock];
  NSHashTable* copiedListeners = listeners.copy;
  [_listenersLock unlock];
  for (id item in copiedListeners) {
    id<PAGImageViewListener> listener = (id<PAGImageViewListener>)item;
    if ([listener respondsToSelector:@selector(onAnimationCancel:)]) {
      [listener onAnimationCancel:self];
    }
  }
  [copiedListeners release];
}

- (void)onAnimationRepeat {
  [_listenersLock lock];
  NSHashTable* copiedListeners = listeners.copy;
  [_listenersLock unlock];
  for (id item in copiedListeners) {
    id<PAGImageViewListener> listener = (id<PAGImageViewListener>)item;
    if ([listener respondsToSelector:@selector(onAnimationRepeat:)]) {
      [listener onAnimationRepeat:self];
    }
  }
  [copiedListeners release];
}

#pragma clang diagnostic pop

- (void)didMoveToWindow {
  [super didMoveToWindow];
  [self checkVisible];
}

- (void)checkVisible {
  BOOL visible = self.window && !self.isHidden && self.alpha > 0.0;
  if (self.isVisible == visible) {
    return;
  }
  self.isVisible = visible;
  if (self.isVisible) {
    if (self.isPlaying) {
      [self play];
    }
  } else {
    if (self.isPlaying) {
      [valueAnimator stop:false];
    }
  }
}

- (BOOL)updateImageViewAtIndex:(NSInteger)frameIndex from:(CVPixelBufferRef)pixelBuffer {
  BOOL status = TRUE;
  if (self.pagContentVersion != [self->pagComposition getContentVersion]) {
    self.pagContentVersion = [self->pagComposition getContentVersion];
    [self unloadAllFrames];
  }
  if (self->imageViewCache && [self->imageViewCache containsObjectForKey:frameIndex]) {
    if ([[self->imagesMap allKeys] containsObject:[NSNumber numberWithInteger:frameIndex]]) {
      self.currentImage = [self->imagesMap objectForKey:[NSNumber numberWithInteger:frameIndex]];
      [self submitToImageView];
    } else {
      status = [self->imageViewCache objectForKey:frameIndex pixelBuffer:pixelBuffer];
      if (!status) {
        return NO;
      }
      UIImage* image = [self imageFromCVPixeBuffer:pixelBuffer];
      self.currentImage = image;
      if (self.memoryCacheEnabled) {
        [self->imagesMap setObject:image forKey:[NSNumber numberWithInteger:frameIndex]];
      }
      [self submitToImageView];
    }
  } else {
      [self.pagDecoder copyFrameAt:frameIndex To:pixelBuffer];
      UIImage* image = [self imageFromCVPixeBuffer:pixelBuffer];
      self.currentImage = image;
      if (self.memoryCacheEnabled) {
        [self->imagesMap setObject:image forKey:[NSNumber numberWithInteger:frameIndex]];
      }
      [self submitToImageView];
      [self->imageViewCache setObject:pixelBuffer
                               forKey:frameIndex
                            withBlock:^{
                            }];
  }
  return status;
}

- (void)updateView {
  NSUInteger frameIndex = (NSUInteger)(floor([valueAnimator getAnimatedFraction] * numFrames));
  if (frameIndex >= numFrames) {
    frameIndex = numFrames - 1;
  }
  if (self.currentFrameIndex != frameIndex) {
    if (_pagDecoder && [imageViewCache count] == numFrames) {
      [_pagDecoder release];
      _pagDecoder = nil;
      if (filePath && [pagComposition getContentVersion] == 0) {
        [pagComposition release];
        pagComposition = nil;
      }
    }
    self.currentFrameIndex = frameIndex;
    __block __typeof(self) weakSelf = self;
    NSOperationQueue* flushQueue = [PAGImageView ImageViewFlushQueue];
    [self retain];
    NSBlockOperation* operation = [NSBlockOperation blockOperationWithBlock:^{
      NSInteger frameIndex = weakSelf.currentFrameIndex;
      CVPixelBufferRef pixelBuffer = [weakSelf getCVPixelBuffer];
      if (weakSelf.currentFrameExplicitlySet >= 0) {
        frameIndex = weakSelf.currentFrameExplicitlySet;
        [weakSelf updateImageViewAtIndex:frameIndex from:pixelBuffer];
        [weakSelf->valueAnimator
            setCurrentPlayTime:((frameIndex + 0.1) * 1.0 / weakSelf.frameRate * 1000000)];
        weakSelf.currentFrameExplicitlySet = -1;
      } else {
        [weakSelf updateImageViewAtIndex:frameIndex from:pixelBuffer];
      }
    }];
    [flushQueue addOperation:operation];
  }
  [_listenersLock lock];
  NSHashTable* copiedListeners = listeners.copy;
  [_listenersLock unlock];
  for (id item in copiedListeners) {
    id<PAGImageViewListener> listener = (id<PAGImageViewListener>)item;
    if ([listener respondsToSelector:@selector(onAnimationUpdate:)]) {
      [listener onAnimationUpdate:self];
    }
  }
  [copiedListeners release];
}

- (void)submitToImageView {
  dispatch_async(dispatch_get_main_queue(), ^{
    [self setImage:self.currentImage];
    [self.layer setNeedsDisplay];
    [self release];
  });
}

- (void)applicationDidBecomeActive:(NSNotification*)notification {
  self.isInBackground = FALSE;
  if (self.isVisible) {
    [PAGImageView RegisterFlushQueueDestoryMethod];
    [self updateView];
  }
}

- (void)applicationWillResignActive:(NSNotification*)notification {
  self.isInBackground = TRUE;
}

- (void)updateSize {
  if (pagComposition) {
    cacheWidth = round(self.scaleFactor * [pagComposition width] * self.renderScale);
    cacheHeight = round(self.scaleFactor * [pagComposition height] * self.renderScale);
  }
}

- (UIImage*)imageFromCVPixeBuffer:(CVPixelBufferRef)pixeBuffer {
  CGImageRef imageRef = nil;
  VTCreateCGImageFromCVPixelBuffer(pixeBuffer, nil, &imageRef);
  UIImage* uiImage = [UIImage imageWithCGImage:imageRef];
  CGImageRelease(imageRef);
  return uiImage;
}

#pragma mark - pubic
+ (NSUInteger)MaxDiskSize {
  return [[PAGCacheManager shareInstance] MaxDiskSize];
}

+ (void)SetMaxDiskSize:(NSUInteger)size {
  [[PAGCacheManager shareInstance] SetMaxDiskSize:size];
}

- (void)setBounds:(CGRect)bounds {
  CGRect oldBounds = self.bounds;
  [super setBounds:bounds];
  if (oldBounds.size.width != bounds.size.width || oldBounds.size.height != bounds.size.height) {
    self.scaleFactor =
        std::max(bounds.size.width * self.contentScaleFactor / [pagComposition width],
                 bounds.size.height * self.contentScaleFactor / [pagComposition height]);
    [self updateSize];
    [self unloadAllFrames];
    [self updateView];
  }
}

- (void)setFrame:(CGRect)frame {
  CGRect oldRect = self.frame;
  [super setFrame:frame];
  if (oldRect.size.width != frame.size.width || oldRect.size.height != frame.size.height) {
    self.scaleFactor =
        std::max(frame.size.width * self.contentScaleFactor / [pagComposition width],
                 frame.size.height * self.contentScaleFactor / [pagComposition height]);
    [self updateSize];
    [self unloadAllFrames];
    [self updateView];
  }
}

- (void)setMatrix:(CGAffineTransform)value {
  if (pagComposition) {
    [pagComposition setMatrix:value];
    [self updateSize];
    [self unloadAllFrames];
    [self updateView];
  }
}

- (CGAffineTransform)matrix {
  if (pagComposition) {
    return [pagComposition matrix];
  }
  return CGAffineTransformIdentity;
}

- (void)setRenderScale:(float)scale {
  _renderScale = scale;
  if (pagComposition) {
    [self updateSize];
    [self unloadAllFrames];
    [self updateView];
  }
}

- (float)renderScale {
  return _renderScale;
}

- (void)setContentScaleFactor:(CGFloat)scaleFactor {
  CGFloat oldScaleFactor = self.contentScaleFactor;
  [super setContentScaleFactor:scaleFactor];
  if (oldScaleFactor != scaleFactor) {
    if (pagComposition) {
      [self updateSize];
      [self unloadAllFrames];
      [self updateView];
    }
  }
}

- (BOOL)cacheAllFramesInMemory {
  return _memoryCacheEnabled;
}

- (void)setCacheAllFramesInMemory:(BOOL)enable {
  _memoryCacheEnabled = enable;
  if (_memoryCacheEnabled) {
    if (imagesMap == nil) {
      imagesMap = [NSMutableDictionary new];
    }
  } else {
    if (imagesMap) {
      [imagesMap removeAllObjects];
      [imagesMap release];
      imagesMap = nil;
    }
  }
}

- (void)setAlpha:(CGFloat)alpha {
  [super setAlpha:alpha];
  [self checkVisible];
}

- (void)setHidden:(BOOL)hidden {
  [super setHidden:hidden];
  [self checkVisible];
}

- (void)play {
  _isPlaying = true;
  if (!self.isVisible) {
    return;
  }
  int64_t playTime = (int64_t)([valueAnimator getAnimatedFraction] * [valueAnimator duration]);
  [valueAnimator setCurrentPlayTime:playTime];
  [valueAnimator start];
}

- (void)pause {
  _isPlaying = false;
  [valueAnimator stop];
}

- (UIImage*)currentImage {
  return [[_currentImage retain] autorelease];
}

- (BOOL)isPlaying {
  return _isPlaying;
}

- (void)addListener:(id<PAGImageViewListener>)listener {
  [_listenersLock lock];
  [listeners addObject:listener];
  [_listenersLock unlock];
}

- (void)removeListener:(id<PAGImageViewListener>)listener {
  [_listenersLock lock];
  [listeners removeObject:listener];
  [_listenersLock unlock];
}

- (void)setRepeatCount:(int)repeatCount {
  if (repeatCount < 0) {
    repeatCount = 0;
  }
  _repeatCount = repeatCount;
  [valueAnimator setRepeatCount:(repeatCount - 1)];
}

- (NSString*)getPath {
  return filePath == nil ? nil : [[filePath retain] autorelease];
}

- (BOOL)setPath:(NSString*)filePath {
  return [self setPath:filePath maxFrameRate:DEFAULT_MAX_FRAMERATE];
}

- (BOOL)setPath:(NSString*)path maxFrameRate:(float)maxFrameRate {
  if (filePath != nil) {
    [filePath release];
  }
  PAGFile* file = [PAGFile Load:path];
  [self setComposition:file maxFrameRate:maxFrameRate];
  filePath = [path retain];
  return file != nil;
}

- (PAGComposition*)getComposition {
  if (pagComposition) {
    return [[pagComposition retain] autorelease];
  }
  return nil;
}

- (void)setComposition:(PAGComposition*)newComposition {
  [self setComposition:newComposition maxFrameRate:DEFAULT_MAX_FRAMERATE];
}

- (void)setComposition:(PAGComposition*)newComposition maxFrameRate:(float)maxFrameRate {
  if (pagComposition == newComposition) {
    return;
  }
  if (pagComposition) {
    [pagComposition release];
  }
  pagComposition = [newComposition retain];
  self.pagContentVersion = [pagComposition getContentVersion];
  self.currentFrameExplicitlySet = 0;
  self.scaleFactor =
      std::max(self.frame.size.width * self.contentScaleFactor / [pagComposition width],
               self.frame.size.height * self.contentScaleFactor / [pagComposition height]);
  cacheKey = [self generateCacheKey];
  self.frameRate = MIN(maxFrameRate, [pagComposition frameRate]);
  numFrames = [pagComposition duration] * self.frameRate / 1000000;
  if (imageViewCache) {
    [imageViewCache release];
    imageViewCache = nil;
    imageViewCache = [PAGDiskCache MakeWithName:cacheKey frameCount:numFrames];
  }
  imageViewCache = [PAGDiskCache MakeWithName:cacheKey frameCount:numFrames];
  [imageViewCache retain];
  [self updateSize];
  [valueAnimator setDuration:[pagComposition duration]];
}

- (void)setCurrentFrame:(NSUInteger)currentFrame {
  self.currentFrameExplicitlySet = currentFrame;
}

- (NSUInteger)currentFrame {
  if (self.currentFrameExplicitlySet >= 0) {
    return self.currentFrameExplicitlySet;
  }
  NSUInteger frame = (NSUInteger)(floor([valueAnimator getAnimatedFraction] * numFrames));
  return MIN(frame, numFrames - 1);
}

- (BOOL)flush {
  return [self updateImageViewAtIndex:[self currentFrame] from:[self getCVPixelBuffer]];
}

- (void)unloadAllFrames {
  [imagesMap removeAllObjects];
  if (filePath == nil || [pagComposition getContentVersion] > 0) {
    [imageViewCache removeCaches];
  }
}

@end
