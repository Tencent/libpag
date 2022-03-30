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
#import <Foundation/Foundation.h>
#import "PAGMarker.h"

typedef NS_ENUM(NSInteger, PAGLayerType) {
  PAGLayerTypeUnknown,
  PAGLayerTypeNull,
  PAGLayerTypeSolid,
  PAGLayerTypeText,
  PAGLayerTypeShape,
  PAGLayerTypeImage,
  PAGLayerTypePreCompose,
};

@class PAGComposition;
@class PAGFile;

PAG_API @interface PAGLayer : NSObject

/**
 * Returns the type of layer.
 */
- (PAGLayerType)layerType;

/**
 * Returns the name of the layer.
 */
- (NSString*)layerName;

/**
 * A matrix object containing values that alter the scaling, rotation, and translation of the layer.
 * Altering it does not change the animation matrix, and it will be concatenated to current
 * animation matrix for displaying.
 */
- (CGAffineTransform)matrix;

- (void)setMatrix:(CGAffineTransform)matrix;

/**
 * Resets the matrix to its default value.
 */
- (void)resetMatrix;

/**
 * The final matrix for displaying, it is the combination of the matrix property and current matrix
 * from animation.
 */
- (CGAffineTransform)getTotalMatrix;

/**
 * Whether or not the layer is visible.
 */
- (BOOL)visible;

- (void)setVisible:(BOOL)visible;

/**
 * Ranges from 0 to PAGFile.numTexts - 1 if the layer type is text, or from 0 to PAGFile.numImages
 * -1 if the layer type is image, otherwise returns -1.
 */
- (NSInteger)editableIndex;

/**
 * Indicates the PAGComposition instance that contains this PAGLayer instance.
 */
- (PAGComposition*)parent;

/**
 * Returns the markers of current PAGLayer.
 */
- (NSArray<PAGMarker*>*)markers;

/**
 * Converts the time from the PAGLayer's (local) timeline to the PAGSurface (global) timeline. The
 * time is in microseconds.
 */
- (int64_t)localTimeToGlobal:(int64_t)localTime;

/**
 * Converts the time from the PAGSurface (global) to the PAGLayer's (local) timeline timeline. The
 * time is in microseconds.
 */
- (int64_t)globalToLocalTime:(int64_t)globalTime;

/**
 * The duration of the layer in microseconds, indicates the length of the visible range.
 */
- (int64_t)duration;

/**
 * Returns the frame rate of this layer.
 */
- (float)frameRate;

/**
 * The start time of the layer in microseconds, indicates the start position of the visible range.
 * It could be a negative value.
 */
- (int64_t)startTime;

/**
 * Set the start time of the layer, in microseconds.
 */
- (void)setStartTime:(int64_t)time;

/**
 * The current time of the layer in microseconds, the layer is invisible if currentTime is not in
 * the visible range (startTime <= currentTime < startTime + duration).
 */
- (int64_t)currentTime;

/**
 * Set the current time of the layer in microseconds.
 */
- (void)setCurrentTime:(int64_t)time;

/**
 * Returns the current progress of play position, the value is from 0.0 to 1.0.
 */
- (double)getProgress;

/**
 * Set the progress of play position, the value ranges from 0.0 to 1.0. A value of 0.0 represents
 * the frame at startTime. A value of 1.0 represents the frame at the end of duration.
 */
- (void)setProgress:(double)value;

/**
 * Returns trackMatte layer of this layer.
 */
- (PAGLayer*)trackMatteLayer;

/**
 * Returns a rectangle that defines the original area of the layer, which is not transformed by the
 * matrix.
 */
- (CGRect)getBounds;

/**
 * Indicate whether this layer is excluded from parent's timeline. If set to true, this layer's
 * current time will not change when parent's current time changes.
 */
- (BOOL)excludedFromTimeline;

/**
 * Set the excludedFromTimeline flag of this layer.
 */
- (void)setExcludedFromTimeline:(BOOL)value;

@end
