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
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include "cli/CliUtils.h"
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXImporter.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/MergePath.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx_xsd.h"

namespace pagx::cli {

// --- Semantic rule checks on the parsed document model ---

// CheckMergePathPainterScope: Validates that no Fill/Stroke appears before a MergePath
// in the same element scope (one layer or group).
//
// MergePath consumes and clears all preceding Fill/Stroke effects within the same
// scope. Any painter (Fill/Stroke) placed before MergePath in the element list will be
// silently discarded at render time, producing an icon that looks wrong with no error.
//
// Detection algorithm (state machine over the element list):
//   1. Track whether any Fill/Stroke has been seen before a MergePath.
//   2. On MergePath: if flag is set, report an error; then reset the flag.
//   3. Reason for reset: MergePath is a rendering "break point". Painters added AFTER
//      MergePath form a new, independent accumulation phase and are not cleared.
//      Resetting ensures we don't double-report for a second MergePath in the same scope.
//   4. Recurse into Groups, which form their own independent scope.
//
// Data flow: called per-layer by CheckSemanticRulesLayer; errors flow into the shared
// ValidationError list returned to the caller of RunValidate.
static void CheckMergePathPainterScope(const std::vector<Element*>& elements,
                                       const std::string& scopeLabel,
                                       std::vector<ValidationError>& errors) {
  bool hasPainterBeforeMergePath = false;
  for (auto* element : elements) {
    NodeType type = element->nodeType();
    if (type == NodeType::Fill || type == NodeType::Stroke) {
      hasPainterBeforeMergePath = true;
    } else if (type == NodeType::MergePath) {
      if (hasPainterBeforeMergePath) {
        ValidationError err = {};
        err.message = scopeLabel +
                      ": Fill/Stroke before MergePath will be cleared by MergePath."
                      " Isolate pre-MergePath rendering in a separate Group.";
        errors.push_back(std::move(err));
      }
      // Reset: painters after MergePath belong to a new accumulation phase and are not cleared.
      hasPainterBeforeMergePath = false;
    } else if (type == NodeType::Group) {
      // Recurse into groups — each group is its own isolated scope for this check.
      auto* group = static_cast<const Group*>(element);
      CheckMergePathPainterScope(group->elements, scopeLabel + "/<Group>", errors);
    }
  }
}

// CheckTextBoxLayoutOverrideScope: Validates that Text elements inside a TextBox scope
// do not carry position or textAnchor attributes that will be silently ignored.
//
// When a TextBox is present in a scope, it takes full control of text layout and
// silently ignores Text.position and Text.textAnchor. Authors who set these attributes
// expecting them to take effect will get a visually wrong result with no error.
//
// Detection logic:
//   - Collect all Text and TextBox nodes in the scope (pass 1).
//   - If no TextBox is present, nothing to validate — return early.
//   - For each Text, check two conditions:
//       hasPosition: position != (0, 0). Zero is the default; any non-zero value means the
//         author explicitly placed the text, but TextBox will override it.
//       hasNonDefaultAnchor: textAnchor != TextAnchor::Start. Start is the default value;
//         any other alignment signals intentional layout that TextBox will discard.
//   - Report one error per scope (break after first hit) to avoid flooding the error list
//     when multiple Text elements share the same problem. Author fixes one, re-validates.
//
// Data flow: called per-layer by CheckSemanticRulesLayer; errors flow into the shared
// ValidationError list returned to the caller of RunValidate.
static void CheckTextBoxLayoutOverrideScope(const std::vector<Element*>& elements,
                                            const std::string& scopeLabel,
                                            std::vector<ValidationError>& errors) {
  bool hasTextBox = false;
  std::vector<const Text*> texts;
  for (auto* element : elements) {
    NodeType type = element->nodeType();
    if (type == NodeType::TextBox) {
      hasTextBox = true;
    } else if (type == NodeType::Text) {
      texts.push_back(static_cast<const Text*>(element));
    } else if (type == NodeType::Group) {
      auto* group = static_cast<const Group*>(element);
      CheckTextBoxLayoutOverrideScope(group->elements, scopeLabel + "/<Group>", errors);
    }
  }
  if (!hasTextBox) {
    return;
  }
  for (auto* text : texts) {
    // position == (0,0) is the default — any non-zero value signals intentional placement.
    bool hasPosition = text->position.x != 0.0f || text->position.y != 0.0f;
    // TextAnchor::Start is the default — any other value signals intentional alignment.
    bool hasNonDefaultAnchor = text->textAnchor != TextAnchor::Start;
    if (hasPosition || hasNonDefaultAnchor) {
      ValidationError err = {};
      err.message = scopeLabel +
                    ": Text has 'position' or 'textAnchor' that will be ignored because TextBox"
                    " overrides text layout. Remove these attributes from the Text element.";
      errors.push_back(std::move(err));
      // Report only the first offending Text per scope. Author should fix it and re-validate
      // rather than seeing a flood of identical errors for multiple Text elements.
      break;
    }
  }
}

static void CheckSemanticRulesLayer(const Layer* layer, std::vector<ValidationError>& errors) {
  std::string label = "Layer";
  if (!layer->name.empty()) {
    label += "[" + layer->name + "]";
  }
  CheckMergePathPainterScope(layer->contents, label, errors);
  CheckTextBoxLayoutOverrideScope(layer->contents, label, errors);
  for (auto* child : layer->children) {
    CheckSemanticRulesLayer(child, errors);
  }
}

static void CheckSemanticRules(const PAGXDocument* document, std::vector<ValidationError>& errors) {
  for (auto* layer : document->layers) {
    CheckSemanticRulesLayer(layer, errors);
  }
  for (auto& node : document->nodes) {
    if (node->nodeType() == NodeType::Composition) {
      auto* comp = static_cast<const Composition*>(node.get());
      for (auto* layer : comp->layers) {
        CheckSemanticRulesLayer(layer, errors);
      }
    }
  }
}

// ---
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

  // Only run semantic checks when XSD validation passes.
  if (errors.empty()) {
    auto pagxDoc = pagx::PAGXImporter::FromFile(filePath);
    if (pagxDoc != nullptr) {
      CheckSemanticRules(pagxDoc.get(), errors);
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
