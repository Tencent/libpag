/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 Tencent. All rights reserved.
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
#include "pag/pag.h"

namespace pag {

class PAGTextLayerModel : public QAbstractListModel {
  Q_OBJECT
 public:
  enum class PAGTextLayerRoles { ValueRole = Qt::UserRole + 1, RevertRole };

  explicit PAGTextLayerModel(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent) const Q_DECL_OVERRIDE;
  QVariant data(const QModelIndex& index, int role) const Q_DECL_OVERRIDE;

  Q_SIGNAL void textChanged();

  Q_SLOT void setPAGFile(std::shared_ptr<PAGFile> pagFile);

  Q_INVOKABLE void revertText(int index);
  Q_INVOKABLE int convertIndex(int index);

  Q_INVOKABLE QStringList fontFamilies();
  Q_INVOKABLE QStringList fontStyles(const QString& fontFamily);
  Q_INVOKABLE bool fauxBold(int index);
  Q_INVOKABLE bool fauxItalic(int index);
  Q_INVOKABLE double fontSize(int index);
  Q_INVOKABLE double strokeWidth(int index);
  Q_INVOKABLE QString text(int index);
  Q_INVOKABLE QString fontStyle(int index);
  Q_INVOKABLE QString fontFamily(int index);
  Q_INVOKABLE QString fillColor(int index);
  Q_INVOKABLE QString strokeColor(int index);

  Q_INVOKABLE void changeText(int index, const QString& text);
  Q_INVOKABLE void changeFontSize(int index, double fontSize);
  Q_INVOKABLE void changeFontStyle(int index, const QString& fontStyle);
  Q_INVOKABLE void changeFontFamily(int index, const QString& fontFamily);
  Q_INVOKABLE void changeFillColor(int index, const QString& color);
  Q_INVOKABLE void changeStrokeColor(int index, const QString& color);
  Q_INVOKABLE void changeStrokeWidth(int index, double width);
  Q_INVOKABLE void changeFauxBold(int index, bool bold);
  Q_INVOKABLE void changeFauxItalic(int index, bool italic);
  Q_INVOKABLE void recordTextDocument(int index);
  Q_INVOKABLE void updateTextDocument(int index);
  Q_INVOKABLE void revertTextDocument(int index);

 protected:
  QHash<int, QByteArray> roleNames() const Q_DECL_OVERRIDE;

 private:
  void replaceText(int index, std::shared_ptr<TextDocument> textData);
  bool compareTextDocument(TextDocument* oldTextDocument, TextDocument* newTextDocument);

 private:
  TextDocument oldTextDocument = {};
  QSet<int> revertSet = {};
  QStringList fontFamilyList = {};
  QList<TextDocumentHandle> textLayers = {};
  std::shared_ptr<PAGFile> _pagFile = nullptr;
};

}  // namespace pag
