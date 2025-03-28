#include "PAGWindowHelper.h"
#include <Cocoa/Cocoa.h>

namespace pag {
PAGWindowHelper::PAGWindowHelper(QObject* parent) : QObject(parent) {
}

auto PAGWindowHelper::setWindowStyle(QQuickWindow* quickWindow, double red, double green,
                                     double blue) -> void {
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