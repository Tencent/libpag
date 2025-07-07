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

#import <Foundation/Foundation.h>
#import "PAGLayer.h"
#import "PAGScaleMode.h"

@class PAGImage;
@class PAGVideoRange;

PAG_API @interface PAGImageLayer : PAGLayer
/**
 * Make a PAGImageLayer with size and duration(in microseconds).
 */
+ (instancetype)Make:(CGSize)size duration:(int64_t)duration;

/**
 * Returns the time ranges of the source video for replacement.
 */
- (NSArray<PAGVideoRange*>*)getVideoRanges;

/**
 * [Deprecated]
 * Replace the original image content with the specified PAGImage object.
 * Passing in null for the image parameter resets the layer to its default image content.
 * The replaceImage() method modifies all associated PAGImageLayers that have the same
 * editableIndex to this layer.
 *
 * @param image The PAGImage object to replace with.
 */
- (void)replaceImage:(PAGImage*)image DEPRECATED_MSG_ATTRIBUTE("Please use setImage:image instead");

/**
 * Replace the original image content with the specified PAGImage object.
 * Passing in null for the image parameter resets the layer to its default image content.
 * The setImage() method only modifies the content of the calling PAGImageLayer.
 *
 * @param image The PAGImage object to replace with.
 */
- (void)setImage:(PAGImage*)image;

/**
 * Returns the content duration in microseconds, which indicates the minimal length required for
 * replacement.
 */
- (int64_t)contentDuration;

/**
 * The default image data of this layer, which is webp format.
 */
- (NSData*)imageBytes;

@end
