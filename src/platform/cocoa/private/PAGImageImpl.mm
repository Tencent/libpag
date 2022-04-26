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

#import "PAGImageImpl.h"
#include "base/utils/Log.h"
#include "pag/pag.h"
#include "rendering/editing/StillImage.h"
#include "tgfx/core/PixelBuffer.h"

@interface PAGImageImpl ()

@property(nonatomic) std::shared_ptr<pag::PAGImage> pagImage;

@end

@implementation PAGImageImpl {
}

+ (PAGImageImpl*)FromPath:(NSString*)path {
  if (path == nil) {
    return nil;
  }
  auto data = pag::PAGImage::FromPath(path.UTF8String);
  if (data == nullptr) {
    return nil;
  }
  PAGImageImpl* image = [[[PAGImageImpl alloc] initWidthPAGImage:data] autorelease];
  return image;
}

+ (PAGImageImpl*)FromBytes:(const void*)bytes size:(size_t)length {
  auto data = pag::PAGImage::FromBytes(bytes, length);
  if (data == nullptr) {
    return nil;
  }
  PAGImageImpl* image = [[[PAGImageImpl alloc] initWidthPAGImage:data] autorelease];
  return image;
}

#if TARGET_IPHONE_SIMULATOR

+ (PAGImageImpl*)FromPixelBuffer:(CVPixelBufferRef)pixelBuffer {
  LOGE("The simulator does not support [PAGImage FromPixelBuffer:].");
  return nil;
}

#else

+ (PAGImageImpl*)FromPixelBuffer:(CVPixelBufferRef)pixelBuffer {
  auto hardwareBuffer = tgfx::PixelBuffer::MakeFrom(pixelBuffer);
  auto image = pag::StillImage::MakeFrom(hardwareBuffer);
  if (image == nullptr) {
    return nil;
  }
  return [[[PAGImageImpl alloc] initWidthPAGImage:image] autorelease];
}

#endif

+ (PAGImageImpl*)FromCGImage:(CGImageRef)cgImage {
  if (cgImage == nil) {
    return nil;
  }
  auto image = tgfx::Image::MakeFrom(cgImage);
  auto pixelBuffer = tgfx::PixelBuffer::Make(image->width(), image->height());
  tgfx::Bitmap bitmap(pixelBuffer);
  if (bitmap.isEmpty()) {
    return nil;
  }
  if (!image->readPixels(bitmap.info(), bitmap.writablePixels())) {
    return nil;
  }
  bitmap.reset();
  auto data = pag::StillImage::MakeFrom(pixelBuffer);
  if (data == nullptr) {
    return nil;
  }
  PAGImageImpl* pagImage = [[[PAGImageImpl alloc] initWidthPAGImage:data] autorelease];
  return pagImage;
}

- (instancetype)initWidthPAGImage:(std::shared_ptr<pag::PAGImage>)value {
  if (self = [super init]) {
    _pagImage = value;
  }
  return self;
}

- (NSInteger)width {
  return _pagImage->width();
}

- (NSInteger)height {
  return _pagImage->height();
}

- (int)scaleMode {
  return _pagImage->scaleMode();
}

- (void)setScaleMode:(int)value {
  _pagImage->setScaleMode(value);
}

- (CGAffineTransform)matrix {
  auto matrix = _pagImage->matrix();
  float values[9];
  matrix.get9(values);
  return {values[0], values[3], values[1], values[4], values[2], values[5]};
}

- (void)setMatrix:(CGAffineTransform)value {
  pag::Matrix matrix = {};
  matrix.setAffine(value.a, value.b, value.c, value.d, value.tx, value.ty);
  _pagImage->setMatrix(matrix);
}

@end
