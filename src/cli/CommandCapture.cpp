/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "cli/CommandCapture.h"
#include <iostream>
#include "cli/NodeTool.h"

namespace pagx::cli {

int RunCapture(int argc, char* argv[]) {
  auto script = ResolveHTMLTool("PAGX_HTML_CAPTURE_BIN", "launch-capture.js", "capture.js");
  if (script.empty()) {
    std::cerr << "pagx capture: runtime not found; install the complete @libpag/pagx package, "
              << "run from a libpag checkout, or set PAGX_HTML_CAPTURE_BIN\n";
    return 1;
  }
  auto executable = CurrentExecutablePath();
  if (executable.empty()) {
    std::cerr << "pagx capture: cannot locate the invoking native binary\n";
    return 1;
  }
  // A caller's explicit --pagx-bin may override this default. PAGX_BIN and PATH must not
  // silently select a different version for import/resolve/render inside this invocation.
  std::vector<std::string> args = {"--pagx-bin", executable};
  for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);
  return RunNodeTool(script, args);
}

}  // namespace pagx::cli
