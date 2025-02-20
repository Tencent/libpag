#ifndef PAGEXPORTER_RUNSCRIPT_H
#define PAGEXPORTER_RUNSCRIPT_H

#include <string>
#include "AEGP_SuiteHandler.h"

std::string RunScript(const AEGP_SuiteHandler& suites, AEGP_PluginID pluginID, const std::string& scriptText);


#endif //PAGEXPORTER_RUNSCRIPT_H
