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

#pragma once

#include <QSyntaxHighlighter>
#include <QTextCharFormat>

namespace pag {

/**
 * Syntax highlighter for PAGX XML source, using the VS Code Dark+ palette shared with the web
 * playground's CodeMirror editor. Highlighting runs per text block and carries a block state
 * across lines, so multi-line comments, CDATA sections, processing instructions, and tags keep
 * the correct colors when they span lines.
 */
class XmlDocumentHighlighter : public QSyntaxHighlighter {
  Q_OBJECT
 public:
  explicit XmlDocumentHighlighter(QTextDocument* document);

 protected:
  void highlightBlock(const QString& text) override;

 private:
  enum BlockState {
    StateNormal = 0,
    StateInComment,
    StateInCData,
    StateInProcessingInstruction,
    StateInTag,
  };

  // Highlights the tag spanning [start, end], where end is the index of the closing '>'.
  void highlightTag(const QString& text, int start, int end);

  // Highlights attribute name/value pairs within the [from, to) range of text.
  void highlightAttributes(const QString& text, int from, int to);

  // Lines longer than this skip token-level highlighting (e.g. embedded base64 payloads),
  // matching the previous per-line highlighter's performance guard.
  static constexpr qsizetype MaxHighlightLength = 10000;

  QTextCharFormat tagFormat;
  QTextCharFormat attrNameFormat;
  QTextCharFormat attrValueFormat;
  QTextCharFormat commentFormat;
  QTextCharFormat grayFormat;
  QTextCharFormat textFormat;
};

}  // namespace pag
