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

#include <QAbstractListModel>
#include <QString>
#include <mutex>
#include <vector>

namespace pag {

/**
 * A QAbstractListModel that provides XML content line by line with syntax highlighting.
 * This enables efficient rendering of large XML files using QML ListView with virtual scrolling.
 */
class XmlLinesModel : public QAbstractListModel {
  Q_OBJECT
  Q_PROPERTY(int lineCount READ lineCount NOTIFY lineCountChanged)
  Q_PROPERTY(QString plainText READ plainText NOTIFY plainTextChanged)
  Q_PROPERTY(qreal maxLineWidth READ maxLineWidth NOTIFY maxLineWidthChanged)
  Q_PROPERTY(qreal charWidth READ charWidth CONSTANT)

 public:
  // Character width for Menlo 13px font, used for cursor positioning and width calculations
  static constexpr qreal CharWidth = 7.8;

  enum Roles {
    LineNumberRole = Qt::UserRole + 1,
    PlainTextRole,
    HighlightedTextRole,
  };

  explicit XmlLinesModel(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  int lineCount() const;
  QString plainText() const;
  qreal maxLineWidth() const;
  qreal charWidth() const {
    return CharWidth;
  }

  Q_INVOKABLE void setContent(const QString& xmlContent);
  Q_INVOKABLE void clear();

  // Get line at specific index (for editing support)
  Q_INVOKABLE QString lineAt(int index) const;

  // Set line at specific index (for single-line editing)
  Q_INVOKABLE void setLineText(int index, const QString& text);

  // Get the character position where a line starts (for cursor positioning)
  Q_INVOKABLE int lineStartPosition(int lineIndex) const;

  // Get all content as plain text (for saving)
  Q_INVOKABLE QString allText() const;

 Q_SIGNALS:
  void lineCountChanged();
  void plainTextChanged();
  void maxLineWidthChanged();
  void contentModified();

 private:
  std::vector<QString> lines = {};
  mutable std::vector<QString> highlightedLines = {};
  mutable std::mutex highlightCacheMutex = {};
  QString fullText = {};
  qreal _maxLineWidth = 0;
};

}  // namespace pag
