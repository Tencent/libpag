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

#include "rendering/pagx/XmlLinesModel.h"
#include "rendering/pagx/XmlSyntaxHighlighter.h"

namespace pag {

XmlLinesModel::XmlLinesModel(QObject* parent) : QAbstractListModel(parent) {
}

int XmlLinesModel::rowCount(const QModelIndex& parent) const {
  if (parent.isValid()) {
    return 0;
  }
  return static_cast<int>(lines.size());
}

QVariant XmlLinesModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(lines.size())) {
    return {};
  }

  const auto& line = lines[static_cast<size_t>(index.row())];

  switch (role) {
    case LineNumberRole:
      return index.row() + 1;
    case PlainTextRole:
      return line;
    case HighlightedTextRole:
      return XmlSyntaxHighlighter::highlightLine(line);
    default:
      return {};
  }
}

QHash<int, QByteArray> XmlLinesModel::roleNames() const {
  return {
      {LineNumberRole, "lineNumber"},
      {PlainTextRole, "plainText"},
      {HighlightedTextRole, "highlightedText"},
  };
}

int XmlLinesModel::lineCount() const {
  return static_cast<int>(lines.size());
}

QString XmlLinesModel::plainText() const {
  return fullText;
}

qreal XmlLinesModel::maxLineWidth() const {
  return _maxLineWidth;
}

void XmlLinesModel::setContent(const QString& xmlContent) {
  beginResetModel();

  fullText = xmlContent;

  // Split by newlines, preserving empty lines
  const auto lineList = xmlContent.split('\n');

  // Release memory if new content is significantly smaller (less than half the previous capacity)
  // This prevents memory bloat when switching from large files to small files
  auto newSize = static_cast<size_t>(lineList.size());
  if (newSize < lines.capacity() / 2) {
    lines = std::vector<QString>();  // Release old memory completely
  } else {
    lines.clear();
  }
  lines.reserve(newSize);
  int maxLength = 0;
  for (const auto& line : lineList) {
    lines.push_back(line);
    if (line.length() > maxLength) {
      maxLength = line.length();
    }
  }

  // Estimate max line width using the constant character width
  _maxLineWidth = static_cast<qreal>(maxLength) * CharWidth;

  endResetModel();

  Q_EMIT lineCountChanged();
  Q_EMIT plainTextChanged();
  Q_EMIT maxLineWidthChanged();
}

void XmlLinesModel::clear() {
  beginResetModel();
  lines.clear();
  fullText.clear();
  _maxLineWidth = 0;
  endResetModel();
  Q_EMIT lineCountChanged();
  Q_EMIT plainTextChanged();
  Q_EMIT maxLineWidthChanged();
}

QString XmlLinesModel::lineAt(int index) const {
  if (index < 0 || index >= static_cast<int>(lines.size())) {
    return {};
  }
  return lines[static_cast<size_t>(index)];
}

void XmlLinesModel::setLineText(int index, const QString& text) {
  if (index < 0 || index >= static_cast<int>(lines.size())) {
    return;
  }

  auto idx = static_cast<size_t>(index);
  if (lines[idx] == text) {
    return;  // No change
  }

  lines[idx] = text;

  // Rebuild fullText from lines
  QStringList lineList;
  lineList.reserve(static_cast<int>(lines.size()));
  for (const auto& line : lines) {
    lineList.append(line);
  }
  fullText = lineList.join('\n');

  // Update max line width if needed
  qreal lineWidth = static_cast<qreal>(text.length()) * CharWidth;
  if (lineWidth > _maxLineWidth) {
    _maxLineWidth = lineWidth;
    Q_EMIT maxLineWidthChanged();
  }

  // Notify view that this row changed
  QModelIndex modelIndex = createIndex(index, 0);
  Q_EMIT dataChanged(modelIndex, modelIndex, {PlainTextRole, HighlightedTextRole});

  Q_EMIT plainTextChanged();
  Q_EMIT contentModified();
}

int XmlLinesModel::lineStartPosition(int lineIndex) const {
  if (lineIndex < 0 || lineIndex >= static_cast<int>(lines.size())) {
    return 0;
  }
  int position = 0;
  for (int i = 0; i < lineIndex; ++i) {
    position += lines[static_cast<size_t>(i)].length() + 1;  // +1 for newline
  }
  return position;
}

QString XmlLinesModel::allText() const {
  return fullText;
}

}  // namespace pag
