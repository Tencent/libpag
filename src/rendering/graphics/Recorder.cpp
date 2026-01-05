/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include "Recorder.h"

namespace pag {
enum class RecordType { Matrix, Layer };

class Record {
 public:
  explicit Record(const tgfx::Matrix& matrix) : matrix(matrix) {
  }

  virtual ~Record() = default;

  virtual RecordType type() const {
    return RecordType::Matrix;
  }

  tgfx::Matrix matrix = {};
};

class LayerRecord : public Record {
 public:
  LayerRecord(const tgfx::Matrix& matrix, std::shared_ptr<Modifier> modifier,
              std::vector<std::shared_ptr<Graphic>> contents)
      : Record(matrix), modifier(std::move(modifier)), oldNodes(std::move(contents)) {
  }

  RecordType type() const override {
    return RecordType::Layer;
  }

  std::shared_ptr<Modifier> modifier = nullptr;
  std::vector<std::shared_ptr<Graphic>> oldNodes = {};
};

tgfx::Matrix Recorder::getMatrix() const {
  tgfx::Matrix totalMatrix = matrix;
  for (int i = static_cast<int>(records.size() - 1); i >= 0; i--) {
    auto& record = records[i];
    if (record->type() == RecordType::Layer) {
      totalMatrix.postConcat(record->matrix);
    }
  }
  return totalMatrix;
}

void Recorder::setMatrix(const tgfx::Matrix& m) {
  matrix = m;
  auto count = static_cast<int>(records.size());
  if (count > 0) {
    auto totalMatrix = tgfx::Matrix::I();
    for (auto i = count - 1; i >= 0; i--) {
      auto& record = records[i];
      if (record->type() == RecordType::Layer) {
        totalMatrix.postConcat(record->matrix);
      }
    }
    if (totalMatrix.invert(&totalMatrix)) {
      matrix.postConcat(totalMatrix);
    }
  }
}

void Recorder::concat(const tgfx::Matrix& m) {
  matrix.preConcat(m);
}

void Recorder::saveClip(float x, float y, float width, float height) {
  tgfx::Path path = {};
  path.addRect(tgfx::Rect::MakeXYWH(x, y, width, height));
  saveClip(path);
}

void Recorder::saveClip(const tgfx::Path& path) {
  auto modifier = Modifier::MakeClip(path);
  saveLayer(modifier);
}

void Recorder::saveLayer(float alpha, tgfx::BlendMode blendMode) {
  auto modifier = Modifier::MakeBlend(alpha, blendMode);
  saveLayer(modifier);
}

void Recorder::saveLayer(std::shared_ptr<Modifier> modifier) {
  if (modifier == nullptr) {
    save();
    return;
  }
  auto record = std::make_shared<LayerRecord>(matrix, modifier, layerContents);
  records.push_back(record);
  matrix = tgfx::Matrix::I();
  layerContents = {};
  layerIndex++;
}

void Recorder::save() {
  auto record = std::make_shared<Record>(matrix);
  records.push_back(record);
}

void Recorder::restore() {
  if (records.empty()) {
    return;
  }
  auto record = records.back();
  records.pop_back();
  matrix = record->matrix;
  if (record->type() == RecordType::Layer) {
    layerIndex--;
    auto layerRecord = std::static_pointer_cast<LayerRecord>(record);
    auto layerGraphic = Graphic::MakeCompose(layerContents);
    layerGraphic = Graphic::MakeCompose(layerGraphic, layerRecord->modifier);
    layerContents = layerRecord->oldNodes;
    drawGraphic(layerGraphic);
  }
}

size_t Recorder::getSaveCount() const {
  return records.size();
}

void Recorder::restoreToCount(size_t saveCount) {
  while (records.size() > saveCount) {
    restore();
  }
}

void Recorder::drawGraphic(std::shared_ptr<Graphic> graphic, const tgfx::Matrix& m) {
  auto oldMatrix = matrix;
  matrix.preConcat(m);
  drawGraphic(std::move(graphic));
  matrix = oldMatrix;
}

void Recorder::drawGraphic(std::shared_ptr<Graphic> graphic) {
  auto content = Graphic::MakeCompose(std::move(graphic), matrix);
  if (content == nullptr) {
    return;
  }
  if (layerIndex == 0) {
    rootContents.push_back(content);
  } else {
    layerContents.push_back(content);
  }
}

std::shared_ptr<Graphic> Recorder::makeGraphic() {
  return Graphic::MakeCompose(rootContents);
}
}  // namespace pag
