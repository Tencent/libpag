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
#include "image/PixelBuffer.h"
#include "pag/pag.h"
#include "platform/apple/BitmapContextUtil.h"
#include "rendering/editing/StillImage.h"

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
  auto hardwareBuffer = pag::PixelBuffer::MakeFrom(pixelBuffer);
  pag::Bitmap bitmap(hardwareBuffer);
  auto image = pag::StillImage::FromBitmap(bitmap);
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
  auto width = static_cast<int>(CGImageGetWidth(cgImage));
  auto height = static_cast<int>(CGImageGetHeight(cgImage));
  pag::Bitmap bitmap = {};
  if (!bitmap.allocPixels(width, height)) {
    return nil;
  }
  auto pixels = bitmap.lockPixels();
  auto context = CreateBitmapContext(bitmap.info(), pixels);
  if (context == nullptr) {
    bitmap.unlockPixels();
    return nil;
  }
  CGRect rect = CGRectMake(0, 0, width, height);
  CGContextSetBlendMode(context, kCGBlendModeCopy);
  CGContextDrawImage(context, rect, cgImage);
  CGContextRelease(context);
  bitmap.unlockPixels();
  auto data = pag::StillImage::FromBitmap(bitmap);
  if (data == nullptr) {
    return nil;
  }
  PAGImageImpl* image = [[[PAGImageImpl alloc] initWidthPAGImage:data] autorelease];
  return image;
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
