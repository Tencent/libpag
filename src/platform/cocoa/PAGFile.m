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

#import "PAGFile.h"
#import "PAGImageImpl.h"
#import "platform/cocoa/private/PAGFileImpl.h"
#import "platform/cocoa/private/PAGLayer+Internal.h"

@interface PAGImage ()

@property(nonatomic, strong) PAGImageImpl* image;

@end

@implementation PAGFile {
}

+ (uint16_t)MaxSupportedTagLevel {
  return [PAGFileImpl MaxSupportedTagLevel];
}

+ (PAGFile*)Load:(NSString*)path {
  return [PAGFileImpl Load:path];
}

+ (PAGFile*)Load:(const void*)bytes size:(size_t)length {
  return [PAGFileImpl Load:bytes size:length];
}

+ (void)LoadAsync:(NSString*)path completionBlock:(void (^)(PAGFile*))callback {
  [PAGFileImpl LoadAsync:path completionBlock:callback];
}

- (uint16_t)tagLevel {
  return [(PAGFileImpl*)self.impl tagLevel];
}

- (int)numTexts {
  return [(PAGFileImpl*)self.impl numTexts];
}

- (int)numImages {
  return [(PAGFileImpl*)self.impl numImages];
}

- (int)numVideos {
  return [(PAGFileImpl*)self.impl numVideos];
}

- (NSString*)path {
  return [(PAGFileImpl*)self.impl path];
}

- (PAGText*)getTextData:(int)index {
  return [(PAGFileImpl*)self.impl getTextData:index];
}

- (void)replaceText:(int)editableTextIndex data:(PAGText*)value {
  [(PAGFileImpl*)self.impl replaceText:editableTextIndex data:value];
}

- (void)replaceImage:(int)editableImageIndex data:(PAGImage*)value {
  if (value != nil) {
    [(PAGFileImpl*)self.impl replaceImage:editableImageIndex data:[value image]];
  } else {
    [(PAGFileImpl*)self.impl replaceImage:editableImageIndex data:nil];
  }
}

- (void)replaceImageByName:(NSString*)layerName data:(PAGImage*)value {
  if (value != nil) {
    [(PAGFileImpl*)self.impl replaceImageByName:layerName data:[value image]];
  } else {
    [(PAGFileImpl*)self.impl replaceImageByName:layerName data:nil];
  }
}

- (NSArray<PAGLayer*>*)getLayersByEditableIndex:(int)index layerType:(PAGLayerType)type {
  return [(PAGFileImpl*)self.impl getLayersByEditableIndex:index layerType:type];
}

- (NSArray<NSNumber*>*)getEditableIndices:(PAGLayerType)layerType {
  return [self.impl getEditableIndices:layerType];
}

- (PAGTimeStretchMode)timeStretchMode {
  return (PAGTimeStretchMode)[(PAGFileImpl*)self.impl timeStretchMode];
}

- (void)seTimeStretchMode:(PAGTimeStretchMode)value {
  [(PAGFileImpl*)self.impl seTimeStretchMode:value];
}

- (void)setDuration:(int64_t)duration {
  [(PAGFileImpl*)self.impl setDuration:duration];
}

- (PAGFile*)copyOriginal {
  return [(PAGFileImpl*)self.impl copyOriginal];
}

@end
