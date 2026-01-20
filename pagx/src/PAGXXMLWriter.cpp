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

#include "PAGXXMLWriter.h"
#include <sstream>

namespace pagx {

static std::string EscapeXML(const std::string& str) {
  std::string result;
  result.reserve(str.size());
  for (char c : str) {
    switch (c) {
      case '<':
        result += "&lt;";
        break;
      case '>':
        result += "&gt;";
        break;
      case '&':
        result += "&amp;";
        break;
      case '"':
        result += "&quot;";
        break;
      case '\'':
        result += "&apos;";
        break;
      default:
        result += c;
        break;
    }
  }
  return result;
}

static void WriteNode(std::ostringstream& oss, const PAGXNode* node, int indent) {
  std::string indentStr(indent * 2, ' ');

  oss << indentStr << "<" << PAGXNodeTypeName(node->type());

  // Write ID if present
  if (!node->id().empty()) {
    oss << " id=\"" << EscapeXML(node->id()) << "\"";
  }

  // Write attributes
  auto attrNames = node->getAttributeNames();
  for (const auto& name : attrNames) {
    std::string value = node->getAttribute(name);
    oss << " " << name << "=\"" << EscapeXML(value) << "\"";
  }

  if (node->childCount() == 0) {
    oss << "/>\n";
  } else {
    oss << ">\n";
    for (size_t i = 0; i < node->childCount(); ++i) {
      WriteNode(oss, node->childAt(i), indent + 1);
    }
    oss << indentStr << "</" << PAGXNodeTypeName(node->type()) << ">\n";
  }
}

std::string PAGXXMLWriter::Write(const PAGXDocument* document) {
  if (!document || !document->root()) {
    return "";
  }

  std::ostringstream oss;
  oss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";

  auto root = document->root();

  oss << "<" << PAGXNodeTypeName(root->type());
  oss << " version=\"" << document->version() << "\"";
  oss << " width=\"" << document->width() << "\"";
  oss << " height=\"" << document->height() << "\"";

  // Write other root attributes
  auto attrNames = root->getAttributeNames();
  for (const auto& name : attrNames) {
    if (name == "version" || name == "width" || name == "height") {
      continue;
    }
    std::string value = root->getAttribute(name);
    oss << " " << name << "=\"" << EscapeXML(value) << "\"";
  }

  // Check if there are resources or children
  auto resources = document->resources();
  bool hasResources = resources && resources->childCount() > 0;
  bool hasChildren = root->childCount() > 0;

  if (!hasResources && !hasChildren) {
    oss << "/>\n";
  } else {
    oss << ">\n";

    // Write resources
    if (hasResources) {
      oss << "  <Resources>\n";
      for (size_t i = 0; i < resources->childCount(); ++i) {
        WriteNode(oss, resources->childAt(i), 2);
      }
      oss << "  </Resources>\n";
    }

    // Write children
    for (size_t i = 0; i < root->childCount(); ++i) {
      WriteNode(oss, root->childAt(i), 1);
    }

    oss << "</" << PAGXNodeTypeName(root->type()) << ">\n";
  }

  return oss.str();
}

}  // namespace pagx
