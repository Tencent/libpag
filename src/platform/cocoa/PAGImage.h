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

#import <CoreGraphics/CoreGraphics.h>
#import <CoreVideo/CoreVideo.h>
#import <Foundation/Foundation.h>
#import "PAG.h"
#import "PAGScaleMode.h"

PAG_API @interface PAGImage : NSObject
/**
 * Creates a PAGImage object from a CGImage object, return null if it's not valid CGImage object.
 */
+ (PAGImage*)FromCGImage:(CGImageRef)cgImage;

/**
 * Creates a PAGImage object from a path of a image file, return null if the file does not exist or
 * it's not a valid image file.
 */
+ (PAGImage*)FromPath:(NSString*)path;

/**
 * Creates a PAGImage object from the specified byte data, return null if the bytes is empty or it's
 * not a valid image file.
 */
+ (PAGImage*)FromBytes:(const void*)bytes size:(size_t)length;

/**
 * Creates a PAGImage object from the specified CVPixelBuffer, return null if the CVPixelBuffer is
 * invalid.
 */
+ (PAGImage*)FromPixelBuffer:(CVPixelBufferRef)pixelBuffer;

/**
 * Returns the width in pixels of the image.
 */
- (NSInteger)width;

/**
 * Returns the height in pixels of the image.
 */
- (NSInteger)height;

/**
 * Returns the current scale mode.
 */
- (PAGScaleMode)scaleMode;

/**
 * Specifies the rule of how to scale the image content to fit the original image size. The matrix
 * changes when this method is called.
 */
- (void)setScaleMode:(PAGScaleMode)value;

/**
 * Returns a copy of current matrix.
 */
- (CGAffineTransform)matrix;

/**
 * Set the transformation which will be applied to the image content. The scaleMode property
 * will be set to PAGScaleMode::None when this method is called.
 */
- (void)setMatrix:(CGAffineTransform)value;

@end
