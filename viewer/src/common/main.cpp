#include <iostream>
#include "common/version.h"

int main() {
  std::cout << "AppVersion: " << AppVersion << std::endl;
  std::cout << "UpdateChannel: " << UpdateChannel << std::endl;
  std::cout << "AEPluginVersion: " << AEPluginVersion << std::endl;

  return 0;
}