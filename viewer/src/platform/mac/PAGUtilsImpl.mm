#include "PAGUtilsImpl.h"
#import <AppKit/AppKit.h>

namespace pag::Utils {

auto openFileInFinder(QFileInfo& fileInfo) -> void {
  if (!fileInfo.exists()) {
    return;
  }
  @autoreleasepool {
    NSString* filePath =
        [NSString stringWithUTF8String:fileInfo.absoluteFilePath().toUtf8().constData()];
    NSURL* fileUrl = [NSURL fileURLWithPath:filePath];
    if (!fileUrl) {
      return;
    }
    fileUrl = [fileUrl URLByStandardizingPath];
    [[NSWorkspace sharedWorkspace] selectFile:filePath
                     inFileViewerRootedAtPath:[[fileUrl URLByDeletingLastPathComponent] path]];
  }
}

}  // namespace pag::Utils