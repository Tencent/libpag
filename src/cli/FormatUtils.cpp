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

#include "cli/FormatUtils.h"
#include <libxml/parser.h>
#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>
#include "pagx_xsd.h"

namespace pagx::cli {

// ============================================================================
// Attribute order map parsed from the embedded XSD schema
// ============================================================================

static std::string XsdAttrStr(xmlNodePtr node, const char* name) {
  auto* val = xmlGetProp(node, reinterpret_cast<const xmlChar*>(name));
  if (val == nullptr) {
    return {};
  }
  std::string result(reinterpret_cast<const char*>(val));
  xmlFree(val);
  return result;
}

static bool XsdNodeIs(xmlNodePtr node, const char* localName) {
  return node->type == XML_ELEMENT_NODE &&
         xmlStrcmp(node->name, reinterpret_cast<const xmlChar*>(localName)) == 0;
}

static void CollectAttributes(xmlNodePtr parent,
                              std::unordered_map<std::string, std::vector<std::string>>& groupMap,
                              std::vector<std::string>& attrs) {
  for (auto cur = parent->children; cur != nullptr; cur = cur->next) {
    if (XsdNodeIs(cur, "attribute")) {
      auto name = XsdAttrStr(cur, "name");
      if (!name.empty()) {
        attrs.push_back(std::move(name));
      }
    } else if (XsdNodeIs(cur, "attributeGroup")) {
      auto ref = XsdAttrStr(cur, "ref");
      auto it = groupMap.find(ref);
      if (it != groupMap.end()) {
        attrs.insert(attrs.end(), it->second.begin(), it->second.end());
      }
    }
  }
}

static std::unordered_map<std::string, std::vector<std::string>> ParseXsdAttributeOrder() {
  auto& xsdContent = PagxXsdContent();
  auto* doc =
      xmlReadMemory(xsdContent.data(), static_cast<int>(xsdContent.size()), nullptr, nullptr, 0);
  if (doc == nullptr) {
    return {};
  }
  auto* root = xmlDocGetRootElement(doc);
  if (root == nullptr) {
    xmlFreeDoc(doc);
    return {};
  }

  // First pass: collect attributeGroup definitions.
  std::unordered_map<std::string, std::vector<std::string>> groupMap;
  for (auto cur = root->children; cur != nullptr; cur = cur->next) {
    if (XsdNodeIs(cur, "attributeGroup")) {
      auto name = XsdAttrStr(cur, "name");
      if (name.empty()) {
        continue;
      }
      std::vector<std::string> attrs;
      for (auto child = cur->children; child != nullptr; child = child->next) {
        if (XsdNodeIs(child, "attribute")) {
          auto attrName = XsdAttrStr(child, "name");
          if (!attrName.empty()) {
            attrs.push_back(std::move(attrName));
          }
        }
      }
      groupMap[std::move(name)] = std::move(attrs);
    }
  }

  // Second pass: collect complexType attribute orders.
  std::unordered_map<std::string, std::vector<std::string>> result;
  for (auto cur = root->children; cur != nullptr; cur = cur->next) {
    if (!XsdNodeIs(cur, "complexType")) {
      continue;
    }
    auto typeName = XsdAttrStr(cur, "name");
    if (typeName.empty()) {
      continue;
    }
    // Derive element name: remove "Type" suffix, special-case "PagxType" -> "pagx".
    std::string elementName;
    if (typeName == "PagxType") {
      elementName = "pagx";
    } else if (typeName.size() > 4 && typeName.compare(typeName.size() - 4, 4, "Type") == 0) {
      elementName = typeName.substr(0, typeName.size() - 4);
    } else {
      elementName = typeName;
    }

    std::vector<std::string> attrs;
    CollectAttributes(cur, groupMap, attrs);
    result[std::move(elementName)] = std::move(attrs);
  }

  xmlFreeDoc(doc);
  return result;
}

const std::unordered_map<std::string, std::vector<std::string>>& GetAttributeOrderMap() {
  static const auto order = ParseXsdAttributeOrder();
  return order;
}

struct SortKeyComparator {
  const std::vector<int>* keys = nullptr;
  bool operator()(size_t a, size_t b) const {
    return (*keys)[a] < (*keys)[b];
  }
};

void ReorderAttributes(xmlNodePtr node) {
  if (node->type != XML_ELEMENT_NODE || node->properties == nullptr) {
    return;
  }
  auto elementName = std::string(reinterpret_cast<const char*>(node->name));
  auto& attrOrder = GetAttributeOrderMap();
  auto it = attrOrder.find(elementName);
  if (it == attrOrder.end()) {
    return;
  }
  const auto& order = it->second;

  // Build a position map: attribute name -> index in canonical order.
  std::unordered_map<std::string, int> positionMap = {};
  for (int i = 0; i < static_cast<int>(order.size()); i++) {
    positionMap[order[i]] = i;
  }

  // Collect all existing attributes.
  std::vector<xmlAttrPtr> attrs = {};
  for (auto attr = node->properties; attr != nullptr; attr = attr->next) {
    attrs.push_back(attr);
  }

  // Sort: known attributes by canonical order, unknown attributes after them in original order.
  int unknownBase = static_cast<int>(order.size());
  int unknownIndex = 0;
  std::vector<int> sortKeys = {};
  sortKeys.reserve(attrs.size());
  for (auto* attr : attrs) {
    auto name = std::string(reinterpret_cast<const char*>(attr->name));
    auto posIt = positionMap.find(name);
    if (posIt != positionMap.end()) {
      sortKeys.push_back(posIt->second);
    } else {
      sortKeys.push_back(unknownBase + unknownIndex);
      unknownIndex++;
    }
  }

  // Build sorted index array and apply stable sort.
  std::vector<size_t> indices(attrs.size());
  for (size_t i = 0; i < indices.size(); i++) {
    indices[i] = i;
  }
  SortKeyComparator comparator = {&sortKeys};
  std::stable_sort(indices.begin(), indices.end(), comparator);

  // Rebuild the attribute linked list in sorted order.
  node->properties = nullptr;
  xmlAttrPtr prev = nullptr;
  for (auto idx : indices) {
    auto* attr = attrs[idx];
    attr->prev = prev;
    attr->next = nullptr;
    if (prev != nullptr) {
      prev->next = attr;
    } else {
      node->properties = attr;
    }
    prev = attr;
  }
}

void ReorderAttributesRecursive(xmlNodePtr node) {
  for (auto cur = node; cur != nullptr; cur = cur->next) {
    if (cur->type == XML_ELEMENT_NODE) {
      ReorderAttributes(cur);
      ReorderAttributesRecursive(cur->children);
    }
  }
}

std::string EscapeXmlAttr(const std::string& s) {
  std::string out;
  for (char c : s) {
    switch (c) {
      case '&':
        out += "&amp;";
        break;
      case '"':
        out += "&quot;";
        break;
      case '<':
        out += "&lt;";
        break;
      case '>':
        out += "&gt;";
        break;
      default:
        out += c;
    }
  }
  return out;
}

void SerializeNode(std::string& output, xmlNodePtr node, int indentLevel, int indentSpaces) {
  for (auto cur = node; cur != nullptr; cur = cur->next) {
    if (cur->type == XML_ELEMENT_NODE) {
      auto indent = std::string(static_cast<size_t>(indentLevel * indentSpaces), ' ');
      output += indent;
      output += "<";
      output += reinterpret_cast<const char*>(cur->name);

      for (auto attr = cur->properties; attr != nullptr; attr = attr->next) {
        auto* value = xmlGetProp(cur, attr->name);
        output += " ";
        output += reinterpret_cast<const char*>(attr->name);
        output += "=\"";
        if (value != nullptr) {
          // The value from xmlGetProp is already entity-escaped for attribute context.
          // But we need to ensure proper escaping: xmlGetProp returns decoded value,
          // so we re-escape for attribute output.
          auto* raw = reinterpret_cast<const char*>(value);
          for (auto* p = raw; *p != '\0'; p++) {
            switch (*p) {
              case '&':
                output += "&amp;";
                break;
              case '<':
                output += "&lt;";
                break;
              case '"':
                output += "&quot;";
                break;
              case '\n':
                output += "&#10;";
                break;
              case '\r':
                output += "&#13;";
                break;
              case '\t':
                output += "&#9;";
                break;
              default:
                output += *p;
                break;
            }
          }
          xmlFree(value);
        }
        output += "\"";
      }

      if (cur->children != nullptr) {
        output += ">\n";
        SerializeNode(output, cur->children, indentLevel + 1, indentSpaces);
        output += indent;
        output += "</";
        output += reinterpret_cast<const char*>(cur->name);
        output += ">\n";
      } else {
        output += "/>\n";
      }
    } else if (cur->type == XML_COMMENT_NODE) {
      auto indent = std::string(static_cast<size_t>(indentLevel * indentSpaces), ' ');
      auto* content = reinterpret_cast<const char*>(cur->content);
      if (content != nullptr) {
        output += indent;
        output += "<!--";
        output += content;
        output += "-->\n";
      }
    }
  }
}

const char* NodeTypeName(NodeType type) {
  switch (type) {
    case NodeType::Layer:
      return "Layer";
    case NodeType::Group:
      return "Group";
    case NodeType::TextBox:
      return "TextBox";
    case NodeType::Rectangle:
      return "Rectangle";
    case NodeType::Ellipse:
      return "Ellipse";
    case NodeType::Path:
      return "Path";
    case NodeType::Polystar:
      return "Polystar";
    case NodeType::Text:
      return "Text";
    case NodeType::TextPath:
      return "TextPath";
    case NodeType::Fill:
      return "Fill";
    case NodeType::Stroke:
      return "Stroke";
    case NodeType::TrimPath:
      return "TrimPath";
    case NodeType::RoundCorner:
      return "RoundCorner";
    case NodeType::MergePath:
      return "MergePath";
    case NodeType::Repeater:
      return "Repeater";
    case NodeType::PathData:
      return "PathData";
    case NodeType::LinearGradient:
      return "LinearGradient";
    case NodeType::RadialGradient:
      return "RadialGradient";
    case NodeType::ConicGradient:
      return "ConicGradient";
    case NodeType::DiamondGradient:
      return "DiamondGradient";
    case NodeType::Composition:
      return "Composition";
    case NodeType::Image:
      return "Image";
    case NodeType::BlurFilter:
      return "BlurFilter";
    case NodeType::DropShadowStyle:
      return "DropShadowStyle";
    case NodeType::InnerShadowStyle:
      return "InnerShadowStyle";
    case NodeType::BackgroundBlurStyle:
      return "BackgroundBlurStyle";
    default:
      return "Node";
  }
}

const char* LayoutModeName(LayoutMode mode) {
  switch (mode) {
    case LayoutMode::Horizontal:
      return "horizontal";
    case LayoutMode::Vertical:
      return "vertical";
    default:
      return nullptr;
  }
}

const char* AlignmentName(Alignment alignment) {
  switch (alignment) {
    case Alignment::Start:
      return "start";
    case Alignment::Center:
      return "center";
    case Alignment::End:
      return "end";
    default:
      return nullptr;
  }
}

const char* ArrangementName(Arrangement arrangement) {
  switch (arrangement) {
    case Arrangement::Center:
      return "center";
    case Arrangement::End:
      return "end";
    case Arrangement::SpaceBetween:
      return "spaceBetween";
    case Arrangement::SpaceEvenly:
      return "spaceEvenly";
    case Arrangement::SpaceAround:
      return "spaceAround";
    default:
      return nullptr;
  }
}

void WritePaddingAttr(std::ostream& os, const Padding& padding) {
  if (padding.isZero()) {
    return;
  }
  bool allEqual = (padding.top == padding.right && padding.right == padding.bottom &&
                   padding.bottom == padding.left);
  bool vhEqual = (padding.top == padding.bottom && padding.left == padding.right);
  if (allEqual) {
    os << " padding=\"" << static_cast<int>(padding.top) << "\"";
  } else if (vhEqual) {
    os << " padding=\"" << static_cast<int>(padding.top) << "," << static_cast<int>(padding.left)
       << "\"";
  } else {
    os << " padding=\"" << static_cast<int>(padding.top) << "," << static_cast<int>(padding.right)
       << "," << static_cast<int>(padding.bottom) << "," << static_cast<int>(padding.left) << "\"";
  }
}

void WriteLayoutAttrs(std::ostream& os, LayoutMode layout, float gap, float flex,
                      const Padding& padding, Alignment alignment, Arrangement arrangement,
                      bool includeInLayout, bool clipToBounds) {
  auto* layoutName = LayoutModeName(layout);
  if (layoutName != nullptr) {
    os << " layout=\"" << layoutName << "\"";
  }
  if (gap != 0) {
    os << " gap=\"" << static_cast<int>(gap) << "\"";
  }
  if (flex != 0) {
    os << " flex=\"" << static_cast<int>(flex) << "\"";
  }
  WritePaddingAttr(os, padding);
  auto* alignmentName = AlignmentName(alignment);
  if (alignmentName != nullptr) {
    os << " alignment=\"" << alignmentName << "\"";
  }
  auto* arrangementName = ArrangementName(arrangement);
  if (arrangementName != nullptr) {
    os << " arrangement=\"" << arrangementName << "\"";
  }
  if (!includeInLayout) {
    os << " includeInLayout=\"false\"";
  }
  if (clipToBounds) {
    os << " clipToBounds=\"true\"";
  }
}

void WriteBoundsAttr(std::ostream& os, float x, float y, float width, float height) {
  os << " bounds=\"" << static_cast<int>(x) << "," << static_cast<int>(y) << ","
     << static_cast<int>(width) << "," << static_cast<int>(height) << "\"";
}

}  // namespace pagx::cli
