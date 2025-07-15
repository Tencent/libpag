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

#import "PAGFileImpl.h"

#include "pag/pag.h"
#include "tgfx/core/Task.h"

#import "PAGDiskCacheImpl.h"
#import "PAGLayerImpl+Internal.h"
#import "PAGTextImpl.h"

@interface PAGImageImpl ()

@property(nonatomic) std::shared_ptr<pag::PAGImage> pagImage;

@end

@implementation PAGFileImpl {
}

+ (uint16_t)MaxSupportedTagLevel {
  return pag::PAGFile::MaxSupportedTagLevel();
}

+ (BOOL)IsNetWorkPath:(NSString*)path {
  if (path == nil) {
    return NO;
  }
  NSError* error = nil;
  NSRegularExpression* regex =
      [NSRegularExpression regularExpressionWithPattern:@"^(http|https|ftp)://.*$"
                                                options:NSRegularExpressionCaseInsensitive
                                                  error:&error];
  if (!error) {
    NSRange range = [regex rangeOfFirstMatchInString:path
                                             options:0
                                               range:NSMakeRange(0, path.length)];
    if (range.location != NSNotFound) {
      return YES;
    }
  }
  return NO;
}

+ (PAGFile*)Load:(NSString*)path {
  if (path == nil) {
    return nil;
  }
  if ([PAGFileImpl IsNetWorkPath:path]) {
    NSData* cacheData = [PAGDiskCacheImpl ReadFile:path];
    if (cacheData == nil) {
      NSError* error = nil;
      cacheData = [NSData dataWithContentsOfURL:[NSURL URLWithString:path]
                                        options:NSDataReadingUncached
                                          error:&error];
      if (error == nil && cacheData != nil) {
        [PAGDiskCacheImpl WritFile:path data:cacheData];
      }
    }
    return [PAGFileImpl Load:cacheData.bytes size:cacheData.length path:path];
  }
  auto pagFile = pag::PAGFile::Load([path UTF8String]);
  if (pagFile == nullptr) {
    return nil;
  }
  return (PAGFile*)[PAGLayerImpl ToPAGLayer:pagFile];
}

+ (PAGFile*)Load:(const void*)bytes size:(size_t)length {
  return [PAGFileImpl Load:bytes size:length path:@""];
}

+ (PAGFile*)Load:(const void*)bytes size:(size_t)length path:(NSString*)filePath {
  if (bytes == nil || length == 0) {
    return nil;
  }
  auto pagFile = pag::PAGFile::Load(bytes, length, [filePath UTF8String]);
  if (pagFile == nullptr) {
    return nil;
  }
  return (PAGFile*)[PAGLayerImpl ToPAGLayer:pagFile];
}

+ (void)LoadAsync:(NSString*)path completionBlock:(void (^)(PAGFile*))callback {
  if (path == nil) {
    callback(nil);
    return;
  }
  void (^copyCallback)(PAGFile*) = Block_copy(callback);
  [path retain];
  tgfx::Task::Run([callBack = copyCallback, path]() {
    PAGFile* file = [PAGFileImpl Load:path];
    [path release];
    callBack(file);
    Block_release(callBack);
  });
}

- (uint16_t)tagLevel {
  return std::static_pointer_cast<pag::PAGFile>(self.pagLayer)->tagLevel();
}

- (int)numTexts {
  return std::static_pointer_cast<pag::PAGFile>(self.pagLayer)->numTexts();
}

- (int)numImages {
  return std::static_pointer_cast<pag::PAGFile>(self.pagLayer)->numImages();
}

- (int)numVideos {
  return std::static_pointer_cast<pag::PAGFile>(self.pagLayer)->numVideos();
}

- (NSString*)path {
  auto path = std::static_pointer_cast<pag::PAGFile>(self.pagLayer)->path();
  if (path.empty()) {
    return nil;
  }
  return [NSString stringWithUTF8String:path.c_str()];
}

- (PAGText*)getTextData:(int)index {
  auto textDocument = std::static_pointer_cast<pag::PAGFile>(self.pagLayer)->getTextData(index);
  return ToPAGText(textDocument);
}

- (void)replaceText:(int)editableTextIndex data:(PAGText*)value {
  auto textDocument = ToTextDocument(value);
  auto pagFile = std::static_pointer_cast<pag::PAGFile>(self.pagLayer);
  return pagFile->replaceText(editableTextIndex, textDocument);
}

- (void)replaceImage:(int)editableImageIndex data:(PAGImageImpl*)value {
  auto pagFile = std::static_pointer_cast<pag::PAGFile>(self.pagLayer);
  if (value != nil) {
    pagFile->replaceImage(editableImageIndex, value.pagImage);
  } else {
    pagFile->replaceImage(editableImageIndex, nullptr);
  }
}

- (void)replaceImageByName:(NSString*)layerName data:(PAGImageImpl*)value {
  if (layerName == nil || layerName.length == 0) {
    return;
  }
  std::string name = layerName.UTF8String;
  auto pagFile = std::static_pointer_cast<pag::PAGFile>(self.pagLayer);
  if (value != nil) {
    pagFile->replaceImageByName(name, value.pagImage);
  } else {
    pagFile->replaceImageByName(name, nullptr);
  }
}

- (NSArray<PAGLayer*>*)getLayersByEditableIndex:(int)index layerType:(PAGLayerType)type {
  auto pagFile = std::static_pointer_cast<pag::PAGFile>(self.pagLayer);
  auto layerVector = pagFile->getLayersByEditableIndex(index, static_cast<pag::LayerType>(type));
  return [PAGLayerImpl BatchConvertToPAGLayers:layerVector];
}

- (NSArray<NSNumber*>*)getEditableIndices:(PAGLayerType)type {
  auto pagFile = std::static_pointer_cast<pag::PAGFile>(self.pagLayer);
  auto indices = pagFile->getEditableIndices(static_cast<pag::LayerType>(type));
  NSMutableArray<NSNumber*>* result = [[NSMutableArray new] autorelease];
  for (size_t i = 0; i < indices.size(); ++i) {
    [result addObject:[NSNumber numberWithInt:indices[i]]];
  }
  return result;
}

- (int)timeStretchMode {
  auto pagFile = std::static_pointer_cast<pag::PAGFile>(self.pagLayer);
  return static_cast<int>(pagFile->timeStretchMode());
}

- (void)seTimeStretchMode:(int)value {
  auto pagFile = std::static_pointer_cast<pag::PAGFile>(self.pagLayer);
  return pagFile->setTimeStretchMode(static_cast<pag::PAGTimeStretchMode>(value));
}

- (void)setDuration:(int64_t)duration {
  auto pagFile = std::static_pointer_cast<pag::PAGFile>(self.pagLayer);
  return pagFile->setDuration(duration);
}

- (PAGFile*)copyOriginal {
  auto pagFile = std::static_pointer_cast<pag::PAGFile>(self.pagLayer);
  auto newFile = pagFile->copyOriginal();
  if (newFile == nullptr) {
    return nil;
  }
  return [(PAGFile*)[PAGLayerImpl ToPAGLayer:newFile] retain];
}
@end
