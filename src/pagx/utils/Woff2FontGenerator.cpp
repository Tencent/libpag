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

#include "pagx/utils/Woff2FontGenerator.h"
#ifdef PAG_USE_WOFF2
#include <woff2/encode.h>
#endif
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <numeric>
#include "pagx/nodes/Font.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/PathData.h"
#include "tgfx/core/Data.h"
#include "tgfx/core/ImageCodec.h"

namespace pagx {

static constexpr int MinOpenTypeUnitsPerEm = 16;
static constexpr int MaxOpenTypeUnitsPerEm = 16384;

struct FontExportMetrics {
  int sourceUnitsPerEm = 1;
  uint16_t exportUnitsPerEm = MinOpenTypeUnitsPerEm;
  float designScale = 1.0f;
  uint8_t bitmapPpem = 1;
  float bitmapMetricScale = 1.0f;
};

static uint16_t RoundToU16(float value) {
  if (!std::isfinite(value) || value <= 0.0f) {
    return 0;
  }
  if (value >= static_cast<float>(std::numeric_limits<uint16_t>::max())) {
    return std::numeric_limits<uint16_t>::max();
  }
  return static_cast<uint16_t>(std::round(value));
}

static int16_t ClampToI16(float value) {
  if (!std::isfinite(value)) {
    return 0;
  }
  if (value <= static_cast<float>(std::numeric_limits<int16_t>::min())) {
    return std::numeric_limits<int16_t>::min();
  }
  if (value >= static_cast<float>(std::numeric_limits<int16_t>::max())) {
    return std::numeric_limits<int16_t>::max();
  }
  return static_cast<int16_t>(value);
}

static FontExportMetrics ResolveFontExportMetrics(const Font* font) {
  FontExportMetrics metrics;
  if (font == nullptr) {
    return metrics;
  }
  metrics.sourceUnitsPerEm = font->unitsPerEm > 0 ? font->unitsPerEm : 1;
  metrics.exportUnitsPerEm = static_cast<uint16_t>(
      std::clamp(metrics.sourceUnitsPerEm, MinOpenTypeUnitsPerEm, MaxOpenTypeUnitsPerEm));
  metrics.designScale =
      static_cast<float>(metrics.exportUnitsPerEm) / static_cast<float>(metrics.sourceUnitsPerEm);
  auto bitmapPpem = std::clamp(metrics.sourceUnitsPerEm, 1, 255);
  metrics.bitmapPpem = static_cast<uint8_t>(bitmapPpem);
  metrics.bitmapMetricScale =
      static_cast<float>(metrics.bitmapPpem) / static_cast<float>(metrics.sourceUnitsPerEm);
  return metrics;
}

// --- Big-endian write helpers ---

static void WriteU8(std::vector<uint8_t>& buf, uint8_t v) {
  buf.push_back(v);
}

static void WriteU16(std::vector<uint8_t>& buf, uint16_t v) {
  buf.push_back(static_cast<uint8_t>(v >> 8));
  buf.push_back(static_cast<uint8_t>(v));
}

static void WriteI16(std::vector<uint8_t>& buf, int16_t v) {
  WriteU16(buf, static_cast<uint16_t>(v));
}

static void WriteU32(std::vector<uint8_t>& buf, uint32_t v) {
  buf.push_back(static_cast<uint8_t>(v >> 24));
  buf.push_back(static_cast<uint8_t>(v >> 16));
  buf.push_back(static_cast<uint8_t>(v >> 8));
  buf.push_back(static_cast<uint8_t>(v));
}

static void WriteI8(std::vector<uint8_t>& buf, int8_t v) {
  buf.push_back(static_cast<uint8_t>(v));
}

static void WriteTag(std::vector<uint8_t>& buf, const char* tag) {
  buf.push_back(static_cast<uint8_t>(tag[0]));
  buf.push_back(static_cast<uint8_t>(tag[1]));
  buf.push_back(static_cast<uint8_t>(tag[2]));
  buf.push_back(static_cast<uint8_t>(tag[3]));
}

static void PadTo4(std::vector<uint8_t>& buf) {
  while (buf.size() % 4 != 0) {
    buf.push_back(0);
  }
}

static uint32_t CalcChecksum(const uint8_t* data, size_t length) {
  uint32_t sum = 0;
  size_t numLongs = (length + 3) / 4;
  for (size_t i = 0; i < numLongs; i++) {
    uint32_t val = 0;
    for (int j = 0; j < 4; j++) {
      size_t idx = i * 4 + j;
      val <<= 8;
      if (idx < length) {
        val |= data[idx];
      }
    }
    sum += val;
  }
  return sum;
}

static bool ReadU16BE(const std::vector<uint8_t>& data, size_t offset, uint16_t* value) {
  if (value == nullptr || offset + 2 > data.size()) {
    return false;
  }
  *value = static_cast<uint16_t>((data[offset] << 8) | data[offset + 1]);
  return true;
}

static bool ReadU32BE(const std::vector<uint8_t>& data, size_t offset, uint32_t* value) {
  if (value == nullptr || offset + 4 > data.size()) {
    return false;
  }
  *value = (static_cast<uint32_t>(data[offset]) << 24) |
           (static_cast<uint32_t>(data[offset + 1]) << 16) |
           (static_cast<uint32_t>(data[offset + 2]) << 8) | static_cast<uint32_t>(data[offset + 3]);
  return true;
}

static bool FindSFNTTable(const std::vector<uint8_t>& sfnt, const char* tag, uint32_t* tableOffset,
                          uint32_t* tableLength) {
  uint16_t numTables = 0;
  if (tag == nullptr || tableOffset == nullptr || tableLength == nullptr ||
      !ReadU16BE(sfnt, 4, &numTables)) {
    return false;
  }
  size_t recordOffset = 12;
  for (uint16_t i = 0; i < numTables; i++) {
    if (recordOffset + 16 > sfnt.size()) {
      return false;
    }
    if (std::memcmp(sfnt.data() + recordOffset, tag, 4) == 0) {
      if (!ReadU32BE(sfnt, recordOffset + 8, tableOffset) ||
          !ReadU32BE(sfnt, recordOffset + 12, tableLength)) {
        return false;
      }
      return static_cast<size_t>(*tableOffset) + static_cast<size_t>(*tableLength) <= sfnt.size();
    }
    recordOffset += 16;
  }
  return false;
}

static uint16_t ReadHeadUnitsPerEm(const std::vector<uint8_t>& sfnt) {
  uint32_t tableOffset = 0;
  uint32_t tableLength = 0;
  if (!FindSFNTTable(sfnt, "head", &tableOffset, &tableLength) || tableLength < 20) {
    return 0;
  }
  uint16_t unitsPerEm = 0;
  if (!ReadU16BE(sfnt, tableOffset + 18, &unitsPerEm)) {
    return 0;
  }
  return unitsPerEm;
}

// --- CFF number encoding ---

static void EncodeCFFInt(std::vector<uint8_t>& buf, int32_t val) {
  if (val >= -107 && val <= 107) {
    WriteU8(buf, static_cast<uint8_t>(val + 139));
  } else if (val >= 108 && val <= 1131) {
    int v = val - 108;
    WriteU8(buf, static_cast<uint8_t>((v >> 8) + 247));
    WriteU8(buf, static_cast<uint8_t>(v & 0xFF));
  } else if (val >= -1131 && val <= -108) {
    int v = -val - 108;
    WriteU8(buf, static_cast<uint8_t>((v >> 8) + 251));
    WriteU8(buf, static_cast<uint8_t>(v & 0xFF));
  } else {
    // 5-byte encoding: prefix 29 + 4-byte big-endian int
    WriteU8(buf, 29);
    WriteU8(buf, static_cast<uint8_t>((val >> 24) & 0xFF));
    WriteU8(buf, static_cast<uint8_t>((val >> 16) & 0xFF));
    WriteU8(buf, static_cast<uint8_t>((val >> 8) & 0xFF));
    WriteU8(buf, static_cast<uint8_t>(val & 0xFF));
  }
}

static void EncodeCFFFixed(std::vector<uint8_t>& buf, float val) {
  // CFF 16.16 fixed point: prefix 255 + 4 bytes
  int32_t fixed = static_cast<int32_t>(std::round(val * 65536.0f));
  WriteU8(buf, 255);
  WriteU8(buf, static_cast<uint8_t>((fixed >> 24) & 0xFF));
  WriteU8(buf, static_cast<uint8_t>((fixed >> 16) & 0xFF));
  WriteU8(buf, static_cast<uint8_t>((fixed >> 8) & 0xFF));
  WriteU8(buf, static_cast<uint8_t>(fixed & 0xFF));
}

static void EncodeCFFNumber(std::vector<uint8_t>& buf, float val) {
  int32_t intVal = static_cast<int32_t>(std::round(val));
  if (std::abs(val - static_cast<float>(intVal)) < 0.001f) {
    EncodeCFFInt(buf, intVal);
  } else {
    EncodeCFFFixed(buf, val);
  }
}

// --- CFF INDEX structure ---

static void WriteOffsetValue(std::vector<uint8_t>& buf, uint32_t off, uint8_t offSize) {
  for (int i = offSize - 1; i >= 0; i--) {
    WriteU8(buf, static_cast<uint8_t>((off >> (i * 8)) & 0xFF));
  }
}

static void WriteCFFIndex(std::vector<uint8_t>& buf,
                          const std::vector<std::vector<uint8_t>>& items) {
  uint16_t count = static_cast<uint16_t>(items.size());
  WriteU16(buf, count);
  if (count == 0) {
    return;
  }
  // Calculate total data size to determine offSize
  size_t totalSize = 0;
  for (auto& item : items) {
    totalSize += item.size();
  }
  uint8_t offSize;
  if (totalSize + 1 <= 0xFF) {
    offSize = 1;
  } else if (totalSize + 1 <= 0xFFFF) {
    offSize = 2;
  } else if (totalSize + 1 <= 0xFFFFFF) {
    offSize = 3;
  } else {
    offSize = 4;
  }
  WriteU8(buf, offSize);
  // Write offset array (1-based)
  uint32_t offset = 1;
  WriteOffsetValue(buf, offset, offSize);
  for (auto& item : items) {
    offset += static_cast<uint32_t>(item.size());
    WriteOffsetValue(buf, offset, offSize);
  }
  // Write data
  for (auto& item : items) {
    buf.insert(buf.end(), item.begin(), item.end());
  }
}

// --- CFF DICT encoding helpers ---

static void EncodeDictOperand(std::vector<uint8_t>& buf, int32_t val) {
  EncodeCFFInt(buf, val);
}

static void EncodeDictOperator(std::vector<uint8_t>& buf, uint8_t op) {
  WriteU8(buf, op);
}

// --- CharString building ---

struct CFFCharStringVisitor {
  std::vector<uint8_t>& cs;
  float designScale = 1.0f;
  float curX = 0.0f;
  float curY = 0.0f;

