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

#import "PAGFileImpl.h"
#import "PAGDiskCacheImpl.h"
#import "PAGLayerImpl+Internal.h"
#import "PAGTextImpl.h"
#import "pag/pag.h"

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
  std::string filePath;
  if ([PAGFileImpl IsNetWorkPath:path]) {
    NSString* cachePath = [PAGDiskCacheImpl GetFilePath:path];
    if (cachePath == nil) {
      __block PAGFile* file;
      dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
      [[[NSURLSession sharedSession]
          dataTaskWithRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:path]]
            completionHandler:^(NSData* _Nullable data, NSURLResponse* _Nullable,
                                NSError* _Nullable error) {
              if (error == nil && data != nil) {
                [PAGDiskCacheImpl WritFile:path data:data];
                file = [PAGFile Load:data.bytes size:data.length];
              }
              dispatch_semaphore_signal(semaphore);
            }] resume];
      dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
      return file;
    } else {
      filePath = [cachePath UTF8String];
    }
  } else {
    filePath = [path UTF8String];
  }
  auto pagFile = pag::PAGFile::Load(filePath);
  if (pagFile == nullptr) {
    return nil;
  }
  return (PAGFile*)[PAGLayerImpl ToPAGLayer:pagFile];
}

+ (PAGFile*)Load:(const void*)bytes size:(size_t)length {
  auto pagFile = pag::PAGFile::Load(bytes, length);
  if (pagFile == nullptr) {
    return nil;
  }
  return (PAGFile*)[PAGLayerImpl ToPAGLayer:pagFile];
}

+ (void)Load:(NSString*)path completionBlock:(void (^)(PAGFile*))callback {
  if (path == nil) {
    callback(nil);
    return;
  }
  NSString* filePath = nil;
  if ([PAGFileImpl IsNetWorkPath:path]) {
    NSString* cachePath = [PAGDiskCacheImpl GetFilePath:path];
    if (cachePath != nil) {
      filePath = cachePath;
    }
  } else {
    filePath = path;
  }
  if (filePath != nil && filePath.length > 0) {
    NSOperationQueue* queue = [[[NSOperationQueue alloc] init] autorelease];
    [queue addOperationWithBlock:^{
      callback([PAGFileImpl Load:filePath]);
    }];
  } else {
    [[[NSURLSession sharedSession]
        dataTaskWithRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:path]]
          completionHandler:^(NSData* _Nullable data, NSURLResponse* _Nullable,
                              NSError* _Nullable error) {
            if (error == nil && data != nil) {
              [PAGDiskCacheImpl WritFile:path data:data];
              callback([PAGFileImpl Load:data.bytes size:data.length]);
            } else {
              callback(nil);
            }
          }] resume];
  }
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
  return pagFile->timeStretchMode();
}

- (void)seTimeStretchMode:(int)value {
  auto pagFile = std::static_pointer_cast<pag::PAGFile>(self.pagLayer);
  return pagFile->setTimeStretchMode(value);
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
