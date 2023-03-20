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

#import "PAGDiskCache.h"

#include <compression.h>
#include <sys/stat.h>
#import "PAGCacheManager.h"
#import "PAGCacheQueueManager.h"

/**
 * structure of the cache file
 * | width | height | cacheFrames |  maxCacheFrameSize | data range | ... | data range
 * | frame data |  ... | frame data |
 */

@implementation PAGDiskCache {
  dispatch_queue_t ioQueue;
  dispatch_queue_t cacheQueue;
  NSString* _path;
  int _fd;
  NSInteger numFrames;
  int32_t cacheFrameCount;
  int32_t maxCacheEncodedBufferSize;
  void* scratchEncodeBuffer;
  NSInteger scratchEncodeLength;
  void* scratchDecodeBuffer;
  NSInteger scratchDecodeLength;

  uint8_t* encodeBuffer;
  NSInteger encodeLength;
  uint8_t* decodeBuffer;
  NSInteger decodeLength;
}

static const int32_t FileHeaderSize = 4 * sizeof(int32_t);

- (instancetype)initWithPath:(NSString*)filePath
                          fd:(int)fd
                       queue:(dispatch_queue_t)queue
                  frameCount:(NSUInteger)frameCount {
  self = [super init];
  if (self) {
    _path = [filePath retain];
    numFrames = frameCount;
    _fd = fd;
    scratchEncodeLength = compression_encode_scratch_buffer_size(COMPRESSION_LZ4);
    scratchDecodeLength = compression_decode_scratch_buffer_size(COMPRESSION_LZ4);
    cacheQueue = dispatch_queue_create("PAGDiskFileCache.art.pag", DISPATCH_QUEUE_SERIAL);
    ioQueue = queue;
    encodeLength = 0;
    decodeLength = 0;
    dispatch_retain(ioQueue);
    __block __typeof(self) weakSelf = self;
    dispatch_sync(ioQueue, ^{
      [weakSelf initializeCacheFile];
    });
  }
  return self;
}

- (void)dealloc {
  [_path release];
  dispatch_release(ioQueue);
  dispatch_release(cacheQueue);
  if (scratchEncodeBuffer) {
    free(scratchEncodeBuffer);
  }
  if (scratchDecodeBuffer) {
    free(scratchDecodeBuffer);
  }
  if (encodeBuffer) {
    free(encodeBuffer);
  }
  if (decodeBuffer) {
    free(decodeBuffer);
  }
  close(_fd);
  [super dealloc];
}

