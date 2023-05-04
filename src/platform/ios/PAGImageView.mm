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
#include <mutex>
#import "PAGDecoder.h"
#import "PAGFile.h"
#import "platform/cocoa/PAGDiskCache.h"
#import "platform/ios/private/PAGContentVersion.h"
#import "private/PAGValueAnimator.h"

namespace pag {
static NSOperationQueue* imageViewFlushQueue;
void DestoryImageViewFlushQueue() {
  NSOperationQueue* queue = imageViewFlushQueue;
  [queue cancelAllOperations];
  [queue waitUntilAllOperationsAreFinished];
  [queue release];
  queue = nil;
}
}  // namespace pag

static const float DEFAULT_MAX_FRAMERATE = 30.0;

#ifndef dispatch_main_async_safe
#define dispatch_main_async_safe(block)                         \
  if (dispatch_queue_get_label(DISPATCH_CURRENT_QUEUE_LABEL) == \
      dispatch_queue_get_label(dispatch_get_main_queue())) {    \
    block();                                                    \
  } else {                                                      \
    dispatch_async(dispatch_get_main_queue(), block);           \
  }
#endif

@interface PAGImageView ()
@property(atomic, assign) BOOL isInBackground;
@property(atomic, assign) BOOL isVisible;
@property(nonatomic, assign) BOOL isPlaying;
@property(atomic, assign) NSInteger currentFrameIndex;
@property(atomic, retain) UIImage* currentUIImage;
@property(atomic, assign) NSInteger pagContentVersion;
@property(atomic, assign) NSInteger currentFrameExplicitlySet;
@property(nonatomic, assign) BOOL memoryCacheEnabled;
@property(nonatomic, assign) BOOL memeoryCacheFinished;
@property(nonatomic, assign) NSInteger fileWidth;
@property(nonatomic, assign) NSInteger fileHeight;
@property(nonatomic, assign) float maxFrameRate;
@property(nonatomic, assign) CGSize viewSize;

@end

@implementation PAGImageView {
  NSString* filePath;
  PAGValueAnimator* valueAnimator;
  NSHashTable* listeners;
  PAGComposition* pagComposition;
  PAGDecoder* pagDecoder;
  NSLock* listenersLock;
  NSInteger totalFrames;
  float frameRate;
  float renderScaleFactor;

  NSMutableDictionary<NSNumber*, UIImage*>* imagesMap;

  CFMutableDataRef currentImageDataRef;
  NSInteger cacheImageSize;
  NSInteger cacheImageRowBytes;
  std::mutex imageViewLock;
}

