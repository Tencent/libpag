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
#import "platform/cocoa/private/PAGContentVersion.h"
#import "platform/cocoa/private/PixelBufferUtils.h"
#import "private/PAGCacheManager.h"
#import "private/PAGDiskCache.h"
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
@property(atomic, assign) float scaleFactor;
@property(atomic, assign) NSInteger currentFrameIndex;
@property(nonatomic, retain) UIImage* currentUIImage;
@property(nonatomic, assign) NSInteger pagContentVersion;
@property(atomic, assign) float renderScale;
@property(atomic, assign) NSInteger currentFrameExplicitlySet;
@property(nonatomic, assign) float frameRate;
@property(nonatomic, assign) BOOL memoryCacheEnabled;
@property(nonatomic, assign) NSInteger fileWidth;
@property(nonatomic, assign) NSInteger fileHeight;

@end

@implementation PAGImageView {
  NSString* filePath;
  PAGValueAnimator* valueAnimator;
  NSHashTable* listeners;
  PAGComposition* pagComposition;
  PAGDecoder* pagDecoder;
  NSLock* listenersLock;
  NSLock* flushLock;

  NSInteger numFrames;
  NSInteger cacheWidth;
  NSInteger cacheHeight;

  PAGDiskCache* imageViewCache;
  NSMutableDictionary<NSNumber*, UIImage*>* imagesMap;

  CFMutableDataRef currentImageDataRef;
  NSInteger cacheImageSize;
  NSInteger cacheImageRowBytes;
}

