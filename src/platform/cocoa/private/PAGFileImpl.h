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
#import "PAGCompositionImpl.h"
#import "PAGFile.h"
#import "PAGImageImpl.h"
#import "PAGText.h"
#import "platform/cocoa/PAGFont.h"
#import "platform/cocoa/PAGText.h"

@class PAGComposition;

@interface PAGFileImpl : PAGCompositionImpl

+ (uint16_t)MaxSupportedTagLevel;

+ (PAGFile*)Load:(NSString*)path;

+ (PAGFile*)Load:(const void*)bytes size:(size_t)length;

- (uint16_t)tagLevel;

- (int)numTexts;

- (int)numImages;

- (int)numVideos;

- (NSString*)path;

- (PAGText*)getTextData:(int)index;

- (void)replaceText:(int)editableTextIndex data:(PAGText*)value;

- (void)replaceImage:(int)editableImageIndex data:(PAGImageImpl*)value;

- (NSArray<PAGLayer*>*)getLayersByEditableIndex:(int)index layerType:(PAGLayerType)type;

- (NSArray<NSNumber*>*)getEditableIndices:(PAGLayerType)layerType;

- (int)timeStretchMode;

- (void)seTimeStretchMode:(int)value;

- (void)setDuration:(int64_t)duration;

- (PAGFile*)copyOriginal;

@end
