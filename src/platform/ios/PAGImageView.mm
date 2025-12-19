/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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
#include <VideoToolbox/VideoToolbox.h>
#include <mutex>

#include "base/utils/TimeUtil.h"
#include "pag/pag.h"
#include "rendering/layers/ContentVersion.h"

#import "PAGFile.h"
#import "platform/cocoa/PAGDiskCache.h"
#import "platform/cocoa/private/PAGAnimator.h"
#import "platform/cocoa/private/PAGLayer+Internal.h"
#import "platform/cocoa/private/PAGLayerImpl+Internal.h"
#import "platform/cocoa/private/PixelBufferUtil.h"

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
@property(atomic, assign) BOOL isVisible;
@property(atomic, assign) NSInteger currentFrameIndex;
@property(atomic, retain) UIImage* currentUIImage;
@property(nonatomic, assign) BOOL memoryCacheEnabled;
@property(nonatomic, assign) BOOL memeoryCacheFinished;
@property(nonatomic, assign) NSInteger fileWidth;
@property(nonatomic, assign) NSInteger fileHeight;
@property(nonatomic, assign) float maxFrameRate;
@property(nonatomic, assign) CGSize viewSize;

@end

@implementation PAGImageView {
  NSString* filePath;
  PAGAnimator* animator;
  PAGComposition* pagCompositionObjC;
  std::shared_ptr<pag::PAGComposition> pagComposition;
  std::shared_ptr<pag::PAGDecoder> pagDecoder;
  int64_t duration;
  float renderScaleFactor;
  NSInteger width;
  NSInteger height;
  NSUInteger numFrames;
  uint32_t pagContentVersion;

  NSMutableDictionary<NSNumber*, UIImage*>* imagesMap;
  std::mutex imageViewLock;
  CVPixelBufferPoolRef diskBufferPool;
}

@synthesize memoryCacheEnabled = _memoryCacheEnabled;

- (instancetype)initWithFrame:(CGRect)frame {
  if (self = [super initWithFrame:frame]) {
    [self initPAG];
  }
  return self;
}

- (void)initPAG {
  pagDecoder = nullptr;
  pagComposition = nullptr;
  pagCompositionObjC = nil;
  self.currentFrameIndex = -1;
  renderScaleFactor = 1.0;
  duration = 0;
  pagContentVersion  =  0;
  self.memoryCacheEnabled = NO;
  self.memeoryCacheFinished = NO;
  self.isVisible = NO;
  filePath = nil;
  width = 0;
  height = 0;
  numFrames = 0;
  self.backgroundColor = [UIColor clearColor];
  animator = [[PAGAnimator alloc] initWithUpdater:(id<PAGAnimatorUpdater>)self];

  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(applicationDidBecomeActive:)
                                               name:UIApplicationDidBecomeActiveNotification
                                             object:nil];
}

