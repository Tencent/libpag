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

#import "PAGComposition.h"
#import "PAGImage.h"

__attribute__((visibility("default")))
@interface PAGMovie : PAGImage

/**
 *  Creates a PAGMovie object from a PAGComposition object, returns null if the composition is null.
 */
+ (PAGMovie*)FromComposition:(PAGComposition*)composition;

/**
 * Creates a PAGMovie object from a path of a video file, return null if the file does not exist or
 * it's not a valid video file.
 */
+ (PAGMovie*)FromVideoPath:(NSString*)filePath;

/**
 * Creates a PAGMovie object from a specified range of a video file, return null if the file does
 * not exist or it's not a valid video file.
 *
 * @param startTime start time of the movie in microseconds.
 * @param duration duration of the movie in microseconds.
 */
+ (PAGMovie*)FromVideoPath:(NSString*)filePath
                 startTime:(int64_t)startTime
                  duration:(int64_t)duration;

/**
 * Returns the duration of this movie in microseconds.
 */
- (int64_t)duration;

@end
