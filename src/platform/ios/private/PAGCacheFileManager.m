/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

#import "PAGCacheFileManager.h"

// The maximum default storage of the Disk: 2GB
static const long long defaultMaxDiskSize = 1 * 1024 * 1024 * 1024;
// When the disk cache exceeds the maximum limit, clean up to remainingRate * _maxDiskSize.
static const float remainingRate = 0.6;

@implementation PAGCacheFileManager {
  NSString* _diskCachePath;
  long long _maxDiskSize;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _maxDiskSize = defaultMaxDiskSize;
    _diskCachePath = [self cacheDirPath];
    if (_diskCachePath) {
      [_diskCachePath retain];
    }
  }
  return self;
}

- (void)dealloc {
  if (_diskCachePath) {
    [_diskCachePath release];
  }
  [super dealloc];
}

#pragma mark - public
+ (instancetype)shareInstance {
  static PAGCacheFileManager* fileManager;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    fileManager = [[PAGCacheFileManager alloc] init];
  });
  return fileManager;
}

- (NSString*)diskCachePath {
  return _diskCachePath ? [[_diskCachePath retain] autorelease] : nil;
}

- (void)SetMaxDiskSize:(long long)size {
  _maxDiskSize = size;
}

- (long long)MaxDiskSize {
  return _maxDiskSize;
}

- (long long)totalSize {
  if (!_diskCachePath) {
    return 0;
  }
  if (![[NSFileManager defaultManager] fileExistsAtPath:_diskCachePath]) {
    return 0;
  }
  NSArray* allFilePath = [self getAllFiles:_diskCachePath];
  long long resultSize = 0;
  for (NSString* path in allFilePath) {
    resultSize += [self getFileSize:path];
  }

  return resultSize;
}

- (void)removeAllFiles {
  if (!_diskCachePath) {
    return;
  }
  NSArray* allFiles = [self getAllFiles:_diskCachePath];
  for (NSString* file in allFiles) {
    NSError* err = nil;
    [[NSFileManager defaultManager] removeItemAtPath:file error:&err];
  }
}

- (void)removeFilesToLimit {
  long long totalSize = [self totalSize];
  if (totalSize < _maxDiskSize) {
    return;
  }
  NSArray* allSortedFiles = [self getAllSortedFiles];
  long long remainingSize = _maxDiskSize * remainingRate;
  for (NSString* filePath in allSortedFiles) {
    NSInteger fileSize = [self getFileSize:filePath];
    if ([[NSFileManager defaultManager] removeItemAtPath:filePath error:nil]) {
      totalSize -= fileSize;
      if (totalSize <= remainingSize) {
        break;
      }
    }
  }
}

- (void)removeFileForPath:(NSString*)path {
  [[NSFileManager defaultManager] removeItemAtPath:path error:nil];
}

#pragma mark - private

- (NSString*)cacheDirPath {
  NSString* cachePath =
      [NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES) firstObject];
  cachePath = [cachePath stringByAppendingPathComponent:@"PAGImageViewCache"];
  BOOL isDir = NO;
  BOOL isExist = [[NSFileManager defaultManager] fileExistsAtPath:cachePath isDirectory:&isDir];
  if (!isExist || !isDir) {
    BOOL success = [[NSFileManager defaultManager] createDirectoryAtPath:cachePath
                                             withIntermediateDirectories:YES
                                                              attributes:nil
                                                                   error:nil];
    if (!success) {
      return nil;
    }
  }
  return cachePath;
}

- (NSArray*)getAllFiles:(NSString*)folderPath {
  if (!folderPath) {
    return nil;
  }
  NSMutableArray* allFiles = [[NSMutableArray new] autorelease];
  NSFileManager* fileManager = [NSFileManager defaultManager];
  BOOL isDir = NO;
  BOOL isExist = [fileManager fileExistsAtPath:folderPath isDirectory:&isDir];
  if (!isExist || !isDir) {
    return allFiles;
  }
  NSArray* dirctorysArray = [fileManager contentsOfDirectoryAtPath:folderPath error:nil];
  for (NSString* name in dirctorysArray) {
    NSString* subPath = [folderPath stringByAppendingPathComponent:name];
    BOOL isSubDir = NO;
    [fileManager fileExistsAtPath:subPath isDirectory:&isSubDir];
    if (isSubDir) {
      [allFiles addObjectsFromArray:[self getAllFiles:subPath]];
    } else {
      [allFiles addObject:subPath];
    }
  }
  return allFiles;
}

- (NSArray*)getAllSortedFiles {
  if (!_diskCachePath) {
    return nil;
  }
  NSArray* allFiles = [self getAllFiles:_diskCachePath];
  if (!allFiles) {
    return nil;
  }
  NSArray* allSortedFiles =
      [allFiles sortedArrayUsingComparator:^NSComparisonResult(id _Nonnull obj1, id _Nonnull obj2) {
        NSDate* data1 = [self getModificationDateOfFile:(NSString*)obj1];
        NSDate* data2 = [self getModificationDateOfFile:(NSString*)obj2];
        return [data1 compare:data2];
      }];
  return allSortedFiles;
}

- (NSDate*)getModificationDateOfFile:(NSString*)filePath {
  NSFileManager* fileManager = [NSFileManager defaultManager];
  if (![fileManager fileExistsAtPath:filePath]) {
    return nil;
  }
  NSDictionary* fileAttributes = [fileManager attributesOfItemAtPath:filePath error:nil];
  NSDate* fileDate = fileAttributes[NSFileModificationDate];
  return fileDate;
}

- (long long)getFileSize:(NSString*)filePath {
  NSFileManager* fileManager = [NSFileManager defaultManager];
  if (![fileManager fileExistsAtPath:filePath]) {
    return 0;
  }
  NSDictionary* fileAttributes = [fileManager attributesOfItemAtPath:filePath error:nil];
  return [fileAttributes[NSFileSize] longLongValue];
}

@end
