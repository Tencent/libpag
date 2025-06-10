/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 THL A29 Limited, a Tencent company. All rights reserved.
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
#include "platform/PlatformHelper.h"
#import <Foundation/Foundation.h>
std::string GetRoamingPath() {
  NSArray* arr =
      NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
  NSString* path = [[NSString alloc] initWithFormat:@"%@/", arr[0]];
  auto ret = std::string([path UTF8String]);
  [path release];
  return ret;
}

std::string GetConfigPath() {
  auto path = GetRoamingPath() + "PAGExporter/";
  NSFileManager* fileManager = [NSFileManager defaultManager];
  NSString* nsPath = [NSString stringWithUTF8String:path.c_str()];
  if (![fileManager fileExistsAtPath:nsPath]) {
    [fileManager createDirectoryAtPath:nsPath
           withIntermediateDirectories:YES
                            attributes:nil
                                 error:nil];
  }
  return path;
}