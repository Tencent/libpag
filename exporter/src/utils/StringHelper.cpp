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

#include "StringHelper.h"
#include <QFile>
#include <QTextStream>
#include <iostream>
#include "AEHelper.h"
#include "src/base/utils/Log.h"

namespace exporter {

const std::string CompositionBmpSuffix = "_bmp";

std::string AeMemoryHandleToString(const AEGP_MemHandle& handle) {
  if (handle == nullptr) {
    return "";
  }
  const auto& suites = GetSuites();

  char16_t* str = nullptr;
  suites->MemorySuite1()->AEGP_LockMemHandle(handle, reinterpret_cast<void**>(&str));
  std::string u8Str = Utf16ToUtf8(str);
  suites->MemorySuite1()->AEGP_UnlockMemHandle(handle);
  if (u8Str.empty() && str != nullptr && str[0] != u'\0') {
    LOGE("AeMemoryHandleToString failed to convert UTF-16 content to UTF-8.");
  }

  return u8Str;
}

bool LastIsSpace(const std::string& text) {
  if (text.empty()) {
    return false;
  }
  char last = text.back();
  return last == ' ' || last == '\n' || last == '\t' || last == '\r' || last == '\v' ||
         last == '\f';
}

std::string DeleteLastSpace(const std::string& text) {
  std::string result = text;
  while (!result.empty() && LastIsSpace(result)) {
    result.pop_back();
  }
  return result;
}

std::vector<std::string> Split(const std::string& text, const std::string& separator) {
  std::vector<std::string> result;
  if (text.empty() || separator.empty()) {
    result.push_back(text);
    return result;
  }

  size_t pos1 = 0;
  size_t pos2 = text.find(separator);
  while (pos2 != std::string::npos) {
    result.push_back(text.substr(pos1, pos2 - pos1));
    pos1 = pos2 + separator.size();
    pos2 = text.find(separator, pos1);
  }
  if (pos1 < text.length()) {
    result.push_back(text.substr(pos1));
  }

  return result;
}

std::string ToLowerCase(const std::string& text) {
  std::string result(text);
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return result;
}

std::string ToUpperCase(const std::string& text) {
  std::string result(text);
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char c) { return std::toupper(c); });
  return result;
}

std::string ReplaceAll(const std::string& text, const std::string& from, const std::string& to) {
  if (from.empty()) {
    return text;
  }
  std::string str = text;
  size_t startPos = 0;
  const size_t fromLength = from.length();
  const size_t toLength = to.length();

  while ((startPos = str.find(from, startPos)) != std::string::npos) {
    str.replace(startPos, fromLength, to);
    startPos += toLength;
  }
  return str;
}

std::string GetMapValue(const std::unordered_map<std::string, std::string>& map,
                        const std::string& key) {
  const auto& result = map.find(key);
  return result != map.end() ? result->second : "";
}

bool StringToBoolean(const std::string& value, const bool defaultValue) {
  if (value.empty()) {
    return defaultValue;
  }
  auto lowerValue = ToLowerCase(value);
  return lowerValue == "true";
}

float StringToFloat(const std::string& value, const float defaultValue) {
  if (value.empty()) {
    return defaultValue;
  }
  try {
    return std::stof(value);
  } catch (const std::invalid_argument&) {
    return defaultValue;
  } catch (const std::out_of_range&) {
    return defaultValue;
  }
}

pag::ParagraphJustification IntToParagraphJustification(
    int justification, const pag::ParagraphJustification defaultValue) {
  if (justification < 0) {
    return defaultValue;
  }

  switch (justification) {  // justification represents the text alignment method in After Effects.
    case 7414:
      return pag::ParagraphJustification::RightJustify;
    case 7415:
      return pag::ParagraphJustification::CenterJustify;
    case 7416:
      return pag::ParagraphJustification::FullJustifyLastLineLeft;
    case 7417:
      return pag::ParagraphJustification::FullJustifyLastLineRight;
    case 7418:
      return pag::ParagraphJustification::FullJustifyLastLineCenter;
    case 7419:
      return pag::ParagraphJustification::FullJustifyLastLineFull;
    default:
      return pag::ParagraphJustification::LeftJustify;
  }
}

pag::Point StringToPoint(const std::vector<std::string>& pointArr, const pag::Point& defaultValue) {
  if (pointArr.size() < 2) {
    return defaultValue;
  }
  pag::Point point = {};
  try {
    point.x = static_cast<float>(std::stod(pointArr[0]));
    point.y = static_cast<float>(std::stod(pointArr[1]));
  } catch (const std::invalid_argument&) {
    return defaultValue;
  } catch (const std::out_of_range&) {
    return defaultValue;
  }
  return point;
}

pag::Color StringToColor(const std::vector<std::string>& colorArr, const pag::Color& defaultValue) {
  if (colorArr.size() < 3) {
    return defaultValue;
  }
  pag::Color color = {};
  try {
    color.red = static_cast<uint8_t>(std::stod(colorArr[0]) * 255);
    color.green = static_cast<uint8_t>(std::stod(colorArr[1]) * 255);
    color.blue = static_cast<uint8_t>(std::stod(colorArr[2]) * 255);
  } catch (const std::invalid_argument&) {
    return defaultValue;
  } catch (const std::out_of_range&) {
    return defaultValue;
  }
  return color;
}

std::string FormatString(const std::string& value, const std::string& defaultValue) {
  if (value.empty()) {
    return defaultValue;
  }
  return ReplaceAll(value, "\\n", "\n");
}

