/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#import <Foundation/Foundation.h>
#import <QuartzCore/QuartzCore.h>
#include <mutex>

@class PAGSurface;

namespace pag::cocoa {

/**
 * Stateless backend operations for PAGView, selected by CMake. The view owns the player, surface,
 * animator and mutex. These operations never trigger animator updates or retain the view.
 */
class __attribute__((visibility("hidden"))) PAGViewSurface {
 public:
  /**
   * Returns the backing-layer class, or Nil to use NSView's default backing layer.
   */
  static Class LayerClass();

  /**
   * Configures the view before creating its player and animation state.
   */
  static void Configure(id view);

  /**
   * Registers the view's callback for backend-specific surface-prepared notifications.
   */
  static void ObserveSurfacePrepared(id view, SEL callback);

  /**
   * Creates an autoreleased surface after configuring the native host and its drawable size.
   */
  static ::PAGSurface* CreateSurface(id view);

  /**
   * Updates the native drawable size and invalidates the surface's size-dependent caches.
   */
  static void UpdateSize(id view, ::PAGSurface* surface);

  /**
   * Updates drawable sizes after iOS window attachment or macOS backing changes, preserving the
   * selected backend's cache invalidation policy.
   */
  static void UpdateWindowSize(id view, ::PAGSurface* surface);

  /**
   * Locks the view-owned mutex for Metal operations, or returns an empty lock for OpenGL. Callers
   * release this lock before updating the animator because synchronous updates reenter rendering.
   */
  static std::unique_lock<std::mutex> Lock(std::mutex& mutex);
};

}  // namespace pag::cocoa
