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
#include "CommandBounds.h"
#include "CommandFont.h"
#include "CommandFormat.h"
#include "CommandMeasure.h"
#include "CommandOptimize.h"
#include "CommandRender.h"
#include "CommandValidator.h"

static const char* VERSION = "0.1.0";

static void PrintUsage() {
  std::cout << "pagx - PAGX command-line tool\n"
            << "\n"
            << "Usage: pagx <command> [options] <file>\n"
            << "\n"
            << "Commands:\n"
            << "  validate   Validate PAGX structure against the specification\n"
            << "  render     Render PAGX to an image file (supports crop and scale)\n"
            << "  bounds     Query the precise bounds of a node or layer\n"
            << "  measure    Measure visual properties of a PAGX node or layer\n"
            << "  font       Embed fonts into a PAGX file\n"
            << "  format     Format a PAGX file (indentation and attribute ordering)\n"
            << "  optimize   Apply deterministic structural optimizations\n"
            << "\n"
            << "Global Options:\n"
            << "  --format json    Output in JSON format (validate, bounds, measure)\n"
            << "  --help, -h       Show help\n"
            << "  --version, -v    Show version\n";
}

int main(int argc, char* argv[]) {
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
    std::cout << "pagx " << VERSION << "\n";
    return 0;
  }

  if (command == "validate") {
    return pagx::cli::RunValidate(argc - 1, argv + 1);
  }
  if (command == "render") {
    return pagx::cli::RunRender(argc - 1, argv + 1);
  }
  if (command == "bounds") {
    return pagx::cli::RunBounds(argc - 1, argv + 1);
  }
  if (command == "measure") {
    return pagx::cli::RunMeasure(argc - 1, argv + 1);
  }
  if (command == "font") {
    return pagx::cli::RunFont(argc - 1, argv + 1);
  }
  if (command == "format") {
    return pagx::cli::RunFormat(argc - 1, argv + 1);
  }
  if (command == "optimize") {
    return pagx::cli::RunOptimize(argc - 1, argv + 1);
  }

  std::cerr << "pagx: unknown command '" << command << "'\n";
  std::cerr << "Run 'pagx --help' for usage.\n";
  return 1;
}
