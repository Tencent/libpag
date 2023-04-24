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

#import "PAGDecoderImpl.h"
#import <VideoToolbox/VideoToolbox.h>
#import "PAGLayer+Internal.h"
#import "PAGLayerImpl+Internal.h"
#include "PixelBufferUtils.h"
#include "base/utils/Log.h"
#include "pag/pag.h"

@implementation PAGDecoderImpl {
  std::shared_ptr<pag::PAGDecoder> pagDecoder;
}

+ (nullable instancetype)Make:(nullable PAGComposition*)composition
                 maxFrameRate:(float)maxFrameRate
                        scale:(float)scale
                 useDiskCache:(BOOL)useDiskCache {
  std::shared_ptr<pag::PAGComposition> pagComposition = nullptr;
  if (composition != nil) {
    auto layer = [[composition impl] pagLayer];
    pagComposition = std::static_pointer_cast<pag::PAGComposition>(layer);
  }
  auto decoder = pag::PAGDecoder::MakeFrom(pagComposition, maxFrameRate, scale, useDiskCache);
  if (decoder == nullptr) {
    return nil;
  }
  PAGDecoderImpl* pagDecoder = [[PAGDecoderImpl alloc] initWithDecoder:decoder];
  return [pagDecoder autorelease];
}

- (instancetype)initWithDecoder:(std::shared_ptr<pag::PAGDecoder>)value {
  if (self = [super init]) {
    pagDecoder = value;
  }
  return self;
}

- (NSInteger)width {
  return pagDecoder->width();
}

- (NSInteger)height {
  return pagDecoder->height();
}

- (NSInteger)numFrames {
  return pagDecoder->numFrames();
}

- (float)frameRate {
  return pagDecoder->frameRate();
}

- (BOOL)checkFrameChanged:(int)index {
  return pagDecoder->checkFrameChanged(index);
}

- (BOOL)copyFrameTo:(void*)pixels rowBytes:(size_t)rowBytes at:(NSInteger)index {
  return pagDecoder->readFrame(index, pixels, rowBytes, pag::ColorType::BGRA_8888,
                               pag::AlphaType::Premultiplied);
}

- (nullable UIImage*)frameAtIndex:(NSInteger)index {
  auto pixelBuffer = pag::PixelBufferUtils::Make(pagDecoder->width(), pagDecoder->height());
  if (pixelBuffer == nil) {
    LOGE("PAGDecoder: CVPixelBufferRef create failed!");
    return nil;
  }
  CVPixelBufferLockBaseAddress(pixelBuffer, 0);
  auto rowBytes = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 0);
  auto pixels = CVPixelBufferGetBaseAddress(pixelBuffer);
  auto success = pagDecoder->readFrame(index, pixels, rowBytes, pag::ColorType::BGRA_8888,
                                       pag::AlphaType::Premultiplied);
  CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
  if (!success) {
    return nil;
  }
  CGImageRef imageRef = nil;
  VTCreateCGImageFromCVPixelBuffer(pixelBuffer, nil, &imageRef);
  UIImage* uiImage = [UIImage imageWithCGImage:imageRef];
  CGImageRelease(imageRef);
  return uiImage;
}

@end
