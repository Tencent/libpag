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

PAG_API @interface PAGComposition : PAGLayer

/**
 * Make a empty PAGComposition with specified size.
 */
+ (PAGComposition*)Make:(CGSize)size;

/**
 * Returns the width of the Composition.
 */
- (NSInteger)width;

/**
 * Returns the height of the Composition.
 */
- (NSInteger)height;

/**
 * Set the size of the Composition.
 */
- (void)setContentSize:(CGSize)size;

/**
 * Returns the number of child layers of this composition.
 */
- (NSInteger)numChildren;

/**
 * Returns the child layer that exists at the specified index.
 * @param index The index position of the child layer.
 * @returns The child layer at the specified index position.
 */
- (PAGLayer*)getLayerAt:(int)index;

/**
 * Returns the index position of a child layer.
 * @param child The layer instance to identify.
 * @returns The index position of the child layer to identify.
 */
- (NSInteger)getLayerIndex:(PAGLayer*)child;

/**
 * Changes the position of an existing child in the container. This affects the layering of child
 * layers.
 * @param child The child layer for which you want to change the index number.
 * @param index The resulting index number for the child layer.
 */
- (void)setLayerIndex:(NSInteger)index layer:(PAGLayer*)child;

/**
 Add PAGLayer to current PAGComposition at the top.
 @param pagLayer specified PAGLayer to add
 @return return YES if operation succeed, otherwise NO.
 */
- (BOOL)addLayer:(PAGLayer*)pagLayer;

/**
 Add PAGLayer to current PAGComposition at the specified index.
 @param pagLayer specified PAGLayer to add
 @param index specified index where to add
 @return return YES if operation succeed,else NO.
 */
- (BOOL)addLayer:(PAGLayer*)pagLayer atIndex:(int)index;

/**
 Check whether current PAGComposition cotains the specified pagLayer.
 @param pagLayer specified layer to check
 @return return YES if the current PAGComposition contains the paglayer,else NO.
 */
- (BOOL)contains:(PAGLayer*)pagLayer;

/**
 Remove the specified PAGLayer from current PAGComposition.
 @param pagLayer specified PAGLayer to remove
 @return return the PAGLayer if remove successfully,else nil.
 */
- (PAGLayer*)removeLayer:(PAGLayer*)pagLayer;

/**
 Remove the PAGLayer at specified index from current PAGComposition.

 @param index specified index to remove
 @return return the PAGLayer if remove successfully,else nil.
 */
- (PAGLayer*)removeLayerAt:(int)index;

/**
 Remove all PAGLayers from current PAGComposition.
 */
- (void)removeAllLayers;

/**
 Swap the layers.
 @param pagLayer1 specified PAGLayer 1 to swap
 @param pagLayer2 specified PAGLayer 2 to swap
 */
- (void)swapLayer:(PAGLayer*)pagLayer1 withLayer:(PAGLayer*)pagLayer2;

/**
 Swap the layers at the specified index.
 @param index1 specified index 1 to swap
 @param index2 specified index 2 to swap
 */
- (void)swapLayerAt:(int)index1 withIndex:(int)index2;

/**
 * Returns the audio data of this composition, which is an AAC audio in an MPEG-4 container.
 */
- (NSData*)audioBytes;

/**
 * Returns when the first frame of the audio plays in the composition's timeline.
 */
- (int64_t)audioStartTime;

/**
 * Returns the audio Markers of the composition.
 */
- (NSArray<PAGMarker*>*)audioMarkers;

/**
 * Returns an array of layers that match the specified layer name.
 */
- (NSArray<PAGLayer*>*)getLayersByName:(NSString*)layerName;

/**
 * Returns an array of layers that lie under the specified point. The point is in pixels and from
 * this PAGComposition's local coordinates.
 */
- (NSArray<PAGLayer*>*)getLayersUnderPoint:(CGPoint)point;

@end
