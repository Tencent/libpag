/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "TextEdit.h"
#include <limits.h>
#include <QtCore/QDebug>
#include <QtGui/QPainter>

TextEdit::TextEdit(QObject* parent) {
}

TextEdit::~TextEdit() noexcept {
}

int TextEdit::getTextContentWidth(const QFontMetrics& fontMetrics, const QString& textContent) {
  int pixelsWidth = fontMetrics.horizontalAdvance(textContent);
  return pixelsWidth;
}

int TextEdit::getTextContentWidth(const QString& textContent) {
  QFont* font = getDefaultFont();
  QFontMetrics fontMetrics(*font);
  return getTextContentWidth(fontMetrics, textContent);
}

int TextEdit::getTextContentWidth(const QString& textContent, int fontSize) {
  QFont* font = getDefaultFont();
  font->setPixelSize(fontSize);
  QFontMetrics fontMetrics(*font);
  return getTextContentWidth(fontMetrics, textContent);
}

int TextEdit::getTextContentWidth(const QString& textContent, QPainter* painter, int fontSize) {
  painter->save();
  initPenToPainter(fontSize, painter);
  const QFontMetrics fontMetrics = painter->fontMetrics();
  int textWidth = getTextContentWidth(textContent);
  painter->restore();
  return textWidth;
}

QString TextEdit::elideText(QFont& font, int width, QString& strInfo, Qt::TextElideMode elideMode) {
  QFontMetrics fontMetrics(font);
  if (fontMetrics.horizontalAdvance(strInfo) > width) {
    strInfo = QFontMetrics(font).elidedText(strInfo, elideMode, width);
  }
  return strInfo;
}

QString TextEdit::elideText(QString info, const int& fontSize, const int& textWidth,
                            Qt::TextElideMode elideMode) {
  QFont* font = getDefaultFont();
  font->setPixelSize(fontSize);
  return elideText(*font, textWidth, info, elideMode);
}

ErrorLayerNameData* TextEdit::addFirstLineIndent(int indentDistance, QString info, int fontSize,
                                                 QPainter* painter) {
  const QString space(" ");
  QString finalShow("");

  QFont* font = getDefaultFont();
  QFontMetrics fontMetrics(*font);
  if (painter) {
    painter->save();
    initPenToPainter(fontSize, painter);
    fontMetrics = painter->fontMetrics();
  }
  int addSpaceCount = 0;
  while (TextEdit::getTextContentWidth(fontMetrics, finalShow) < indentDistance) {
    finalShow.append(space);
    addSpaceCount++;
  }
  finalShow.append(info);
  if (painter) {
    painter->restore();
  }
  ErrorLayerNameData* layerNameData = new ErrorLayerNameData();
  layerNameData->layerName = finalShow;
  layerNameData->addSpaceCount = addSpaceCount;
  return layerNameData;
}

QFont* TextEdit::getDefaultFont() {
  QFont* font = new QFont();
  font->setFamily(PINGFANG_SC_FONT_FAMILY);
  font->setPixelSize(LAYER_NAME_FONT_SIZE);
  return font;
}

void TextEdit::initPenToPainter(int fontSize, QPainter* painter) {
  QFont font;
  font.setPixelSize(fontSize);
  font.setFamily(PINGFANG_SC_FONT_FAMILY);
  painter->setFont(font);
  auto pen = new QPen();
  pen->setWidth(1);
  pen->setStyle(Qt::SolidLine);
  pen->setColor(Qt::white);
  painter->setPen(*pen);
}
