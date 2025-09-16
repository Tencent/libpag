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

#import "PAGPlayerImpl.h"
#import "PAGLayer+Internal.h"
#import "PAGLayerImpl+Internal.h"
#import "pag/pag.h"

@interface PAGSurfaceImpl ()

@property(nonatomic) std::shared_ptr<pag::PAGSurface> pagSurface;

@end

@implementation PAGPlayerImpl {
  pag::PAGPlayer* pagPlayer;
}

- (instancetype)init {
  if (self = [super init]) {
    pagPlayer = new pag::PAGPlayer();
  }
  return self;
}

- (void)dealloc {
  delete pagPlayer;
  [super dealloc];
}

- (PAGComposition*)getComposition {
  // 必须从pagPlayer里返回，不能额外存储一个引用，因为同一个PAGComposition添加到别的PAGPlayer后会从当前的移除。
  auto composition = pagPlayer->getComposition();
  return (PAGComposition*)[PAGLayerImpl ToPAGLayer:composition];
}

- (void)setComposition:(PAGComposition*)newComposition {
  if (newComposition != nil) {
    auto layer = [[newComposition impl] pagLayer];
    pagPlayer->setComposition(std::static_pointer_cast<pag::PAGComposition>(layer));
  } else {
    pagPlayer->setComposition(nullptr);
  }
}

- (void)setSurface:(PAGSurfaceImpl*)surface {
  if (surface != nil) {
    pagPlayer->setSurface([surface pagSurface]);
  } else {
    pagPlayer->setSurface(nullptr);
  }
}

- (BOOL)videoEnabled {
  return pagPlayer->videoEnabled();
}

- (void)setVideoEnabled:(BOOL)value {
  pagPlayer->setVideoEnabled(value);
}

- (BOOL)cacheEnabled {
  return pagPlayer->cacheEnabled();
}

- (void)setCacheEnabled:(BOOL)value {
  pagPlayer->setCacheEnabled(value);
}

- (BOOL)useDiskCache {
  return pagPlayer->useDiskCache();
}

- (void)setUseDiskCache:(BOOL)value {
  pagPlayer->setUseDiskCache(value);
}

- (float)cacheScale {
  return pagPlayer->cacheScale();
}

- (void)setCacheScale:(float)value {
  pagPlayer->setCacheScale(value);
}

- (float)maxFrameRate {
  return pagPlayer->maxFrameRate();
}

- (void)setMaxFrameRate:(float)value {
  pagPlayer->setMaxFrameRate(value);
}

- (int)scaleMode {
  return static_cast<int>(pagPlayer->scaleMode());
}

- (void)setScaleMode:(int)value {
  pagPlayer->setScaleMode(static_cast<pag::PAGScaleMode>(value));
}

- (CGAffineTransform)matrix {
  auto matrix = pagPlayer->matrix();
  float values[9];
  matrix.get9(values);
  return {values[0], values[3], values[1], values[4], values[2], values[5]};
}

- (void)setMatrix:(CGAffineTransform)value {
  pag::Matrix matrix = {};
  matrix.setAffine(value.a, value.b, value.c, value.d, value.tx, value.ty);
  pagPlayer->setMatrix(matrix);
}

- (int64_t)duration {
  return pagPlayer->duration();
}

- (double)getProgress {
  return pagPlayer->getProgress();
}

- (void)setProgress:(double)value {
  pagPlayer->setProgress(value);
}

- (int64_t)currentFrame {
  return pagPlayer->currentFrame();
}

- (void)prepare {
  pagPlayer->prepare();
}

- (BOOL)flush {
  return pagPlayer->flush();
}

- (CGRect)getBounds:(PAGLayer*)pagLayer {
  auto layer = [[pagLayer impl] pagLayer];
  auto bounds = pagPlayer->getBounds(layer);
  return CGRectMake(bounds.x(), bounds.y(), bounds.width(), bounds.height());
}

- (NSArray<PAGLayer*>*)getLayersUnderPoint:(CGPoint)point {
  auto pagLayerVector =
      pagPlayer->getLayersUnderPoint(static_cast<float>(point.x), static_cast<float>(point.y));
  return [PAGLayerImpl BatchConvertToPAGLayers:pagLayerVector];
}

- (BOOL)hitTestPoint:(PAGLayer*)layer point:(CGPoint)point pixelHitTest:(BOOL)pixelHitTest {
  if (layer == nil) {
    return false;
  }
  auto pagLayer = [[layer impl] pagLayer];
  return pagPlayer->hitTestPoint(pagLayer, point.x, point.y, pixelHitTest);
}
@end
