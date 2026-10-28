/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "pagx/svg/SVGCssRules.h"
#include <cctype>

namespace pagx {

// Appends declarations to target, adding the separator only when target does not already end with
// one, so a rule body that ends with a semicolon does not produce an empty declaration that the
// style parsers would read as part of the next property name.
static void AppendDeclarations(std::string* target, const std::string& declarations) {
  if (!target->empty() && target->back() != ';') {
    *target += ';';
  }
  *target += declarations;
}

// Parses the text content of a <style> element into class rules: .name { property: value; }.
// Comma-separated selector groups register every class they contain. Later rules for the same
// class append their declarations with ";" so they win over earlier ones.
static void ParseStyleRuleText(const std::string& cssContent,
                               std::unordered_map<std::string, std::string>* rules) {
  size_t pos = 0;
  while (pos < cssContent.size()) {
    // Skip whitespace.
    while (pos < cssContent.size() && std::isspace(static_cast<unsigned char>(cssContent[pos]))) {
      pos++;
    }
    if (pos >= cssContent.size()) {
      break;
    }

    // Skip CSS comments.
    if (pos + 1 < cssContent.size() && cssContent[pos] == '/' && cssContent[pos + 1] == '*') {
      auto commentEnd = cssContent.find("*/", pos + 2);
      pos = (commentEnd == std::string::npos) ? cssContent.size() : commentEnd + 2;
      continue;
    }

    // Find the opening brace for this rule.
    size_t bracePos = cssContent.find('{', pos);
    if (bracePos == std::string::npos) {
      break;
    }

    // Extract the selector string before the brace.
    std::string selectorStr = cssContent.substr(pos, bracePos - pos);

    // Find the closing brace.
    size_t closeBracePos = cssContent.find('}', bracePos);
    if (closeBracePos == std::string::npos) {
      break;
    }

    // Extract the style content between braces.
    std::string styleContent = cssContent.substr(bracePos + 1, closeBracePos - bracePos - 1);

    // Trim whitespace and newlines from style content.
    size_t contentStart = styleContent.find_first_not_of(" \t\n\r");
    size_t contentEnd = styleContent.find_last_not_of(" \t\n\r");
    if (contentStart != std::string::npos && contentEnd != std::string::npos) {
      styleContent = styleContent.substr(contentStart, contentEnd - contentStart + 1);
    }

    // Parse comma-separated selectors and store rules for each class selector.
    if (!styleContent.empty()) {
      size_t selPos = 0;
      while (selPos < selectorStr.size()) {
        // Skip whitespace.
        while (selPos < selectorStr.size() &&
               std::isspace(static_cast<unsigned char>(selectorStr[selPos]))) {
          selPos++;
        }
        if (selPos >= selectorStr.size()) {
          break;
        }

        // Find end of this selector (comma or end of string).
        size_t commaPos = selectorStr.find(',', selPos);
        size_t selEnd = (commaPos == std::string::npos) ? selectorStr.size() : commaPos;
        std::string selector = selectorStr.substr(selPos, selEnd - selPos);

        // Trim whitespace from selector.
        size_t selStart = selector.find_first_not_of(" \t\n\r");
        size_t selLast = selector.find_last_not_of(" \t\n\r");
        if (selStart != std::string::npos && selLast != std::string::npos) {
          selector = selector.substr(selStart, selLast - selStart + 1);
        }

        // Only handle class selectors (starting with dot).
        if (!selector.empty() && selector[0] == '.') {
          std::string className = selector.substr(1);
          if (!className.empty()) {
            AppendDeclarations(&(*rules)[className], styleContent);
          }
        }

        selPos = (commaPos == std::string::npos) ? selectorStr.size() : commaPos + 1;
      }
    }

    pos = closeBracePos + 1;
  }
}

void CollectSvgClassRules(const std::shared_ptr<DOMNode>& svgRoot,
                          std::unordered_map<std::string, std::string>* rules) {
  if (svgRoot == nullptr || rules == nullptr) {
    return;
  }
  for (auto child = svgRoot->getFirstChild(); child; child = child->getNextSibling()) {
    if (child->name == "style") {
      auto textNode = child->getFirstChild();
      if (textNode != nullptr && textNode->type == DOMNodeType::Text) {
        // The text content is stored in the node's name for text nodes.
        ParseStyleRuleText(textNode->name, rules);
      }
    } else if (child->name == "defs") {
      for (auto defsChild = child->getFirstChild(); defsChild;
           defsChild = defsChild->getNextSibling()) {
        if (defsChild->name == "style") {
          auto textNode = defsChild->getFirstChild();
          if (textNode != nullptr && textNode->type == DOMNodeType::Text) {
            ParseStyleRuleText(textNode->name, rules);
          }
        }
      }
    }
  }
}

std::string CollectSvgClassDeclarations(const std::unordered_map<std::string, std::string>& rules,
                                        const std::shared_ptr<DOMNode>& node) {
  const auto* classAttr = node != nullptr ? node->findAttribute("class") : nullptr;
  if (classAttr == nullptr || classAttr->empty()) {
    return "";
  }
  std::string declarations = {};
  const std::string& classNames = *classAttr;
  size_t pos = 0;
  while (pos < classNames.size()) {
    while (pos < classNames.size() && std::isspace(static_cast<unsigned char>(classNames[pos]))) {
      pos++;
    }
    if (pos >= classNames.size()) {
      break;
    }
    size_t endPos = pos;
    while (endPos < classNames.size() &&
           !std::isspace(static_cast<unsigned char>(classNames[endPos]))) {
      endPos++;
    }
    auto rule = rules.find(classNames.substr(pos, endPos - pos));
    if (rule != rules.end()) {
      AppendDeclarations(&declarations, rule->second);
    }
    pos = endPos;
  }
  return declarations;
}

}  // namespace pagx