- (void)dealloc {
  [animator cancel];
  [animator release];
  [self reset];
  if (pagCompositionObjC) {
    [pagCompositionObjC release];
    pagCompositionObjC = nil;
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

- (void)setCompositionInternal:(std::shared_ptr<pag::PAGComposition>)newComposition
                  maxFrameRate:(float)maxFrameRate {
  if (pagComposition == newComposition) {
    return;
  }
    if (!filePath) {
        pagComposition = newComposition;
    }
  if (newComposition) {
    self.fileWidth = newComposition->width();
    self.fileHeight = newComposition->height();
    pagContentVersion = pag::ContentVersion::Get(newComposition);
    duration = newComposition->duration();
  } else {
    self.fileWidth = 0;
    self.fileHeight = 0;
    pagContentVersion = 0;
    duration = 0;
  }
  self.maxFrameRate = maxFrameRate;

  [self reset];
  [self updatePAGDecoder];
  if (self.isVisible) {
    [animator setDuration:duration];
  }
}

- (CVPixelBufferRef)getDiskCacheCVPixelBuffer {
  if (diskBufferPool == nil) {
    NSDictionary* options = @{
      (id)kCVPixelBufferIOSurfacePropertiesKey : @{},
      (id)kCVPixelBufferWidthKey : @(width),
      (id)kCVPixelBufferHeightKey : @(height),
      (id)kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_32BGRA)
    };
    CVReturn status = CVPixelBufferPoolCreate(kCFAllocatorDefault, nil, (CFDictionaryRef)options,
                                              &diskBufferPool);
    if (status != kCVReturnSuccess || diskBufferPool == nil) {
      return nil;
    }
  }
  CVPixelBufferRef pixelBuffer;
  CVReturn status =
      CVPixelBufferPoolCreatePixelBuffer(kCFAllocatorDefault, diskBufferPool, &pixelBuffer);
  CVPixelBufferPoolFlush(diskBufferPool, kCVPixelBufferPoolFlushExcessBuffers);
  if (status != kCVReturnSuccess) {
    return nil;
  }
  CFAutorelease(pixelBuffer);
  return pixelBuffer;
}

- (CVPixelBufferRef)getMemoryCacheCVPixelBuffer {
  CVPixelBufferRef pixelBuffer =
      pag::PixelBufferUtil::Make(static_cast<int>(width), static_cast<int>(height));
  if (pixelBuffer == nil) {
    NSLog(@"PAGImageView: CVPixelBufferRef create failed!");
    return nil;
  }
  return pixelBuffer;
}

- (void)updatePAGDecoder {
  if (pagDecoder == nullptr) {
    float scaleFactor;
    if (self.viewSize.width >= self.viewSize.height) {
      scaleFactor = static_cast<float>(
          renderScaleFactor * (self.viewSize.width * [UIScreen mainScreen].scale / self.fileWidth));
    } else {
      scaleFactor =
          static_cast<float>(renderScaleFactor * (self.viewSize.height *
                                                  [UIScreen mainScreen].scale / self.fileHeight));
    }
    if (pagComposition) {
      pagDecoder = pag::PAGDecoder::MakeFrom(pagComposition, self.maxFrameRate, scaleFactor);
    } else if (filePath) {
      auto file = pag::PAGFile::Load([filePath UTF8String]);
      pagDecoder = pag::PAGDecoder::MakeFrom(file, self.maxFrameRate, scaleFactor);
    }
    if (pagDecoder) {
      width = pagDecoder->width();
      height = pagDecoder->height();
      numFrames = pagDecoder->numFrames();
    }
  }
}

- (void)onAnimationFlush:(double)progress {
  [self flush];
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
    int64_t currentDuration = pagComposition ? pagComposition->duration() : duration;
    [animator setDuration:currentDuration];
  } else {
    [animator setDuration:0];
  }
}

- (BOOL)updateImageViewFrom:(CVPixelBufferRef)pixelBuffer atIndex:(NSInteger)frameIndex {
  [self freeCache];
  if ([[imagesMap allKeys] containsObject:@(frameIndex)]) {
    UIImage* image = imagesMap[@(frameIndex)];
    if (image) {
      self.currentFrameIndex = frameIndex;
      self.currentUIImage = image;
      [self submitToImageView];
    }
    if ([imagesMap count] == numFrames) {
      self.memeoryCacheFinished = YES;
    }
    return YES;
  }
  [self updatePAGDecoder];
  if (pagDecoder == nullptr) {
    return false;
  }
  if (pagDecoder->checkFrameChanged(static_cast<int>(frameIndex))) {
    BOOL status = pagDecoder->readFrame(static_cast<int>(frameIndex), pixelBuffer);
    if (!status) {
      return status;
    }
    UIImage* image = [self imageForCVPixelBuffer:pixelBuffer];
    if (image) {
      self.currentFrameIndex = frameIndex;
      self.currentUIImage = image;
      [self submitToImageView];
    }
  }
  if (self.memoryCacheEnabled && self.currentUIImage) {
    if (imagesMap == nil) {
      imagesMap = [NSMutableDictionary new];
    }
    self->imagesMap[@(frameIndex)] = self.currentUIImage;
  }

  return YES;
}

