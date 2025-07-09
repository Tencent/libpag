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
#import "PAGImageLayerImpl.h"
#import "platform/cocoa/private/PAGLayer+Internal.h"

@implementation PAGImageLayer
+ (instancetype)Make:(CGSize)size duration:(int64_t)duration {
  PAGImageLayerImpl* impl = [PAGImageLayerImpl Make:size duration:duration];
  if (impl == nil) {
    return nil;
  }
  return [[[PAGImageLayer alloc] initWithImpl:impl] autorelease];
}

- (NSArray<PAGVideoRange*>*)getVideoRanges {
  return [(PAGImageLayerImpl*)self.impl getVideoRanges];
}

- (void)replaceImage:(PAGImage*)image {
  [(PAGImageLayerImpl*)self.impl replaceImage:image];
}

- (void)setImage:(PAGImage*)image {
  [(PAGImageLayerImpl*)self.impl setImage:image];
}

- (int64_t)contentDuration {
  return [(PAGImageLayerImpl*)self.impl contentDuration];
}

- (NSData*)imageBytes {
  return [(PAGImageLayerImpl*)self.impl imageBytes];
}
@end
