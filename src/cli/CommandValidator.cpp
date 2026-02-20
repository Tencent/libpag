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

#include "CommandValidator.h"
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include <libxml/parser.h>
#include <libxml/xmlschemas.h>
#include "pagx_xsd.h"

namespace pagx::cli {

static void CollectValidationError(void* context, const char* format, ...) {
  auto* errors = static_cast<std::vector<ValidationError>*>(context);
  char buffer[4096] = {};
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  std::string msg(buffer);
  // Remove trailing newline
  while (!msg.empty() && msg.back() == '\n') {
    msg.pop_back();
  }
  if (msg.empty()) {
    return;
  }
  ValidationError error = {};
  error.message = std::move(msg);
  errors->push_back(std::move(error));
}

static void CollectStructuredError(void* context, xmlErrorPtr xmlError) {
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

static std::string EscapeJson(const std::string& input) {
  std::string result;
  result.reserve(input.size() + 16);
  for (char ch : input) {
    switch (ch) {
      case '"':
        result += "\\\"";
        break;
      case '\\':
        result += "\\\\";
        break;
      case '\n':
        result += "\\n";
        break;
      case '\r':
        result += "\\r";
        break;
      case '\t':
        result += "\\t";
        break;
      default:
        result += ch;
        break;
    }
  }
  return result;
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
  std::cerr << "Usage: pagx validate [--format json] <file.pagx>\n";
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

  xmlSchemaSetValidErrors(validCtxt, CollectValidationError, CollectValidationError, &errors);
  xmlSchemaSetValidStructuredErrors(validCtxt, CollectStructuredError, &errors);
  xmlSchemaValidateDoc(validCtxt, doc);

  xmlSchemaFreeValidCtxt(validCtxt);
  xmlSchemaFree(schema);
  xmlFreeDoc(doc);

  return errors;
}

int RunValidate(int argc, char* argv[]) {
  bool jsonOutput = false;
  std::string filePath = {};

  // Parse arguments (argv[0] is "validate")
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--format") == 0) {
      if (i + 1 < argc) {
        ++i;
        if (strcmp(argv[i], "json") == 0) {
          jsonOutput = true;
        } else {
          std::cerr << "pagx validate: unknown format '" << argv[i] << "'\n";
          return 1;
        }
      } else {
        std::cerr << "pagx validate: --format requires an argument\n";
        return 1;
      }
    } else if (argv[i][0] == '-') {
      std::cerr << "pagx validate: unknown option '" << argv[i] << "'\n";
      PrintUsage();
      return 1;
    } else {
      filePath = argv[i];
    }
  }

  if (filePath.empty()) {
    PrintUsage();
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
