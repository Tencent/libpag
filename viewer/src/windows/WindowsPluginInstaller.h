#ifndef WINDOWS_WINDOWS_PLUGIN_INSTALLER_H_
#define WINDOWS_WINDOWS_PLUGIN_INSTALLER_H_

#include <string>

class WindowsPluginInstaller {
public:
  static auto HasUpdate() -> bool;
  static auto InstallPlugins(bool bForceInstall = false) -> bool;
  static auto UninstallPlugins() -> bool;
};

#endif // WINDOWS_WINDOWS_PLUGIN_INSTALLER_H_