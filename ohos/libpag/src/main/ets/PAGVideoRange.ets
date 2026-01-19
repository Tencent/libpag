/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2024 Tencent. All rights reserved.
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

/**
 * Represents a time range from the content of PAGImageLayer.
 */
export interface PAGVideoRange {
  /**
   * The start time of the source video, in microseconds.
   */
  startTime: number;

  /**
   * The end time of the source video (not included), in microseconds.
   */
  endTime: number;

  /**
   * The duration for playing after applying speed.
   */
  playDuration: number;

  /**
   * Indicates whether the video should play backward.
   */
  reversed: boolean;
}
