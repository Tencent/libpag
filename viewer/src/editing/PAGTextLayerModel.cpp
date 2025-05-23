/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

void PAGTextLayerModel::setFile(const std::shared_ptr<PAGFile>& pagFile,
                                const std::string& filePath) {
  Q_UNUSED(filePath);
  beginResetModel();
  textLayers.clear();
  revertSet.clear();
  endResetModel();

  if (pagFile == nullptr) {
    return;
  }

  this->pagFile = pagFile;
  auto editableList = pagFile->getEditableIndices(LayerType::Text);
  if (editableList.empty()) {
    return;
  }

  beginResetModel();
  for (auto i : editableList) {
    textLayers.append(pagFile->getTextData(i));
  }
  endResetModel();
}

void PAGTextLayerModel::revertText(int index) {
  if (index < 0 || index >= textLayers.count()) {
    return;
  }
  pagFile->replaceText(convertIndex(index), nullptr);
  beginResetModel();
  revertSet.remove(index);
  textLayers.removeAt(index);
  auto newTextDocument = pagFile->getTextData(convertIndex(index));
  textLayers.insert(index, newTextDocument);
  endResetModel();
}

int PAGTextLayerModel::convertIndex(int index) {
  auto editableIndices = pagFile->getEditableIndices(LayerType::Text);
  if ((index < 0) || (index >= static_cast<int>(editableIndices.size()))) {
    return index;
  }

  return editableIndices.at(index);
}

QStringList PAGTextLayerModel::getFontFamilies() {
  if (fontFamilies.count() > 0) {
    return fontFamilies;
  }

  fontFamilies = QFontDatabase::families();
  fontFamilies.removeDuplicates();
  return fontFamilies;
}

QStringList PAGTextLayerModel::getFontStyles(const QString& fontFamily) {
  if (fontFamily.isEmpty()) {
    return {};
  }

  return QFontDatabase::styles(fontFamily);
}

bool PAGTextLayerModel::getFauxBold(int index) {
  if (index < 0 || index >= textLayers.size()) {
    return false;
  }

  return textLayers.at(index)->fauxBold;
}

bool PAGTextLayerModel::getFauxItalic(int index) {
  if (index < 0 || index >= textLayers.size()) {
    return false;
  }

  return textLayers.at(index)->fauxItalic;
}

double PAGTextLayerModel::getFontSize(int index) {
  if (index < 0 || index >= textLayers.size()) {
    return 0.0;
  }

  return textLayers.at(index)->fontSize;
}

double PAGTextLayerModel::getStrokeWidth(int index) {
  if (index < 0 || index >= textLayers.size()) {
    return 0.0;
  }

  return textLayers.at(index)->strokeWidth;
}

QString PAGTextLayerModel::getText(int index) {
  if (index < 0 || index >= textLayers.size()) {
    return "";
  }

  return QString::fromStdString(textLayers.at(index)->text);
}

QString PAGTextLayerModel::getFontStyle(int index) {
  if (index < 0 || index >= textLayers.size()) {
    return "";
  }

  return QString::fromStdString(textLayers.at(index)->fontStyle);
}

QString PAGTextLayerModel::getFontFamily(int index) {
  if (index < 0 || index >= textLayers.size()) {
    return "";
  }

  return QString::fromStdString(textLayers.at(index)->fontFamily);
}

QString PAGTextLayerModel::getFillColor(int index) {
  if (index < 0 || index >= textLayers.size()) {
    return "";
  }

  return Utils::PAGColorToQString(textLayers.at(index)->fillColor);
}

QString PAGTextLayerModel::getStrokeColor(int index) {
  if (index < 0 || index >= textLayers.size()) {
    return "";
  }

  return Utils::PAGColorToQString(textLayers.at(index)->strokeColor);
}

void PAGTextLayerModel::changeText(int index, const QString& text) {
  if (index < 0 || index >= textLayers.size()) {
    return;
  }
  auto textDocument = textLayers.at(index);
  if (textDocument->text == text) {
    return;
  }
  auto textData = pagFile->getTextData(convertIndex(index))->text;
  if (textData != text.toStdString()) {
    revertSet.insert(index);
  }
  textDocument->text = text.toStdString();
  beginResetModel();
  endResetModel();
  pagFile->replaceText(convertIndex(index), textDocument);
}

void PAGTextLayerModel::changeFontSize(int index, double fontSize) {
  if (index < 0 || index >= textLayers.size()) {
    return;
  }

  auto textDocument = textLayers.at(index);
  textDocument->fontSize = static_cast<float>(fontSize);
  pagFile->replaceText(convertIndex(index), textDocument);
}

void PAGTextLayerModel::changeFontStyle(int index, const QString& fontStyle) {
  if (index < 0 || index >= textLayers.size()) {
    return;
  }

  auto textDocument = textLayers.at(index);
  textDocument->fontStyle = fontStyle.toStdString();
  pagFile->replaceText(convertIndex(index), textDocument);
}

void PAGTextLayerModel::changeFontFamily(int index, const QString& fontFamily) {
  if (index < 0 || index >= textLayers.size()) {
    return;
  }

  auto textDocument = textLayers.at(index);
  textDocument->fontFamily = fontFamily.toStdString();
  pagFile->replaceText(convertIndex(index), textDocument);
}

