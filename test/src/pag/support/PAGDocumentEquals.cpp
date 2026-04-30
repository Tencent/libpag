// Copyright (C) 2026 Tencent. All rights reserved.
#include "PAGDocumentEquals.h"
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

namespace pagx::test {

using namespace pagx::pag;

namespace {

// Track the dotted-path of the field currently being compared so error
// messages can pinpoint the divergence in deeply nested trees.
struct PathScope {
  std::string& path;
  size_t prevLen;
  PathScope(std::string& p, std::string segment) : path(p), prevLen(p.size()) {
    if (!path.empty() && path.back() != '[') {
      path += '.';
    }
    path += segment;
  }
  ~PathScope() {
    path.resize(prevLen);
  }
};

struct IndexScope {
  std::string& path;
  size_t prevLen;
  IndexScope(std::string& p, size_t i) : path(p), prevLen(p.size()) {
    path += '[';
    path += std::to_string(i);
    path += ']';
  }
  ~IndexScope() {
    path.resize(prevLen);
  }
};

bool ReportMismatch(const std::string& path, const std::string& detail, std::string* diff) {
  if (diff != nullptr) {
    *diff = path + ": " + detail;
  }
  return false;
}

// ---- Bit-exact float comparison (handles -0.0 vs +0.0 and NaN payloads).
// Use memcpy rather than std::bit_cast for C++17 portability.
bool FloatEq(float a, float b) {
  uint32_t ai = 0;
  uint32_t bi = 0;
  std::memcpy(&ai, &a, sizeof(ai));
  std::memcpy(&bi, &b, sizeof(bi));
  return ai == bi;
}

[[maybe_unused]] bool DoubleEq(double a, double b) {
  uint64_t ai = 0;
  uint64_t bi = 0;
  std::memcpy(&ai, &a, sizeof(ai));
  std::memcpy(&bi, &b, sizeof(bi));
  return ai == bi;
}

// ---- Core type comparators ----

bool ColorEq(const Color& a, const Color& b) {
  return FloatEq(a.red, b.red) && FloatEq(a.green, b.green) && FloatEq(a.blue, b.blue) &&
         FloatEq(a.alpha, b.alpha);
}

[[maybe_unused]] bool PointEq(const Point& a, const Point& b) {
  return FloatEq(a.x, b.x) && FloatEq(a.y, b.y);
}

bool RectEq(const Rect& a, const Rect& b) {
  return FloatEq(a.left, b.left) && FloatEq(a.top, b.top) && FloatEq(a.right, b.right) &&
         FloatEq(a.bottom, b.bottom);
}

bool MatrixEq(const Matrix& a, const Matrix& b) {
  return a == b;
}

bool Matrix3DEq(const Matrix3D& a, const Matrix3D& b) {
  return a == b;
}

bool DataEq(const std::shared_ptr<const tgfx::Data>& a,
            const std::shared_ptr<const tgfx::Data>& b) {
  if (a.get() == b.get()) {
    return true;
  }
  if (a == nullptr || b == nullptr) {
    return false;
  }
  if (a->size() != b->size()) {
    return false;
  }
  return std::memcmp(a->data(), b->data(), a->size()) == 0;
}

// ---- Property<T> comparator (encoding + value) ----

template <typename T, typename ValueEq>
bool PropertyEq(const Property<T>& a, const Property<T>& b, ValueEq value_eq,
                const std::string& path, std::string* diff) {
  if (a.encoding != b.encoding) {
    return ReportMismatch(path + ".encoding",
                          std::to_string(static_cast<int>(a.encoding)) + " vs " +
                              std::to_string(static_cast<int>(b.encoding)),
                          diff);
  }
  if (!value_eq(a.value, b.value)) {
    return ReportMismatch(path + ".value", "value differs", diff);
  }
  return true;
}

// ---- ImageAsset / FontAsset ----

bool ImageAssetEq(const ImageAsset& a, const ImageAsset& b, std::string& path, std::string* diff) {
  if (a.width != b.width) {
    return ReportMismatch(path + ".width",
                          std::to_string(a.width) + " vs " + std::to_string(b.width), diff);
  }
  if (a.height != b.height) {
    return ReportMismatch(path + ".height",
                          std::to_string(a.height) + " vs " + std::to_string(b.height), diff);
  }
  if (a.kind != b.kind) {
    return ReportMismatch(path + ".kind", "kind differs", diff);
  }
  if (!DataEq(a.data, b.data)) {
    return ReportMismatch(path + ".data", "byte payload differs", diff);
  }
  return true;
}

bool FontAssetEq(const FontAsset& a, const FontAsset& b, std::string& path, std::string* diff) {
  if (a.kind != b.kind) {
    return ReportMismatch(path + ".kind", "kind differs", diff);
  }
  if (a.family != b.family) {
    return ReportMismatch(path + ".family", a.family + " vs " + b.family, diff);
  }
  if (a.style != b.style) {
    return ReportMismatch(path + ".style", a.style + " vs " + b.style, diff);
  }
  if (!DataEq(a.data, b.data)) {
    return ReportMismatch(path + ".data", "byte payload differs", diff);
  }
  if (a.axes.size() != b.axes.size()) {
    return ReportMismatch(path + ".axes.size",
                          std::to_string(a.axes.size()) + " vs " + std::to_string(b.axes.size()),
                          diff);
  }
  for (size_t i = 0; i < a.axes.size(); ++i) {
    const auto& ax = a.axes[i];
    const auto& bx = b.axes[i];
    if (ax.tag != bx.tag || !FloatEq(ax.defaultValue, bx.defaultValue) ||
        !FloatEq(ax.minValue, bx.minValue) || !FloatEq(ax.maxValue, bx.maxValue)) {
      IndexScope is(path, i);
      return ReportMismatch(path, "axis differs", diff);
    }
  }
  return true;
}

// ---- Layer (recursive via children) ----
// Phase 2 deliverable comparator focuses on the static-document scope —
// the per-payload comparator is reusable and grows as Phase 4-8 add real
// payload content. Keeping it open-ended for now (only the wrappers + a
// pointer-equality fallback for the payload variants) lets Phase 2 ship a
// working comparator without knowing every payload field. Later phases
// extend the per-payload visitors.

bool LayerEq(const Layer& a, const Layer& b, std::string& path, std::string* diff);

bool ChildrenEq(const std::vector<std::unique_ptr<Layer>>& a,
                const std::vector<std::unique_ptr<Layer>>& b, std::string& path,
                std::string* diff) {
  if (a.size() != b.size()) {
    return ReportMismatch(path + ".children.size",
                          std::to_string(a.size()) + " vs " + std::to_string(b.size()), diff);
  }
  PathScope ps(path, "children");
  for (size_t i = 0; i < a.size(); ++i) {
    IndexScope is(path, i);
    if (a[i] == nullptr || b[i] == nullptr) {
      if (a[i] == b[i]) {
        continue;
      }
      return ReportMismatch(path, "null vs non-null layer", diff);
    }
    if (!LayerEq(*a[i], *b[i], path, diff)) {
      return false;
    }
  }
  return true;
}

bool LayerEq(const Layer& a, const Layer& b, std::string& path, std::string* diff) {
  if (a.type != b.type) {
    return ReportMismatch(path + ".type",
                          std::to_string(static_cast<int>(a.type)) + " vs " +
                              std::to_string(static_cast<int>(b.type)),
                          diff);
  }
  if (a.name != b.name) {
    return ReportMismatch(path + ".name", a.name + " vs " + b.name, diff);
  }
  if (a.startTime != b.startTime || a.duration != b.duration ||
      a.stretch.numerator != b.stretch.numerator ||
      a.stretch.denominator != b.stretch.denominator) {
    return ReportMismatch(path + ".timeline", "timeline fields differ", diff);
  }
  if (!PropertyEq(
          a.visible, b.visible, [](bool x, bool y) { return x == y; }, path + ".visible", diff)) {
    return false;
  }
  if (!PropertyEq(a.alpha, b.alpha, FloatEq, path + ".alpha", diff)) {
    return false;
  }
  if (!PropertyEq(
          a.blendMode, b.blendMode, [](BlendMode x, BlendMode y) { return x == y; },
          path + ".blendMode", diff)) {
    return false;
  }
  if (!PropertyEq(a.matrix, b.matrix, MatrixEq, path + ".matrix", diff)) {
    return false;
  }
  if (!PropertyEq(a.matrix3D, b.matrix3D, Matrix3DEq, path + ".matrix3D", diff)) {
    return false;
  }
  if (!PropertyEq(a.scrollRect, b.scrollRect, RectEq, path + ".scrollRect", diff)) {
    return false;
  }
  if (a.hasScrollRect != b.hasScrollRect || a.preserve3D != b.preserve3D ||
      a.passThroughBackground != b.passThroughBackground ||
      a.allowsEdgeAntialiasing != b.allowsEdgeAntialiasing ||
      a.allowsGroupOpacity != b.allowsGroupOpacity) {
    return ReportMismatch(path + ".flags", "non-animatable flags differ", diff);
  }
  if (a.maskLayerPath != b.maskLayerPath) {
    return ReportMismatch(path + ".maskLayerPath", "mask path differs", diff);
  }
  if (a.maskType != b.maskType) {
    return ReportMismatch(path + ".maskType", "mask type differs", diff);
  }
  if (a.compositionIndex != b.compositionIndex) {
    return ReportMismatch(
        path + ".compositionIndex",
        std::to_string(a.compositionIndex) + " vs " + std::to_string(b.compositionIndex), diff);
  }
  // Filter / Style / Payload: shallow size comparison this cycle. Phase 5+
  // will extend with full per-element walks once those sub-models stabilize.
  if (a.filters.size() != b.filters.size()) {
    return ReportMismatch(
        path + ".filters.size",
        std::to_string(a.filters.size()) + " vs " + std::to_string(b.filters.size()), diff);
  }
  if (a.styles.size() != b.styles.size()) {
    return ReportMismatch(
        path + ".styles.size",
        std::to_string(a.styles.size()) + " vs " + std::to_string(b.styles.size()), diff);
  }
  if (!ChildrenEq(a.children, b.children, path, diff)) {
    return false;
  }
  return true;
}

bool CompositionEq(const Composition& a, const Composition& b, std::string& path,
                   std::string* diff) {
  if (a.id != b.id) {
    return ReportMismatch(path + ".id", a.id + " vs " + b.id, diff);
  }
  if (a.width != b.width || a.height != b.height) {
    return ReportMismatch(path + ".size",
                          std::to_string(a.width) + "x" + std::to_string(a.height) + " vs " +
                              std::to_string(b.width) + "x" + std::to_string(b.height),
                          diff);
  }
  return ChildrenEq(a.layers, b.layers, path, diff);
}

bool FileHeaderEq(const FileHeader& a, const FileHeader& b, std::string& path, std::string* diff) {
  if (!FloatEq(a.width, b.width) || !FloatEq(a.height, b.height)) {
    return ReportMismatch(path + ".size", "canvas size differs", diff);
  }
  if (!ColorEq(a.backgroundColor, b.backgroundColor)) {
    return ReportMismatch(path + ".backgroundColor", "color differs", diff);
  }
  if (a.frameRate.numerator != b.frameRate.numerator ||
      a.frameRate.denominator != b.frameRate.denominator) {
    return ReportMismatch(path + ".frameRate", "frame rate differs", diff);
  }
  if (a.duration != b.duration) {
    return ReportMismatch(path + ".duration",
                          std::to_string(a.duration) + " vs " + std::to_string(b.duration), diff);
  }
  return true;
}

}  // namespace

bool DocumentsEqual(const PAGDocument& a, const PAGDocument& b, std::string* diff) {
  std::string path = "doc";
  if (!FileHeaderEq(a.header, b.header, path, diff)) {
    return false;
  }
  if (a.images.size() != b.images.size()) {
    return ReportMismatch(
        "doc.images.size",
        std::to_string(a.images.size()) + " vs " + std::to_string(b.images.size()), diff);
  }
  for (size_t i = 0; i < a.images.size(); ++i) {
    std::string subPath = "doc.images[" + std::to_string(i) + "]";
    if (a.images[i] == nullptr || b.images[i] == nullptr) {
      if (a.images[i] == b.images[i]) {
        continue;
      }
      return ReportMismatch(subPath, "null vs non-null image", diff);
    }
    if (!ImageAssetEq(*a.images[i], *b.images[i], subPath, diff)) {
      return false;
    }
  }
  if (a.fonts.size() != b.fonts.size()) {
    return ReportMismatch("doc.fonts.size",
                          std::to_string(a.fonts.size()) + " vs " + std::to_string(b.fonts.size()),
                          diff);
  }
  for (size_t i = 0; i < a.fonts.size(); ++i) {
    std::string subPath = "doc.fonts[" + std::to_string(i) + "]";
    if (a.fonts[i] == nullptr || b.fonts[i] == nullptr) {
      if (a.fonts[i] == b.fonts[i]) {
        continue;
      }
      return ReportMismatch(subPath, "null vs non-null font", diff);
    }
    if (!FontAssetEq(*a.fonts[i], *b.fonts[i], subPath, diff)) {
      return false;
    }
  }
  if (a.compositions.size() != b.compositions.size()) {
    return ReportMismatch(
        "doc.compositions.size",
        std::to_string(a.compositions.size()) + " vs " + std::to_string(b.compositions.size()),
        diff);
  }
  for (size_t i = 0; i < a.compositions.size(); ++i) {
    std::string subPath = "doc.compositions[" + std::to_string(i) + "]";
    if (a.compositions[i] == nullptr || b.compositions[i] == nullptr) {
      if (a.compositions[i] == b.compositions[i]) {
        continue;
      }
      return ReportMismatch(subPath, "null vs non-null composition", diff);
    }
    if (!CompositionEq(*a.compositions[i], *b.compositions[i], subPath, diff)) {
      return false;
    }
  }
  return true;
}

bool DocumentsEqual(const PAGDocument* a, const PAGDocument* b, std::string* diff) {
  if (a == b) {
    return true;
  }
  if (a == nullptr || b == nullptr) {
    if (diff != nullptr) {
      *diff = "doc: null vs non-null";
    }
    return false;
  }
  return DocumentsEqual(*a, *b, diff);
}

}  // namespace pagx::test
