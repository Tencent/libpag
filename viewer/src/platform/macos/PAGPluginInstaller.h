#ifndef PLATFORM_MACOS_PAG_PLUGIN_INSTALLER_H_
#define PLATFORM_MACOS_PAG_PLUGIN_INSTALLER_H_

#include <string>

class PAGPluginInstaller {
public:
  static auto HasUpdate() -> bool;
  static auto copyFileByCmd(char* originPath, char* targetPath) -> int;
  static auto InstallPlugin(std::string pluginName) -> int;
  static auto InstallPlugins(bool bForceInstall) -> int;
  static auto UninstallPlugins() -> int;
};

#endif // PLATFORM_MACOS_PAG_PLUGIN_INSTALLER_H_
