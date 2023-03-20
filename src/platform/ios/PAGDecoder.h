
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

NS_ASSUME_NONNULL_BEGIN

PAG_API @interface PAGDecoder : NSObject
/**
 * Creates a new PAGDecoder with the specified PAGComposition, using the composition's frame rate
 * and size.Returns nil if the composition is nil.
 */
+ (nullable instancetype)Make:(nullable PAGComposition*)composition;

/**
 * Creates a new PAGDecoder with the specified PAGComposition, the frame rate limit, and the scale
 * factor for the output image size. Returns nil if the composition is nil.
 */
+ (nullable instancetype)Make:(nullable PAGComposition*)composition
                 maxFrameRate:(float)maxFrameRate
                        scale:(float)scale;

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
 * Copies pixels of the image frame at the given index to the specified memory address. The format
 * of the copied pixels is in the BGRA color type with the premultiplied alpha type. Returns false
 * if failed.
 */
- (BOOL)copyFrameTo:(void*)pixels rowBytes:(size_t)rowBytes at:(NSInteger)index;

/**
 * Returns the image frame at the specified index. Note that this method must be called while the
 * app is not in the background. Otherwise, it may return nil because the GPU code may not be
 * executed. It's recommended to read the image frames in forward order for better performance.
 */
- (nullable UIImage*)frameAtIndex:(NSInteger)index;

@end

NS_ASSUME_NONNULL_END