@synthesize isPlaying = _isPlaying;
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
  pagDecoder = nil;
  self.currentFrameIndex = -1;
  renderScaleFactor = 1.0;
  totalFrames = 0;
  frameRate = 0;
  cacheImageSize = 0;
  cacheImageRowBytes = 0;
  self.memoryCacheEnabled = NO;
  self.memeoryCacheFinished = NO;
  listeners = [[NSHashTable weakObjectsHashTable] retain];
  self.isPlaying = NO;
  self.isVisible = NO;
  self.isInBackground =
      [UIApplication sharedApplication].applicationState == UIApplicationStateBackground;
  filePath = nil;
  listenersLock = [[NSLock alloc] init];
  self.backgroundColor = [UIColor clearColor];
  valueAnimator = [[PAGValueAnimator alloc] init];
  [valueAnimator setListener:self];

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
  [listenersLock release];
  [valueAnimator stop:false];
  [valueAnimator release];
  [self reset];
  if (pagComposition) {
    [pagComposition release];
  }
  if (_currentUIImage) {
    [_currentUIImage release];
  }
  if (filePath != nil) {
    [filePath release];
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
    pag::imageViewFlushQueue.name = @"PAGImageView.art.pag";
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

- (void)setCompositionInternal:(PAGComposition*)newComposition maxFrameRate:(float)maxFrameRate {
  if (pagComposition == newComposition) {
    return;
  }
  if (pagComposition) {
    [pagComposition release];
    pagComposition = nil;
  }
  if (!filePath) {
    pagComposition = [newComposition retain];
  }
  self.fileWidth = [newComposition width];
  self.fileHeight = [newComposition height];
  self.pagContentVersion = [PAGContentVersion Get:newComposition];
  self.maxFrameRate = maxFrameRate;
  self.currentFrameExplicitlySet = 0;
  [valueAnimator setCurrentPlayTime:0];
  [valueAnimator setDuration:[newComposition duration]];
  [self reset];
}

- (CFMutableDataRef)getDiskCacheData {
  if (currentImageDataRef == nil && [[self getPAGDecoder] width] > 0 &&
      [[self getPAGDecoder] height] > 0) {
    currentImageDataRef = CFDataCreateMutable(kCFAllocatorDefault, cacheImageSize);
    CFDataSetLength(currentImageDataRef, cacheImageSize);
  }
  return currentImageDataRef;
}

- (CFMutableDataRef)getMemoryCacheData {
  if (cacheImageSize == 0) {
    [self getPAGDecoder];
  }
  if (cacheImageSize > 0) {
    CFMutableDataRef dataRef = CFDataCreateMutable(kCFAllocatorDefault, cacheImageSize);
    CFDataSetLength(dataRef, cacheImageSize);
    CFAutorelease(dataRef);
    return dataRef;
  }
  return nil;
}

- (PAGDecoder*)getPAGDecoder {
  if (pagDecoder == nil) {
    float scaleFactor = static_cast<float>(
        renderScaleFactor *
        fmax(self.viewSize.width * [UIScreen mainScreen].scale / self.fileWidth,
             self.viewSize.height * [UIScreen mainScreen].scale / self.fileHeight));
    if (pagComposition) {
      pagDecoder = [PAGDecoder Make:pagComposition
                       maxFrameRate:self.maxFrameRate
                              scale:scaleFactor];
    } else if (filePath) {
      pagDecoder = [PAGDecoder Make:[PAGFile Load:filePath]
                       maxFrameRate:self.maxFrameRate
                              scale:scaleFactor];
    }
    if (pagDecoder) {
      [pagDecoder retain];
      totalFrames = [pagDecoder numFrames];
      frameRate = [pagDecoder frameRate];
      cacheImageSize = [pagDecoder width] * [pagDecoder height] * 4;
      cacheImageRowBytes = [pagDecoder width] * 4;
    }
  }
  return pagDecoder;
}

- (void)onAnimationUpdate {
  [self updateView];
}

- (void)onAnimationStart {
  [listenersLock lock];
  NSHashTable* copiedListeners = listeners.copy;
  [listenersLock unlock];
  for (id item in copiedListeners) {
    auto listener = (id<PAGImageViewListener>)item;
    if ([listener respondsToSelector:@selector(onAnimationStart:)]) {
      [listener onAnimationStart:self];
    }
  }
  [copiedListeners release];
}

- (void)onAnimationEnd {
  self.isPlaying = FALSE;
  [listenersLock lock];
  NSHashTable* copiedListeners = listeners.copy;
  [listenersLock unlock];
  for (id item in copiedListeners) {
    auto listener = (id<PAGImageViewListener>)item;
    if ([listener respondsToSelector:@selector(onAnimationEnd:)]) {
      [listener onAnimationEnd:self];
    }
  }
  [copiedListeners release];
}

- (void)onAnimationCancel {
  [listenersLock lock];
  NSHashTable* copiedListeners = listeners.copy;
  [listenersLock unlock];
  for (id item in copiedListeners) {
    auto listener = (id<PAGImageViewListener>)item;
    if ([listener respondsToSelector:@selector(onAnimationCancel:)]) {
      [listener onAnimationCancel:self];
    }
  }
  [copiedListeners release];
}

- (void)onAnimationRepeat {
  [listenersLock lock];
  NSHashTable* copiedListeners = listeners.copy;
  [listenersLock unlock];
  for (id item in copiedListeners) {
    auto listener = (id<PAGImageViewListener>)item;
    if ([listener respondsToSelector:@selector(onAnimationRepeat:)]) {
      [listener onAnimationRepeat:self];
    }
  }
  [copiedListeners release];
}

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

- (BOOL)updateImageViewFrom:(CFMutableDataRef)dataRef atIndex:(NSInteger)frameIndex {
  [self freeCache];
  if ([[imagesMap allKeys] containsObject:@(frameIndex)]) {
    UIImage* image = imagesMap[@(frameIndex)];
    self.currentUIImage = image;
    [self submitToImageView];
    if ([imagesMap count] == (NSUInteger)[[self getPAGDecoder] numFrames]) {
      self.memeoryCacheFinished = YES;
    }
    return YES;
  }
  if ([PAGContentVersion CheckFrameChanged:[self getPAGDecoder] index:frameIndex]) {
    uint8_t* rgbaData = CFDataGetMutableBytePtr(dataRef);
    BOOL status = [[self getPAGDecoder] copyFrameTo:rgbaData
                                           rowBytes:static_cast<size_t>(cacheImageRowBytes)
                                                 at:frameIndex];
    if (!status) {
      return status;
    }
    UIImage* image = [self imageForCFMutableData:dataRef];
    if (image) {
      self.currentUIImage = image;
      [self submitToImageView];
    }
  }
  if (self.memoryCacheEnabled && self.currentUIImage) {
    self->imagesMap[@(frameIndex)] = self.currentUIImage;
  }

  return YES;
}

- (BOOL)checkPAGCompositionChanged {
  if ([PAGContentVersion Get:pagComposition] != self.pagContentVersion) {
    self.pagContentVersion = [PAGContentVersion Get:pagComposition];
    [self reset];
    return YES;
  }
  return NO;
}

- (void)updateView {
  if (self.memeoryCacheFinished) {
    if ([self checkPAGCompositionChanged] == NO) {
      NSInteger frame = [self nextFrame];
      if (self.currentFrameIndex != frame) {
        UIImage* image = imagesMap[@(frame)];
        if (image) {
          self.currentFrameIndex = frame;
          self.currentUIImage = image;
          [self submitToImageView];
          if (self.currentFrameExplicitlySet >= 0) {
            self.currentFrameExplicitlySet = -1;
            [valueAnimator setCurrentPlayTime:[self FrameToTime:frame]];
          }
          return;
        }
      }
    }
  }

  NSOperationQueue* flushQueue = [PAGImageView ImageViewFlushQueue];
  [self retain];
  NSBlockOperation* operation = [NSBlockOperation blockOperationWithBlock:^{
    [self flush];
    [self releaseSelf];
  }];
  [flushQueue addOperation:operation];
  [listenersLock lock];
  NSHashTable* copiedListeners = listeners.copy;
  [listenersLock unlock];
  for (id item in copiedListeners) {
    auto listener = (id<PAGImageViewListener>)item;
    if ([listener respondsToSelector:@selector(onAnimationUpdate:)]) {
      [listener onAnimationUpdate:self];
    }
  }
  [copiedListeners release];
}

- (void)submitToImageView {
  dispatch_main_async_safe(^{
    [self setImage:self.currentUIImage];
    [self setNeedsDisplay];
  });
}

- (void)releaseSelf {
  dispatch_main_async_safe(^{
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

- (void)freeCache {
  if (self.memoryCacheEnabled && self->pagDecoder &&
      [self->imagesMap count] == (NSUInteger)[self->pagDecoder numFrames]) {
    [self->pagDecoder release];
    self->pagDecoder = nil;
  }
}

- (UIImage*)imageForCFMutableData:(CFMutableDataRef)dataRef {
  CGBitmapInfo bitmapInfo = kCGBitmapByteOrder32Little | kCGImageAlphaPremultipliedFirst;
  size_t componentsPerPixel = 4;
  size_t bitsPerPixel = 8;
  CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
  CGDataProviderRef provider = CGDataProviderCreateWithCFData(dataRef);
  CGImageRef cgImage =
      CGImageCreate(static_cast<size_t>([[self getPAGDecoder] width]),
                    static_cast<size_t>([[self getPAGDecoder] height]), bitsPerPixel,
                    bitsPerPixel * componentsPerPixel, static_cast<size_t>(cacheImageRowBytes),
                    colorSpace, bitmapInfo, provider, NULL, NO, kCGRenderingIntentDefault);
  UIImage* image = [UIImage imageWithCGImage:cgImage];
  CGColorSpaceRelease(colorSpace);
  CGImageRelease(cgImage);
  CGDataProviderRelease(provider);

  return image;
}

- (int64_t)FrameToTime:(NSInteger)frame {
  if (frameRate <= 0) {
    frameRate = [[self getPAGDecoder] frameRate];
    if (frameRate <= 0) {
      return 0;
    }
  }
  return static_cast<int64_t>((frame + 0.1) * 1.0 / frameRate * 1000000);
}

- (NSInteger)nextFrame {
  if (self.currentFrameExplicitlySet >= 0) {
    return self.currentFrameExplicitlySet;
  }
  if (totalFrames == 0) {
    totalFrames = [[self getPAGDecoder] numFrames];
  }
  NSInteger frame =
      static_cast<NSInteger>(floor([valueAnimator getAnimatedFraction] * totalFrames));
  return MAX(MIN(frame, totalFrames - 1), 0);
}

- (void)reset {
  if (imagesMap) {
    [imagesMap removeAllObjects];
    imagesMap = nil;
    self.memeoryCacheFinished = NO;
  }
  if (currentImageDataRef) {
    CFRelease(currentImageDataRef);
    currentImageDataRef = nil;
  }
  if (pagDecoder) {
    [pagDecoder release];
    pagDecoder = nil;
    totalFrames = 0;
    frameRate = 0;
    cacheImageSize = 0;
    cacheImageRowBytes = 0;
  }
}

#pragma mark - pubic

+ (NSUInteger)MaxDiskSize {
  return [PAGDiskCache MaxDiskSize];
}

+ (void)SetMaxDiskSize:(NSUInteger)size {
  [PAGDiskCache SetMaxDiskSize:size];
}

- (void)setBounds:(CGRect)bounds {
  CGRect oldBounds = self.bounds;
  [super setBounds:bounds];
  self.viewSize = CGSizeMake(bounds.size.width, bounds.size.height);
  if (oldBounds.size.width != bounds.size.width || oldBounds.size.height != bounds.size.height) {
    std::lock_guard<std::mutex> autoLock(imageViewLock);
    if (pagComposition || filePath) {
      [self reset];
    }
  }
}

- (void)setFrame:(CGRect)frame {
  CGRect oldRect = self.frame;
  [super setFrame:frame];
  self.viewSize = CGSizeMake(frame.size.width, frame.size.height);
  if (oldRect.size.width != frame.size.width || oldRect.size.height != frame.size.height) {
    std::lock_guard<std::mutex> autoLock(imageViewLock);
    if (pagComposition || filePath) {
      [self reset];
    }
  }
}

- (void)setRenderScale:(float)scale {
  std::lock_guard<std::mutex> autoLock(imageViewLock);
  if (renderScaleFactor == scale) {
    return;
  }
  renderScaleFactor = scale;
  if (pagComposition || filePath) {
    [self reset];
  }
}

- (float)renderScale {
  std::lock_guard<std::mutex> autoLock(imageViewLock);
  return renderScaleFactor;
}

- (void)setContentScaleFactor:(CGFloat)scaleFactor {
  CGFloat oldScaleFactor = self.contentScaleFactor;
  [super setContentScaleFactor:scaleFactor];
  if (oldScaleFactor != scaleFactor) {
    if (pagComposition || filePath) {
      std::lock_guard<std::mutex> autoLock(imageViewLock);
      [self reset];
      self.viewSize = CGSizeMake(self.frame.size.width, self.frame.size.height);
    }
  }
}

- (BOOL)cacheAllFramesInMemory {
  std::lock_guard<std::mutex> autoLock(imageViewLock);
  return _memoryCacheEnabled;
}

- (void)setCacheAllFramesInMemory:(BOOL)enable {
  std::lock_guard<std::mutex> autoLock(imageViewLock);
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
      self.memoryCacheEnabled = NO;
    }
  }
}

- (void)setAlpha:(CGFloat)alpha {
  [super setAlpha:alpha];
  std::lock_guard<std::mutex> autoLock(imageViewLock);
  [self checkVisible];
}

- (void)setHidden:(BOOL)hidden {
  [super setHidden:hidden];
  std::lock_guard<std::mutex> autoLock(imageViewLock);
  [self checkVisible];
}

- (void)play {
  _isPlaying = true;
  if (!self.isVisible) {
    return;
  }
  [valueAnimator start];
}

- (void)pause {
  _isPlaying = false;
  [valueAnimator stop];
}

- (NSUInteger)numFrames {
  std::lock_guard<std::mutex> autoLock(imageViewLock);
  if (pagDecoder) {
    totalFrames = [pagDecoder numFrames];
  }
  if (totalFrames == 0) {
    totalFrames = [[self getPAGDecoder] numFrames];
  }
  return static_cast<NSUInteger>(totalFrames);
}

- (UIImage*)currentImage {
  std::lock_guard<std::mutex> autoLock(imageViewLock);
  return [[_currentUIImage retain] autorelease];
}

- (BOOL)isPlaying {
  return _isPlaying;
}

- (void)addListener:(id<PAGImageViewListener>)listener {
  [listenersLock lock];
  [listeners addObject:listener];
  [listenersLock unlock];
}

- (void)removeListener:(id<PAGImageViewListener>)listener {
  [listenersLock lock];
  [listeners removeObject:listener];
  [listenersLock unlock];
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

- (BOOL)setPath:(NSString*)newPath {
  return [self setPath:newPath maxFrameRate:DEFAULT_MAX_FRAMERATE];
}

- (BOOL)setPath:(NSString*)path maxFrameRate:(float)maxFrameRate {
  std::lock_guard<std::mutex> autoLock(imageViewLock);
  if ([filePath isEqualToString:path]) {
    return YES;
  }
  if (filePath) {
    [filePath release];
    filePath = nil;
  }
  filePath = [path retain];
  PAGFile* file = [PAGFile Load:path];
  [self setCompositionInternal:file maxFrameRate:maxFrameRate];
  return file != nil;
}

- (PAGComposition*)getComposition {
  if (filePath) {
    return nil;
  }
  return pagComposition ? [[pagComposition retain] autorelease] : nil;
}

- (void)setComposition:(PAGComposition*)newComposition {
  [self setComposition:newComposition maxFrameRate:DEFAULT_MAX_FRAMERATE];
}

- (void)setComposition:(PAGComposition*)newComposition maxFrameRate:(float)maxFrameRate {
  std::lock_guard<std::mutex> autoLock(imageViewLock);
  if (filePath) {
    [filePath release];
    filePath = nil;
  }
  [self setCompositionInternal:newComposition maxFrameRate:maxFrameRate];
}

- (void)setCurrentFrame:(NSUInteger)currentFrame {
  std::lock_guard<std::mutex> autoLock(imageViewLock);
  self.currentFrameExplicitlySet = currentFrame;
}

- (NSUInteger)currentFrame {
  if (self.currentFrameExplicitlySet >= 0) {
    return static_cast<NSUInteger>(self.currentFrameExplicitlySet);
  }
  return static_cast<NSUInteger>(MAX(self.currentFrameIndex, 0));
}

- (BOOL)flush {
  std::lock_guard<std::mutex> autoLock(imageViewLock);
  NSInteger frameIndex = [self nextFrame];
  if (self.currentFrameIndex == frameIndex) {
    return NO;
  }
  [self checkPAGCompositionChanged];
  CFMutableDataRef dataRef =
      self.memoryCacheEnabled ? [self getMemoryCacheData] : [self getDiskCacheData];
  if (dataRef == nil) {
    self.currentUIImage = nil;
    [self submitToImageView];
    return NO;
  }
  BOOL status;
  if (self.currentFrameExplicitlySet >= 0) {
    self.currentFrameExplicitlySet = -1;
    status = [self updateImageViewFrom:dataRef atIndex:frameIndex];
    [self->valueAnimator setCurrentPlayTime:[self FrameToTime:frameIndex]];
  } else {
    status = [self updateImageViewFrom:dataRef atIndex:frameIndex];
  }
  self.currentFrameIndex = frameIndex;
  return status;
}

@end
