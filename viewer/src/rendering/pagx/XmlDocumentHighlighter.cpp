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
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "rendering/pagx/XmlDocumentHighlighter.h"
#include <QRegularExpression>

namespace pag {

// Matches name="value" / name='value' / bare name inside a tag. Kept in the .cpp file because
// moc's lexer cannot handle C++ raw string literals, which corrupted namespace tracking for
// every header on this include chain.
static const QRegularExpression AttributePattern =
    QRegularExpression("(\\s+)([a-zA-Z_:][-a-zA-Z0-9_:.]*)(\\s*=\\s*)(\"[^\"]*\"|'[^']*')?");

static QTextCharFormat MakeColorFormat(const char* hexColor) {
  QTextCharFormat format = {};
  format.setForeground(QColor(hexColor));
  return format;
}

XmlDocumentHighlighter::XmlDocumentHighlighter(QTextDocument* document)
    : QSyntaxHighlighter(document) {
  tagFormat = MakeColorFormat("#569CD6");
  attrNameFormat = MakeColorFormat("#9CDCFE");
  attrValueFormat = MakeColorFormat("#CE9178");
  commentFormat = MakeColorFormat("#6A9955");
  grayFormat = MakeColorFormat("#808080");
  textFormat = MakeColorFormat("#D4D4D4");
}

void XmlDocumentHighlighter::highlightBlock(const QString& text) {
  const auto length = static_cast<int>(text.length());
  auto state = previousBlockState();
  if (state < StateNormal || state > StateInTag) {
    state = StateNormal;
  }
  setCurrentBlockState(StateNormal);

  if (length > MaxHighlightLength) {
    setFormat(0, length, textFormat);
    return;
  }

  auto index = 0;
  while (index < length) {
    if (state != StateNormal) {
      // Continuation of a multi-line construct started on an earlier line: paint the whole
      // region with the construct's color until its terminator is found on this line.
      const char* terminator = ">";
      const QTextCharFormat* format = &tagFormat;
      switch (state) {
        case StateInComment:
          terminator = "-->";
          format = &commentFormat;
          break;
        case StateInCData:
          terminator = "]]>";
          format = &grayFormat;
          break;
        case StateInProcessingInstruction:
          terminator = "?>";
          format = &grayFormat;
          break;
        default:
          break;
      }
      const auto end = text.indexOf(QLatin1String(terminator), index);
      if (end < 0) {
        setFormat(index, length - index, *format);
        setCurrentBlockState(state);
        return;
      }
      const auto stop = end + static_cast<int>(qstrlen(terminator));
      setFormat(index, stop - index, *format);
      index = stop;
      state = StateNormal;
      continue;
    }

    if (text.mid(index, 4) == QLatin1String("<!--")) {
      const auto end = text.indexOf(QLatin1String("-->"), index);
      if (end < 0) {
        setFormat(index, length - index, commentFormat);
        setCurrentBlockState(StateInComment);
        return;
      }
      setFormat(index, end + 3 - index, commentFormat);
      index = end + 3;
      continue;
    }

    if (text.mid(index, 9) == QLatin1String("<![CDATA[")) {
      const auto end = text.indexOf(QLatin1String("]]>"), index);
      if (end < 0) {
        setFormat(index, length - index, grayFormat);
        setCurrentBlockState(StateInCData);
        return;
      }
      setFormat(index, end + 3 - index, grayFormat);
      index = end + 3;
      continue;
    }

    if (text.mid(index, 2) == QLatin1String("<?")) {
      const auto end = text.indexOf(QLatin1String("?>"), index);
      if (end < 0) {
        setFormat(index, length - index, grayFormat);
        setCurrentBlockState(StateInProcessingInstruction);
        return;
      }
      setFormat(index, end + 2 - index, grayFormat);
      index = end + 2;
      continue;
    }

    if (text.at(index) == u'<') {
      const auto tagEnd = text.indexOf(u'>', index);
      if (tagEnd < 0) {
        // A tag split across lines keeps the tag color on this line and resumes after the
        // closing '>' is found on a later line.
        setFormat(index, length - index, tagFormat);
        setCurrentBlockState(StateInTag);
        return;
      }
      highlightTag(text, index, tagEnd);
      index = tagEnd + 1;
      continue;
    }

    auto nextTag = text.indexOf(u'<', index);
    if (nextTag < 0) {
      nextTag = length;
    }
    setFormat(index, nextTag - index, textFormat);
    index = nextTag;
  }
}

void XmlDocumentHighlighter::highlightTag(const QString& text, int start, int end) {
  if (text.mid(start, 2) == QLatin1String("</")) {
    // Closing tags keep a single tag color for the whole region, matching the web editor.
    setFormat(start, end - start + 1, tagFormat);
    return;
  }
  setFormat(start, 1, tagFormat);
  auto nameStart = start + 1;
  auto nameEnd = nameStart;
  while (nameEnd < end && !text.at(nameEnd).isSpace()) {
    ++nameEnd;
  }
  setFormat(nameStart, nameEnd - nameStart, tagFormat);
  auto tailStart = end;
  if (end > nameStart && text.at(end - 1) == u'/') {
    tailStart = end - 1;
  }
  if (nameEnd < tailStart) {
    highlightAttributes(text, nameEnd, tailStart);
  }
  setFormat(tailStart, end - tailStart + 1, tagFormat);
}

void XmlDocumentHighlighter::highlightAttributes(const QString& text, int from, int to) {
  const auto segment = text.mid(from, to - from);
  auto iterator = AttributePattern.globalMatch(segment);
  while (iterator.hasNext()) {
    const auto match = iterator.next();
    const auto nameLength = static_cast<int>(match.capturedLength(2));
    if (nameLength > 0) {
      setFormat(from + static_cast<int>(match.capturedStart(2)), nameLength, attrNameFormat);
    }
    const auto valueLength = static_cast<int>(match.capturedLength(4));
    if (valueLength > 0) {
      setFormat(from + static_cast<int>(match.capturedStart(4)), valueLength, attrValueFormat);
    }
  }
}

}  // namespace pag
