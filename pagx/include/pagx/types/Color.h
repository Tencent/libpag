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

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

namespace pagx {

namespace detail {
inline int ParseHexDigit(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return 0;
}
}  // namespace detail

/**
 * An RGBA color with floating-point components in [0, 1].
 */
struct Color {
  float red = 0;
  float green = 0;
  float blue = 0;
  float alpha = 1;

  /**
   * Returns a Color from a hex value (0xRRGGBB or 0xRRGGBBAA).
   */
  static Color FromHex(uint32_t hex, bool hasAlpha = false) {
    Color color = {};
    if (hasAlpha) {
      color.red = static_cast<float>((hex >> 24) & 0xFF) / 255.0f;
      color.green = static_cast<float>((hex >> 16) & 0xFF) / 255.0f;
      color.blue = static_cast<float>((hex >> 8) & 0xFF) / 255.0f;
      color.alpha = static_cast<float>(hex & 0xFF) / 255.0f;
    } else {
      color.red = static_cast<float>((hex >> 16) & 0xFF) / 255.0f;
      color.green = static_cast<float>((hex >> 8) & 0xFF) / 255.0f;
      color.blue = static_cast<float>(hex & 0xFF) / 255.0f;
      color.alpha = 1.0f;
    }
    return color;
  }

  /**
   * Returns a Color from RGBA components in [0, 1].
   */
  static Color FromRGBA(float r, float g, float b, float a = 1) {
    return {r, g, b, a};
  }

  /**
   * Parses a color string. Supports:
   * - Hex: "#RGB", "#RRGGBB", "#RRGGBBAA"
   * - RGB: "rgb(r,g,b)", "rgba(r,g,b,a)"
   * Returns black if parsing fails.
   */
  static Color Parse(const std::string& str) {
    if (str.empty()) {
      return {};
    }
    if (str[0] == '#') {
      auto hex = str.substr(1);
      if (hex.size() == 3) {
        int r = detail::ParseHexDigit(hex[0]);
        int g = detail::ParseHexDigit(hex[1]);
        int b = detail::ParseHexDigit(hex[2]);
        return Color::FromRGBA(static_cast<float>(r * 17) / 255.0f,
                               static_cast<float>(g * 17) / 255.0f,
                               static_cast<float>(b * 17) / 255.0f, 1.0f);
      }
      if (hex.size() == 6) {
        int r = detail::ParseHexDigit(hex[0]) * 16 + detail::ParseHexDigit(hex[1]);
        int g = detail::ParseHexDigit(hex[2]) * 16 + detail::ParseHexDigit(hex[3]);
        int b = detail::ParseHexDigit(hex[4]) * 16 + detail::ParseHexDigit(hex[5]);
        return Color::FromRGBA(static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f,
                               static_cast<float>(b) / 255.0f, 1.0f);
      }
      if (hex.size() == 8) {
        int r = detail::ParseHexDigit(hex[0]) * 16 + detail::ParseHexDigit(hex[1]);
        int g = detail::ParseHexDigit(hex[2]) * 16 + detail::ParseHexDigit(hex[3]);
        int b = detail::ParseHexDigit(hex[4]) * 16 + detail::ParseHexDigit(hex[5]);
        int a = detail::ParseHexDigit(hex[6]) * 16 + detail::ParseHexDigit(hex[7]);
        return Color::FromRGBA(static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f,
                               static_cast<float>(b) / 255.0f, static_cast<float>(a) / 255.0f);
      }
    }
    if (str.substr(0, 4) == "rgb(" || str.substr(0, 5) == "rgba(") {
      auto start = str.find('(');
      auto end = str.find(')');
      if (start != std::string::npos && end != std::string::npos) {
        auto values = str.substr(start + 1, end - start - 1);
        std::istringstream iss(values);
        std::string token = {};
        std::vector<float> components = {};
        while (std::getline(iss, token, ',')) {
          auto trimmed = token;
          trimmed.erase(0, trimmed.find_first_not_of(" \t"));
          trimmed.erase(trimmed.find_last_not_of(" \t") + 1);
          components.push_back(std::stof(trimmed));
        }
        if (components.size() >= 3) {
          float r = components[0] / 255.0f;
          float g = components[1] / 255.0f;
          float b = components[2] / 255.0f;
          float a = components.size() >= 4 ? components[3] : 1.0f;
          return Color::FromRGBA(r, g, b, a);
        }
      }
    }
    return {};
  }

  /**
   * Returns the color as a hex string "#RRGGBB" or "#RRGGBBAA".
   */
  std::string toHexString(bool includeAlpha = false) const {
    auto toHex = [](float v) {
      int i = static_cast<int>(std::round(v * 255.0f));
      i = std::max(0, std::min(255, i));
      char buf[3] = {};
      snprintf(buf, sizeof(buf), "%02X", i);
      return std::string(buf);
    };
    std::string result = "#" + toHex(red) + toHex(green) + toHex(blue);
    if (includeAlpha && alpha < 1.0f) {
      result += toHex(alpha);
    }
    return result;
  }

  bool operator==(const Color& other) const {
    return red == other.red && green == other.green && blue == other.blue && alpha == other.alpha;
  }

  bool operator!=(const Color& other) const {
    return !(*this == other);
  }
};

}  // namespace pagx