- (BOOL)checkPAGCompositionChanged {
  uint32_t currentVersion = pag::ContentVersion::Get(pagComposition);
  if (currentVersion != pagContentVersion) {
    pagContentVersion = currentVersion;
    [self reset];
    [self updatePAGDecoder];
    if (self.isVisible && pagComposition) {
      [animator setDuration:pagComposition->duration()];
    }
    return YES;
  }
  return NO;
}

- (void)submitToImageView {
  dispatch_main_async_safe(^{
    [self setImage:self.currentUIImage];
    [self setNeedsDisplay];
  });
}

- (void)applicationDidBecomeActive:(NSNotification*)notification {
  if (self.isVisible) {
    [PAGImageView RegisterFlushQueueDestoryMethod];
    [animator update];
  }
}

- (void)freeCache {
  if (self.memoryCacheEnabled && self->pagDecoder && [self->imagesMap count] == numFrames) {
    self->pagDecoder = nullptr;
  }
}

- (void)reset {
  if (imagesMap) {
    [imagesMap removeAllObjects];
    [imagesMap release];
    imagesMap = nil;
    self.memeoryCacheFinished = NO;
  }
  if (diskBufferPool) {
    CVPixelBufferPoolRelease(diskBufferPool);
    diskBufferPool = nil;
  }
  pagDecoder = nullptr;
  width = 0;
  height = 0;
  numFrames = 0;
}

