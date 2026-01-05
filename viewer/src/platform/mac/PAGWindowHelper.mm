/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 Tencent. All rights reserved.
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

#include "PAGWindowHelper.h"
#include <Cocoa/Cocoa.h>

namespace pag {
PAGWindowHelper::PAGWindowHelper(QObject* parent) : QObject(parent) {
}

void PAGWindowHelper::setWindowStyle(QQuickWindow* quickWindow, double red, double green,
                                     double blue) {
  if (quickWindow != nullptr) {
    NSView* view = (NSView*)quickWindow->winId();
    NSWindow* window = [view window];
    if (@available(macOS 10.10, *)) {
      window.titleVisibility = NSWindowTitleHidden;
      window.styleMask |= NSWindowStyleMaskFullSizeContentView;
      window.titlebarAppearsTransparent = true;
      window.contentView.wantsLayer = true;
    }
    window.colorSpace = [NSColorSpace extendedSRGBColorSpace];
    window.backgroundColor = [NSColor colorWithRed:red green:green blue:blue alpha:1.];
  }
}

}  // namespace pag
