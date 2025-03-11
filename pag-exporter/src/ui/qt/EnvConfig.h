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
#ifndef ENVCONFIG_H
#define ENVCONFIG_H
#include <QtWidgets/QApplication>
#include <string>
QApplication* SetupQT();
QApplication* CurrentUseQtApp();
std::string GetPAGExporterVersion(); // 获取导出插件的版本号
std::string GetSystemVersion(); // 获取系统版本号
std::string GetAuthorName(); // 获取用户名称

std::string QStringToString(const std::string& str);

void PreviewPagFile(const std::string& filePath);
#endif  //ENVCONFIG_H