#pragma mark - public
+ (instancetype)MakeWithName:(NSString*)name frameCount:(NSUInteger)frameCount {
  if (name.length == 0) {
    return nil;
  }
  NSString* filePath = [[PAGCacheManager shareInstance] diskCachePath];
  if (filePath.length == 0) {
    return nil;
  }
  filePath = [filePath stringByAppendingPathComponent:[name stringByAppendingString:@".cache"]];
  dispatch_queue_t queue = [[PAGCacheQueueManager shareInstance] queueForPath:filePath];
  __block int fd = 0;
  dispatch_sync(queue, ^{
    fd = open([filePath UTF8String], O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd < 0) {
      fd = open([filePath UTF8String], O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    }
  });
  if (fd < 0) {
    return nil;
  }
  return [[[PAGDiskCache alloc] initWithPath:filePath
                                          fd:fd
                                       queue:(dispatch_queue_t)queue
                                  frameCount:frameCount] autorelease];
}

- (BOOL)containsObjectForKey:(NSInteger)index {
  if (index < 0 || index >= numFrames) {
    return NO;
  }
  NSRange frameRange = [self frameRangeAtIndex:index];
  if (frameRange.location == NSNotFound || frameRange.length == NSNotFound ||
      frameRange.length == 0) {
    return NO;
  }
  return YES;
}

- (BOOL)objectForKey:(NSInteger)index to:(uint8_t*)pixels length:(NSInteger)length {
  if (decodeLength == 0) {
    decodeLength = length;
  }
  NSData* compressData = [self readObjectForKey:index];
  if (compressData) {
    return [self deCompressData:pixels
                      dstLength:length
                      srcBuffer:(uint8_t*)compressData.bytes
                      srcLength:compressData.length];
  }
  return NO;
}

- (void)setObject:(uint8_t*)pixels length:(NSInteger)length forKey:(NSInteger)index {
  encodeLength = length;
  NSData* compressData = [self compressRGBAData:pixels length:length];
  if (compressData) {
    __block __typeof(self) weakSelf = self;
    dispatch_sync(ioQueue, ^{
      [weakSelf saveObject:compressData forKey:index];
    });
  }
}

- (NSInteger)count {
  return cacheFrameCount;
}

- (NSInteger)maxEncodedBufferSize {
  return maxCacheEncodedBufferSize;
}

- (void)removeCachesWithBlock:(void (^_Nullable)(void))block {
  __block __typeof(self) weakSelf = self;
  dispatch_async(cacheQueue, ^{
    [weakSelf removeCaches];
    block();
  });
}

- (void)removeCaches {
  __block __typeof(self) weakSelf = self;
  dispatch_sync(ioQueue, ^{
    close(weakSelf->_fd);
    [[PAGCacheManager shareInstance] removeFileForPath:weakSelf->_path];
    weakSelf->_fd = open([weakSelf->_path UTF8String], O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  });
}

#pragma mark - private
- (void)initializeCacheFile {
  if ([self fileSize] < (NSInteger)(FileHeaderSize + self->numFrames * 2 * sizeof(int32_t))) {
    ftruncate(self->_fd, 0);
    int32_t zero = 0;
    write(self->_fd, &zero, sizeof(int32_t));  // width
    write(self->_fd, &zero, sizeof(int32_t));  // height
    write(self->_fd, &zero, sizeof(int32_t));  // cacheFrames
    write(self->_fd, &zero, sizeof(int32_t));  // maxCacheFrameSize
    for (uint i = 0; i < self->numFrames; i++) {
      write(self->_fd, &zero, sizeof(int32_t));
      write(self->_fd, &zero, sizeof(int32_t));
    }
    self->cacheFrameCount = 0;
    self->maxCacheEncodedBufferSize = 0;
  } else {
    lseek(self->_fd, 2 * sizeof(int32_t), SEEK_SET);
    read(self->_fd, &self->cacheFrameCount, sizeof(int32_t));
    read(self->_fd, &self->maxCacheEncodedBufferSize, sizeof(int32_t));
  }
}

- (NSInteger)fileSize {
  struct stat statbuf;
  fstat(self->_fd, &statbuf);
  NSInteger size = statbuf.st_size;
  return size;
}

- (NSRange)frameRangeAtIndex:(NSInteger)index {
  NSRange notFoundRange = NSMakeRange(NSNotFound, NSNotFound);
  if (index < 0 || index >= numFrames) {
    return notFoundRange;
  }
  lseek(_fd, FileHeaderSize + index * 2 * sizeof(int32_t), SEEK_SET);
  int32_t offset = 0;
  int32_t length = 0;
  if (read(_fd, &offset, sizeof(int32_t)) != sizeof(int32_t)) {
    return notFoundRange;
  }
  if (read(_fd, &length, sizeof(int32_t)) != sizeof(int32_t)) {
    return notFoundRange;
  }
  if (length <= 0 || offset < 0) {
    return notFoundRange;
  }

  return NSMakeRange(offset, length);
}

- (NSString*)path {
  return [[_path retain] autorelease];
}

- (NSData*)readObjectForKey:(NSInteger)index {
  if (index < 0 || index >= numFrames) {
    return nil;
  }
  NSRange frameRange = [self frameRangeAtIndex:index];
  if (frameRange.location == NSNotFound || frameRange.length == NSNotFound) {
    return nil;
  }
  off_t offsset = lseek(_fd, frameRange.location, SEEK_SET);
  if (offsset <= 0) {
    return nil;
  }
  uint8_t* buffer = [self getDecoderBuffer];
  if (buffer == nil) {
    return nil;
  }
  ssize_t dataSize = read(_fd, buffer, frameRange.length);
  if (dataSize <= 0) {
    return nil;
  }
  return [[[NSData alloc] initWithBytesNoCopy:buffer length:frameRange.length
                                 freeWhenDone:NO] autorelease];
}

- (void)saveObject:(NSData*)object forKey:(NSInteger)index {
  if (!object || [self containsObjectForKey:index]) {
    return;
  }
  NSRange frameRange = [self frameRangeAtIndex:index];
  if ((frameRange.location != NSNotFound) && (frameRange.length != NSNotFound)) {
    return;
  }
  NSInteger currentSize = [self fileSize];
  lseek(_fd, FileHeaderSize + index * 2 * sizeof(int32_t), SEEK_SET);
  int32_t offset = (int32_t)currentSize;
  int32_t length = (int32_t)object.length;
  write(_fd, &offset, sizeof(int32_t));
  write(_fd, &length, sizeof(int32_t));
  lseek(_fd, currentSize, SEEK_SET);
  write(_fd, object.bytes, length);

  cacheFrameCount++;
  lseek(_fd, 2 * sizeof(int32_t), SEEK_SET);
  write(_fd, &cacheFrameCount, sizeof(int32_t));
  if (length > maxCacheEncodedBufferSize) {
    maxCacheEncodedBufferSize = length;
    write(_fd, &maxCacheEncodedBufferSize, sizeof(int32_t));
  }
}

- (NSData*)compressRGBAData:(uint8_t*)rgbaData length:(NSInteger)length {
  if (!rgbaData || length <= 0) {
    return nil;
  }
  uint8_t* buffer = [self getEncodeBuffer];
  if (buffer == nil) {
    return nil;
  }
  size_t resultSize = compression_encode_buffer(buffer, length, rgbaData, length,
                                                [self getScratchEncodeBuffer], COMPRESSION_LZ4);
  NSData* compressData = nil;
  if (resultSize > 0) {
    compressData = [NSData dataWithBytesNoCopy:buffer length:resultSize freeWhenDone:NO];
  }
  return compressData;
}

- (BOOL)deCompressData:(uint8_t*)dstBuffer
             dstLength:(NSInteger)dstLength
             srcBuffer:(uint8_t*)srcBuffer
             srcLength:(NSInteger)srcLength {
  if (dstLength <= 0) {
    return NO;
  }
  ssize_t resultLength = compression_decode_buffer(dstBuffer, dstLength, srcBuffer, srcLength,
                                                   [self getScratchDecodeBuffer], COMPRESSION_LZ4);
  if (resultLength > 0) {
    return YES;
  }
  return NO;
}

- (void*)getScratchEncodeBuffer {
  if (scratchEncodeBuffer == nil) {
    scratchEncodeBuffer = (void*)malloc(scratchEncodeLength);
  }
  return scratchEncodeBuffer;
}

- (void*)getScratchDecodeBuffer {
  if (scratchDecodeBuffer == nil) {
    scratchDecodeBuffer = (void*)malloc(scratchDecodeLength);
  }
  return scratchDecodeBuffer;
}

- (uint8_t*)getEncodeBuffer {
  if (encodeBuffer == nil && encodeLength > 0) {
    encodeBuffer = (uint8_t*)malloc(encodeLength);
  }
  return encodeBuffer;
}

- (uint8_t*)getDecoderBuffer {
  if (decodeBuffer == nil) {
    if ([self count] == numFrames) {
      decodeLength = [self maxEncodedBufferSize];
    }
    decodeBuffer = (uint8_t*)malloc(decodeLength);
  }
  return decodeBuffer;
}

@end
