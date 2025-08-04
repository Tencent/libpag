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
#import "PAGImageLayerImpl.h"
#import "PAGLayer+Internal.h"
#import "PAGLayerImpl+Internal.h"

@implementation PAGCompositionImpl

+ (PAGComposition*)Make:(CGSize)size {
  auto pagComposition = pag::PAGComposition::Make(size.width, size.height);
  return (PAGComposition*)[PAGLayerImpl ToPAGLayer:pagComposition];
}

- (NSInteger)width {
  return std::static_pointer_cast<pag::PAGComposition>(self.pagLayer)->width();
}

- (NSInteger)height {
  return std::static_pointer_cast<pag::PAGComposition>(self.pagLayer)->height();
}

- (void)setContentSize:(CGSize)size {
  return std::static_pointer_cast<pag::PAGComposition>(self.pagLayer)
      ->setContentSize(size.width, size.height);
}

- (NSInteger)numChildren {
  return std::static_pointer_cast<pag::PAGComposition>(self.pagLayer)->numChildren();
}

- (PAGLayer*)getLayerAt:(int)index {
  auto pagLayer = std::static_pointer_cast<pag::PAGComposition>(self.pagLayer)->getLayerAt(index);
  return [PAGLayerImpl ToPAGLayer:pagLayer];
}

- (NSInteger)getLayerIndex:(PAGLayer*)layer {
  if (!layer) {
    return -1;
  }
  auto pagLayer = [[layer impl] pagLayer];
  return std::static_pointer_cast<pag::PAGComposition>(self.pagLayer)->getLayerIndex(pagLayer);
}

- (void)setLayerIndex:(NSInteger)index layer:(PAGLayer*)child {
  if (!child) {
    return;
  }
  auto pagLayer = [[child impl] pagLayer];
  std::static_pointer_cast<pag::PAGComposition>(self.pagLayer)->setLayerIndex(pagLayer, (int)index);
}

- (CGRect)getBounds {
  auto rect = std::static_pointer_cast<pag::PAGComposition>(self.pagLayer)->getBounds();
  return CGRectMake(rect.x(), rect.y(), rect.width(), rect.height());
}

- (BOOL)addLayer:(PAGLayer*)pagLayer {
  if (!pagLayer) {
    return NO;
  }
  auto layer = [[pagLayer impl] pagLayer];
  return std::static_pointer_cast<pag::PAGComposition>(self.pagLayer)->addLayer(layer);
}

- (BOOL)addLayer:(PAGLayer*)pagLayer atIndex:(int)index {
  if (!pagLayer) {
    return NO;
  }
  auto layer = [[pagLayer impl] pagLayer];
  return std::static_pointer_cast<pag::PAGComposition>(self.pagLayer)->addLayerAt(layer, index);
}

- (BOOL)contains:(PAGLayer*)pagLayer {
  if (!pagLayer) {
    return NO;
  }
  auto layer = [[pagLayer impl] pagLayer];
  return std::static_pointer_cast<pag::PAGComposition>(self.pagLayer)->contains(layer);
}

- (PAGLayer*)removeLayer:(PAGLayer*)pagLayer {
  if (!pagLayer) {
    return nil;
  }
  auto layer = [[pagLayer impl] pagLayer];
  auto result = std::static_pointer_cast<pag::PAGComposition>(self.pagLayer)->removeLayer(layer);
  if (result) {
    return pagLayer;
  }
  return nil;
}

- (PAGLayer*)removeLayerAt:(int)index {
  auto pagLayer =
      std::static_pointer_cast<pag::PAGComposition>(self.pagLayer)->removeLayerAt(index);
  return [PAGLayerImpl ToPAGLayer:pagLayer];
}

- (void)removeAllLayers {
  std::static_pointer_cast<pag::PAGComposition>(self.pagLayer)->removeAllLayers();
}

- (void)swapLayer:(PAGLayer*)pagLayer1 withLayer:(PAGLayer*)pagLayer2 {
  if (!pagLayer1 || !pagLayer2) {
    return;
  }
  auto layer1 = [[pagLayer1 impl] pagLayer];
  auto layer2 = [[pagLayer2 impl] pagLayer];
  std::static_pointer_cast<pag::PAGComposition>(self.pagLayer)->swapLayer(layer1, layer2);
}

- (void)swapLayerAt:(int)index1 withIndex:(int)index2 {
  std::static_pointer_cast<pag::PAGComposition>(self.pagLayer)->swapLayerAt(index1, index2);
}

- (NSData*)audioBytes {
  auto temp = std::static_pointer_cast<pag::PAGComposition>(self.pagLayer)->audioBytes();
  if (temp == nullptr) {
    return nil;
  }
  return [NSData dataWithBytes:temp->data() length:temp->length()];
}

- (int64_t)audioStartTime {
  return std::static_pointer_cast<pag::PAGComposition>(self.pagLayer)->audioStartTime();
}

- (NSArray<PAGMarker*>*)audioMarkers {
  auto temp = std::static_pointer_cast<pag::PAGComposition>(self.pagLayer)->audioMarkers();
  return [PAGLayerImpl BatchConvertToPAGMarkers:temp];
}

- (NSArray<PAGLayer*>*)getLayersByName:(NSString*)layerName {
  std::string name = layerName == nil ? "" : layerName.UTF8String;
  auto layers = std::static_pointer_cast<pag::PAGComposition>(self.pagLayer)->getLayersByName(name);
  return [PAGLayerImpl BatchConvertToPAGLayers:layers];
}

- (NSArray<PAGLayer*>*)getLayersUnderPoint:(CGPoint)point {
  auto layers = std::static_pointer_cast<pag::PAGComposition>(self.pagLayer)
                    ->getLayersUnderPoint(static_cast<float>(point.x), static_cast<float>(point.y));
  return [PAGLayerImpl BatchConvertToPAGLayers:layers];
}

@end
