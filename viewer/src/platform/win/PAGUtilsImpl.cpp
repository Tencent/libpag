#include "PAGUtilsImpl.h"
#include <shlobj_core.h>
#include <windows.h>
#include <QDir>

namespace pag::Utils {

auto openFileInFinder(QFileInfo& fileInfo) -> void {
  HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if (FAILED(hr)) {
    return;
  }

  QString folderPath = QDir::toNativeSeparators(fileInfo.absolutePath());
  QString filePath = QDir::toNativeSeparators(fileInfo.absoluteFilePath());

  std::wstring folderWPath = folderPath.toStdWString();
  std::wstring fileWPath = filePath.toStdWString();

  PIDLIST_ABSOLUTE pidlFolder(ILCreateFromPathW(folderWPath.c_str()));
  PIDLIST_ABSOLUTE pidlFile(ILCreateFromPathW(fileWPath.c_str()));

  if (!pidlFolder || !pidlFile) {
    if (pidlFolder) {
      ILFree(pidlFolder);
    }
    if (pidlFile) {
      ILFree(pidlFile);
    }
    CoUninitialize();
    return;
  }

  PCUITEMID_CHILD pidlChild = ILFindChild(pidlFolder, pidlFile);
  if (!pidlChild) {
    ILFree(pidlFolder);
    ILFree(pidlFile);
    CoUninitialize();
    return;
  }

  PCUITEMID_CHILD_ARRAY pidls = const_cast<PCUITEMID_CHILD_ARRAY>(&pidlChild);
  SHOpenFolderAndSelectItems(pidlFolder, 1, pidls, 0);

  ILFree(pidlFolder);
  ILFree(pidlFile);
  CoUninitialize();
}

}  // namespace pag::Utils