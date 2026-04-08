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

#include "cli/CommandFormat.h"
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include "cli/FormatUtils.h"

namespace pagx::cli {

static void PrintFormatUsage() {
  std::cout << "Usage: pagx format [options] <file.pagx>\n"
            << "\n"
            << "Formats (pretty-prints) a PAGX file with consistent indentation and attribute\n"
            << "ordering. Does not modify values or structure.\n"
            << "\n"
            << "Options:\n"
            << "  -o, --output <path>   Output file path (default: overwrite input)\n"
            << "  --indent <n>          Indentation spaces (default: 2)\n"
            << "  -h, --help            Show this help message\n";
}

int RunFormat(int argc, char* argv[]) {
  std::string inputPath = {};
  std::string outputPath = {};
  int indentSpaces = 2;

  for (int i = 1; i < argc; i++) {
    if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0) {
      PrintFormatUsage();
      return 0;
    }
    if ((std::strcmp(argv[i], "-o") == 0 || std::strcmp(argv[i], "--output") == 0) &&
        i + 1 < argc) {
      outputPath = argv[++i];
      continue;
    }
    if (std::strcmp(argv[i], "--indent") == 0 && i + 1 < argc) {
      char* endPtr = nullptr;
      errno = 0;
      long value = strtol(argv[++i], &endPtr, 10);
      if (errno != 0 || endPtr == argv[i] || *endPtr != '\0' || value < 0 || value > INT_MAX) {
        std::cerr << "pagx format: invalid indent value '" << argv[i] << "'\n";
        return 1;
      }
      indentSpaces = static_cast<int>(value);
      continue;
    }
    if (argv[i][0] == '-') {
      std::cerr << "pagx format: unknown option '" << argv[i] << "'\n";
      return 1;
    }
    if (inputPath.empty()) {
      inputPath = argv[i];
    } else {
      std::cerr << "pagx format: unexpected argument '" << argv[i] << "'\n";
      return 1;
    }
  }

  if (inputPath.empty()) {
    std::cerr << "pagx format: missing input file\n";
    PrintFormatUsage();
    return 1;
  }

  if (outputPath.empty()) {
    outputPath = inputPath;
  }

  auto doc = xmlReadFile(inputPath.c_str(), nullptr, XML_PARSE_NONET | XML_PARSE_NOBLANKS);
  if (doc == nullptr) {
    std::cerr << "pagx format: failed to parse '" << inputPath << "'\n";
    return 1;
  }

  auto root = xmlDocGetRootElement(doc);
  if (root == nullptr) {
    std::cerr << "pagx format: empty document '" << inputPath << "'\n";
    xmlFreeDoc(doc);
    return 1;
  }

  ReorderAttributesRecursive(root);

  std::string output = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  SerializeNode(output, root, 0, indentSpaces);

  xmlFreeDoc(doc);

  std::ofstream out(outputPath);
  if (!out.is_open()) {
    std::cerr << "pagx format: failed to write '" << outputPath << "'\n";
    return 1;
  }
  out << output;
  out.close();
  if (out.fail()) {
    std::cerr << "pagx format: error writing to '" << outputPath << "'\n";
    return 1;
  }

  std::cout << "pagx format: wrote " << outputPath << "\n";
  return 0;
}

}  // namespace pagx::cli
