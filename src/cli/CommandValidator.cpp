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

#include "cli/CommandValidator.h"
#include <libxml/parser.h>
#include <libxml/xmlschemas.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include "cli/CliUtils.h"
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXImporter.h"
#include "pagx_xsd.h"

namespace pagx::cli {

// libxml2 2.12+ changed xmlStructuredErrorFunc signature to use const xmlError*
#if LIBXML_VERSION >= 21200
static void CollectStructuredError(void* context, const xmlError* xmlError) {
#else
static void CollectStructuredError(void* context, xmlError* xmlError) {
#endif
  if (xmlError == nullptr) {
    return;
  }
  auto* errors = static_cast<std::vector<ValidationError>*>(context);
  ValidationError error = {};
  error.line = xmlError->line;
  std::string msg(xmlError->message != nullptr ? xmlError->message : "Unknown error");
  while (!msg.empty() && msg.back() == '\n') {
    msg.pop_back();
  }
  error.message = std::move(msg);
  errors->push_back(std::move(error));
}

static void PrintErrorsText(const std::vector<ValidationError>& errors, const std::string& file) {
  for (const auto& error : errors) {
    if (error.line > 0) {
      std::cerr << file << ":" << error.line << ": " << error.message << "\n";
    } else {
      std::cerr << file << ": " << error.message << "\n";
    }
  }
}

static void PrintErrorsJson(const std::vector<ValidationError>& errors, const std::string& file) {
  std::cout << "{\n";
  std::cout << "  \"file\": \"" << EscapeJson(file) << "\",\n";
  std::cout << "  \"valid\": " << (errors.empty() ? "true" : "false") << ",\n";
  std::cout << "  \"errors\": [";
  for (size_t i = 0; i < errors.size(); ++i) {
    if (i > 0) {
      std::cout << ",";
    }
    std::cout << "\n    {\"line\": " << errors[i].line << ", \"message\": \""
              << EscapeJson(errors[i].message) << "\"}";
  }
  if (!errors.empty()) {
    std::cout << "\n  ";
  }
  std::cout << "]\n";
  std::cout << "}\n";
}

static void PrintUsage() {
  std::cout << "Usage: pagx validate [options] <file.pagx>\n"
            << "\n"
            << "Validate a PAGX file against the specification schema (XML structure,\n"
            << "required attributes, valid values, element nesting). This is a static check\n"
            << "on the source XML. For runtime layout issues, use 'pagx layout' instead.\n"
            << "\n"
            << "Options:\n"
            << "  --json                 Output in JSON format\n"
            << "  -h, --help             Show this help message\n";
}

std::vector<ValidationError> ValidateFile(const std::string& filePath) {
  std::vector<ValidationError> errors = {};

  xmlDocPtr doc = xmlReadFile(filePath.c_str(), nullptr, XML_PARSE_NONET);
  if (doc == nullptr) {
    ValidationError error = {};
    error.message = "Failed to parse XML document";
    errors.push_back(std::move(error));
    return errors;
  }

  const auto& xsdContent = PagxXsdContent();
  xmlSchemaParserCtxtPtr parserCtxt =
      xmlSchemaNewMemParserCtxt(xsdContent.c_str(), static_cast<int>(xsdContent.size()));
  if (parserCtxt == nullptr) {
    xmlFreeDoc(doc);
    ValidationError error = {};
    error.message = "Internal error: failed to create schema parser context";
    errors.push_back(std::move(error));
    return errors;
  }

  xmlSchemaPtr schema = xmlSchemaParse(parserCtxt);
  xmlSchemaFreeParserCtxt(parserCtxt);
  if (schema == nullptr) {
    xmlFreeDoc(doc);
    ValidationError error = {};
    error.message = "Internal error: failed to parse XSD schema";
    errors.push_back(std::move(error));
    return errors;
  }

  xmlSchemaValidCtxtPtr validCtxt = xmlSchemaNewValidCtxt(schema);
  if (validCtxt == nullptr) {
    xmlSchemaFree(schema);
    xmlFreeDoc(doc);
    ValidationError error = {};
    error.message = "Internal error: failed to create validation context";
    errors.push_back(std::move(error));
    return errors;
  }

  xmlSchemaSetValidStructuredErrors(validCtxt, CollectStructuredError, &errors);
  xmlSchemaValidateDoc(validCtxt, doc);

  xmlSchemaFreeValidCtxt(validCtxt);
  xmlSchemaFree(schema);
  xmlFreeDoc(doc);

  // Application-level validation: load the document and collect semantic errors such as
  // conflicting constraint combinations that the XSD schema cannot express.
  auto pagxDoc = pagx::PAGXImporter::FromFile(filePath);
  if (pagxDoc != nullptr) {
    for (const auto& errorStr : pagxDoc->errors) {
      ValidationError error = {};
      // Parse "line {number}: {message}" format from PAGXImporter.
      if (errorStr.compare(0, 5, "line ") == 0) {
        auto colonPos = errorStr.find(':', 5);
        if (colonPos != std::string::npos) {
          auto lineStr = errorStr.substr(5, colonPos - 5);
          char* endPtr = nullptr;
          long lineNum = strtol(lineStr.c_str(), &endPtr, 10);
          error.line =
              (endPtr != lineStr.c_str() && *endPtr == '\0') ? static_cast<int>(lineNum) : 0;
          error.message = errorStr.substr(colonPos + 2);  // skip ": "
        } else {
          error.message = errorStr;
        }
      } else {
        error.message = errorStr;
      }
      errors.push_back(std::move(error));
    }
  }

  return errors;
}

int RunValidate(int argc, char* argv[]) {
  bool jsonOutput = false;
  std::string filePath = {};

  // Parse arguments (argv[0] is "validate")
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--json") == 0) {
      jsonOutput = true;
    } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      PrintUsage();
      return 0;
    } else if (argv[i][0] == '-') {
      std::cerr << "pagx validate: unknown option '" << argv[i] << "'\n";
      return 1;
    } else {
      filePath = argv[i];
    }
  }

  if (filePath.empty()) {
    std::cerr << "pagx validate: missing input file\n";
    return 1;
  }

  auto errors = ValidateFile(filePath);

  // Output results
  if (jsonOutput) {
    PrintErrorsJson(errors, filePath);
  } else if (!errors.empty()) {
    PrintErrorsText(errors, filePath);
  } else {
    std::cout << filePath << ": valid\n";
  }

  return errors.empty() ? 0 : 1;
}

}  // namespace pagx::cli