- (UIImage*)imageForCVPixelBuffer:(CVPixelBufferRef)pixelBuffer {
  if (pixelBuffer == nil) {
    return nil;
  }
  CGImageRef imageRef = nil;
  VTCreateCGImageFromCVPixelBuffer(pixelBuffer, nil, &imageRef);
  UIImage* uiImage = [UIImage imageWithCGImage:imageRef];
  CGImageRelease(imageRef);
  return uiImage;
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
      [self updatePAGDecoder];
      if (oldBounds.size.width == 0 || oldBounds.size.height == 0) {
        [animator update];
      }
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
      [self updatePAGDecoder];
      if (oldRect.size.width == 0 || oldRect.size.height == 0) {
        [animator update];
      }
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
    [self updatePAGDecoder];
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
      [self updatePAGDecoder];
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
  if (!_memoryCacheEnabled && imagesMap) {
    [imagesMap removeAllObjects];
    [imagesMap release];
    imagesMap = nil;
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
  [animator start];
}

- (void)pause {
  [animator cancel];
}

- (NSUInteger)numFrames {
  std::lock_guard<std::mutex> autoLock(imageViewLock);
  return numFrames;
}

- (UIImage*)currentImage {
  std::lock_guard<std::mutex> autoLock(imageViewLock);
  return [[_currentUIImage retain] autorelease];
}

- (BOOL)isPlaying {
  return [animator isRunning];
}

- (void)addListener:(id<PAGImageViewListener>)listener {
  [animator addListener:(id<PAGAnimatorListener>)listener];
}

- (void)removeListener:(id<PAGImageViewListener>)listener {
  [animator removeListener:(id<PAGAnimatorListener>)listener];
}

- (int)repeatCount {
  return [animator repeatCount];
}

- (void)setRepeatCount:(int)repeatCount {
  [animator setRepeatCount:repeatCount];
}

- (NSString*)getPath {
  std::lock_guard<std::mutex> autoLock(imageViewLock);
  return filePath == nil ? nil : [[filePath retain] autorelease];
}

- (BOOL)setPath:(NSString*)newPath {
  return [self setPath:newPath maxFrameRate:DEFAULT_MAX_FRAMERATE];
}

- (BOOL)setPath:(NSString*)path maxFrameRate:(float)maxFrameRate {
  std::lock_guard<std::mutex> autoLock(imageViewLock);
  if (filePath != nil) {
    [filePath release];
    filePath = nil;
  }
  if (pagCompositionObjC) {
    [pagCompositionObjC release];
    pagCompositionObjC = nil;
  }
  filePath = [path retain];
  auto file = pag::PAGFile::Load([path UTF8String]);
  [self setCompositionInternal:file maxFrameRate:maxFrameRate];
  return file != nullptr;
}

- (void)setPathAsync:(NSString*)path completionBlock:(void (^)(PAGFile*))callback {
  [self setPathAsync:path
         maxFrameRate:DEFAULT_MAX_FRAMERATE
      completionBlock:^(PAGFile* pagFile) {
        callback(pagFile);
      }];
}

- (void)setPathAsync:(NSString*)path
        maxFrameRate:(float)maxFrameRate
     completionBlock:(void (^)(PAGFile*))callback {
  std::lock_guard<std::mutex> autoLock(imageViewLock);
  if (filePath != nil) {
    [filePath release];
    filePath = nil;
  }
  if (pagCompositionObjC) {
    [pagCompositionObjC release];
    pagCompositionObjC = nil;
  }
  filePath = [path retain];
  [self retain];
  [PAGFile LoadAsync:path
      completionBlock:^(PAGFile* pagFile) {
        std::shared_ptr<pag::PAGFile> cppFile = nullptr;
        if (pagFile != nil) {
          auto layer = [[pagFile impl] pagLayer];
          cppFile = std::static_pointer_cast<pag::PAGFile>(layer);
        }
        imageViewLock.lock();
        [self setCompositionInternal:cppFile maxFrameRate:maxFrameRate];
        imageViewLock.unlock();
        callback(pagFile);
        [self release];
      }];
}

- (PAGComposition*)getComposition {
  std::lock_guard<std::mutex> autoLock(imageViewLock);
  if (filePath) {
    return nil;
  }
  return pagCompositionObjC ? [[pagCompositionObjC retain] autorelease] : nil;
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
  if (pagCompositionObjC != newComposition) {
    if (pagCompositionObjC) {
      [pagCompositionObjC release];
      pagCompositionObjC = nil;
    }
    pagCompositionObjC = [newComposition retain];
  }
  std::shared_ptr<pag::PAGComposition> cppComposition = nullptr;
  if (newComposition != nil) {
    auto layer = [[newComposition impl] pagLayer];
    cppComposition = std::static_pointer_cast<pag::PAGComposition>(layer);
  }
  [self setCompositionInternal:cppComposition maxFrameRate:maxFrameRate];
}

- (void)setCurrentFrame:(NSUInteger)currentFrame {
  std::lock_guard<std::mutex> autoLock(imageViewLock);
  [animator setProgress:pag::FrameToProgress(currentFrame, numFrames)];
}

- (NSUInteger)currentFrame {
  std::lock_guard<std::mutex> autoLock(imageViewLock);
  return [self currentFrameInternal];
}

- (NSUInteger)currentFrameInternal {
  return pag::ProgressToFrame([animator progress], numFrames);
}

- (BOOL)flush {
  std::lock_guard<std::mutex> autoLock(imageViewLock);
  NSInteger frameIndex = [self currentFrameInternal];
  if (self.memeoryCacheFinished) {
    if ([self checkPAGCompositionChanged] == NO) {
      if (self.currentFrameIndex != frameIndex) {
        UIImage* image = imagesMap[@(frameIndex)];
        if (image) {
          self.currentFrameIndex = frameIndex;
          self.currentUIImage = image;
          [self submitToImageView];
          return YES;
        }
      }
    }
  }
  if (self.currentFrameIndex == frameIndex) {
    return NO;
  }
  [self checkPAGCompositionChanged];
  CVPixelBufferRef pixelBuffer = self.memoryCacheEnabled ? [self getMemoryCacheCVPixelBuffer]
                                                         : [self getDiskCacheCVPixelBuffer];
  if (pixelBuffer == nil) {
    self.currentUIImage = nil;
    [self submitToImageView];
    return NO;
  }
  return [self updateImageViewFrom:pixelBuffer atIndex:frameIndex];
}

@end
