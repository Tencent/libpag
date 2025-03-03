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
#include <QtCore/QString>
#include <stdio.h>
#include <math.h>
#include "ExportProgressItem.h"
#include "StringUtil.h"
#include "src/ui/qt/TextEdit/TextEdit.h"


static const int INFO_LINE_WIDTH = 538;
static const int INFO_LINE_HEIGHT = 20;
static const int INFO_FONT_SIZE = 14;

ExportProgressItem::ExportProgressItem(std::string inputPagName) {
  pagName = inputPagName;
}

void ExportProgressItem::updateErrorTextInfo(std::string newErrorInfo) {
  int linesNum = 0;
  std::vector<std::string> errorInfoList = StringUtil::Split(newErrorInfo, "\n");
  for (int i = 0;i < errorInfoList.size();i++) {
    int infoWidth = TextEdit::getTextContentWidth(QString::fromStdString(errorInfoList[i]), INFO_FONT_SIZE);
    linesNum += ceil((float)(infoWidth) / (float)(INFO_LINE_WIDTH));
  }
  errorInfo = newErrorInfo;
  textHeight = linesNum * INFO_LINE_HEIGHT;
  itemHeight = textHeight + DEFAULT_ITEM_HEIGHT;
}