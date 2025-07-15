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
#import "PAGFileImpl.h"
#import "PAGImageLayerImpl.h"
#import "PAGLayer+Internal.h"
#import "PAGLayerImpl+Internal.h"
#import "PAGShapeLayerImpl.h"
#import "PAGSolidLayerImpl.h"
#import "PAGTextLayerImpl.h"
#import "platform/cocoa/PAGFile.h"
#import "platform/cocoa/PAGImageLayer.h"
#import "platform/cocoa/PAGShapeLayer.h"
#import "platform/cocoa/PAGSolidLayer.h"
#import "platform/cocoa/PAGTextLayer.h"

@interface PAGLayerImpl () {
  std::shared_ptr<pag::PAGLayer> _pagLayer;
}

@property(nonatomic) std::shared_ptr<pag::PAGLayer> pagLayer;

@end

@implementation PAGLayerImpl

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
  return (PAGComposition*)[PAGLayerImpl ToPAGLayer:pagComposition];
}

- (NSArray<PAGMarker*>*)markers {
  auto temp = _pagLayer->markers();
  return [PAGLayerImpl BatchConvertToPAGMarkers:temp];
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
  return [PAGLayerImpl ToPAGLayer:pagLayer];
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
      PAGSolidLayerImpl* impl = [[[PAGSolidLayerImpl alloc] initWithPagLayer:layer] autorelease];
      result = [[PAGSolidLayer alloc] initWithImpl:impl];
    } break;
    case pag::LayerType::Text: {
      PAGTextLayerImpl* impl = [[[PAGTextLayerImpl alloc] initWithPagLayer:layer] autorelease];
      result = [[PAGTextLayer alloc] initWithImpl:impl];
    } break;
    case pag::LayerType::Shape: {
      PAGShapeLayerImpl* impl = [[[PAGShapeLayerImpl alloc] initWithPagLayer:layer] autorelease];
      result = [[PAGShapeLayer alloc] initWithImpl:impl];
    } break;
    case pag::LayerType::Image: {
      PAGImageLayerImpl* impl = [[[PAGImageLayerImpl alloc] initWithPagLayer:layer] autorelease];
      result = [[PAGImageLayer alloc] initWithImpl:impl];
    } break;
    case pag::LayerType::PreCompose: {
      auto composition = std::static_pointer_cast<pag::PAGComposition>(layer);
      if (composition->isPAGFile()) {
        PAGFileImpl* impl = [[[PAGFileImpl alloc] initWithPagLayer:composition] autorelease];
        result = [[PAGFile alloc] initWithImpl:impl];
      } else {
        PAGCompositionImpl* impl =
            [[[PAGCompositionImpl alloc] initWithPagLayer:composition] autorelease];
        result = [[PAGComposition alloc] initWithImpl:impl];
      }
    } break;
    default: {
      PAGLayerImpl* impl = [[[PAGLayerImpl alloc] initWithPagLayer:layer] autorelease];
      result = [[PAGLayer alloc] initWithImpl:impl];
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
    PAGLayer* obj = [PAGLayerImpl ToPAGLayer:pagLayer];
    if (obj) {
      [mArray addObject:obj];
    }
  }
  NSArray* result = [mArray copy];
  [mArray release];
  return [result autorelease];
}

- (void)dealloc {
  _pagLayer->externalHandle = nullptr;
  [super dealloc];
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

@end