@synthesize isPlaying = _isPlaying;
@synthesize renderScale = _renderScale;
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
  _renderScale = 1.0;
  self.memoryCacheEnabled = NO;
  listeners = [[NSHashTable weakObjectsHashTable] retain];
  self.isPlaying = FALSE;
  self.isVisible = FALSE;
  self.isInBackground =
      [UIApplication sharedApplication].applicationState == UIApplicationStateBackground;
  filePath = nil;
  listenersLock = [[NSLock alloc] init];
  flushLock = [[NSLock alloc] init];
  self.contentScaleFactor = [UIScreen mainScreen].scale;
  self.backgroundColor = [UIColor clearColor];
  valueAnimator = [[PAGValueAnimator alloc] init];
  [valueAnimator setRepeatCount:-1];
  [valueAnimator setListener:self];
  [self updateSize];

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
  [flushLock release];
  [valueAnimator stop:false];
  [valueAnimator release];
  [self unloadAllFrames];
  if (pagDecoder != nil) {
    [pagDecoder release];
  }
  if (imageViewCache) {
    [imageViewCache release];
  }
  if (_currentUIImage) {
    [_currentUIImage release];
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
  if (currentImageDataRef) {
    CFRelease(currentImageDataRef);
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

- (CFMutableDataRef)getDiskCacheData {
  if (currentImageDataRef == nil && cacheWidth > 0 && cacheHeight > 0) {
    currentImageDataRef = CFDataCreateMutable(kCFAllocatorDefault, cacheImageSize);
    CFDataSetLength(currentImageDataRef, cacheImageSize);
  }
  return currentImageDataRef;
}

- (CFMutableDataRef)getMemoryCacheData {
  if (cacheWidth > 0 && cacheHeight > 0) {
    CFMutableDataRef dataRef = CFDataCreateMutable(kCFAllocatorDefault, cacheImageSize);
    CFDataSetLength(dataRef, cacheImageSize);
    CFAutorelease(dataRef);
    return dataRef;
  }
  return nil;
}

- (NSString*)generateCacheKey {
  NSString* cacheKey = nil;
  NSInteger contentVersion = 0;
  if (pagComposition && [PAGContentVersion Get:pagComposition] > 0) {
    cacheKey = [NSString stringWithFormat:@"%@", pagComposition];
    contentVersion = [PAGContentVersion Get:pagComposition];
  } else if (filePath) {
    cacheKey = RemovePathVariableComponent(filePath);
  }
  cacheKey = [cacheKey
      stringByAppendingFormat:@"_%ld_%f_%f", contentVersion, self.frameRate, self.scaleFactor];
  return NSStringMD5(cacheKey);
}

- (PAGDecoder*)getPAGDecoder {
  if (pagDecoder == nil) {
    if (filePath) {
      pagDecoder = [PAGDecoder Make:[PAGFile Load:filePath]
                       maxFrameRate:self.frameRate
                              scale:self.scaleFactor];
      [pagDecoder retain];
    } else if (pagComposition) {
      pagDecoder = [PAGDecoder Make:pagComposition
                       maxFrameRate:self.frameRate
                              scale:self.scaleFactor];
      [pagDecoder retain];
    }
  }
  return pagDecoder;
}

- (PAGDiskCache*)getImageViewCache {
  if (imageViewCache == nil) {
    imageViewCache = [PAGDiskCache MakeWithName:[self generateCacheKey] frameCount:numFrames];
    [imageViewCache retain];
  }
  return imageViewCache;
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

- (void)onAnimationStart {
  [listenersLock lock];
  NSHashTable* copiedListeners = listeners.copy;
  [listenersLock unlock];
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
  [listenersLock lock];
  NSHashTable* copiedListeners = listeners.copy;
  [listenersLock unlock];
  for (id item in copiedListeners) {
    id<PAGImageViewListener> listener = (id<PAGImageViewListener>)item;
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
    id<PAGImageViewListener> listener = (id<PAGImageViewListener>)item;
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
    id<PAGImageViewListener> listener = (id<PAGImageViewListener>)item;
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
  BOOL status = TRUE;
  [flushLock lock];
  [self freeCache];
  if (self->pagComposition &&
      (self.pagContentVersion != [PAGContentVersion Get:self->pagComposition])) {
    self.pagContentVersion = [PAGContentVersion Get:self->pagComposition];
    [self unloadAllFrames];
  }
  PAGDiskCache* diskCache = [self getImageViewCache];
  uint8_t* rgbaData = CFDataGetMutableBytePtr(dataRef);
  if ([diskCache containsObjectForKey:frameIndex]) {
    status = [diskCache objectForKey:frameIndex to:rgbaData length:cacheImageSize];
    if (!status) {
      [flushLock unlock];
      return status;
    }
    UIImage* image = [self imageForCFMutableData:dataRef];
    self.currentUIImage = image;
    if (self.memoryCacheEnabled) {
      [self->imagesMap setObject:image forKey:[NSNumber numberWithInteger:frameIndex]];
    }
    [self submitToImageView];
  } else {
    @autoreleasepool {
      status = [[self getPAGDecoder] copyFrameTo:rgbaData
                                        rowBytes:cacheImageRowBytes
                                              at:frameIndex];
      if (!status) {
        [flushLock unlock];
        return status;
      }
      UIImage* image = [self imageForCFMutableData:dataRef];
      self.currentUIImage = image;
      if (self.memoryCacheEnabled) {
        [self->imagesMap setObject:image forKey:[NSNumber numberWithInteger:frameIndex]];
      }
      [self submitToImageView];
      [diskCache setObject:rgbaData length:cacheImageSize forKey:frameIndex];
    }
  }
  [flushLock unlock];
  return status;
}

- (void)updateView {
  NSInteger frame = (NSInteger)(floor([valueAnimator getAnimatedFraction] * numFrames));
  if (frame >= numFrames) {
    frame = numFrames - 1;
  }
  if (self.currentFrameIndex != frame) {
    self.currentFrameIndex = frame;
    if ([[imagesMap allKeys] containsObject:[NSNumber numberWithInteger:frame]]) {
      UIImage* image = [imagesMap objectForKey:[NSNumber numberWithInteger:frame]];
      self.currentUIImage = image;
      [self submitToImageView];
    } else {
      NSOperationQueue* flushQueue = [PAGImageView ImageViewFlushQueue];
      __block NSInteger frameIndex = frame;
      [self retain];
      NSBlockOperation* operation = [NSBlockOperation blockOperationWithBlock:^{
        CFMutableDataRef dataRef =
            self.memoryCacheEnabled ? [self getMemoryCacheData] : [self getDiskCacheData];
        if (dataRef == nil) {
          [self releaseSelf];
          return;
        }
        if (self.currentFrameExplicitlySet >= 0) {
          frameIndex = self.currentFrameExplicitlySet;
          [self updateImageViewFrom:dataRef atIndex:frameIndex];
          [self->valueAnimator
              setCurrentPlayTime:((frameIndex + 0.1) * 1.0 / self.frameRate * 1000000)];
          self.currentFrameExplicitlySet = -1;
        } else {
          [self updateImageViewFrom:dataRef atIndex:frameIndex];
        }
        [self releaseSelf];
      }];
      [flushQueue addOperation:operation];
    }
  }
  [listenersLock lock];
  NSHashTable* copiedListeners = listeners.copy;
  [listenersLock unlock];
  for (id item in copiedListeners) {
    id<PAGImageViewListener> listener = (id<PAGImageViewListener>)item;
    if ([listener respondsToSelector:@selector(onAnimationUpdate:)]) {
      [listener onAnimationUpdate:self];
    }
  }
  [copiedListeners release];
}

- (void)submitToImageView {
  dispatch_main_async_safe(^{
    [self setImage:self.currentUIImage];
    [self.layer setNeedsDisplay];
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

- (void)updateSize {
  cacheWidth = round(self.scaleFactor * self.fileWidth);
  cacheHeight = round(self.scaleFactor * self.fileHeight);
  cacheImageSize = cacheWidth * cacheHeight * 4;
  cacheImageRowBytes = cacheWidth * 4;
}

- (void)freeCache {
  if (pagDecoder && [self->imageViewCache count] == self->numFrames) {
    [pagDecoder release];
    pagDecoder = nil;
    if (self->filePath && [PAGContentVersion Get:pagComposition] == 0) {
      [pagComposition release];
      pagComposition = nil;
    }
  }
  if (self.memoryCacheEnabled && [self->imagesMap count] == (NSUInteger)self->numFrames) {
    [self->imageViewCache release];
    self->imageViewCache = nil;
  }
}

- (void)reset {
  if (imageViewCache) {
    [imageViewCache release];
    imageViewCache = nil;
  }
  if (pagDecoder) {
    [pagDecoder release];
    pagDecoder = nil;
  }
  if (filePath && [imageViewCache count] == numFrames) {
    [pagComposition release];
    pagComposition = nil;
  }
  if (currentImageDataRef) {
    CFRelease(currentImageDataRef);
    currentImageDataRef = nil;
  }
  [self unloadAllFrames];
  [self updateSize];
  self.scaleFactor =
      _renderScale * fmax(self.frame.size.width * self.contentScaleFactor / self.fileWidth,
                          self.frame.size.height * self.contentScaleFactor / self.fileHeight);
}

- (UIImage*)imageForCFMutableData:(CFMutableDataRef)dataRef {
  CGBitmapInfo bitmapInfo = kCGBitmapByteOrder32Little | kCGImageAlphaPremultipliedFirst;
  size_t componentsPerPixel = 4;
  size_t bitsPerPixel = 8;
  CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
  CGDataProviderRef provider = CGDataProviderCreateWithCFData(dataRef);
  CGImageRef cgImage = CGImageCreate(
      cacheWidth, cacheHeight, bitsPerPixel, bitsPerPixel * componentsPerPixel, cacheImageRowBytes,
      colorSpace, bitmapInfo, provider, NULL, NO, kCGRenderingIntentDefault);
  UIImage* image = [UIImage imageWithCGImage:cgImage];
  CGColorSpaceRelease(colorSpace);
  CGImageRelease(cgImage);
  CGDataProviderRelease(provider);

  return image;
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
    if (pagComposition || filePath) {
      self.scaleFactor =
          _renderScale * fmax(bounds.size.width * self.contentScaleFactor / self.fileWidth,
                              bounds.size.height * self.contentScaleFactor / self.fileHeight);
      [self updateSize];
      [self unloadAllFrames];
      [self updateView];
    }
  }
}

- (void)setFrame:(CGRect)frame {
  CGRect oldRect = self.frame;
  [super setFrame:frame];
  if (oldRect.size.width != frame.size.width || oldRect.size.height != frame.size.height) {
    if (pagComposition || filePath) {
      self.scaleFactor =
          _renderScale * fmax(frame.size.width * self.contentScaleFactor / self.fileWidth,
                              frame.size.height * self.contentScaleFactor / self.fileHeight);
      [self reset];
      [self updateView];
    }
  }
}

- (void)setRenderScale:(float)scale {
  _renderScale = scale;
  if (pagComposition || filePath) {
    [flushLock lock];
    [self reset];
    [flushLock unlock];
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
    if (pagComposition || filePath) {
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

- (BOOL)setPath:(NSString*)filePath {
  return [self setPath:filePath maxFrameRate:DEFAULT_MAX_FRAMERATE];
}

- (BOOL)setPath:(NSString*)path maxFrameRate:(float)maxFrameRate {
  if ([filePath isEqualToString:path]) {
    return YES;
  }
  if (filePath != nil) {
    [filePath release];
    filePath = nil;
  }
  filePath = [path retain];
  PAGFile* file = [PAGFile Load:filePath];
  [self setComposition:file maxFrameRate:maxFrameRate];
  return file != nil;
}

- (PAGComposition*)getComposition {
  if (filePath) {
    return nil;
  }
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
  [flushLock lock];
  if (pagComposition) {
    [pagComposition release];
  }
  pagComposition = [newComposition retain];
  self.pagContentVersion = [PAGContentVersion Get:pagComposition];
  self.currentFrameExplicitlySet = 0;
  self.fileWidth = [pagComposition width];
  self.fileHeight = [pagComposition height];
  self.scaleFactor =
      _renderScale * fmax(self.frame.size.width * self.contentScaleFactor / self.fileWidth,
                          self.frame.size.height * self.contentScaleFactor / self.fileHeight);
  self.frameRate = MIN(maxFrameRate, [pagComposition frameRate]);
  numFrames = [pagComposition duration] * self.frameRate / 1000000;
  [valueAnimator setDuration:[pagComposition duration]];

  [self reset];
  [flushLock unlock];
}

- (void)setCurrentFrame:(NSUInteger)currentFrame {
  self.currentFrameExplicitlySet = currentFrame;
}

- (NSUInteger)currentFrame {
  if (self.currentFrameExplicitlySet >= 0) {
    return self.currentFrameExplicitlySet;
  }
  NSInteger frame = floor([valueAnimator getAnimatedFraction] * numFrames);
  return MIN(frame, numFrames - 1);
}

- (BOOL)flush {
  CFMutableDataRef dataRef =
      self.memoryCacheEnabled ? [self getMemoryCacheData] : [self getDiskCacheData];
  if (dataRef == nil) {
    return NO;
  }
  return [self updateImageViewFrom:dataRef atIndex:[self currentFrame]];
}

- (void)unloadAllFrames {
  if (imagesMap) {
    [imagesMap removeAllObjects];
  }
  if (pagComposition && [PAGContentVersion Get:pagComposition] > 0) {
    [imageViewCache removeCaches];
  }
}

@end
