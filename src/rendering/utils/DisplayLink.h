/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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

#pragma once

namespace pag {
/**
 * A high-priority thread that notifies your app when a given display will need each frame. You can
 * use a display link to easily synchronize with the refresh rate of a display.
 */
class DisplayLink {
 public:
  virtual ~DisplayLink() = default;

  /**
   * Starts the display link.
   */
  virtual void start() = 0;

  /**
   * Stops the display link.
   */
  virtual void stop() = 0;
};
}  // namespace pag
