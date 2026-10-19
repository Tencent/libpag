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

#include <cstring>
#include <iostream>
#include <string>
#include "cli/CommandBounds.h"
#include "cli/NodeTool.h"
#ifdef _WIN32
#include <windows.h>

#include <shellapi.h>
#endif
#include "cli/CommandEmbed.h"
#include "cli/CommandExport.h"
#include "cli/CommandFont.h"
#include "cli/CommandFormat.h"
#include "cli/CommandImport.h"
#include "cli/CommandLayout.h"
#include "cli/CommandRender.h"
#include "cli/CommandResolve.h"
#include "cli/CommandVerify.h"

#ifndef PAGX_CLI_VERSION
#define PAGX_CLI_VERSION "0.0.0-dev"
#endif

static void PrintUsage() {
  std::cout << "pagx - PAGX command-line tool\n"
            << "\n"
            << "Usage: pagx <command> [options] <file>\n"
            << "\n"
            << "Commands:\n"
            << "  verify         Resolve imports, check all issues, render screenshot + layout\n"
            << "  resolve        Resolve import directives (inline SVG, external imports)\n"
            << "  layout         Display layout tree with bounds\n"
            << "  render         Render PAGX to an image file (supports crop and scale)\n"
            << "  bounds         Query rendered pixel bounds of layers (for crop regions)\n"
            << "  font           Query font metrics\n"
            << "  embed          Embed fonts and images into a PAGX file\n"
            << "  format         Format a PAGX file (indentation and attribute ordering)\n"
            << "  import         Import SVG, HTML and interaction states as PAGX\n"
            << "  export         Export a PAGX file to another format (e.g. SVG)\n"
            << "\n"
            << "Options:\n"
            << "  --help, -h       Show help\n"
            << "  --version, -v    Show version\n"
            << "\n"
            << "Run 'pagx <command> --help' for detailed options and usage of each command.\n";
}

static int RunCommand(int argc, char* argv[]) {
  if (argc < 2) {
    PrintUsage();
    return 1;
  }

  std::string command = argv[1];

  if (command == "--help" || command == "-h") {
    PrintUsage();
    return 0;
  }

  if (command == "--version" || command == "-v") {
    std::cout << "pagx " << PAGX_CLI_VERSION << "\n";
    return 0;
  }

  if (command == "verify") {
    return pagx::cli::RunVerify(argc - 1, argv + 1);
  }
  if (command == "layout") {
    return pagx::cli::RunLayout(argc - 1, argv + 1);
  }
  if (command == "render") {
    return pagx::cli::RunRender(argc - 1, argv + 1);
  }
  if (command == "bounds") {
    return pagx::cli::RunBounds(argc - 1, argv + 1);
  }
  if (command == "font") {
    return pagx::cli::RunFont(argc - 1, argv + 1);
  }
  if (command == "embed") {
    return pagx::cli::RunEmbed(argc - 1, argv + 1);
  }
  if (command == "format") {
    return pagx::cli::RunFormat(argc - 1, argv + 1);
  }
  if (command == "import") {
    return pagx::cli::RunImport(argc - 1, argv + 1);
  }
  if (command == "resolve") {
    return pagx::cli::RunResolve(argc - 1, argv + 1);
  }
  if (command == "export") {
    return pagx::cli::RunExport(argc - 1, argv + 1);
  }

  std::cerr << "pagx: unknown command '" << command << "'\n";
  std::cerr << "Run 'pagx --help' for usage.\n";
  return 1;
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
  // The narrow CRT argv uses the local codepage. Preserve Unicode paths from npm/PowerShell.
  int count = 0;
  auto wideArgs = CommandLineToArgvW(GetCommandLineW(), &count);
  if (!wideArgs) return 1;
  std::vector<std::string> utf8;
  for (int i = 0; i < count; ++i) utf8.push_back(pagx::cli::WideToUTF8(wideArgs[i]));
  LocalFree(wideArgs);
  std::vector<char*> args;
  for (auto& arg : utf8) args.push_back(arg.data());
  args.push_back(nullptr);
  return RunCommand(count, args.data());
#else
  return RunCommand(argc, argv);
#endif
}