float CalculateLineSpacing(const std::vector<std::string>& locArr, float fontSize) {
  if (locArr.size() < 6) {
    return 0;
  }
  try {
    auto firstY = static_cast<float>(std::stod(locArr[1]));
    auto secondY = static_cast<float>(std::stod(locArr[5]));
    auto leading = roundf(secondY - firstY);
    if (leading == roundf(fontSize * 1.2f)) {
      return 0;
    }
    if (leading > 100000000) {
      return 0;
    }
    return leading;
  } catch (const std::exception&) {
    return 0;
  }
}

float CalculateFirstBaseline(const std::vector<std::string>& locArr, float lineHeight,
                             float baselineShift, bool isVertical) {
  if (locArr.size() < 4) {
    return 0;
  }

  const auto lineCount = static_cast<int>(floorf(static_cast<float>(locArr.size()) / 4.0f));
  if (lineCount < 1) {
    return 0;
  }
  const auto index = (lineCount - 1) * 4;
  float firstBaseLine;
  try {
    if (isVertical) {
      firstBaseLine = static_cast<float>(std::stod(locArr[0]));
      if (fabsf(firstBaseLine) > 100000000) {
        auto lastBaseLine = static_cast<float>(std::stod(locArr[index + 0]));
        firstBaseLine = lastBaseLine + lineHeight * (lineCount - 1);
      }
    } else {
      firstBaseLine = static_cast<float>(std::stod(locArr[1]));
      if (fabsf(firstBaseLine) > 100000000) {
        auto lastBaseLine = static_cast<float>(std::stod(locArr[index + 1]));
        firstBaseLine = lastBaseLine - lineHeight * (lineCount - 1);
      }
    }
  } catch (const std::exception&) {
    firstBaseLine = 0;
  }

  if (fabsf(firstBaseLine) > 100000000) {
    firstBaseLine = 0;
  }
  return isVertical ? (firstBaseLine - baselineShift) : (firstBaseLine + baselineShift);
}

std::string ConvertStringEncoding(const std::string& str) {
  return QString::fromStdString(str).toLocal8Bit().toStdString();
}

std::string Utf16ToUtf8(const char16_t* u16str) {
  if (u16str == nullptr) {
    return "";
  }
  QString qString = QString::fromUtf16(u16str);
  return qString.toUtf8().toStdString();
}

std::u16string Utf8ToUtf16(const std::string& u8str) {
  std::u16string u16str;
  size_t i = 0;
  while (i < u8str.size()) {
    char32_t codePoint = 0;
    unsigned char c = u8str[i++];

    if (c <= 0x7F) {
      codePoint = c;
    } else if ((c & 0xE0) == 0xC0) {
      if (i >= u8str.size()) throw std::runtime_error("Invalid UTF-8 sequence");
      codePoint = ((c & 0x1F) << 6) | (u8str[i++] & 0x3F);
    } else if ((c & 0xF0) == 0xE0) {
      if (i + 1 >= u8str.size()) throw std::runtime_error("Invalid UTF-8 sequence");
      codePoint = ((c & 0x0F) << 12) | ((u8str[i] & 0x3F) << 6) | (u8str[i + 1] & 0x3F);
      i += 2;
    } else if ((c & 0xF8) == 0xF0) {
      if (i + 2 >= u8str.size()) throw std::runtime_error("Invalid UTF-8 sequence");
      codePoint = ((c & 0x07) << 18) | ((u8str[i] & 0x3F) << 12) | ((u8str[i + 1] & 0x3F) << 6) |
                  (u8str[i + 2] & 0x3F);
      i += 3;
    } else {
      throw std::runtime_error("Invalid UTF-8 sequence");
    }

    if (codePoint <= 0xFFFF) {
      u16str.push_back(static_cast<char16_t>(codePoint));
    } else if (codePoint <= 0x10FFFF) {
      codePoint -= 0x10000;
      u16str.push_back(static_cast<char16_t>(0xD800 | (codePoint >> 10)));
      u16str.push_back(static_cast<char16_t>(0xDC00 | (codePoint & 0x3FF)));
    } else {
      throw std::runtime_error("Invalid Unicode code point");
    }
  }
  return u16str;
}

std::string GetJavaScriptFromQRC(const QString& jsPath) {
  QFile jsFile(jsPath);
  if (!jsFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return "";
  }

  QTextStream in(&jsFile);
  in.setEncoding(QStringConverter::Encoding::Utf8);
  QString jsContent = in.readAll();
  jsFile.close();
  return jsContent.toStdString();
}

bool IsEndWidthSuffix(const std::string& str, const std::string& suffix) {
  if (str.length() < suffix.length()) {
    return false;
  }

  std::string newStr = str.substr(str.length() - suffix.length(), suffix.length());
  std::transform(newStr.begin(), newStr.end(), newStr.begin(), ::tolower);

  return newStr == suffix;
}

QString ColorToQString(pag::Color color) {
  return QString("#%1%2%3")
      .arg(color.red, 2, 16, QChar('0'))
      .arg(color.green, 2, 16, QChar('0'))
      .arg(color.blue, 2, 16, QChar('0'));
}

void EnsureStringSuffix(std::string& filePath, const std::string& suffix) {
  if (filePath.size() >= suffix.size()) {
    auto tail = filePath.substr(filePath.size() - suffix.size());
    if (std::equal(tail.begin(), tail.end(), suffix.begin(), suffix.end(),
                   [](char a, char b) { return std::tolower(a) == std::tolower(b); })) {
      return;
    }
  }
  filePath += suffix;
}

}  // namespace exporter
