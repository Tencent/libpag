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
#ifndef TEXTEDIT_H
#define TEXTEDIT_H
#include <QtCore/QObject>
#include <QtGui/QFont>
#include <QtGui/QFontMetrics>
#include "ErrorList/ErrorListModel.h"
#include "ExportCommonConfig.h"

class TextEdit : public QObject {
  Q_OBJECT
 public:
  explicit TextEdit(QObject* parent = nullptr);
  ~TextEdit() override;

  /**
   * 获取文本内容宽度
   * @return
   */
  static int getTextContentWidth(const QFontMetrics& fontMetrics, const QString& textContent);

  static int getTextContentWidth(const QString& textContent);

  static int getTextContentWidth(const QString& textContent, int fontSize);

  static int getTextContentWidth(const QString& textContent, QPainter* painter,
                                 int fontSize = LAYER_NAME_FONT_SIZE);

  /**
   * 字符大于指定宽度则裁剪为省略号
   */
  static QString elideText(QFont& font, int width, QString& strInfo,
                           Qt::TextElideMode elideMode = Qt::ElideMiddle);

  Q_INVOKABLE static QString elideText(QString info, const int& fontSize, const int& textWidth,
                                       Qt::TextElideMode elideMode = Qt::ElideMiddle);

  /**
   * 错误信息首行增加缩进
   * @param indentDistance 需要缩进距离
   * @param info 错误信息文本
   * @param fontSize 字体大小
   * @return 增加完缩进内容的最终显示文案
   */
  Q_INVOKABLE static ErrorLayerNameData* addFirstLineIndent(int indentDistance, QString info,
                                                            int fontSize = LAYER_NAME_FONT_SIZE,
                                                            QPainter* painter = nullptr);
  static void initPenToPainter(int fontSize, QPainter* painter);

 private:
  static QFont* getDefaultFont();
  static QFont* defaultFont;
};

#endif  //TEXTEDIT_H