  CFFCharStringVisitor(std::vector<uint8_t>& output, float designScale)
      : cs(output), designScale(designScale), curX(0.0f), curY(0.0f) {
  }

  float x(const Point& point) const {
    return point.x * designScale;
  }

  float y(const Point& point) const {
    return -point.y * designScale;
  }

  void operator()(PathVerb verb, const Point* pts) {
    switch (verb) {
      case PathVerb::Move: {
        float targetX = x(pts[0]);
        float targetY = y(pts[0]);
        float dx = targetX - curX;
        float dy = targetY - curY;
        EncodeCFFNumber(cs, dx);
        EncodeCFFNumber(cs, dy);
        WriteU8(cs, 21);  // rmoveto
        curX = targetX;
        curY = targetY;
        break;
      }
      case PathVerb::Line: {
        float targetX = x(pts[0]);
        float targetY = y(pts[0]);
        float dx = targetX - curX;
        float dy = targetY - curY;
        EncodeCFFNumber(cs, dx);
        EncodeCFFNumber(cs, dy);
        WriteU8(cs, 5);  // rlineto
        curX = targetX;
        curY = targetY;
        break;
      }
      case PathVerb::Quad: {
        // Degree-elevate quadratic to cubic:
        // CP1 = P0 + 2/3 * (QCP - P0)
        // CP2 = P2 + 2/3 * (QCP - P2)
        float qcpX = x(pts[0]);
        float qcpY = y(pts[0]);
        float endX = x(pts[1]);
        float endY = y(pts[1]);
        float cp1X = curX + (2.0f / 3.0f) * (qcpX - curX);
        float cp1Y = curY + (2.0f / 3.0f) * (qcpY - curY);
        float cp2X = endX + (2.0f / 3.0f) * (qcpX - endX);
        float cp2Y = endY + (2.0f / 3.0f) * (qcpY - endY);
        float dx1 = cp1X - curX;
        float dy1 = cp1Y - curY;
        float dx2 = cp2X - cp1X;
        float dy2 = cp2Y - cp1Y;
        float dx3 = endX - cp2X;
        float dy3 = endY - cp2Y;
        EncodeCFFNumber(cs, dx1);
        EncodeCFFNumber(cs, dy1);
        EncodeCFFNumber(cs, dx2);
        EncodeCFFNumber(cs, dy2);
        EncodeCFFNumber(cs, dx3);
        EncodeCFFNumber(cs, dy3);
        WriteU8(cs, 8);  // rrcurveto
        curX = endX;
        curY = endY;
        break;
      }
      case PathVerb::Cubic: {
        float cp1X = x(pts[0]);
        float cp1Y = y(pts[0]);
        float cp2X = x(pts[1]);
        float cp2Y = y(pts[1]);
        float endX = x(pts[2]);
        float endY = y(pts[2]);
        float dx1 = cp1X - curX;
        float dy1 = cp1Y - curY;
        float dx2 = cp2X - cp1X;
        float dy2 = cp2Y - cp1Y;
        float dx3 = endX - cp2X;
        float dy3 = endY - cp2Y;
        EncodeCFFNumber(cs, dx1);
        EncodeCFFNumber(cs, dy1);
        EncodeCFFNumber(cs, dx2);
        EncodeCFFNumber(cs, dy2);
        EncodeCFFNumber(cs, dx3);
        EncodeCFFNumber(cs, dy3);
        WriteU8(cs, 8);  // rrcurveto
        curX = endX;
        curY = endY;
        break;
      }
      case PathVerb::Close:
        break;
    }
  }
};

static std::vector<uint8_t> BuildCharString(const PathData* path,
                                            const FontExportMetrics& metrics) {
  std::vector<uint8_t> cs;
  if (path == nullptr || path->isEmpty()) {
    WriteU8(cs, 14);  // endchar
    return cs;
  }

  // PAGX uses screen coordinates (Y axis points down), but CFF/OpenType uses typographic
  // coordinates (Y axis points up). Negate all Y values during charstring encoding.
  CFFCharStringVisitor visitor(cs, metrics.designScale);
  path->forEach(std::ref(visitor));

  WriteU8(cs, 14);  // endchar
  return cs;
}

// --- Table builders ---

static std::vector<uint8_t> BuildCFFTopDict(uint32_t charsetOff, uint32_t charStringsOff,
                                            uint32_t privateDictSize, uint32_t privateDictOff) {
  std::vector<uint8_t> dict;
  EncodeCFFInt(dict, static_cast<int32_t>(charsetOff));
  EncodeDictOperator(dict, 15);
  EncodeCFFInt(dict, static_cast<int32_t>(charStringsOff));
  EncodeDictOperator(dict, 17);
  EncodeCFFInt(dict, static_cast<int32_t>(privateDictSize));
  EncodeCFFInt(dict, static_cast<int32_t>(privateDictOff));
  EncodeDictOperator(dict, 18);
  return dict;
}

static std::vector<uint8_t> BuildCFF(const Font* font, const std::string& familyName,
                                     const FontExportMetrics& metrics) {
  std::vector<uint8_t> cff;

  // Header: major=1, minor=0, hdrSize=4, offSize=1
  WriteU8(cff, 1);
  WriteU8(cff, 0);
  WriteU8(cff, 4);
  WriteU8(cff, 1);

  // Name INDEX
  std::vector<std::vector<uint8_t>> names;
  names.push_back(std::vector<uint8_t>(familyName.begin(), familyName.end()));
  WriteCFFIndex(cff, names);

  // Top DICT offsets are computed below after all sub-structures are serialized.

  // Build CharStrings
  uint16_t numGlyphs = static_cast<uint16_t>(font->glyphs.size() + 1);  // +1 for .notdef

  // String INDEX: CFF has 391 standard strings (SID 0-390). The charset assigns SID 1
  // through numGlyphs-1 to each glyph, so SID 391 and above need custom strings.
  static constexpr uint16_t CFFStandardStringCount = 391;
  std::vector<std::vector<uint8_t>> stringEntries;
  if (numGlyphs > CFFStandardStringCount) {
    uint16_t extraCount = numGlyphs - CFFStandardStringCount;
    stringEntries.reserve(extraCount);
    for (uint16_t i = 0; i < extraCount; i++) {
      auto name = "g" + std::to_string(CFFStandardStringCount + i);
      stringEntries.push_back(std::vector<uint8_t>(name.begin(), name.end()));
    }
  }

  // Global Subr INDEX
  std::vector<std::vector<uint8_t>> emptyIndex;

  std::vector<std::vector<uint8_t>> charStrings;
  // .notdef glyph
  std::vector<uint8_t> notdef;
  WriteU8(notdef, 14);  // endchar
  charStrings.push_back(std::move(notdef));
  // Font glyphs
  for (auto* glyph : font->glyphs) {
    charStrings.push_back(BuildCharString(glyph->path, metrics));
  }

  // Build charset (format 2: single range)
  std::vector<uint8_t> charset;
  WriteU8(charset, 2);                                      // format 2
  WriteU16(charset, 1);                                     // first SID
  WriteU16(charset, static_cast<uint16_t>(numGlyphs - 2));  // nLeft

  // Build Private DICT (minimal)
  std::vector<uint8_t> privateDict;
  // defaultWidthX = 0
  EncodeDictOperand(privateDict, 0);
  EncodeDictOperator(privateDict, 20);  // defaultWidthX (single-byte operator)
  // nominalWidthX = 0
  EncodeDictOperand(privateDict, 0);
  EncodeDictOperator(privateDict, 21);  // nominalWidthX (single-byte operator)

  // Now compute actual offsets and rebuild the CFF with correct Top DICT
  cff.clear();

  // Header
  WriteU8(cff, 1);
  WriteU8(cff, 0);
  WriteU8(cff, 4);
  WriteU8(cff, 1);

  // Name INDEX
  WriteCFFIndex(cff, names);

  // We need to figure out where things land. First, serialize the string and global subr INDEXes,
  // then the charset, charstrings, and private dict.

  // Compute Top DICT size to know where subsequent data goes.
  // We'll use a two-pass approach: first estimate, then finalize.

  // Serialize String INDEX
  std::vector<uint8_t> stringIndex;
  WriteCFFIndex(stringIndex, stringEntries);

  // Serialize Global Subr INDEX
  std::vector<uint8_t> globalSubrIndex;
  WriteCFFIndex(globalSubrIndex, emptyIndex);

  // Serialize CharStrings INDEX
  std::vector<uint8_t> charStringsData;
  WriteCFFIndex(charStringsData, charStrings);

  // Now compute offsets. Layout after header + Name INDEX:
  //   Top DICT INDEX
  //   String INDEX
  //   Global Subr INDEX
  //   charset
  //   CharStrings INDEX
  //   Private DICT

  // We need to know the Top DICT INDEX size to compute downstream offsets.
  // Top DICT needs to encode the offsets, whose byte length depends on their magnitude.
  // Use a conservative estimate first, then finalize.

  size_t afterNameIndex = cff.size();

  // Estimate top dict: we'll build it with 5-byte operands for all offsets
  auto tempDict =
      BuildCFFTopDict(0xFFFF, 0xFFFF, static_cast<uint32_t>(privateDict.size()), 0xFFFF);
  std::vector<std::vector<uint8_t>> topDictItems;
  topDictItems.push_back(tempDict);
  std::vector<uint8_t> topDictIndex;
  WriteCFFIndex(topDictIndex, topDictItems);

  size_t topDictIndexSize = topDictIndex.size();

  // Compute offsets relative to start of CFF data
  size_t baseOffset =
      afterNameIndex + topDictIndexSize + stringIndex.size() + globalSubrIndex.size();
  uint32_t charsetOffset = static_cast<uint32_t>(baseOffset);
  uint32_t charStringsOffset = static_cast<uint32_t>(baseOffset + charset.size());
  uint32_t privateDictOffset =
      static_cast<uint32_t>(baseOffset + charset.size() + charStringsData.size());

  // Rebuild top dict with real offsets
  auto finalDict = BuildCFFTopDict(charsetOffset, charStringsOffset,
                                   static_cast<uint32_t>(privateDict.size()), privateDictOffset);

  // Check if size changed (it might due to different integer encoding sizes)
  topDictItems.clear();
  topDictItems.push_back(finalDict);
  topDictIndex.clear();
  WriteCFFIndex(topDictIndex, topDictItems);

  if (topDictIndex.size() != topDictIndexSize) {
    // Offset encoding size changed (typically 1 iteration suffices to stabilize).
    topDictIndexSize = topDictIndex.size();
    baseOffset = afterNameIndex + topDictIndexSize + stringIndex.size() + globalSubrIndex.size();
    charsetOffset = static_cast<uint32_t>(baseOffset);
    charStringsOffset = static_cast<uint32_t>(baseOffset + charset.size());
    privateDictOffset = static_cast<uint32_t>(baseOffset + charset.size() + charStringsData.size());
    finalDict = BuildCFFTopDict(charsetOffset, charStringsOffset,
                                static_cast<uint32_t>(privateDict.size()), privateDictOffset);
    topDictItems.clear();
    topDictItems.push_back(finalDict);
    topDictIndex.clear();
    WriteCFFIndex(topDictIndex, topDictItems);
  }

  // Write Top DICT INDEX
  cff.insert(cff.end(), topDictIndex.begin(), topDictIndex.end());
  // Write String INDEX
  cff.insert(cff.end(), stringIndex.begin(), stringIndex.end());
  // Write Global Subr INDEX
  cff.insert(cff.end(), globalSubrIndex.begin(), globalSubrIndex.end());
  // Write charset
  cff.insert(cff.end(), charset.begin(), charset.end());
  // Write CharStrings INDEX
  cff.insert(cff.end(), charStringsData.begin(), charStringsData.end());
  // Write Private DICT
  cff.insert(cff.end(), privateDict.begin(), privateDict.end());

  return cff;
}

static std::vector<uint8_t> BuildOS2(const FontExportMetrics& metrics, uint16_t numGlyphs) {
  std::vector<uint8_t> table;
  WriteU16(table, 4);    // version
  WriteI16(table, 0);    // xAvgCharWidth (placeholder)
  WriteU16(table, 400);  // usWeightClass (Regular)
  WriteU16(table, 5);    // usWidthClass (Medium)
  WriteU16(table, 0);    // fsType
  WriteI16(table, 0);    // ySubscriptXSize
  WriteI16(table, 0);    // ySubscriptYSize
  WriteI16(table, 0);    // ySubscriptXOffset
  WriteI16(table, 0);    // ySubscriptYOffset
  WriteI16(table, 0);    // ySuperscriptXSize
  WriteI16(table, 0);    // ySuperscriptYSize
  WriteI16(table, 0);    // ySuperscriptXOffset
  WriteI16(table, 0);    // ySuperscriptYOffset
  WriteI16(table, 0);    // yStrikeoutSize
  WriteI16(table, 0);    // yStrikeoutPosition
  WriteI16(table, 0);    // sFamilyClass
  // panose (10 bytes)
  for (int i = 0; i < 10; i++) {
    WriteU8(table, 0);
  }
  // ulUnicodeRange1-4: set bit 57 (PUA) -> ulUnicodeRange3 bit 25
  WriteU32(table, 0);           // ulUnicodeRange1
  WriteU32(table, 0);           // ulUnicodeRange2
  WriteU32(table, 0x02000000);  // ulUnicodeRange3 (bit 57 = PUA)
  WriteU32(table, 0);           // ulUnicodeRange4
  WriteTag(table, "    ");      // achVendID
  WriteU16(table, 0);           // fsSelection
  WriteU16(table, 0xE000);      // usFirstCharIndex
  // Clamp to the PUA BMP segment end (0xF8FF) so we never declare a charIndex outside the
  // segment we actually map. BuildWoff2FromFont rejects fonts whose glyph count exceeds the
  // segment, so this clamp is defensive only.
  uint32_t lastChar = 0xE000u + static_cast<uint32_t>(numGlyphs) - 2u;
  WriteU16(table, static_cast<uint16_t>(std::min(0xF8FFu, lastChar)));  // usLastCharIndex
  auto upm = static_cast<int16_t>(metrics.exportUnitsPerEm);
  WriteI16(table, upm);                         // sTypoAscender
  WriteI16(table, 0);                           // sTypoDescender
  WriteI16(table, 0);                           // sTypoLineGap
  WriteU16(table, static_cast<uint16_t>(upm));  // usWinAscent
  WriteU16(table, 0);                           // usWinDescent
  WriteU32(table, 0);                           // ulCodePageRange1
  WriteU32(table, 0);                           // ulCodePageRange2
  WriteI16(table, 0);                           // sxHeight
  WriteI16(table, 0);                           // sCapHeight
  WriteU16(table, 0);                           // usDefaultChar
  WriteU16(table, 0x0020);                      // usBreakChar
  WriteU16(table, 1);                           // usMaxContext
  return table;
}

static std::vector<uint8_t> BuildCmap(uint16_t numGlyphs) {
  // numGlyphs includes .notdef; actual mapped glyphs = numGlyphs - 1
  uint16_t mappedCount = numGlyphs - 1;
  std::vector<uint8_t> table;

  // cmap header
  WriteU16(table, 0);  // version
  WriteU16(table, 1);  // numTables (1 encoding record)

  // Encoding record: platform 3 (Windows), encoding 1 (BMP)
  WriteU16(table, 3);   // platformID
  WriteU16(table, 1);   // encodingID
  WriteU32(table, 12);  // offset to subtable (after header: 4 + 8 = 12)

  // Format 4 subtable
  uint16_t segCount = 2;  // 1 real segment + 1 sentinel
  uint16_t segCountX2 = segCount * 2;
  uint16_t searchRange = 4;                        // 2 * (2^floor(log2(segCount))) = 2*2 = 4
  uint16_t entrySelector = 1;                      // floor(log2(segCount)) = 1
  uint16_t rangeShift = segCountX2 - searchRange;  // 0
  // Format 4 layout: 7 uint16 header (14B) + endCode[segCount] + reservedPad(2B)
  //                  + startCode[segCount] + idDelta[segCount] + idRangeOffset[segCount]
  uint16_t subtableLength = 14 + segCount * 2 + 2 + segCount * 2 + segCount * 2 + segCount * 2;

  WriteU16(table, 4);               // format
  WriteU16(table, subtableLength);  // length
  WriteU16(table, 0);               // language
  WriteU16(table, segCountX2);
  WriteU16(table, searchRange);
  WriteU16(table, entrySelector);
  WriteU16(table, rangeShift);

  // endCode array. mappedCount is bounded above by kMaxPUAGlyphCount in BuildWoff2FromFont, so
  // (0xE000 + mappedCount - 1) cannot exceed 0xF8FF. The clamp here is defensive in case the
  // bound is ever loosened — without it, mappedCount > 8192 would wrap uint16_t and produce a
  // segment that overlaps the sentinel 0xFFFF, breaking cmap lookup entirely.
  uint32_t endCode32 = 0xE000u + static_cast<uint32_t>(mappedCount) - 1u;
  uint16_t endCode = static_cast<uint16_t>(std::min(0xF8FFu, endCode32));
  WriteU16(table, endCode);  // segment 1 end
  WriteU16(table, 0xFFFF);   // sentinel end

  // reservedPad
  WriteU16(table, 0);

  // startCode array
  WriteU16(table, 0xE000);  // segment 1 start
  WriteU16(table, 0xFFFF);  // sentinel start

  // idDelta array
  // delta such that startCode + delta = first GlyphID (1)
  // delta = 1 - 0xE000
  int16_t idDelta = static_cast<int16_t>(1 - 0xE000);
  WriteI16(table, idDelta);  // segment 1 delta
  WriteI16(table, 1);        // sentinel delta

  // idRangeOffset array
  WriteU16(table, 0);  // segment 1
  WriteU16(table, 0);  // sentinel

  return table;
}

static std::vector<uint8_t> BuildHead(const FontExportMetrics& metrics, int16_t xMin, int16_t yMin,
                                      int16_t xMax, int16_t yMax, int16_t indexToLocFormat) {
  std::vector<uint8_t> table;
  WriteU16(table, 1);           // majorVersion
  WriteU16(table, 0);           // minorVersion
  WriteU32(table, 0x00010000);  // fontRevision (1.0 fixed)
  WriteU32(table, 0);           // checksumAdjustment (will be patched)
  WriteU32(table, 0x5F0F3CF5);  // magicNumber
  WriteU16(table, 0x000B);      // flags (baseline at y=0, lsb at x=0, integer ppem)
  WriteU16(table, metrics.exportUnitsPerEm);
  // created/modified timestamps (8 bytes each, set to 0)
  WriteU32(table, 0);
  WriteU32(table, 0);
  WriteU32(table, 0);
  WriteU32(table, 0);
  WriteI16(table, xMin);
  WriteI16(table, yMin);
  WriteI16(table, xMax);
  WriteI16(table, yMax);
  WriteU16(table, 0);                 // macStyle
  WriteU16(table, 8);                 // lowestRecPPEM
  WriteI16(table, 2);                 // fontDirectionHint
  WriteI16(table, indexToLocFormat);  // indexToLocFormat (0=short, 1=long)
  WriteI16(table, 0);                 // glyphDataFormat
  return table;
}

static std::vector<uint8_t> BuildHhea(const Font* font, const FontExportMetrics& metrics,
                                      uint16_t numGlyphs) {
  std::vector<uint8_t> table;
  auto upm = static_cast<int16_t>(metrics.exportUnitsPerEm);
  auto advanceWidthMax = metrics.exportUnitsPerEm;
  for (auto* glyph : font->glyphs) {
    advanceWidthMax = std::max(advanceWidthMax, RoundToU16(glyph->advance * metrics.designScale));
  }
  WriteU16(table, 1);                // majorVersion
  WriteU16(table, 0);                // minorVersion
  WriteI16(table, upm);              // ascender = unitsPerEm
  WriteI16(table, 0);                // descender
  WriteI16(table, 0);                // lineGap
  WriteU16(table, advanceWidthMax);  // advanceWidthMax (conservative)
  WriteI16(table, 0);                // minLeftSideBearing
  WriteI16(table, 0);                // minRightSideBearing
  WriteI16(table, upm);              // xMaxExtent
  WriteI16(table, 1);                // caretSlopeRise
  WriteI16(table, 0);                // caretSlopeRun
  WriteI16(table, 0);                // caretOffset
  WriteI16(table, 0);                // reserved
  WriteI16(table, 0);                // reserved
  WriteI16(table, 0);                // reserved
  WriteI16(table, 0);                // reserved
  WriteI16(table, 0);                // metricDataFormat
  WriteU16(table, numGlyphs);        // numberOfHMetrics
  return table;
}

static std::vector<uint8_t> BuildHmtx(const Font* font, const FontExportMetrics& metrics) {
  std::vector<uint8_t> table;
  // .notdef: advance=0, lsb=0
  WriteU16(table, 0);
  WriteI16(table, 0);
  // Each glyph
  for (auto* glyph : font->glyphs) {
    auto advance = RoundToU16(glyph->advance * metrics.designScale);
    WriteU16(table, advance);
    WriteI16(table, 0);  // lsb = 0
  }
  return table;
}

static std::vector<uint8_t> BuildMaxp(uint16_t numGlyphs) {
  std::vector<uint8_t> table;
  WriteU32(table, 0x00005000);  // version 0.5 (CFF)
  WriteU16(table, numGlyphs);
  return table;
}

static std::vector<uint8_t> BuildMaxpTrueType(uint16_t numGlyphs) {
  std::vector<uint8_t> table;
  WriteU32(table, 0x00010000);  // version 1.0 (TrueType)
  WriteU16(table, numGlyphs);   // numGlyphs
  WriteU16(table, 0);           // maxPoints
  WriteU16(table, 0);           // maxContours
  WriteU16(table, 0);           // maxCompositePoints
  WriteU16(table, 0);           // maxCompositeContours
  WriteU16(table, 1);           // maxZones
  WriteU16(table, 0);           // maxTwilightPoints
  WriteU16(table, 0);           // maxStorage
  WriteU16(table, 0);           // maxFunctionDefs
  WriteU16(table, 0);           // maxInstructionDefs
  WriteU16(table, 0);           // maxStackElements
  WriteU16(table, 0);           // maxSizeOfInstructions
  WriteU16(table, 0);           // maxComponentElements
  WriteU16(table, 0);           // maxComponentDepth
  return table;
}

static std::vector<uint8_t> BuildName(const std::string& familyName) {
  // Encode familyName as UTF-16BE
  std::vector<uint8_t> utf16Name;
  for (char c : familyName) {
    WriteU8(utf16Name, 0);
    WriteU8(utf16Name, static_cast<uint8_t>(c));
  }
  uint16_t nameLength = static_cast<uint16_t>(utf16Name.size());

  std::vector<uint8_t> table;
  WriteU16(table, 0);  // format
  WriteU16(table, 4);  // count (4 name records)
  // stringOffset: 6 + 4*12 = 54
  uint16_t stringOffset = 6 + 4 * 12;
  WriteU16(table, stringOffset);

  // All 4 records point to the same string: familyName
  // Name IDs: 1 (family), 2 (subfamily), 4 (full name), 6 (PostScript name)
  uint16_t nameIDs[] = {1, 2, 4, 6};
  for (auto nameID : nameIDs) {
    WriteU16(table, 3);       // platformID (Windows)
    WriteU16(table, 1);       // encodingID (BMP)
    WriteU16(table, 0x0409);  // languageID (English US)
    WriteU16(table, nameID);
    WriteU16(table, nameLength);  // length
    WriteU16(table, 0);           // offset into string storage
  }

  // String storage
  table.insert(table.end(), utf16Name.begin(), utf16Name.end());
  return table;
}

static std::vector<uint8_t> BuildPost() {
  std::vector<uint8_t> table;
  WriteU32(table, 0x00030000);  // version 3.0
  WriteU32(table, 0);           // italicAngle
  WriteI16(table, 0);           // underlinePosition
  WriteI16(table, 0);           // underlineThickness
  WriteU32(table, 0);           // isFixedPitch
  WriteU32(table, 0);           // minMemType42
  WriteU32(table, 0);           // maxMemType42
  WriteU32(table, 0);           // minMemType1
  WriteU32(table, 0);           // maxMemType1
  return table;
}

// --- Bitmap font table builders ---

static std::vector<uint8_t> BuildPlaceholderGlyf(uint16_t numGlyphs) {
  // Build a minimal glyf table with a single-point contour for each glyph. A truly empty
  // glyph (numberOfContours=0) gets optimized away by WOFF2 encoders and fonttools, causing
  // OTS to report "glyf: zero-length table". A single degenerate contour satisfies OTS while
  // being invisible (zero-area). CBDT bitmaps take rendering priority over glyf outlines.
  std::vector<uint8_t> table;
  for (uint16_t i = 0; i < numGlyphs; i++) {
    WriteI16(table, 1);  // numberOfContours = 1
    WriteI16(table, 0);  // xMin
    WriteI16(table, 0);  // yMin
    WriteI16(table, 0);  // xMax
    WriteI16(table, 0);  // yMax
    // Simple glyph data:
    WriteU16(table, 0);    // endPtsOfContours[0] = 0 (1 point)
    WriteU16(table, 0);    // instructionLength = 0
    WriteU8(table, 0x31);  // flags: ON_CURVE | X_IS_SAME | Y_IS_SAME (delta 0,0 — no bytes)
    // X_SHORT=0 + X_IS_SAME=1 means x delta=0 with no data bytes.
    // Y_SHORT=0 + Y_IS_SAME=1 means y delta=0 with no data bytes.
  }
  return table;
}

static std::vector<uint8_t> BuildLoca(uint16_t numGlyphs) {
  // Short format loca: (numGlyphs + 1) uint16 entries.
  // Each glyph in BuildPlaceholderGlyf is 15 bytes. Short loca stores offset/2.
  // Pad each glyph to 16 bytes (even) so offset/2 is exact.
  // Actually, use long format loca (uint32 offsets) to avoid padding issues.
  // Wait — head.indexToLocFormat must match. We set it to 0 (short). Let's use long (1).
  // For simplicity: use long format loca (4 bytes per entry), 15 bytes per glyph.
  std::vector<uint8_t> table;
  for (uint16_t i = 0; i <= numGlyphs; i++) {
    WriteU32(table, static_cast<uint32_t>(i) * 15);
  }
  return table;
}

static std::vector<uint8_t> BuildCBDT(const Font* font, const FontExportMetrics& metrics) {
  std::vector<uint8_t> table;
  // Header
  WriteU16(table, 3);  // majorVersion
  WriteU16(table, 0);  // minorVersion

  // For each glyph: format 17 data (smallGlyphMetrics + dataLen + PNG bytes)
  for (auto* glyph : font->glyphs) {
    const Image* image = glyph->image;
    const uint8_t* pngBytes = image->data->bytes();
    size_t pngSize = image->data->size();

    // Decode image to get dimensions
    auto tgfxData = tgfx::Data::MakeWithCopy(pngBytes, pngSize);
    auto codec = tgfx::ImageCodec::MakeFrom(std::move(tgfxData));
    uint8_t width = 0;
    uint8_t height = 0;
    if (codec) {
      width = static_cast<uint8_t>(std::min(codec->width(), 255));
      height = static_cast<uint8_t>(std::min(codec->height(), 255));
    }

    // SmallGlyphMetrics BearingY: distance from bitmap top to baseline (positive = above).
    // Setting BearingY = ppem - offset.y places the bitmap at em-box top when offset=0,
    // which aligns with cssTop = posY in the HTML emitter (span box starts at posY, no overflow).
    // Formula: bitmap_top = cssTop + fontSize - BearingY*(fontSize/ppem) = posY + offset.y*scale.
    auto bearingX = static_cast<int8_t>(std::clamp(
        static_cast<int>(std::round(glyph->offset.x * metrics.bitmapMetricScale)), -128, 127));
    int ppem = metrics.bitmapPpem;
    auto bearingY = static_cast<int8_t>(
        std::clamp(static_cast<int>(std::round(ppem - glyph->offset.y * metrics.bitmapMetricScale)),
                   -128, 127));
    auto advance = static_cast<uint8_t>(
        std::min(static_cast<int>(std::round(glyph->advance * metrics.bitmapMetricScale)), 255));

    // SmallGlyphMetrics (5 bytes)
    WriteU8(table, height);
    WriteU8(table, width);
    WriteI8(table, bearingX);
    WriteI8(table, bearingY);
    WriteU8(table, advance);

    // dataLen (uint32 big-endian)
    WriteU32(table, static_cast<uint32_t>(pngSize));

    // Raw PNG data
    table.insert(table.end(), pngBytes, pngBytes + pngSize);
  }

  return table;
}

static std::vector<uint8_t> BuildCBLC(const Font* font, const FontExportMetrics& metrics,
                                      uint16_t numGlyphs) {
  std::vector<uint8_t> table;

  // CBLC Header (8 bytes)
  WriteU16(table, 3);  // majorVersion
  WriteU16(table, 0);  // minorVersion
  WriteU32(table, 1);  // numSizes

  // BitmapSize record (48 bytes)
  // We need to compute the offset to the IndexSubTableArray.
  // BitmapSize starts at offset 8, is 48 bytes. IndexSubTableArray follows at offset 56.
  uint32_t indexSubTableArrayOffset = 56;

  // IndexSubTableArray (8 bytes) + IndexSubTable1 header (8 bytes) + offsets
  uint16_t glyphCount = numGlyphs - 1;  // exclude .notdef
  // IndexSubTable1 data: (glyphCount + 1) uint32 offsets
  uint32_t indexSubTable1DataSize = (static_cast<uint32_t>(glyphCount) + 1) * 4;
  uint32_t indexTablesSize = 8 + 8 + indexSubTable1DataSize;  // array entry + header + offsets

  WriteU32(table, indexSubTableArrayOffset);
  WriteU32(table, indexTablesSize);
  WriteU32(table, 1);  // numberOfIndexSubTables
  WriteU32(table, 0);  // colorRef

  // sbitLineMetrics hori (12 bytes) - all zeros
  for (int i = 0; i < 12; i++) {
    WriteU8(table, 0);
  }
  // sbitLineMetrics vert (12 bytes) - all zeros
  for (int i = 0; i < 12; i++) {
    WriteU8(table, 0);
  }

  WriteU16(table, 1);                                     // startGlyphIndex
  WriteU16(table, static_cast<uint16_t>(numGlyphs - 1));  // endGlyphIndex

  WriteU8(table, metrics.bitmapPpem);  // ppemX
  WriteU8(table, metrics.bitmapPpem);  // ppemY
  WriteU8(table, 32);                  // bitDepth (RGBA)
  WriteU8(table, 0x01);                // flags (horizontal)

  // IndexSubTableArray entry (8 bytes)
  WriteU16(table, 1);                                     // firstGlyphIndex
  WriteU16(table, static_cast<uint16_t>(numGlyphs - 1));  // lastGlyphIndex
  // additionalOffsetToIndexSubtable: offset from start of IndexSubTableArray to IndexSubTable
  // The IndexSubTableArray itself is 8 bytes, so the IndexSubTable starts right after.
  WriteU32(table, 8);

  // IndexSubTable header (8 bytes)
  WriteU16(table, 1);   // indexFormat = 1 (variable metrics, 4-byte offsets)
  WriteU16(table, 17);  // imageFormat = 17 (small metrics + PNG)
  // imageDataOffset: offset into CBDT where first glyph data starts (after CBDT header = 4 bytes)
  WriteU32(table, 4);

  // IndexSubTable1 data: (glyphCount + 1) uint32 offsets relative to imageDataOffset
  // Each glyph's data: 5 bytes metrics + 4 bytes dataLen + PNG size
  uint32_t currentOffset = 0;
  for (uint16_t i = 0; i < glyphCount; i++) {
    WriteU32(table, currentOffset);

    const Image* image = font->glyphs[i]->image;
    if (!image || !image->data) {
      return {};
    }
    size_t pngSize = image->data->size();

    // Decode to get dimensions for height/width in metrics
    auto tgfxData = tgfx::Data::MakeWithCopy(image->data->bytes(), pngSize);
    auto codec = tgfx::ImageCodec::MakeFrom(std::move(tgfxData));
    (void)codec;  // dimensions already accounted for in CBDT

    // Each glyph entry: 5 (metrics) + 4 (dataLen) + pngSize
    size_t glyphSize = 5 + 4 + pngSize;
    if (currentOffset > UINT32_MAX - glyphSize) {
      return {};
    }
    currentOffset += static_cast<uint32_t>(glyphSize);
  }
  // Final offset marking end of last glyph's data
  WriteU32(table, currentOffset);

  return table;
}

// --- SFNT assembly ---

struct TableEntry {
  char tag[5];
  std::vector<uint8_t> data;
};

static bool CompareTableEntries(const TableEntry& a, const TableEntry& b) {
  return strcmp(a.tag, b.tag) < 0;
}

static std::vector<uint8_t> AssembleSFNT(std::vector<TableEntry>& tables, bool isTrueType) {
  // Sort tables alphabetically by tag
  std::sort(tables.begin(), tables.end(), CompareTableEntries);

  uint16_t numTables = static_cast<uint16_t>(tables.size());
  // searchRange = (2^floor(log2(numTables))) * 16
  uint16_t entryPower = 1;
  uint16_t entrySelector = 0;
  while (entryPower * 2 <= numTables) {
    entryPower *= 2;
    entrySelector++;
  }
  uint16_t searchRange = entryPower * 16;
  uint16_t rangeShift = numTables * 16 - searchRange;

  std::vector<uint8_t> sfnt;

  // Offset table (12 bytes)
  if (isTrueType) {
    WriteU32(sfnt, 0x00010000);  // TrueType magic
  } else {
    WriteTag(sfnt, "OTTO");  // CFF magic
  }
  WriteU16(sfnt, numTables);
  WriteU16(sfnt, searchRange);
  WriteU16(sfnt, entrySelector);
  WriteU16(sfnt, rangeShift);

  // Calculate table offsets
  size_t recordsStart = 12;
  size_t dataStart = recordsStart + numTables * 16;
  // Align data start to 4 bytes (already is since 12 + N*16 is always divisible by 4)

  // Compute offsets for each table
  std::vector<uint32_t> offsets(numTables);
  uint32_t currentOffset = static_cast<uint32_t>(dataStart);
  for (size_t i = 0; i < numTables; i++) {
    if (tables[i].data.size() > UINT32_MAX ||
        currentOffset > UINT32_MAX - static_cast<uint32_t>(tables[i].data.size())) {
      return {};
    }
    offsets[i] = currentOffset;
    currentOffset += static_cast<uint32_t>(tables[i].data.size());
    // 4-byte align
    currentOffset = (currentOffset + 3) & ~3u;
  }

  // Write table records
  for (size_t i = 0; i < numTables; i++) {
    WriteTag(sfnt, tables[i].tag);
    uint32_t checksum = CalcChecksum(tables[i].data.data(), tables[i].data.size());
    WriteU32(sfnt, checksum);
    WriteU32(sfnt, offsets[i]);
    WriteU32(sfnt, static_cast<uint32_t>(tables[i].data.size()));
  }

  // Write table data
  for (size_t i = 0; i < numTables; i++) {
    sfnt.insert(sfnt.end(), tables[i].data.begin(), tables[i].data.end());
    PadTo4(sfnt);
  }

  // Patch head.checksumAdjustment
  uint32_t wholeChecksum = CalcChecksum(sfnt.data(), sfnt.size());
  uint32_t adjustment = 0xB1B0AFBA - wholeChecksum;

  // Find head table offset
  for (size_t i = 0; i < numTables; i++) {
    if (strcmp(tables[i].tag, "head") == 0) {
      uint32_t headOffset = offsets[i];
      // checksumAdjustment is at byte offset 8 within head table
      if (headOffset + 11 < sfnt.size()) {
        sfnt[headOffset + 8] = static_cast<uint8_t>(adjustment >> 24);
        sfnt[headOffset + 9] = static_cast<uint8_t>(adjustment >> 16);
        sfnt[headOffset + 10] = static_cast<uint8_t>(adjustment >> 8);
        sfnt[headOffset + 11] = static_cast<uint8_t>(adjustment);
      }
      break;
    }
  }

  return sfnt;
}

// --- Public API ---

Woff2FontResult BuildWoff2FromFont(const Font* font, const std::string& fontId) {
  Woff2FontResult result;
  if (font == nullptr || font->glyphs.empty()) {
    return result;
  }

  // PUA BMP segment 0xE000–0xF8FF holds 6400 codepoints. We map glyph IDs 1..N to that range
  // 1:1, so the maximum glyph count we can encode in a single-segment cmap is 6400. Returning
  // empty here lets the caller fall back to the per-glyph <path> rendering path; quietly
  // truncating instead would silently drop glyphs at the tail of a CJK subset.
  constexpr size_t kMaxPUAGlyphCount = 0xF900u - 0xE000u;  // 6400
  if (font->glyphs.size() > kMaxPUAGlyphCount) {
    return result;
  }

  // Detect font type: vector (path) or bitmap (image). The PAGX Glyph contract is that path and
  // image are mutually exclusive, but the importer does not enforce it. Inspect every glyph here
  // to reject fonts that mix vector and bitmap glyphs (and to reject null glyph entries) — the
  // table builders below assume a single, uniform type for the whole font, so a mismatched glyph
  // would silently corrupt either CBDT/CBLC (bitmap path) or CFF (vector path) tables.
  for (auto* glyph : font->glyphs) {
    if (glyph == nullptr) {
      return result;
    }
  }
  bool isBitmapFont = (font->glyphs[0]->image != nullptr);

  // Validate: all glyphs must be the same type. For vector fonts, glyphs without
  // a path are treated as blank/space glyphs (advance only, no outline).
  for (auto* glyph : font->glyphs) {
    if (isBitmapFont) {
      if (glyph->image == nullptr || glyph->image->data == nullptr) {
        return result;
      }
    } else {
      // Vector font: a stray glyph carrying an image breaks the CFF assumption that every glyph
      // is a path. Reject so the caller falls back to the per-glyph rendering path that handles
      // mixed types correctly.
      if (glyph->image != nullptr) {
        return result;
      }
    }
  }

  std::string familyName = "pagx-font-" + fontId;
  uint16_t numGlyphs = static_cast<uint16_t>(font->glyphs.size() + 1);
  auto metrics = ResolveFontExportMetrics(font);
  result.unitsPerEm = metrics.exportUnitsPerEm;
  result.designScale = metrics.designScale;

  std::vector<TableEntry> tables;

  if (isBitmapFont) {
    // --- Bitmap font: TrueType with CBDT/CBLC ---

    TableEntry cbdtEntry;
    strcpy(cbdtEntry.tag, "CBDT");
    cbdtEntry.data = BuildCBDT(font, metrics);
    tables.push_back(std::move(cbdtEntry));

    TableEntry cblcEntry;
    strcpy(cblcEntry.tag, "CBLC");
    cblcEntry.data = BuildCBLC(font, metrics, numGlyphs);
    tables.push_back(std::move(cblcEntry));

    TableEntry os2Entry;
    strcpy(os2Entry.tag, "OS/2");
    os2Entry.data = BuildOS2(metrics, numGlyphs);
    tables.push_back(std::move(os2Entry));

    TableEntry cmapEntry;
    strcpy(cmapEntry.tag, "cmap");
    cmapEntry.data = BuildCmap(numGlyphs);
    tables.push_back(std::move(cmapEntry));

    TableEntry glyfEntry;
    strcpy(glyfEntry.tag, "glyf");
    glyfEntry.data = BuildPlaceholderGlyf(numGlyphs);
    tables.push_back(std::move(glyfEntry));

    TableEntry headEntry;
    strcpy(headEntry.tag, "head");
    headEntry.data = BuildHead(metrics, 0, 0, static_cast<int16_t>(metrics.exportUnitsPerEm),
                               static_cast<int16_t>(metrics.exportUnitsPerEm),
                               1);  // 1 = long loca
    tables.push_back(std::move(headEntry));

    TableEntry hheaEntry;
    strcpy(hheaEntry.tag, "hhea");
    hheaEntry.data = BuildHhea(font, metrics, numGlyphs);
    tables.push_back(std::move(hheaEntry));

    TableEntry hmtxEntry;
    strcpy(hmtxEntry.tag, "hmtx");
    hmtxEntry.data = BuildHmtx(font, metrics);
    tables.push_back(std::move(hmtxEntry));

    TableEntry locaEntry;
    strcpy(locaEntry.tag, "loca");
    locaEntry.data = BuildLoca(numGlyphs);
    tables.push_back(std::move(locaEntry));

    TableEntry maxpEntry;
    strcpy(maxpEntry.tag, "maxp");
    maxpEntry.data = BuildMaxpTrueType(numGlyphs);
    tables.push_back(std::move(maxpEntry));

    TableEntry nameEntry;
    strcpy(nameEntry.tag, "name");
    nameEntry.data = BuildName(familyName);
    tables.push_back(std::move(nameEntry));

    TableEntry postEntry;
    strcpy(postEntry.tag, "post");
    postEntry.data = BuildPost();
    tables.push_back(std::move(postEntry));

  } else {
    // --- Vector font: CFF (existing logic) ---

    // Compute global bounding box from all glyph paths (in typographic coordinates: Y flipped)
    int16_t xMin = 0, yMin = 0, xMax = 0, yMax = 0;
    bool hasBounds = false;
    for (auto* glyph : font->glyphs) {
      if (glyph->path == nullptr || glyph->path->isEmpty()) {
        continue;
      }
      auto& points = glyph->path->points();
      for (auto& pt : points) {
        auto px = ClampToI16(std::floor(pt.x * metrics.designScale));
        auto py = ClampToI16(std::floor(-pt.y * metrics.designScale));
        auto pxMax = ClampToI16(std::ceil(pt.x * metrics.designScale));
        auto pyMax = ClampToI16(std::ceil(-pt.y * metrics.designScale));
        if (!hasBounds) {
          xMin = std::min(px, pxMax);
          yMin = std::min(py, pyMax);
          xMax = std::max(px, pxMax);
          yMax = std::max(py, pyMax);
          hasBounds = true;
        } else {
          xMin = std::min({xMin, px, pxMax});
          yMin = std::min({yMin, py, pyMax});
          xMax = std::max({xMax, px, pxMax});
          yMax = std::max({yMax, py, pyMax});
        }
      }
    }

    TableEntry cffEntry;
    strcpy(cffEntry.tag, "CFF ");
    cffEntry.data = BuildCFF(font, familyName, metrics);
    tables.push_back(std::move(cffEntry));

    TableEntry os2Entry;
    strcpy(os2Entry.tag, "OS/2");
    os2Entry.data = BuildOS2(metrics, numGlyphs);
    tables.push_back(std::move(os2Entry));

    TableEntry cmapEntry;
    strcpy(cmapEntry.tag, "cmap");
    cmapEntry.data = BuildCmap(numGlyphs);
    tables.push_back(std::move(cmapEntry));

    TableEntry headEntry;
    strcpy(headEntry.tag, "head");
    headEntry.data = BuildHead(metrics, xMin, yMin, xMax, yMax, 1);
    tables.push_back(std::move(headEntry));

    TableEntry hheaEntry;
    strcpy(hheaEntry.tag, "hhea");
    hheaEntry.data = BuildHhea(font, metrics, numGlyphs);
    tables.push_back(std::move(hheaEntry));

    TableEntry hmtxEntry;
    strcpy(hmtxEntry.tag, "hmtx");
    hmtxEntry.data = BuildHmtx(font, metrics);
    tables.push_back(std::move(hmtxEntry));

    TableEntry maxpEntry;
    strcpy(maxpEntry.tag, "maxp");
    maxpEntry.data = BuildMaxp(numGlyphs);
    tables.push_back(std::move(maxpEntry));

    TableEntry nameEntry;
    strcpy(nameEntry.tag, "name");
    nameEntry.data = BuildName(familyName);
    tables.push_back(std::move(nameEntry));

    TableEntry postEntry;
    strcpy(postEntry.tag, "post");
    postEntry.data = BuildPost();
    tables.push_back(std::move(postEntry));
  }

  // Assemble SFNT
  std::vector<uint8_t> ttfData = AssembleSFNT(tables, isBitmapFont);
  auto writtenUnitsPerEm = ReadHeadUnitsPerEm(ttfData);
  if (writtenUnitsPerEm < MinOpenTypeUnitsPerEm || writtenUnitsPerEm > MaxOpenTypeUnitsPerEm) {
    return result;
  }
  result.unitsPerEm = writtenUnitsPerEm;

#ifdef PAG_USE_WOFF2
  // Compress to WOFF2. Disable transforms for bitmap fonts because the glyf table transform
  // eliminates empty glyph outlines, producing a zero-length table that OTS rejects.
  woff2::WOFF2Params params;
  params.allow_transforms = !isBitmapFont;
  size_t maxSize =
      woff2::MaxWOFF2CompressedSize(ttfData.data(), ttfData.size(), params.extended_metadata);
  std::vector<uint8_t> woff2Buf(maxSize);
  size_t woff2Size = maxSize;
  if (!woff2::ConvertTTFToWOFF2(ttfData.data(), ttfData.size(), woff2Buf.data(), &woff2Size,
                                params)) {
    return result;
  }
  woff2Buf.resize(woff2Size);

  result.woff2Data = std::move(woff2Buf);
#endif
  result.familyName = familyName;
  return result;
}

}  // namespace pagx
