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
#import "PAG.h"

/**
 * Represents a time range from the content of PAGImageLayer.
 */
PAG_API @interface PAGVideoRange : NSObject

/**
 * The start time of the source video, in microseconds.
 */
@property(nonatomic) int64_t startTime;
/**
 * The end time of the source video (not included), in microseconds.
 */
@property(nonatomic) int64_t endTime;
/**
 * The duration for playing after applying speed.
 */
@property(nonatomic) int64_t playDuration;
/**
 * Indicates whether the video should play backward.
 */
@property(nonatomic) int64_t reversed;

@end
