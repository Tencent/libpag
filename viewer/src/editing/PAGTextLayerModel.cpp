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

#include "PAGTextLayerModel.h"
#include <QFontDatabase>
#include <QRegularExpression>
#include "utils/StringUtils.h"

namespace pag {

PAGTextLayerModel::PAGTextLayerModel(QObject* parent) : QAbstractListModel(parent) {
}

int PAGTextLayerModel::rowCount(const QModelIndex& parent) const {
  Q_UNUSED(parent);
  return static_cast<int>(textLayers.count());
}

QVariant PAGTextLayerModel::data(const QModelIndex& index, int role) const {
  if ((index.row()) < 0 || (index.row() >= textLayers.count())) {
    return {};
  }

  if (role == static_cast<int>(PAGTextLayerRoles::ValueRole)) {
    return QString::fromStdString(textLayers[index.row()]->text);
  }
  if (role == static_cast<int>(PAGTextLayerRoles::RevertRole)) {
    return {revertSet.find(index.row()) != revertSet.end()};
  }

  return {};
}

void PAGTextLayerModel::setPAGFile(std::shared_ptr<PAGFile> pagFile) {
  textLayers.clear();
  revertSet.clear();

  if (pagFile == nullptr) {
    return;
  }

  _pagFile = std::move(pagFile);
  auto editableList = _pagFile->getEditableIndices(LayerType::Text);
  if (editableList.empty()) {
    return;
  }

  for (const auto& layerIndex : editableList) {
    textLayers.append(_pagFile->getTextData(layerIndex));
  }
  beginResetModel();
  endResetModel();
}

void PAGTextLayerModel::revertText(int index) {
  if (index < 0 || index >= textLayers.count()) {
    return;
  }
  replaceText(index, nullptr);
  revertSet.remove(index);
  auto newTextDocument = _pagFile->getTextData(convertIndex(index));
  textLayers[index] = newTextDocument;
  auto modelIndex = createIndex(index, 0);
  dataChanged(modelIndex, modelIndex);
}

int PAGTextLayerModel::convertIndex(int index) {
  auto editableIndices = _pagFile->getEditableIndices(LayerType::Text);
  if ((index < 0) || (index >= static_cast<int>(editableIndices.size()))) {
    return index;
  }

  return editableIndices.at(index);
}

QStringList PAGTextLayerModel::fontFamilies() {
  if (fontFamilyList.count() > 0) {
    return fontFamilyList;
  }

  fontFamilyList = QFontDatabase::families();
  fontFamilyList.removeDuplicates();
  return fontFamilyList;
}

QStringList PAGTextLayerModel::fontStyles(const QString& fontFamily) {
  if (fontFamily.isEmpty()) {
    return {};
  }

  auto styles = QFontDatabase::styles(fontFamily);
  styles.removeDuplicates();
  return styles;
}

bool PAGTextLayerModel::fauxBold(int index) {
  if (index < 0 || index >= textLayers.size()) {
    return false;
  }

  return textLayers.at(index)->fauxBold;
}

bool PAGTextLayerModel::fauxItalic(int index) {
  if (index < 0 || index >= textLayers.size()) {
    return false;
  }

  return textLayers.at(index)->fauxItalic;
}

double PAGTextLayerModel::fontSize(int index) {
  if (index < 0 || index >= textLayers.size()) {
    return 0.0;
  }

  return textLayers.at(index)->fontSize;
}

double PAGTextLayerModel::strokeWidth(int index) {
  if (index < 0 || index >= textLayers.size()) {
    return 0.0;
  }

  return textLayers.at(index)->strokeWidth;
}

QString PAGTextLayerModel::text(int index) {
  if (index < 0 || index >= textLayers.size()) {
    return "";
  }

  return QString::fromStdString(textLayers.at(index)->text);
}

QString PAGTextLayerModel::fontStyle(int index) {
  if (index < 0 || index >= textLayers.size()) {
    return "";
  }

  return QString::fromStdString(textLayers.at(index)->fontStyle);
}

QString PAGTextLayerModel::fontFamily(int index) {
  if (index < 0 || index >= textLayers.size()) {
    return "";
  }

  return QString::fromStdString(textLayers.at(index)->fontFamily);
}

QString PAGTextLayerModel::fillColor(int index) {
  if (index < 0 || index >= textLayers.size()) {
    return "";
  }

  return Utils::ColorToQString(textLayers.at(index)->fillColor);
}

QString PAGTextLayerModel::strokeColor(int index) {
  if (index < 0 || index >= textLayers.size()) {
    return "";
  }

  return Utils::ColorToQString(textLayers.at(index)->strokeColor);
}

void PAGTextLayerModel::changeText(int index, const QString& text) {
  if (index < 0 || index >= textLayers.size()) {
    return;
  }
  auto textDocument = textLayers.at(index);
  if (textDocument->text == text) {
    return;
  }
  auto textData = _pagFile->getTextData(convertIndex(index))->text;
  if (textData != text.toStdString()) {
    revertSet.insert(index);
  }
  textDocument->text = text.toStdString();
  replaceText(index, std::move(textDocument));
  auto modelIndex = createIndex(index, 0);
  dataChanged(modelIndex, modelIndex);
}

void PAGTextLayerModel::changeFontSize(int index, double fontSize) {
  if (index < 0 || index >= textLayers.size()) {
    return;
  }

  auto textDocument = textLayers.at(index);
  textDocument->fontSize = static_cast<float>(fontSize);
  replaceText(index, std::move(textDocument));
}

void PAGTextLayerModel::changeFontStyle(int index, const QString& fontStyle) {
  if (index < 0 || index >= textLayers.size()) {
    return;
  }

  auto textDocument = textLayers.at(index);
  textDocument->fontStyle = fontStyle.toStdString();
  replaceText(index, std::move(textDocument));
}

void PAGTextLayerModel::changeFontFamily(int index, const QString& fontFamily) {
  if (index < 0 || index >= textLayers.size()) {
    return;
  }

  auto textDocument = textLayers.at(index);
  textDocument->fontFamily = fontFamily.toStdString();
  replaceText(index, std::move(textDocument));
}

void PAGTextLayerModel::changeFillColor(int index, const QString& color) {
  if (index < 0 || index >= textLayers.size()) {
    return;
  }

  auto textDocument = textLayers.at(index);
  textDocument->fillColor = Utils::QStringToColor(color);
  replaceText(index, std::move(textDocument));
}

void PAGTextLayerModel::changeStrokeColor(int index, const QString& color) {
  if (index < 0 || index >= textLayers.size()) {
    return;
  }

  auto textDocument = textLayers.at(index);
  textDocument->strokeColor = Utils::QStringToColor(color);
  replaceText(index, std::move(textDocument));
}

void PAGTextLayerModel::changeStrokeWidth(int index, double width) {
  if (index < 0 || index >= textLayers.size()) {
    return;
  }

  auto textDocument = textLayers.at(index);
  textDocument->applyStroke = true;
  textDocument->strokeWidth = static_cast<float>(width);
  replaceText(index, std::move(textDocument));
}

void PAGTextLayerModel::changeFauxBold(int index, bool bold) {
  if (index < 0 || index >= textLayers.size()) {
    return;
  }

  auto textDocument = textLayers.at(index);
  textDocument->fauxBold = bold;
  replaceText(index, std::move(textDocument));
}

void PAGTextLayerModel::changeFauxItalic(int index, bool italic) {
  if (index < 0 || index >= textLayers.size()) {
    return;
  }

  auto textDocument = textLayers.at(index);
  textDocument->fauxItalic = italic;
  replaceText(index, std::move(textDocument));
}

void PAGTextLayerModel::recordTextDocument(int index) {
  if (index < 0 || index >= textLayers.size()) {
    return;
  }
  auto textDocument = textLayers.at(index);
  oldTextDocument = *textDocument;
}

void PAGTextLayerModel::updateTextDocument(int index) {
  if (index < 0 || index >= textLayers.size()) {
    return;
  }
  auto textData = _pagFile->getTextData(convertIndex(index));
  auto textDucoument = textLayers.at(index);
  if (compareTextDocument(textDucoument.get(), &oldTextDocument)) {
    revertSet.remove(index);
  } else {
    revertSet.insert(index);
  }

  auto modelIndex = createIndex(index, 0);
  dataChanged(modelIndex, modelIndex);
}

void PAGTextLayerModel::revertTextDocument(int index) {
  if (index < 0 || index >= textLayers.size()) {
    return;
  }

  auto textDocument = textLayers.at(index);
  *textDocument = oldTextDocument;

  auto defaultTextDocument = _pagFile->getTextData(convertIndex(index));
  if (compareTextDocument(defaultTextDocument.get(), textDocument.get())) {
    revertSet.remove(index);
  } else {
    revertSet.insert(index);
  }

  auto modelIndex = createIndex(index, 0);
  dataChanged(modelIndex, modelIndex);
}

QHash<int, QByteArray> PAGTextLayerModel::roleNames() const {
  static QHash<int, QByteArray> roles = {
      {static_cast<int>(PAGTextLayerRoles::ValueRole), "value"},
      {static_cast<int>(PAGTextLayerRoles::RevertRole), "canRevert"}};
  return roles;
}

void PAGTextLayerModel::replaceText(int index, std::shared_ptr<TextDocument> textData) {
  _pagFile->replaceText(convertIndex(index), std::move(textData));
  updateTextDocument(index);
  Q_EMIT textChanged();
}

bool PAGTextLayerModel::compareTextDocument(TextDocument* oldTextDocument,
                                            TextDocument* newTextDocument) {
  if ((oldTextDocument == nullptr) || (newTextDocument == nullptr)) {
    return false;
  }

  return std::tie(
             oldTextDocument->applyFill, oldTextDocument->applyStroke,
             oldTextDocument->baselineShift, oldTextDocument->boxText, oldTextDocument->boxTextPos,
             oldTextDocument->boxTextSize, oldTextDocument->firstBaseLine,
             oldTextDocument->fauxBold, oldTextDocument->fauxItalic, oldTextDocument->fillColor,
             oldTextDocument->fontFamily, oldTextDocument->fontStyle, oldTextDocument->fontSize,
             oldTextDocument->strokeColor, oldTextDocument->strokeOverFill,
             oldTextDocument->strokeWidth, oldTextDocument->text, oldTextDocument->justification,
             oldTextDocument->leading, oldTextDocument->tracking, oldTextDocument->backgroundColor,
             oldTextDocument->backgroundAlpha, oldTextDocument->direction) ==
         std::tie(
             newTextDocument->applyFill, newTextDocument->applyStroke,
             newTextDocument->baselineShift, newTextDocument->boxText, newTextDocument->boxTextPos,
             newTextDocument->boxTextSize, newTextDocument->firstBaseLine,
             newTextDocument->fauxBold, newTextDocument->fauxItalic, newTextDocument->fillColor,
             newTextDocument->fontFamily, newTextDocument->fontStyle, newTextDocument->fontSize,
             newTextDocument->strokeColor, newTextDocument->strokeOverFill,
             newTextDocument->strokeWidth, newTextDocument->text, newTextDocument->justification,
             newTextDocument->leading, newTextDocument->tracking, newTextDocument->backgroundColor,
             newTextDocument->backgroundAlpha, newTextDocument->direction);
}

}  // namespace pag
