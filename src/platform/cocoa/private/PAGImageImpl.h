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

#import <CoreGraphics/CoreGraphics.h>
#import <CoreVideo/CoreVideo.h>
#import <Foundation/Foundation.h>
#import "PAGScaleMode.h"

@interface PAGImageImpl : NSObject

+ (PAGImageImpl*)FromPath:(NSString*)path;

+ (PAGImageImpl*)FromBytes:(const void*)bytes size:(size_t)length;

+ (PAGImageImpl*)FromPixelBuffer:(CVPixelBufferRef)pixelBuffer;

+ (PAGImageImpl*)FromCGImage:(CGImageRef)cgImage;

- (NSInteger)width;

- (NSInteger)height;

- (int)scaleMode;

- (void)setScaleMode:(int)value;

- (CGAffineTransform)matrix;

- (void)setMatrix:(CGAffineTransform)value;

@end
