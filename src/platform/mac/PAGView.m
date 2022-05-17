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
#import "platform/mac/private/PAGValueAnimator.h"

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
  pagFile = nil;
  filePath = nil;
  self.layer.backgroundColor = [NSColor clearColor].CGColor;
  pagPlayer = [[PAGPlayer alloc] init];
  valueAnimator = [[PAGValueAnimator alloc] init];
  [valueAnimator setListener:self];
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
  [super dealloc];
}

- (void)setBounds:(CGRect)bounds {
  CGRect oldBounds = self.bounds;
  [super setBounds:bounds];
  if (pagSurface != nil &&
      (oldBounds.size.width != bounds.size.width || oldBounds.size.height != bounds.size.height)) {
    [pagSurface updateSize];
  }
}

- (void)setFrame:(CGRect)frame {
  CGRect oldRect = self.frame;
  [super setFrame:frame];
  if (pagSurface != nil &&
      (oldRect.size.width != frame.size.width || oldRect.size.height != frame.size.height)) {
    [pagSurface updateSize];
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
  BOOL visible =
      self.window && !self.isHidden && self.alphaValue > 0.0 && [valueAnimator duration] > 0.0;
  if (_isVisible == visible) {
    return;
  }
  _isVisible = visible;
  if (_isVisible) {
    if (pagSurface == nil) {
      [self initPAGSurface];
    }
    if (_isPlaying) {
      [self doPlay];
    }
  } else {
    if (_isPlaying) {
      [valueAnimator stop:false];
    }
  }
}

- (void)initPAGSurface {
  pagSurface = [[PAGSurface FromView:self] retain];
  [pagPlayer setSurface:pagSurface];
  [self flush];
}

- (void)onAnimationUpdate {
  [self retain];
  dispatch_async(dispatch_get_main_queue(), ^{
    [self flush];
    [self release];
  });
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
  if ([pagPlayer getProgress] == 1.0) {
    [self setProgress:0];
  }
  [self doPlay];
}

- (void)doPlay {
  if (!_isVisible) {
    return;
  }
  double progress = [pagPlayer getProgress];
  int64_t playTime = (int64_t)(progress * [valueAnimator duration]);
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
  [self checkVisible];
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
  [valueAnimator setRepeatCount:0];
}

- (BOOL)flush {
  [pagPlayer setProgress:[valueAnimator getAnimatedFraction]];
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

- (PAGFile*)getFile {
  return pagFile == nil ? nil : [[pagFile retain] autorelease];
}

- (void)setFile:(PAGFile*)newFile {
  if (pagFile != nil) {
    [pagFile release];
    pagFile = nil;
  }
  PAGComposition* composition = newFile != nil ? [[newFile copyOriginal] autorelease] : nil;
  [self setComposition:composition];
  if (newFile != nil) {
    pagFile = [newFile retain];
    [self applyReplacements];
  }
}

- (PAGComposition*)getRootComposition {
  return [pagPlayer getComposition];
}

- (BOOL)flush:(BOOL)force {
  return [pagPlayer flush];
}

- (void)setTextData:(int)index data:(PAGText*)value {
  PAGComposition* composition = [pagPlayer getComposition];
  if (composition != nil && pagFile == nil) {
    return;
  }
  if (composition != nil) {
    [(PAGFile*)composition replaceText:index data:value];
  } else {
    textReplacementMap[@(index)] = value;
  }
}

- (void)replaceImage:(int)index data:(PAGImage*)value {
  PAGComposition* composition = [pagPlayer getComposition];
  if (composition != nil && pagFile == nil) {
    return;
  }
  if (composition != nil) {
    [(PAGFile*)composition replaceImage:index data:value];
  } else {
    imageReplacementMap[@(index)] = value;
  }
}

- (void)applyReplacements {
  PAGFile* file = (PAGFile*)[pagPlayer getComposition];
  if (file == nil) {
    return;
  }
  for (NSNumber* index in textReplacementMap) {
    PAGText* text = textReplacementMap[index];
    [file replaceText:(int)[index integerValue] data:text];
  }
  [textReplacementMap removeAllObjects];

  for (NSNumber* index in imageReplacementMap) {
    PAGImage* image = imageReplacementMap[index];
    [file replaceImage:(int)[index integerValue] data:image];
  }
  [imageReplacementMap removeAllObjects];
}

@end
