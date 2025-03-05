#ifndef WINDOWS_WINDOWSUPDATER_H_
#define WINDOWS_WINDOWSUPDATER_H_

#include <string>

class WindowsUpdater
{
public:
  static auto initUpdater(const wchar_t* version) -> void;
  static auto checkUpdates(bool showUI, std::string feedUrl) -> void;
};

#endif // WINDOWS_WINDOWSUPDATER_H_

