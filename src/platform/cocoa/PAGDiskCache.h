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
#import "PAG.h"

/**
 * Defines methods to manage the disk cache capabilities.
 */
PAG_API @interface PAGDiskCache : NSObject

/**
 * Sets the disk cache directory. This should be called before any disk cache operations.
 * If the directory does not exist, it will be created automatically.
 * Note: Changing the cache directory after cache operations have started may cause
 * existing cached files to become inaccessible.
 * @param dir The absolute path of the cache directory. Pass nil or empty string to use the
 * platform default cache directory.
 */
+ (void)SetCacheDir:(NSString*)dir;

/**
 * Returns the size limit of the disk cache in bytes. The default value is 1 GB.
 */
+ (size_t)MaxDiskSize;

/**
 * Sets the size limit of the disk cache in bytes, which will triggers the cache cleanup if the
 * disk usage exceeds the limit. The opened files are not removed immediately, even if their disk
 * usage exceeds the limit, and they will be rechecked after they are closed.
 */
+ (void)SetMaxDiskSize:(size_t)size;

/**
 * Removes all cached files from the disk. All the opened files will be also removed after they
 * are closed.
 */
+ (void)RemoveAll;

@end
