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

#include "rendering/pagx/XmlSyntaxHighlighter.h"
#include <QRegularExpression>

namespace pag {

// Color constants matching VS Code dark theme
const QString XmlSyntaxHighlighter::TagColor = "#569CD6";
const QString XmlSyntaxHighlighter::AttrNameColor = "#9CDCFE";
const QString XmlSyntaxHighlighter::AttrValueColor = "#CE9178";
const QString XmlSyntaxHighlighter::CommentColor = "#6A9955";
const QString XmlSyntaxHighlighter::XmlDeclColor = "#808080";
const QString XmlSyntaxHighlighter::TextColor = "#D4D4D4";
const QString XmlSyntaxHighlighter::CDataColor = "#808080";

QString XmlSyntaxHighlighter::escapeHtml(const QString& text) {
  QString result;
  result.reserve(text.size() * 1.1);

  for (const QChar& c : text) {
    switch (c.unicode()) {
      case '&':
        result += "&amp;";
        break;
      case '<':
        result += "&lt;";
        break;
      case '>':
        result += "&gt;";
        break;
      case '"':
        result += "&quot;";
        break;
      case ' ':
        result += "&nbsp;";
        break;
      default:
        result += c;
        break;
    }
  }
  return result;
}

QString XmlSyntaxHighlighter::highlightLine(const QString& line) {
  if (line.isEmpty()) {
    return "<br>";
  }

  // For very long lines (e.g., base64 image data), skip detailed highlighting
  if (line.length() > MaxHighlightLength) {
    QString preview = escapeHtml(line.left(200));
    int remaining = line.length() - 200;
    return QString(
               "<span style='color:%1;'>%2</span>"
               "<span style='color:#808080;font-style:italic;'> ... (%3 more characters, line too "
               "long for syntax highlighting)</span>")
        .arg(TextColor, preview, QString::number(remaining));
  }

  QString result;
  result.reserve(line.size() * 3);

  auto colorSpan = [](const QString& text, const QString& color) {
    return QString("<span style='color:%1;'>%2</span>").arg(color, escapeHtml(text));
  };

  int i = 0;
  int len = line.length();

  while (i < len) {
    // Check for XML declaration <?xml ... ?>
    if (line.mid(i, 5) == "<?xml") {
      int end = line.indexOf("?>", i);
      if (end != -1) {
        end += 2;
        result += colorSpan(line.mid(i, end - i), XmlDeclColor);
        i = end;
        continue;
      }
    }

    // Check for comments <!-- ... -->
    if (line.mid(i, 4) == "<!--") {
      int end = line.indexOf("-->", i);
      if (end != -1) {
        end += 3;
        result += colorSpan(line.mid(i, end - i), CommentColor);
        i = end;
        continue;
      } else {
        result += colorSpan(line.mid(i), CommentColor);
        break;
      }
    }

    // Check for CDATA <![CDATA[ ... ]]>
    if (line.mid(i, 9) == "<![CDATA[") {
      int end = line.indexOf("]]>", i);
      if (end != -1) {
        end += 3;
        result += colorSpan(line.mid(i, end - i), CDataColor);
        i = end;
        continue;
      } else {
        result += colorSpan(line.mid(i), CDataColor);
        break;
      }
    }

    // Check for tags < ... >
    if (line[i] == '<') {
      int tagEnd = line.indexOf('>', i);
      if (tagEnd != -1) {
        QString tagContent = line.mid(i, tagEnd - i + 1);
        result += highlightTag(tagContent);
        i = tagEnd + 1;
        continue;
      } else {
        // Unclosed tag extends to end of line — highlight it as a tag
        result += colorSpan(line.mid(i), TagColor);
        break;
      }
    }

    // Regular text content until next tag
    int nextTag = line.indexOf('<', i);
    if (nextTag == -1) {
      nextTag = len;
    }
    if (nextTag > i) {
      result += colorSpan(line.mid(i, nextTag - i), TextColor);
      i = nextTag;
    } else {
      result += escapeHtml(QString(line[i]));
      i++;
    }
  }

  return result;
}

QString XmlSyntaxHighlighter::highlightTag(const QString& tagStr) {
  QString result;

  // Handle closing tags </tagName>
  if (tagStr.startsWith("</")) {
    QString tagName = tagStr.mid(2, tagStr.length() - 3);
    result =
        QString("<span style='color:%1;'>&lt;/%2&gt;</span>").arg(TagColor, escapeHtml(tagName));
    return result;
  }

  // Handle self-closing and opening tags
  bool isSelfClosing = tagStr.endsWith("/>");
  QString innerContent = tagStr.mid(1, tagStr.length() - (isSelfClosing ? 3 : 2));

  result += QString("<span style='color:%1;'>&lt;</span>").arg(TagColor);

  // Parse tag name and attributes
  int spaceIdx = -1;
  for (int i = 0; i < innerContent.length(); ++i) {
    if (innerContent[i].isSpace()) {
      spaceIdx = i;
      break;
    }
  }

  QString tagName = (spaceIdx == -1) ? innerContent : innerContent.left(spaceIdx);
  result += QString("<span style='color:%1;'>%2</span>").arg(TagColor, escapeHtml(tagName));

  if (spaceIdx != -1) {
    QString attrPart = innerContent.mid(spaceIdx);
    result += highlightAttributes(attrPart);
  }

  if (isSelfClosing) {
    result += QString("<span style='color:%1;'>/&gt;</span>").arg(TagColor);
  } else {
    result += QString("<span style='color:%1;'>&gt;</span>").arg(TagColor);
  }

  return result;
}

QString XmlSyntaxHighlighter::highlightAttributes(const QString& attrStr) {
  QString result;

  // Match attribute pattern: name="value" or name='value'
  static QRegularExpression regex(
      R"((\s+)([a-zA-Z_:][-a-zA-Z0-9_:.]*)(\s*=\s*)("[^"]*"|'[^']*')?)");

  int lastIndex = 0;
  auto matchIterator = regex.globalMatch(attrStr);

  while (matchIterator.hasNext()) {
    auto match = matchIterator.next();

    // Add any skipped content
    if (match.capturedStart() > lastIndex) {
      QString skipped = attrStr.mid(lastIndex, match.capturedStart() - lastIndex);
      result += escapeHtml(skipped);
    }

    QString whitespace = match.captured(1);
    QString attrName = match.captured(2);
    QString equals = match.captured(3);
    QString attrValue = match.captured(4);

    result += escapeHtml(whitespace);
    result += QString("<span style='color:%1;'>%2</span>").arg(AttrNameColor, escapeHtml(attrName));
    result += QString("<span style='color:%1;'>%2</span>").arg(TextColor, escapeHtml(equals));
    if (!attrValue.isEmpty()) {
      result +=
          QString("<span style='color:%1;'>%2</span>").arg(AttrValueColor, escapeHtml(attrValue));
    }

    lastIndex = match.capturedEnd();
  }

  // Add remaining content
  if (lastIndex < attrStr.length()) {
    result += escapeHtml(attrStr.mid(lastIndex));
  }

  return result;
}

}  // namespace pag
