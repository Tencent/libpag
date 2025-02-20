#include "RunScript.h"


std::string RunScript(const AEGP_SuiteHandler& suites, AEGP_PluginID pluginID, const std::string& scriptText) {
  AEGP_MemHandle scriptResult;
  AEGP_MemHandle errorResult;
  suites.UtilitySuite6()->AEGP_ExecuteScript(pluginID, scriptText.c_str(),
                                             FALSE, &scriptResult, &errorResult);
  A_char* result = nullptr;
  suites.MemorySuite1()->AEGP_LockMemHandle(scriptResult, reinterpret_cast<void**>(&result));
  std::string resultText = result;
  suites.MemorySuite1()->AEGP_FreeMemHandle(scriptResult);
  A_char* error = nullptr;
  suites.MemorySuite1()->AEGP_LockMemHandle(errorResult, reinterpret_cast<void**>(&error));
  std::string errorText = error;
  suites.MemorySuite1()->AEGP_FreeMemHandle(errorResult);
  return resultText;
}