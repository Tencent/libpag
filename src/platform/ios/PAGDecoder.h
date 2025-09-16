
/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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

#import <UIKit/UIKit.h>
#import "PAGComposition.h"

/**
 * PAGDecoder provides a utility to read image frames directly from a PAGComposition, and caches the
 * image frames as a sequence file on the disk, which may significantly speed up the reading process
 * depending on the complexity of the PAG files. You can use the PAGDiskCache::SetMaxDiskSize()
 * method to manage the cache limit of the disk usage.
 */
PAG_API @interface PAGDecoder : NSObject
/**
 * Creates a new PAGDecoder with the specified PAGComposition, using the composition's frame
 * rate and size. Please only keep an external reference to the PAGComposition if you need to
 * modify it in the feature. Otherwise, the internal composition will not be released
 * automatically after the associated disk cache is complete, which may cost more memory than
 * necessary. Returns nil if the composition is nil. Note that the returned PAGDecoder may become
 * invalid if the associated PAGComposition is added to a PAGPlayer or another PAGDecoder.
 */
+ (nullable instancetype)Make:(nullable PAGComposition*)composition;

/**
 * Creates a PAGDecoder with a PAGComposition, a frame rate limit, and a scale factor for the
 * decoded image size. Please only keep an external reference to the PAGComposition if you need to
 * modify it in the feature. Otherwise, the internal composition will not be released automatically
 * after the associated disk cache is complete, which may cost more memory than necessary. Returns
 * nullptr if the composition is nullptr. Note that the returned PAGDecoder may become invalid if
 * the associated PAGComposition is added to a PAGPlayer or another PAGDecoder.
 */
+ (nullable instancetype)Make:(nullable PAGComposition*)composition
                 maxFrameRate:(float)maxFrameRate
                        scale:(float)scale;

/**
 * Returns the width of decoded image frames.
 */
- (NSInteger)width;

/**
 * Returns the height of decoded image frames.
 */
- (NSInteger)height;

/**
 * Returns the number of frames in the PAGDecoder. Note that the value may change if the associated
 * PAGComposition was modified.
 */
- (NSInteger)numFrames;

/**
 * Returns the frame rate of decoded image frames. The value may change if the associated
 * PAGComposition was modified.
 */
- (float)frameRate;

/**
 * Returns true if the frame at the given index has changed since the last copyFrameTo(),
 * readFrameTo(), or frameAtIndex() call. The caller should skip the corresponding call if
 * the frame has not changed.
 */
- (BOOL)checkFrameChanged:(int)index;

/**
 * Copies pixels of the image frame at the given index to the specified memory address. The format
 * of the copied pixels is in the BGRA color type with the premultiplied alpha type. Returns false
 * if failed. Note that caller must ensure that the rowBytes stays the same throughout every copying
 * call. Otherwise, it may return false.
 */
- (BOOL)copyFrameTo:(void* _Nonnull)pixels rowBytes:(size_t)rowBytes at:(NSInteger)index;

/**
 * Reads pixels of the image frame at the given index into the specified CVPixelBuffer. Returns
 * false if failed. Reading image frames into HardwareBuffer usually has better performance than
 * reading into memory.
 */
- (BOOL)readFrame:(NSInteger)index to:(CVPixelBufferRef _Nonnull)pixelBuffer;

/**
 * Returns the image frame at the specified index. Note that this method must be called while the
 * app is not in the background. Otherwise, it may return nil because the GPU code may not be
 * executed. It's recommended to read the image frames in forward order for better performance.
 */
- (nullable UIImage*)frameAtIndex:(NSInteger)index;

@end
