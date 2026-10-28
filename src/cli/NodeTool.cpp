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

#include "cli/NodeTool.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>

namespace pagx::cli {
namespace fs = std::filesystem;

#ifdef _WIN32
std::wstring UTF8ToWide(const std::string& value) {
  if (value.empty()) return {};
  int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                 static_cast<int>(value.size()), nullptr, 0);
  if (size <= 0) return {};
  std::wstring wide(static_cast<size_t>(size), L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                      wide.data(), size);
  return wide;
}

// Quotes one argv item using the CommandLineToArgvW / Microsoft C runtime rules. CreateProcessW
// receives this command line directly, so cmd.exe metacharacters never get a chance to execute.
std::wstring QuoteWindowsArg(const std::wstring& value) {
  std::wstring out = L"\"";
  size_t backslashes = 0;
  for (wchar_t ch : value) {
    if (ch == L'\\') {
      backslashes++;
      continue;
    }
    if (ch == L'\"') {
      out.append(backslashes * 2 + 1, L'\\');
      out.push_back(L'\"');
    } else {
      out.append(backslashes, L'\\');
      out.push_back(ch);
    }
    backslashes = 0;
  }
  out.append(backslashes * 2, L'\\');
  out.push_back(L'\"');
  return out;
}

std::string WideToUTF8(const std::wstring& value) {
  if (value.empty()) return {};
  int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                                 static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
  if (size <= 0) return {};
  std::string result(static_cast<size_t>(size), '\0');
  WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                      result.data(), size, nullptr, nullptr);
  return result;
}
#endif

static std::string EnvironmentValue(const char* name) {
#ifdef _WIN32
  auto wideName = UTF8ToWide(name);
  DWORD size = GetEnvironmentVariableW(wideName.c_str(), nullptr, 0);
  if (size == 0) return {};
  std::vector<wchar_t> buffer(size);
  DWORD length = GetEnvironmentVariableW(wideName.c_str(), buffer.data(), size);
  if (length == 0 || length >= size) return {};
  return WideToUTF8(std::wstring(buffer.data(), length));
#else
  const char* value = std::getenv(name);
  return value ? value : "";
#endif
}

std::string CurrentExecutablePath() {
  std::error_code ec;
  fs::path executable;
#ifdef _WIN32
  std::vector<wchar_t> buffer(32768);
  DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
  if (length == 0 || length >= buffer.size()) return {};
  executable = std::wstring(buffer.data(), length);
#elif defined(__APPLE__)
  uint32_t size = 0;
  _NSGetExecutablePath(nullptr, &size);
  std::vector<char> buffer(size);
  if (_NSGetExecutablePath(buffer.data(), &size) != 0) return {};
  executable = buffer.data();
#else
  executable = fs::read_symlink("/proc/self/exe", ec);
  if (ec) return {};
#endif
  auto result = fs::canonical(executable, ec);
  return ec ? std::string() : result.u8string();
}

std::string NodeExecutablePath() {
  auto node = EnvironmentValue("PAGX_NODE_BIN");
  return node.empty() ? "node" : node;
}

std::string ResolveHTMLTool(const char* environment, const char* bundledEntry,
                            const char* sourceEntry) {
  auto regular = [](const fs::path& path) {
    std::error_code ec;
    return fs::is_regular_file(path, ec) && !ec;
  };
  auto absolute = [](const fs::path& path) {
    std::error_code ec;
    auto result = fs::absolute(path, ec);
    return ec ? std::string() : result.u8string();
  };
  auto configured = EnvironmentValue(environment);
  if (!configured.empty()) {
    auto file = fs::u8path(configured);
    return regular(file) ? absolute(file) : std::string();
  }
  const auto executable = fs::u8path(CurrentExecutablePath());
  if (!executable.empty()) {
    auto parent = executable.parent_path();
    // Standalone layout and npm's bin/<platform>-<arch>/pagx layout.
    for (int depth = 0; depth < 3; ++depth) {
      auto file = parent / "html-snapshot" / bundledEntry;
      if (regular(file)) return absolute(file);
      parent = parent.parent_path();
    }
  }
  std::error_code ec;
  auto cwd = fs::current_path(ec);
  if (ec) return {};
  auto relative = fs::path("tools") / "html-snapshot" / sourceEntry;
  if (regular(cwd / relative)) return absolute(cwd / relative);
  for (const auto& start : {cwd, executable.parent_path()}) {
    auto current = start;
    for (int depth = 0; depth < 8 && !current.empty(); ++depth) {
      if (fs::exists(current / ".git", ec) && !ec) {
        if (regular(current / relative)) return absolute(current / relative);
        break;
      }
      auto parent = current.parent_path();
      if (parent == current) break;
      current = parent;
    }
  }
  return {};
}

#ifdef _WIN32
static BOOL WINAPI HandleConsoleSignal(DWORD event) {
  // Node shares the console and handles these events; let it finish cleanup.
  return event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT;
}
#endif

int RunNodeTool(const std::string& script, const std::vector<std::string>& arguments) {
  std::vector<std::string> args = {NodeExecutablePath(), script};
  args.insert(args.end(), arguments.begin(), arguments.end());
#ifdef _WIN32
  std::wstring command;
  for (const auto& arg : args) {
    auto wide = UTF8ToWide(arg);
    if (!arg.empty() && wide.empty()) {
      std::cerr << "pagx: invalid UTF-8 in Node runtime arguments\n";
      return 1;
    }
    if (!command.empty()) command += L" ";
    command += QuoteWindowsArg(wide);
  }
  HANDLE job = CreateJobObjectW(nullptr, nullptr);
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {};
  limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  if (!job ||
      !SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) {
    if (job) CloseHandle(job);
    std::cerr << "pagx: failed to configure Node process lifetime\n";
    return 1;
  }
  STARTUPINFOW startup = {};
  startup.cb = sizeof(startup);
  startup.dwFlags = STARTF_USESTDHANDLES;
  startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
  startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
  startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
  PROCESS_INFORMATION process = {};
  if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE, CREATE_SUSPENDED, nullptr,
                      nullptr, &startup, &process)) {
    auto error = GetLastError();
    CloseHandle(job);
    std::cerr << "pagx: failed to start Node (Windows error " << error
              << "); install Node.js or set PAGX_NODE_BIN\n";
    return 1;
  }
  if (!AssignProcessToJobObject(job, process.hProcess)) {
    TerminateProcess(process.hProcess, 1);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    CloseHandle(job);
    std::cerr << "pagx: failed to attach Node process to cleanup job\n";
    return 1;
  }
  SetConsoleCtrlHandler(HandleConsoleSignal, TRUE);
  ResumeThread(process.hThread);
  CloseHandle(process.hThread);
  WaitForSingleObject(process.hProcess, INFINITE);
  DWORD code = 1;
  GetExitCodeProcess(process.hProcess, &code);
  SetConsoleCtrlHandler(HandleConsoleSignal, FALSE);
  CloseHandle(process.hProcess);
  CloseHandle(job);
  return static_cast<int>(code);
#else
  std::vector<char*> argv;
  for (auto& arg : args) argv.push_back(arg.data());
  argv.push_back(nullptr);
  execvp(argv[0], argv.data());
  std::cerr << "pagx: failed to start Node: " << std::strerror(errno)
            << "; install Node.js or set PAGX_NODE_BIN\n";
  return 1;
#endif
}

}  // namespace pagx::cli
