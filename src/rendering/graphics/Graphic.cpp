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

#include "Graphic.h"
#include "base/utils/MatrixUtil.h"
#include "tgfx/gpu/Canvas.h"

namespace pag {
class ComposeGraphic : public Graphic {
 public:
  GraphicType type() const override {
    return GraphicType::Compose;
  }

  virtual std::shared_ptr<Graphic> mergeWith(const tgfx::Matrix&) const {
    return nullptr;
  }

  virtual std::shared_ptr<Graphic> mergeWith(const Modifier*) const {
    return nullptr;
  }
};

//===================================== MatrixGraphic ==============================================
class MatrixGraphic : public ComposeGraphic {
 public:
  MatrixGraphic(std::shared_ptr<Graphic> graphic, const tgfx::Matrix& matrix)
      : graphic(std::move(graphic)), matrix(matrix) {
  }

  void measureBounds(tgfx::Rect* bounds) const override;
  bool hitTest(RenderCache* cache, float x, float y) override;
  bool getPath(tgfx::Path* path) const override;
  void prepare(RenderCache* cache) const override;
  void draw(tgfx::Canvas* canvas, RenderCache* cache) const override;
  std::shared_ptr<Graphic> mergeWith(const tgfx::Matrix& matrix) const override;

 protected:
  std::shared_ptr<Graphic> graphic = nullptr;
  tgfx::Matrix matrix = {};
};

std::shared_ptr<Graphic> Graphic::MakeCompose(std::shared_ptr<Graphic> graphic,
                                              const tgfx::Matrix& matrix) {
  if (graphic == nullptr || !matrix.invertible()) {
    return nullptr;
  }
  if (matrix.isIdentity()) {
    return graphic;
  }
  if (graphic->type() == GraphicType::Compose) {
    auto result = std::static_pointer_cast<ComposeGraphic>(graphic)->mergeWith(matrix);
    if (result) {
      return result;
    }
  }
  return std::make_shared<MatrixGraphic>(graphic, matrix);
}

void MatrixGraphic::measureBounds(tgfx::Rect* bounds) const {
  graphic->measureBounds(bounds);
  matrix.mapRect(bounds);
}

bool MatrixGraphic::hitTest(RenderCache* cache, float x, float y) {
  tgfx::Point local = {x, y};
  if (!MapPointInverted(matrix, &local)) {
    return false;
  }
  return graphic->hitTest(cache, local.x, local.y);
}

bool MatrixGraphic::getPath(tgfx::Path* path) const {
  tgfx::Path fillPath = {};
  if (!graphic->getPath(&fillPath)) {
    return false;
  }
  fillPath.transform(matrix);
  path->addPath(fillPath);
  return true;
}

void MatrixGraphic::prepare(RenderCache* cache) const {
  graphic->prepare(cache);
}

void MatrixGraphic::draw(tgfx::Canvas* canvas, RenderCache* cache) const {
  canvas->save();
  canvas->concat(matrix);
  graphic->draw(canvas, cache);
  canvas->restore();
}

std::shared_ptr<Graphic> MatrixGraphic::mergeWith(const tgfx::Matrix& m) const {
  auto totalMatrix = matrix;
  totalMatrix.postConcat(m);
  if (totalMatrix.isIdentity()) {
    return graphic;
  }
  return std::make_shared<MatrixGraphic>(graphic, totalMatrix);
}
//===================================== MatrixGraphic ==============================================

//===================================== LayerGraphic ===============================================
class LayerGraphic : public ComposeGraphic {
 public:
  explicit LayerGraphic(std::vector<std::shared_ptr<Graphic>> contents)
      : contents(std::move(contents)) {
  }

  void measureBounds(tgfx::Rect* bounds) const override;
  bool hitTest(RenderCache* cache, float x, float y) override;
  bool getPath(tgfx::Path* path) const override;
  void prepare(RenderCache* cache) const override;
  void draw(tgfx::Canvas* canvas, RenderCache* cache) const override;
  std::shared_ptr<Graphic> mergeWith(const tgfx::Matrix& matrix) const override;