void PAGTextLayerModel::changeFillColor(int index, const QString& color) {
  if (index < 0 || index >= textLayers.size()) {
    return;
  }

  auto textDocument = textLayers.at(index);
  textDocument->fillColor = Utils::QStringToPAGColor(color);
  pagFile->replaceText(convertIndex(index), textDocument);
}

void PAGTextLayerModel::changeStrokeColor(int index, const QString& color) {
  if (index < 0 || index >= textLayers.size()) {
    return;
  }

  auto textDocument = textLayers.at(index);
  textDocument->strokeColor = Utils::QStringToPAGColor(color);
  pagFile->replaceText(convertIndex(index), textDocument);
}

void PAGTextLayerModel::changeStrokeWidth(int index, double width) {
  if (index < 0 || index >= textLayers.size()) {
    return;
  }

  auto textDocument = textLayers.at(index);
  textDocument->applyStroke = true;
  textDocument->strokeWidth = static_cast<float>(width);
  pagFile->replaceText(convertIndex(index), textDocument);
}

void PAGTextLayerModel::changeFauxBold(int index, bool bold) {
  if (index < 0 || index >= textLayers.size()) {
    return;
  }

  auto textDocument = textLayers.at(index);
  textDocument->fauxBold = bold;
  pagFile->replaceText(convertIndex(index), textDocument);
}

void PAGTextLayerModel::changeFauxItalic(int index, bool italic) {
  if (index < 0 || index >= textLayers.size()) {
    return;
  }

  auto textDocument = textLayers.at(index);
  textDocument->fauxItalic = italic;
  pagFile->replaceText(convertIndex(index), textDocument);
}

void PAGTextLayerModel::recordTextDocument(int index) {
  if (index < 0 || index >= textLayers.size()) {
    return;
  }
  auto textDocument = textLayers.at(index);
  copyTextDocument(textDocument.get(), &oldTextDocument);
}

void PAGTextLayerModel::updateTextDocument(int index) {
  if (index < 0 || index >= textLayers.size()) {
    return;
  }
  auto textData = pagFile->getTextData(convertIndex(index));
  auto textDucoument = textLayers.at(index);
  if (compareTextDocument(textDucoument.get(), &oldTextDocument)) {
    revertSet.remove(index);
  } else {
    revertSet.insert(index);
  }

  beginResetModel();
  endResetModel();
}

void PAGTextLayerModel::revertTextDocument(int index) {
  if (index < 0 || index >= textLayers.size()) {
    return;
  }

  auto textDocument = textLayers.at(index);
  copyTextDocument(&oldTextDocument, textDocument.get());

  auto defaultTextDocument = pagFile->getTextData(convertIndex(index));
  if (compareTextDocument(defaultTextDocument.get(), textDocument.get())) {
    revertSet.remove(index);
  } else {
    revertSet.insert(index);
  }

  beginResetModel();
  endResetModel();
}

QHash<int, QByteArray> PAGTextLayerModel::roleNames() const {
  static QHash<int, QByteArray> roles = {
      {static_cast<int>(PAGTextLayerRoles::ValueRole), "value"},
      {static_cast<int>(PAGTextLayerRoles::RevertRole), "canRevert"}};
  return roles;
}

void PAGTextLayerModel::copyTextDocument(TextDocument* oldTextDocument,
                                         TextDocument* newTextDocument) {
  newTextDocument->text = oldTextDocument->text;
  newTextDocument->fauxBold = oldTextDocument->fauxBold;
  newTextDocument->fauxItalic = oldTextDocument->fauxItalic;
  newTextDocument->fontSize = oldTextDocument->fontSize;
  newTextDocument->fontFamily = oldTextDocument->fontFamily;
  newTextDocument->fontStyle = oldTextDocument->fontStyle;
  newTextDocument->fillColor = oldTextDocument->fillColor;
  newTextDocument->strokeColor = oldTextDocument->strokeColor;
  if (oldTextDocument->applyStroke) {
    newTextDocument->applyStroke = true;
    newTextDocument->strokeWidth = oldTextDocument->strokeWidth;
  } else {
    newTextDocument->applyStroke = false;
    newTextDocument->strokeWidth = 0;
  }
}

bool PAGTextLayerModel::compareTextDocument(TextDocument* oldTextDocument,
                                            TextDocument* newTextDocument) {
  if ((oldTextDocument == nullptr) || (newTextDocument == nullptr)) {
    return false;
  }

  if (oldTextDocument->applyStroke != newTextDocument->applyStroke) {
    return false;
  }
  if (oldTextDocument->strokeWidth != newTextDocument->strokeWidth) {
    return false;
  }
  if (oldTextDocument->strokeColor != newTextDocument->strokeColor) {
    return false;
  }
  if (oldTextDocument->fauxBold != newTextDocument->fauxBold) {
    return false;
  }
  if (oldTextDocument->fauxItalic != newTextDocument->fauxItalic) {
    return false;
  }
  if (oldTextDocument->text != newTextDocument->text) {
    return false;
  }
  if (oldTextDocument->fontSize != newTextDocument->fontSize) {
    return false;
  }
  if (oldTextDocument->fontFamily != newTextDocument->fontFamily) {
    return false;
  }
  if (oldTextDocument->fontStyle != newTextDocument->fontStyle) {
    return false;
  }

  return true;
}

}  // namespace pag