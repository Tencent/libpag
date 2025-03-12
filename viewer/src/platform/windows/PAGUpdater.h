#ifndef PLATFORM_WINDOWS_PAG_UPDATER_H_
#define PLATFORM_WINDOWS_PAG_UPDATER_H_

#include <string>

class PAGUpdater
{
public:
  static auto initUpdater(const wchar_t* version) -> void;
  static auto checkUpdates(bool showUI, std::string feedUrl) -> void;
};

#endif // PLATFORM_WINDOWS_PAG_UPDATER_H_

