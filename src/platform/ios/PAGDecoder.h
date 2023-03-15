
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

#import <Foundation/Foundation.h>
#import "PAGComposition.h"
#import "PAGPixelsType.h"

NS_ASSUME_NONNULL_BEGIN

PAG_API @interface PAGDecoder : NSObject

/**
 Convenience method to create a decoder with specified pagComposition.
 @return A new decoder, or nil if an error occurs.
 */
+ (nullable instancetype)Make:(nullable PAGComposition*)pagComposition;

/**
 Convenience method to create a decoder with specified pagComposition.
 @param scale Output image's scale.
 @return A new decoder, or nil if an error occurs.
 */
+ (nullable instancetype)Make:(nullable PAGComposition*)pagComposition scale:(CGFloat)scale;

/**
 * Returns the width of the output image.
 */
- (NSInteger)width;

/**
 * Returns the height of the output image.
 */
- (NSInteger)height;

/**
 * Returns the number of animated frames.
 */
- (NSInteger)numFrames;

/**
 Returns the frame image from a specified index.
 @note It's necessary to execute  in the foreground.
       It's recommended to read in order, performance is the best at this point.
 @param index Frame index (zero based).
 @return A new frame with image, or nil if an error occurs.
 */
- (nullable UIImage*)frameAtIndex:(NSInteger)index;

/**
 * The maximum frame rate for rendering. If set to a value less than the actual frame rate from
 * PAGFile, it drops frames but increases performance. Otherwise, it has no effect. The default
 * value is 30.
 */
- (float)maxFrameRate;

/**
 * Set the maximum frame rate for rendering.
 */
- (void)setMaxFrameRate:(float)value;

/**
 * Copies pixels from current PAGSurface to dstPixels with specified color type, alpha type and
 * row bytes. Returns true if pixels are copied to dstPixels.
 */
- (BOOL)readPixelsWithColorType:(PAGColorType)colorType
                      alphaType:(PAGAlphaType)alphaType
                      dstPixels:(void*)dstPixels
                    dstRowBytes:(size_t)dstRowBytes
                          index:(NSInteger)index;

@end

NS_ASSUME_NONNULL_END
