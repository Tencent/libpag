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
#import "PAGFont.h"
#import "PAGImage.h"
#import "PAGText.h"
#import "PAGTimeStretchMode.h"

PAG_API @interface PAGFile : PAGComposition
/**
 * The maximum tag level current SDK supports.
 */
+ (uint16_t)MaxSupportedTagLevel;

/***
 * Load a pag file from the specified path, return null if the file does not exist or the data is
 * not a pag file. Note: All PAGFiles loaded by the same path share the same internal cache. The
 * internal cache is alive until all PAGFiles are released. Use '[PAGFile Load:size:]' instead if
 * you don't want to load a PAGFile from the intenal caches.
 */
+ (PAGFile*)Load:(NSString*)path;

/**
 *  Load a pag file from byte data, return null if the bytes is empty or it's not a valid pag file.
 */
+ (PAGFile*)Load:(const void*)bytes size:(size_t)length;

/**
 * The tag level this pag file requires.
 */
- (uint16_t)tagLevel;

/**
 * The number of editable texts.
 */
- (int)numTexts;

/**
 * The number of replaceable images.
 */
- (int)numImages;

/**
 * The number of video compositions.
 */
- (int)numVideos;

/**
 * The path string of this file, returns empty string if the file is loaded from byte stream.
 */
- (NSString*)path;

/**
 * Get a text data of the specified index. The index ranges from 0 to numTexts - 1.
 * Note: It always returns the default text data.
 */
- (PAGText*)getTextData:(int)editableTextIndex;

/**
 * Replace the text data of the specified index. The index ranges from 0 to PAGFile.numTexts - 1.
 * Passing in null for the textData parameter will reset it to default text data.
 */
- (void)replaceText:(int)editableTextIndex data:(PAGText*)value;

/**
 * Replace the image data of the specified index. The index ranges from 0 to PAGFile.numImages - 1.
 * Passing in null for the image parameter will reset it to default image data.
 */
- (void)replaceImage:(int)editableImageIndex data:(PAGImage*)value;

/**
 * Return an array of layers by specified editable index and layer type.
 */
- (NSArray<PAGLayer*>*)getLayersByEditableIndex:(int)index layerType:(PAGLayerType)type;

/**
 * Returns the indices of the editable layers in this PAGFile.
 * If the editableIndex of a PAGLayer is not present in the returned indices, the PAGLayer should
 * not be treated as editable.
 */
- (NSArray<NSNumber*>*)getEditableIndices:(PAGLayerType)layerType;

/**
 * Indicate how to stretch the original duration to fit target duration when file's duration is
 * changed. The default value is PAGTimeStretchMode::Repeat.
 */
- (PAGTimeStretchMode)timeStretchMode;

/**
 * Set the timeStretchMode of this file.
 */
- (void)seTimeStretchMode:(PAGTimeStretchMode)value;

/**
 * Set the duration of this PAGFile. Passing a value less than or equal to 0 resets the duration to
 * its default value.
 */
- (void)setDuration:(int64_t)duration;

/**
 * Make a copy of the original file, any modification to current file has no effect on the result
 * file.
 */
- (PAGFile*)copyOriginal;

@end
