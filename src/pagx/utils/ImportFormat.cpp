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

#include "pagx/utils/ImportFormat.h"
#include <cctype>
#include <unordered_set>
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/Layer.h"

namespace pagx {

namespace {

std::string ToLower(std::string text) {
  for (auto& ch : text) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return text;
}

// htm/xhtml are the same product as html, and a fragment rooted at <body> is still html.
std::string NormalizeFormatName(const std::string& name) {
  auto lowered = ToLower(name);
  if (lowered == "htm" || lowered == "xhtml" || lowered == "body") {
    return "html";
  }
  return lowered;
}

std::string ExtensionOf(const std::string& source) {
  auto slash = source.find_last_of("/\\");
  auto dot = source.find_last_of('.');
  if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) {
    return {};
  }
  return source.substr(dot + 1);
}

// Returns the lowercased tag name of the first element in the content, skipping closing tags,
// comments and processing instructions. Empty when the content carries no element at all.
std::string FirstElementTagName(const std::string& content) {
  auto pos = content.find('<');
  while (pos != std::string::npos && pos + 1 < content.size()) {
    size_t end = std::string::npos;
    size_t suffixLength = 1;
    if (content.compare(pos, 4, "<!--") == 0) {
      end = content.find("-->", pos + 4);
      suffixLength = 3;
    } else if (content.compare(pos, 9, "<![CDATA[") == 0) {
      end = content.find("]]>", pos + 9);
      suffixLength = 3;
    } else if (content[pos + 1] == '?') {
      end = content.find("?>", pos + 2);
      suffixLength = 2;
    } else if (content[pos + 1] == '!') {
      char quote = 0;
      int subsetDepth = 0;
      for (size_t i = pos + 2; i < content.size(); i++) {
        char ch = content[i];
        if (quote != 0) {
          if (ch == quote) {
            quote = 0;
          }
        } else if (ch == '\'' || ch == '"') {
          quote = ch;
        } else if (ch == '[') {
          subsetDepth++;
        } else if (ch == ']' && subsetDepth > 0) {
          subsetDepth--;
        } else if (ch == '>' && subsetDepth == 0) {
          end = i;
          break;
        }
      }
    } else if (content[pos + 1] == '/') {
      end = content.find('>', pos + 2);
    } else {
      auto tagEnd = content.find_first_of(" \t\r\n\f/>", pos + 1);
      if (tagEnd != std::string::npos && tagEnd > pos + 1) {
        return ToLower(content.substr(pos + 1, tagEnd - pos - 1));
      }
      return {};
    }
    if (end == std::string::npos) {
      return {};
    }
    pos = content.find('<', end + suffixLength);
  }
  return {};
}

bool HasUnresolvedImports(const PAGXDocument* document,
                          std::unordered_set<const PAGXDocument*>& visited) {
  if (document == nullptr || !visited.insert(document).second) {
    return false;
  }
  for (const auto& node : document->nodes) {
    if (node->nodeType() != NodeType::Layer) {
      continue;
    }
    const auto* layer = static_cast<const Layer*>(node.get());
    if (LayerHasUnresolvedImport(layer) ||
        HasUnresolvedImports(layer->externalDoc.get(), visited)) {
      return true;
    }
  }
  return false;
}

bool HasConflictingSources(const PAGXDocument* document,
                           std::unordered_set<const PAGXDocument*>& visited) {
  if (document == nullptr || !visited.insert(document).second) {
    return false;
  }
  for (const auto& node : document->nodes) {
    if (node->nodeType() != NodeType::Layer) {
      continue;
    }
    const auto* layer = static_cast<const Layer*>(node.get());
    if (ImportSourcesConflict(LayerHasDeclaredComposition(layer, document),
                              !layer->importDirective.source.empty()) ||
        HasConflictingSources(layer->externalDoc.get(), visited)) {
      return true;
    }
  }
  return false;
}

}  // namespace

bool ImportSourcesConflict(bool hasComposition, bool hasImportSource) {
  return hasComposition && hasImportSource;
}

bool LayerHasDeclaredComposition(const Layer* layer, const PAGXDocument* document) {
  return layer != nullptr &&
         (!layer->compositionFilePath.empty() ||
          (layer->composition != nullptr && !document->isMountedComposition(layer->composition)));
}

bool DocumentHasConflictingImportSources(const PAGXDocument* document) {
  std::unordered_set<const PAGXDocument*> visited = {};
  return HasConflictingSources(document, visited);
}

bool DocumentHasUnresolvedImports(const PAGXDocument* document) {
  std::unordered_set<const PAGXDocument*> visited = {};
  return HasUnresolvedImports(document, visited);
}

std::string ResolveImportFormat(const std::string& format, const std::string& source,
                                const std::string& content) {
  if (!format.empty()) {
    return NormalizeFormatName(format);
  }
  auto extension = ExtensionOf(source);
  if (!extension.empty()) {
    return NormalizeFormatName(extension);
  }
  return NormalizeFormatName(FirstElementTagName(content));
}

bool LayerHasUnresolvedImport(const Layer* layer) {
  if (layer == nullptr) {
    return false;
  }
  const auto& directive = layer->importDirective;
  if (directive.source.empty() && directive.content.empty()) {
    return false;
  }
  return !directive.source.empty() ||
         (layer->composition == nullptr && layer->compositionFilePath.empty());
}

}  // namespace pagx
