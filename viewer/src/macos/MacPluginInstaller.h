#ifndef MAC_MAC_PLUGIN_INSTALLER_H
#define MAC_MAC_PLUGIN_INSTALLER_H

#include <string>

class MacPluginInstaller {
public:
  static auto HasUpdate() -> bool;
  static auto copyFileByCmd(char* originPath, char* targetPath) -> int;
  static auto InstallPlugin(std::string pluginName) -> int;
  static auto InstallPlugins(bool bForceInstall) -> int;
  static auto UninstallPlugins() -> int;
};

#endif // MAC_MAC_PLUGIN_INSTALLER_H
