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

#import "PAGCompositionImpl.h"
#import "PAGScaleMode.h"
#import "PAGSurfaceImpl.h"

@interface PAGPlayerImpl : NSObject
- (PAGComposition*)getComposition;

- (void)setComposition:(PAGComposition*)newComposition;

- (void)setSurface:(PAGSurfaceImpl*)surface;

- (BOOL)videoEnabled;

- (void)setVideoEnabled:(BOOL)value;

- (BOOL)cacheEnabled;

- (void)setCacheEnabled:(BOOL)value;

- (BOOL)useDiskCache;

- (void)setUseDiskCache:(BOOL)value;

- (float)cacheScale;

- (void)setCacheScale:(float)value;

- (float)maxFrameRate;

- (void)setMaxFrameRate:(float)value;

- (int)scaleMode;

- (void)setScaleMode:(int)value;

- (CGAffineTransform)matrix;

- (void)setMatrix:(CGAffineTransform)value;

- (int64_t)duration;

- (double)getProgress;

- (void)setProgress:(double)value;

- (int64_t)currentFrame;

- (void)prepare;

- (BOOL)flush;

- (CGRect)getBounds:(PAGLayer*)pagLayer;

- (NSArray<PAGLayer*>*)getLayersUnderPoint:(CGPoint)point;

- (BOOL)hitTestPoint:(PAGLayer*)layer point:(CGPoint)point pixelHitTest:(BOOL)pixelHitTest;

@end
