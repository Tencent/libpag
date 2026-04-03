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

#pragma once

#include <QString>

namespace pag {

/**
 * Utility class for XML syntax highlighting.
 * Provides static methods to convert XML text to HTML with syntax highlighting.
 */
class XmlSyntaxHighlighter {
 public:
  // Color constants for syntax highlighting (VS Code dark theme style)
  static const QString TagColor;        // Blue for tag names
  static const QString AttrNameColor;   // Light blue for attribute names
  static const QString AttrValueColor;  // Orange for attribute values
  static const QString CommentColor;    // Green for comments
  static const QString XmlDeclColor;    // Gray for XML declarations
  static const QString TextColor;       // Light gray for text content
  static const QString CDataColor;      // Gray for CDATA sections

  // Maximum line length for detailed highlighting (performance optimization)
  static constexpr qsizetype MaxHighlightLength = 10000;

  /**
   * Highlight a single line of XML with HTML span tags for coloring.
   * For lines exceeding MaxHighlightLength, returns a truncated preview.
   */
  static QString highlightLine(const QString& line);

  /**
   * Escape HTML special characters (&, <, >, ", space).
   */
  static QString escapeHtml(const QString& text);

 private:
  // Highlight a complete XML tag (e.g., <tagName attr="value">)
  static QString highlightTag(const QString& tagStr);

  // Highlight the attribute portion of a tag
  static QString highlightAttributes(const QString& attrStr);
};

}  // namespace pag
