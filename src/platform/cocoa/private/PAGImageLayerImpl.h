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
#import "PAGLayerImpl.h"
#import "PAGScaleMode.h"

@class PAGVideoRange;

@interface PAGImageLayerImpl : PAGLayerImpl
+ (instancetype)Make:(CGSize)size duration:(int64_t)duration;

- (NSArray<PAGVideoRange*>*)getVideoRanges;

- (void)replaceImage:(PAGImage*)image;

- (void)setImage:(PAGImage*)image;

- (int64_t)contentDuration;

- (NSData*)imageBytes;
@end
