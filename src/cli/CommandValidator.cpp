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
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/TextBox.h"
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

// Heuristic checks for auto layout patterns.
// Only flags cases where constraint attributes are provably ignored by the layout engine.

static void CheckLayerConstraints(const Layer* layer, std::vector<ValidationError>& errors) {
  if (layer == nullptr) {
    return;
  }

  // Check: Container layout with child constraints (provably ignored).
  // Constraint attributes on a child Layer are only effective when the parent has no container
  // layout or the child has includeInLayout="false". When a child participates in container
  // layout flow (includeInLayout=true, parent has container layout), constraints are silently
  // ignored — this is almost always unintentional.
  if (layer->layout != LayoutMode::None) {
    for (const auto& child : layer->children) {
      if (child == nullptr || !child->includeInLayout) {
        continue;  // includeInLayout="false" children CAN use constraints
      }
      bool hasConstraints =
          (!std::isnan(child->left) || !std::isnan(child->right) || !std::isnan(child->centerX) ||
           !std::isnan(child->top) || !std::isnan(child->bottom) || !std::isnan(child->centerY));
      if (hasConstraints) {
        std::string attrs = {};
        if (!std::isnan(child->left)) attrs += "left ";
        if (!std::isnan(child->right)) attrs += "right ";
        if (!std::isnan(child->centerX)) attrs += "centerX ";
        if (!std::isnan(child->top)) attrs += "top ";
        if (!std::isnan(child->bottom)) attrs += "bottom ";
        if (!std::isnan(child->centerY)) attrs += "centerY ";
        std::string nodeInfo = {};
        if (!child->id.empty()) {
          nodeInfo = " (id='" + child->id + "')";
        } else if (!child->name.empty()) {
          nodeInfo = " (name='" + child->name + "')";
        }
        ValidationError error = {};
        error.line = 0;
        error.message = "Constraint attributes (" + attrs + ") on child Layer" + nodeInfo +
                        " will be ignored because the parent uses container "
                        "layout. Use 'padding'/'gap' on the parent, or set "
                        "includeInLayout='false' on the child to enable constraints.";
        errors.push_back(std::move(error));
      }
    }
  }

  // Recursively check children
  for (const auto& child : layer->children) {
    CheckLayerConstraints(child, errors);
  }
}

static void CheckAutoLayoutPatterns(const PAGXDocument* doc, std::vector<ValidationError>& errors) {
  if (doc == nullptr || doc->layers.empty()) {
    return;
  }

  for (const auto& layer : doc->layers) {
    CheckLayerConstraints(layer, errors);
  }
  for (const auto& node : doc->nodes) {
    if (node && node->nodeType() == NodeType::Composition) {
      auto* comp = static_cast<const Composition*>(node.get());
      for (const auto& layer : comp->layers) {
        CheckLayerConstraints(layer, errors);
      }
    }
  }
}

// Check for nested TextBox elements (TextBox inside TextBox or Group inside TextBox).
// TextBox is a text layout container and should not contain other layout containers.

static void CheckTextBoxElement(const Element* element, const TextBox* parentTextBox,
                                std::vector<ValidationError>& errors) {
  if (element == nullptr) {
    return;
  }

  // Check if current element is a TextBox
  if (element->nodeType() == NodeType::TextBox) {
    auto* textBox = static_cast<const TextBox*>(element);
    if (parentTextBox != nullptr) {
      // TextBox nested inside another TextBox
      std::string nodeInfo = {};
      if (!element->id.empty()) {
        nodeInfo = " (id='" + element->id + "')";
      }
      ValidationError error = {};
      error.line = 0;
      error.message = "TextBox" + nodeInfo +
                      " should not be nested inside another TextBox. "
                      "This may lead to unexpected layout behavior.";
      errors.push_back(std::move(error));
    }

    // Check children of this TextBox for nested containers
    for (const auto* child : textBox->elements) {
      if (child == nullptr) {
        continue;
      }

      if (child->nodeType() == NodeType::Group) {
        // Group inside TextBox (TextBox is caught by the outer branch above)
        std::string nodeInfo = {};
        if (!child->id.empty()) {
          nodeInfo = " (id='" + child->id + "')";
        }
        ValidationError error = {};
        error.line = 0;
        error.message = "Group" + nodeInfo +
                        " should not be nested directly inside TextBox. "
                        "TextBox manages layout of Text elements; Groups may interfere with text "
                        "positioning.";
        errors.push_back(std::move(error));
      }

      CheckTextBoxElement(child, textBox, errors);
    }
  } else if (element->nodeType() == NodeType::Group) {
    // Regular Group, recurse with same parentTextBox context
    auto* group = static_cast<const Group*>(element);
    for (const auto* child : group->elements) {
      CheckTextBoxElement(child, parentTextBox, errors);
    }
  }
}

static void CheckTextBoxNesting(const PAGXDocument* doc, std::vector<ValidationError>& errors) {
  if (doc == nullptr) {
    return;
  }

  // Check all layers and their contents
  for (const auto& layer : doc->layers) {
    if (layer != nullptr) {
      for (const auto* element : layer->contents) {
        CheckTextBoxElement(element, nullptr, errors);
      }
    }
  }
  // Check layers inside Compositions
  for (const auto& node : doc->nodes) {
    if (node && node->nodeType() == NodeType::Composition) {
      auto* comp = static_cast<const Composition*>(node.get());
      for (const auto& layer : comp->layers) {
        if (layer != nullptr) {
          for (const auto* element : layer->contents) {
            CheckTextBoxElement(element, nullptr, errors);
          }
        }
      }
    }
  }
}

static void PrintUsage() {
  std::cout << "Usage: pagx validate [options] <file.pagx>\n"
            << "\n"
            << "Validate a PAGX file against the specification schema.\n"
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
    // Heuristic validation: check for common auto layout patterns
    CheckAutoLayoutPatterns(pagxDoc.get(), errors);
    // Check for TextBox nesting issues
    CheckTextBoxNesting(pagxDoc.get(), errors);
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
