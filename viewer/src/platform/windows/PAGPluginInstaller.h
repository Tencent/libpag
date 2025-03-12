#ifndef PLATFORM_WINDOWS_PAG_PLUGIN_INSTALLER_H_
#define PLATFORM_WINDOWS_PAG_PLUGIN_INSTALLER_H_

#include <string>

class PAGPluginInstaller {
public:
  static auto HasUpdate() -> bool;
  static auto InstallPlugins(bool bForceInstall = false) -> bool;
  static auto UninstallPlugins() -> bool;
};

#endif // PLATFORM_WINDOWS_PAG_PLUGIN_INSTALLER_H_