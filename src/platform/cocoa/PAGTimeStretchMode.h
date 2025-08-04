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

typedef enum {
  // Keep the original playing speed, and display the last frame if the content's duration is less
  // than target duration.
  PAGTimeStretchModeNone = 0,
  // Change the playing speed of the content to fit target duration.
  PAGTimeStretchModeScale = 1,
  // Keep the original playing speed, but repeat the content if the content's duration is less than
  // target duration.
  // This is the default mode.
  PAGTimeStretchModeRepeat = 2,
  // Keep the original playing speed, but repeat the content in reversed if the content's duration
  // is less than
  // target duration.
  PAGTimeStretchModeRepeatInverted = 3,
} PAGTimeStretchMode;
