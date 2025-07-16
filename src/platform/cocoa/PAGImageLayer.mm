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

#import "PAGImageLayer.h"
#import "PAGImage.h"
#import "PAGVideoRange.h"
#import "platform/cocoa/private/PAGLayer+Internal.h"

@implementation PAGImageLayer

+ (instancetype)Make:(CGSize)size duration:(int64_t)duration {
  auto pagImageLayer = pag::PAGImageLayer::Make(size.width, size.height, duration);
  return [[[PAGImageLayer alloc] initWithPagLayer:pagImageLayer] autorelease];
}

- (NSArray<PAGVideoRange*>*)getVideoRanges {
  auto pagImageLayer = std::static_pointer_cast<pag::PAGImageLayer>([self pagLayer]);
  auto temp = pagImageLayer->getVideoRanges();
  NSMutableArray* mArray = [NSMutableArray new];
  for (auto range : temp) {
    PAGVideoRange* obj = [PAGVideoRange new];
    obj.startTime = range.startTime();
    obj.endTime = range.endTime();
    obj.playDuration = range.playDuration();
    obj.reversed = range.reversed();
    [mArray addObject:obj];
    [obj release];
  }
  NSArray* result = [mArray copy];
  [mArray release];
  return [result autorelease];
}

- (void)replaceImage:(PAGImage*)image {
  auto pagImageLayer = std::static_pointer_cast<pag::PAGImageLayer>([self pagLayer]);
  pagImageLayer->replaceImage(image.pagImage);
}

- (void)setImage:(PAGImage*)image {
  auto pagImageLayer = std::static_pointer_cast<pag::PAGImageLayer>([self pagLayer]);
  pagImageLayer->setImage(image.pagImage);
}

- (int64_t)contentDuration {
  auto pagImageLayer = std::static_pointer_cast<pag::PAGImageLayer>([self pagLayer]);
  return pagImageLayer->contentDuration();
}

- (NSData*)imageBytes {
  auto temp = std::static_pointer_cast<pag::PAGImageLayer>([self pagLayer])->imageBytes();
  if (temp == nullptr) {
    return nil;
  }
  return [NSData dataWithBytes:temp->data() length:temp->length()];
}

@end