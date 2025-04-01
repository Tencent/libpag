#include "PAGUtilsImpl.h"
#include <shlobj_core.h>
#include <windows.h>

namespace pag::Utils {

auto openFileInFinder(QFileInfo& fileInfo) -> void {
  HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if (FAILED(hr)) {
    return;
  }

  bool success = false;
  PIDLIST_ABSOLUTE pidlFolder = ILCreateFromPathW(fileInfo.absolutePath().toStdWString().c_str());
  PIDLIST_ABSOLUTE pidlFile = ILCreateFromPathW(fileInfo.absoluteFilePath().toStdWString().c_str());
  if (pidlFolder && pidlFile) {
    PCUITEMID_CHILD pidlChild = ILFindChild(pidlFolder, pidlFile);
    if (pidlChild) {
      PCUITEMID_CHILD_ARRAY pidls = const_cast<PCUITEMID_CHILD_ARRAY>(&pidlChild);
      hr = SHOpenFolderAndSelectItems(pidlFolder, 1, pidls, 0);
      success = SUCCEEDED(hr);
    }
  }

  if (pidlFolder) {
    ILFree(pidlFolder);
  }
  if (pidlFile) {
    ILFree(pidlFile);
  }
  CoUninitialize();

  return;
}

}  // namespace pag::Utils