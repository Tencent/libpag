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
#include <AEGP_SuiteHandler.h>
#include <QString>
#include <string>
#include "pag/file.h"
#include "pag/types.h"

namespace exporter {

extern const std::string CompositionBmpSuffix;

std::string AeMemoryHandleToString(const AEGP_MemHandle& handle);

std::string DeleteLastSpace(const std::string& text);

std::vector<std::string> Split(const std::string& text, const std::string& separator);

std::string ToLowerCase(const std::string& text);

std::string ToUpperCase(const std::string& text);

std::string ReplaceAll(const std::string& text, const std::string& from, const std::string& to);

std::string GetMapValue(const std::unordered_map<std::string, std::string>& map,
                        const std::string& key);

bool StringToBoolean(const std::string& value, bool defaultValue);

float StringToFloat(const std::string& value, float defaultValue);

pag::Point StringToPoint(const std::vector<std::string>& pointArr, const pag::Point& defaultValue);

pag::Color StringToColor(const std::vector<std::string>& colorArr, const pag::Color& defaultValue);

std::string FormatString(const std::string& value, const std::string& defaultValue);

pag::ParagraphJustification IntToParagraphJustification(int justification,
                                                        pag::ParagraphJustification defaultValue);

float CalculateLineSpacing(const std::vector<std::string>& locArr, float fontSize);

float CalculateFirstBaseline(const std::vector<std::string>& locArr, float lineHeight,
                             float baselineShift, bool isVertical);

std::string ConvertStringEncoding(const std::string& str);

std::string Utf16ToUtf8(const char16_t* u16str);

std::u16string Utf8ToUtf16(const std::string& u8str);

std::string GetJavaScriptFromQRC(const QString& jsPath);

bool IsEndWidthSuffix(const std::string& str, const std::string& suffix);

QString ColorToQString(pag::Color color);

void EnsureStringSuffix(std::string& filePath, const std::string& suffix);

}  // namespace exporter
