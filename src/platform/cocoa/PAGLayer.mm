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

#import "PAGLayer.h"
#import "platform/cocoa/private/PAGLayer+Internal.h"
#import "platform/cocoa/PAGComposition.h"
#import "platform/cocoa/PAGFile.h"
#import "platform/cocoa/PAGImageLayer.h"
#import "platform/cocoa/PAGShapeLayer.h"
#import "platform/cocoa/PAGSolidLayer.h"
#import "platform/cocoa/PAGTextLayer.h"

@interface PAGLayer () {
  std::shared_ptr<pag::PAGLayer> _pagLayer;
}

@property(nonatomic) std::shared_ptr<pag::PAGLayer> pagLayer;

@end

@implementation PAGLayer

- (instancetype)initWithPagLayer:(std::shared_ptr<pag::PAGLayer>)pagLayer {
  if (self = [super init]) {
    [self setPagLayer:pagLayer];
  }
  return self;
}

- (std::shared_ptr<pag::PAGLayer>)pagLayer {
  return _pagLayer;
}

- (void)setPagLayer:(std::shared_ptr<pag::PAGLayer>)pagLayer {
  _pagLayer = pagLayer;
}

- (PAGLayerType)layerType {
  return (PAGLayerType)(_pagLayer->layerType());
}

- (NSString*)layerName {
  std::string name = _pagLayer ? _pagLayer->layerName() : "";
  return [NSString stringWithUTF8String:name.c_str()];
}

- (CGAffineTransform)matrix {
  auto matrix = _pagLayer->matrix();
  float values[9];
  matrix.get9(values);
  return {values[0], values[3], values[1], values[4], values[2], values[5]};
}

- (void)setMatrix:(CGAffineTransform)value {
  pag::Matrix matrix = {};
  matrix.setAffine(value.a, value.b, value.c, value.d, value.tx, value.ty);
  _pagLayer->setMatrix(matrix);
}

- (void)resetMatrix {
  _pagLayer->resetMatrix();
}

- (CGAffineTransform)getTotalMatrix {
  auto matrix = _pagLayer->getTotalMatrix();
  float values[9];
  matrix.get9(values);
  return {values[0], values[3], values[1], values[4], values[2], values[5]};
}

- (BOOL)visible {
  return _pagLayer->visible();
}

- (void)setVisible:(BOOL)visible {
  _pagLayer->setVisible(visible);
}

- (NSInteger)editableIndex {
  return _pagLayer->editableIndex();
}

- (PAGComposition*)parent {
  auto pagComposition = _pagLayer->parent();
  return (PAGComposition*)[PAGLayer ToPAGLayer:pagComposition];
}

- (NSArray<PAGMarker*>*)markers {
  auto temp = _pagLayer->markers();
  return [PAGLayer BatchConvertToPAGMarkers:temp];
}

- (int64_t)localTimeToGlobal:(int64_t)localTime {
  return _pagLayer->localTimeToGlobal(localTime);
}

- (int64_t)globalToLocalTime:(int64_t)globalTime {
  return _pagLayer->globalToLocalTime(globalTime);
}

- (int64_t)duration {
  return _pagLayer->duration();
}

- (float)frameRate {
  return _pagLayer->frameRate();
}

- (int64_t)startTime {
  return _pagLayer->startTime();
}

- (void)setStartTime:(int64_t)time {
  _pagLayer->setStartTime(time);
}

- (int64_t)currentTime {
  return _pagLayer->currentTime();
}

- (void)setCurrentTime:(int64_t)time {
  _pagLayer->setCurrentTime(time);
}

- (double)getProgress {
  return _pagLayer->getProgress();
}

- (void)setProgress:(double)value {
  _pagLayer->setProgress(value);
}

- (PAGLayer*)trackMatteLayer {
  auto pagLayer = _pagLayer->trackMatteLayer();
  return [PAGLayer ToPAGLayer:pagLayer];
}

- (CGRect)getBounds {
  auto rect = _pagLayer->getBounds();
  return CGRectMake(rect.x(), rect.y(), rect.width(), rect.height());
}

+ (PAGLayer*)ToPAGLayer:(std::shared_ptr<pag::PAGLayer>)layer {
  if (layer == nullptr) {
    return nil;
  }
  if (layer->externalHandle != nullptr) {
    return (PAGLayer*)layer->externalHandle;
  }
  id result = nil;
  switch (layer->layerType()) {
    case pag::LayerType::Solid: {
      result = [[PAGSolidLayer alloc] initWithPagLayer:layer];
    } break;
    case pag::LayerType::Text: {
      result = [[PAGTextLayer alloc] initWithPagLayer:layer];
    } break;
    case pag::LayerType::Shape: {
      result = [[PAGShapeLayer alloc] initWithPagLayer:layer];
    } break;
    case pag::LayerType::Image: {
      result = [[PAGImageLayer alloc] initWithPagLayer:layer];
    } break;
    case pag::LayerType::PreCompose: {
      auto composition = std::static_pointer_cast<pag::PAGComposition>(layer);
      if (composition->isPAGFile()) {
        result = [[PAGFile alloc] initWithPagLayer:composition];
      } else {
        result = [[PAGComposition alloc] initWithPagLayer:composition];
      }
    } break;
    default: {
      result = [[PAGLayer alloc] initWithPagLayer:layer];
    } break;
  }
  layer->externalHandle = result;
  [result autorelease];
  return result;
}

+ (NSArray<PAGLayer*>*)BatchConvertToPAGLayers:
    (const std::vector<std::shared_ptr<pag::PAGLayer>>&)layerVector {
  if (layerVector.empty()) {
    return [NSArray array];
  }
  NSMutableArray* mArray = [NSMutableArray new];
  for (auto pagLayer : layerVector) {
    PAGLayer* obj = [PAGLayer ToPAGLayer:pagLayer];
    if (obj) {
      [mArray addObject:obj];
    }
  }
  NSArray* result = [mArray copy];
  [mArray release];
  return [result autorelease];
}

+ (NSArray<PAGMarker*>*)BatchConvertToPAGMarkers:(std::vector<const pag::Marker*>&)markers {
  if (markers.size() == 0) {
    return nil;
  }
  NSMutableArray* mArray = [NSMutableArray new];
  for (auto marker : markers) {
    PAGMarker* obj = [PAGMarker new];
    obj.startTime = marker->startTime;
    obj.duration = marker->duration;
    obj.comment = [NSString stringWithUTF8String:marker->comment.c_str()];
    [mArray addObject:obj];
    [obj release];
  }
  NSArray* result = [mArray copy];
  [mArray release];
  return [result autorelease];
}

- (BOOL)excludedFromTimeline {
  return _pagLayer->excludedFromTimeline();
}

- (void)setExcludedFromTimeline:(BOOL)value {
  _pagLayer->setExcludedFromTimeline(value);
}

- (float)alpha {
  return _pagLayer->alpha();
}

- (void)setAlpha:(float)value {
  _pagLayer->setAlpha(value);
}

- (void)dealloc {
  if (_pagLayer) {
    _pagLayer->externalHandle = nullptr;
  }
  [super dealloc];
}

@end