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

#pragma once

#include <string>
#include <vector>

namespace pagx::cli {

std::string CurrentExecutablePath();
std::string NodeExecutablePath();
// Explicit override, bundled runtime next to the executable, then repository checkout.
std::string ResolveHTMLTool(const char* environment, const char* bundledEntry,
                            const char* sourceEntry);
// Inherits stdio. POSIX replaces the process so signals/exit codes are preserved.
int RunNodeTool(const std::string& script, const std::vector<std::string>& arguments);

// Own cancellation for a native workflow, including periods between child processes.
class ToolProcessScope {
 public:
  ToolProcessScope();
  ~ToolProcessScope();
  ToolProcessScope(const ToolProcessScope&) = delete;
  ToolProcessScope& operator=(const ToolProcessScope&) = delete;

 private:
  void (*oldInterrupt)(int) = nullptr;
  void (*oldTerminate)(int) = nullptr;
};
int ToolCancellationCode();
// Unlike RunNodeTool, waits and returns to the native workflow. Arguments never pass a shell.
int RunNodeToolWait(const std::string& script, const std::vector<std::string>& arguments);

#ifdef _WIN32
std::wstring UTF8ToWide(const std::string& value);
std::string WideToUTF8(const std::wstring& value);
std::wstring QuoteWindowsArg(const std::wstring& value);
#endif

}  // namespace pagx::cli
