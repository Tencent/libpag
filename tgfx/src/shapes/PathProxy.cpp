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

#include "PathProxy.h"
#include "tgfx/core/PathEffect.h"
#include "utils/Log.h"

namespace tgfx {
class FillPathProxy : public PathProxy {
 public:
  explicit FillPathProxy(const Path& path) : path(path) {
  }

  Rect getBounds(float scale) const override {
    auto bounds = path.getBounds();
    bounds.scale(scale, scale);
    return bounds;
  }

  Path getPath(float scale) const override {
    auto fillPath = path;
    fillPath.transform(Matrix::MakeScale(scale));
    return fillPath;
  }

 private:
  Path path = {};
};

class FillTextProxy : public PathProxy {
 public:
  explicit FillTextProxy(std::shared_ptr<TextBlob> textBlob) : textBlob(std::move(textBlob)) {
  }

  Rect getBounds(float scale) const override {
    auto bounds = textBlob->getBounds();
    bounds.scale(scale, scale);
    return bounds;
  }

  Path getPath(float scale) const override {
    Path fillPath = {};
    textBlob->getPath(&fillPath);
    fillPath.transform(Matrix::MakeScale(scale));
    return fillPath;
  }

 private:
  std::shared_ptr<TextBlob> textBlob = nullptr;
};

class StrokeTextProxy : public PathProxy {
 public:
  explicit StrokeTextProxy(std::shared_ptr<TextBlob> textBlob, const Stroke& stroke)
      : textBlob(std::move(textBlob)), stroke(stroke) {
  }

  Rect getBounds(float scale) const override {
    auto bounds = textBlob->getBounds(&stroke);
    bounds.scale(scale, scale);
    return bounds;
  }

  Path getPath(float scale) const override {
    Path fillPath = {};
    textBlob->getPath(&fillPath, &stroke);
    fillPath.transform(Matrix::MakeScale(scale));
    return fillPath;
  }

 private:
  std::shared_ptr<TextBlob> textBlob = nullptr;
  Stroke stroke = {};
};

std::unique_ptr<PathProxy> PathProxy::MakeFromFill(const Path& path) {
  return std::make_unique<FillPathProxy>(path);
}

std::unique_ptr<PathProxy> PathProxy::MakeFromFill(std::shared_ptr<TextBlob> textBlob) {
  if (textBlob->hasColor()) {
    return nullptr;
  }
  return std::make_unique<FillTextProxy>(std::move(textBlob));
}

std::unique_ptr<PathProxy> PathProxy::MakeFromStroke(std::shared_ptr<TextBlob> textBlob,
                                                     const Stroke& stroke) {
  if (textBlob->hasColor() || stroke.width <= 0) {
    return nullptr;
  }
  return std::make_unique<StrokeTextProxy>(std::move(textBlob), stroke);
}

}  // namespace tgfx
