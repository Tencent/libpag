#include "string.h"
#include <pag/file.h>

namespace Utils {

auto toQString(double num) -> QString {
  char str[100];
  snprintf(str, 100, "%0.2f", num);
  return str;
}

auto toQString(int32_t num) -> QString {
  char str[100];
  snprintf(str, 100, "%d", num);
  return str;
}

auto toQString(int64_t num) -> QString {
  char str[100];
  snprintf(str, 100, "%lld", num);
  return str;
}

auto getMemorySizeUnit(int64_t size) -> QString {
  constexpr int64_t kbThreshold = 1024;
  constexpr int64_t mbThreshold = 1024 * 1024;
  constexpr int64_t gbThreshold = 1024 * 1024 * 1024;
  if (size < kbThreshold) {
    return "B";
  } else if (size < mbThreshold) {
    return "KB";
  } else if (size < gbThreshold) {
    return "MB";
  }
  return "GB";
}

auto getMemorySizeNumString(int64_t size) -> QString {
  constexpr int64_t kbThreshold = 1024;
  constexpr int64_t mbThreshold = 1024 * 1024;
  constexpr int64_t gbThreshold = 1024 * 1024 * 1024;
  if (size < kbThreshold) {
    return toQString(size);
  } else if (size < mbThreshold) {
    return toQString(size / static_cast<double>(kbThreshold));
  } else if (size < gbThreshold) {
    return toQString(size / static_cast<double>(mbThreshold));
  }
  return toQString(size / static_cast<double>(gbThreshold));
}

static const std::vector<std::pair<pag::TagCode, std::string>> TagCodeVersionList = {
  {pag::TagCode::FileAttributes, "1.0"},
  {pag::TagCode::LayerAttributesExtra, "2.0"},
  {pag::TagCode::MosaicEffect, "3.2"},
  {pag::TagCode::GradientOverlayStyle, "4.1"},
  {pag::TagCode::CameraOption, "4.2"},
  {pag::TagCode::ImageScaleModes, "4.3"}
};

auto tagCodeToVersion(uint16_t tagCode) -> std::string {
  for (auto node : TagCodeVersionList) {
    if (tagCode <= static_cast<uint16_t>(node.first)) {
      return node.second;
    }
  }
  return "Unknown";
}

} // namespace Utils