 private:
  std::vector<std::shared_ptr<Graphic>> contents = {};
};

std::shared_ptr<Graphic> Graphic::MakeCompose(std::vector<std::shared_ptr<Graphic>> contents) {
  std::vector<std::shared_ptr<Graphic>> graphics = {};
  for (auto& content : contents) {
    if (content == nullptr) {
      continue;
    }
    graphics.push_back(content);
  }
  if (graphics.empty()) {
    return nullptr;
  }
  if (graphics.size() == 1) {
    return graphics[0];
  }
  return std::make_shared<LayerGraphic>(graphics);
}

void LayerGraphic::measureBounds(tgfx::Rect* bounds) const {
  bounds->setEmpty();
  for (auto& content : contents) {
    tgfx::Rect rect = tgfx::Rect::MakeEmpty();
    content->measureBounds(&rect);
    bounds->join(rect);
  }
}

bool LayerGraphic::hitTest(RenderCache* cache, float x, float y) {
  return std::any_of(contents.begin(), contents.end(), [=](std::shared_ptr<Graphic>& graphic) {
    return graphic->hitTest(cache, x, y);
  });
}

bool LayerGraphic::getPath(tgfx::Path* path) const {
  tgfx::Path fillPath = {};
  for (auto& content : contents) {
    tgfx::Path temp = {};
    if (!content->getPath(&temp)) {
      return false;
    }
    fillPath.addPath(temp, tgfx::PathOp::Union);
  }
  path->addPath(fillPath);
  return true;
}

void LayerGraphic::prepare(RenderCache* cache) const {
  for (auto& content : contents) {
    content->prepare(cache);
  }
}

void LayerGraphic::draw(tgfx::Canvas* canvas, RenderCache* cache) const {
  for (auto& content : contents) {
    canvas->save();
    content->draw(canvas, cache);
    canvas->restore();
  }
}

std::shared_ptr<Graphic> LayerGraphic::mergeWith(const tgfx::Matrix& m) const {
  std::vector<std::shared_ptr<Graphic>> newContents = {};
  for (auto& graphic : contents) {
    if (graphic->type() != GraphicType::Compose) {
      return nullptr;
    }
    auto result = static_cast<ComposeGraphic*>(graphic.get())->mergeWith(m);
    if (!result) {
      return nullptr;
    }
    newContents.push_back(result);
  }
  return std::make_shared<LayerGraphic>(newContents);
}
//===================================== LayerGraphic ===============================================

//==================================== ModifierGraphic =============================================
class ModifierGraphic : public ComposeGraphic {
 public:
  ModifierGraphic(std::shared_ptr<Graphic> graphic, std::shared_ptr<Modifier> modifier)
      : graphic(std::move(graphic)), modifier(std::move(modifier)) {
  }

  void measureBounds(tgfx::Rect* bounds) const override;
  bool hitTest(RenderCache* cache, float x, float y) override;
  bool getPath(tgfx::Path* path) const override;
  void prepare(RenderCache* cache) const override;
  void draw(tgfx::Canvas* canvas, RenderCache* cache) const override;
  std::shared_ptr<Graphic> mergeWith(const Modifier* target) const override;

 private:
  std::shared_ptr<Graphic> graphic = nullptr;
  std::shared_ptr<Modifier> modifier = nullptr;
};

std::shared_ptr<Graphic> Graphic::MakeCompose(std::shared_ptr<Graphic> graphic,
                                              std::shared_ptr<Modifier> modifier) {
  if (modifier == nullptr) {
    return graphic;
  }
  if (graphic == nullptr || modifier->isEmpty()) {
    return nullptr;
  }
  if (graphic->type() == GraphicType::Compose) {
    auto result = std::static_pointer_cast<ComposeGraphic>(graphic)->mergeWith(modifier.get());
    if (result) {
      return result;
    }
  }
  return std::make_shared<ModifierGraphic>(graphic, modifier);
}

void ModifierGraphic::measureBounds(tgfx::Rect* bounds) const {
  graphic->measureBounds(bounds);
  modifier->applyToBounds(bounds);
}

bool ModifierGraphic::hitTest(RenderCache* cache, float x, float y) {
  if (!modifier->hitTest(cache, x, y)) {
    return false;
  }
  return graphic->hitTest(cache, x, y);
}

bool ModifierGraphic::getPath(tgfx::Path* path) const {
  tgfx::Path fillPath = {};
  if (!graphic->getPath(&fillPath)) {
    return false;
  }
  if (!modifier->applyToPath(&fillPath)) {
    return false;
  }
  path->addPath(fillPath);
  return true;
}

void ModifierGraphic::prepare(RenderCache* cache) const {
  modifier->prepare(cache);
  graphic->prepare(cache);
}

void ModifierGraphic::draw(tgfx::Canvas* canvas, RenderCache* cache) const {
  canvas->save();
  modifier->applyToGraphic(canvas, cache, graphic);
  canvas->restore();
}

std::shared_ptr<Graphic> ModifierGraphic::mergeWith(const Modifier* target) const {
  if (target == nullptr || modifier->type() != target->type()) {
    return nullptr;
  }
  auto newModifier = modifier->mergeWith(target);
  if (newModifier == nullptr) {
    return nullptr;
  }
  return std::make_shared<ModifierGraphic>(graphic, newModifier);
}
//==================================== ModifierGraphic =============================================

}  // namespace pag
