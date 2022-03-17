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

#include "CompositionRenderer.h"
#include "LayerRenderer.h"
#include "rendering/caches/LayerCache.h"
#include "rendering/caches/RenderCache.h"
#include "rendering/graphics/Picture.h"
#include "rendering/graphics/Recorder.h"
#include "rendering/readers/BitmapSequenceReader.h"

namespace pag {
class SequenceProxy : public TextureProxy {
 public:
  SequenceProxy(Sequence* sequence, Frame frame, int width, int height)
      : TextureProxy(width, height), sequence(sequence), frame(frame) {
  }

  bool cacheEnabled() const override {
    return !sequence->composition->staticContent();
  }

  void prepare(RenderCache* cache) const override {
    static_cast<RenderCache*>(cache)->prepareSequenceReader(sequence, frame,
                                                            DecodingPolicy::SoftwareToHardware);
  }

  std::shared_ptr<tgfx::Texture> getTexture(RenderCache* cache) const override {
    auto reader = static_cast<RenderCache*>(cache)->getSequenceReader(sequence);
    if (reader) {
      return reader->readTexture(frame, static_cast<RenderCache*>(cache));
    }
    return nullptr;
  }

 private:
  Sequence* sequence = nullptr;
  Frame frame = 0;
};

static std::shared_ptr<Graphic> MakeVideoSequenceGraphic(VideoSequence* sequence,
                                                         Frame contentFrame) {
  // 视频序列帧导出时没有记录准确的画面总宽高，需要自己通过 width 和 alphaStartX 计算，
  // 如果遇到奇数尺寸导出插件会自动加一，这里匹配导出插件的规则。
  auto videoWidth = sequence->alphaStartX + sequence->width;
  if (videoWidth % 2 == 1) {
    videoWidth++;
  }
  auto videoHeight = sequence->alphaStartY + sequence->height;
  if (videoHeight % 2 == 1) {
    videoHeight++;
  }
  auto proxy = new SequenceProxy(sequence, contentFrame, videoWidth, videoHeight);
  tgfx::RGBAAALayout layout = {sequence->width, sequence->height, sequence->alphaStartX,
                               sequence->alphaStartY};
  return Picture::MakeFrom(sequence->composition->uniqueID, std::unique_ptr<SequenceProxy>(proxy),
                           layout);
}

std::shared_ptr<Graphic> RenderVectorComposition(VectorComposition* composition,
                                                 Frame compositionFrame) {
  Recorder recorder = {};
  recorder.saveClip(0, 0, static_cast<float>(composition->width),
                    static_cast<float>(composition->height));
  auto& layers = composition->layers;
  // The index order of Layers in File is different from PAGLayers.
  for (int i = static_cast<int>(layers.size()) - 1; i >= 0; i--) {
    auto childLayer = layers[i];
    if (!childLayer->isActive) {
      continue;
    }
    auto layerCache = LayerCache::Get(childLayer);
    auto filterModifier =
        layerCache->cacheFilters() ? nullptr : FilterModifier::Make(childLayer, compositionFrame);
    auto trackMatte = TrackMatteRenderer::Make(childLayer, compositionFrame);
    LayerRenderer::DrawLayer(&recorder, childLayer, compositionFrame, filterModifier,
                             trackMatte.get());
  }
  recorder.restore();
  auto graphic = recorder.makeGraphic();
  if (layers.size() > 1 && composition->staticContent() && !composition->hasImageContent()) {
    // 仅当子项列表只存在矢量内容并图层数量大于 1 时才包装一个 Image，避免重复的 Image 包装。
    graphic = Picture::MakeFrom(composition->uniqueID, graphic);
  }
  return graphic;
}

std::shared_ptr<Graphic> RenderSequenceComposition(Composition* composition,
                                                   Frame compositionFrame) {
  auto sequence = Sequence::Get(composition);
  if (sequence == nullptr) {
    return nullptr;
  }
  auto sequenceFrame = sequence->toSequenceFrame(compositionFrame);
  std::shared_ptr<Graphic> graphic = nullptr;
  if (composition->type() == CompositionType::Video) {
    graphic = MakeVideoSequenceGraphic(static_cast<VideoSequence*>(sequence), sequenceFrame);
  } else {
    auto proxy = new SequenceProxy(sequence, sequenceFrame, sequence->width, sequence->height);
    graphic =
        Picture::MakeFrom(sequence->composition->uniqueID, std::unique_ptr<SequenceProxy>(proxy));
  }
  auto scaleX = static_cast<float>(composition->width) / static_cast<float>(sequence->width);
  auto scaleY = static_cast<float>(composition->height) / static_cast<float>(sequence->height);
  return Graphic::MakeCompose(graphic, tgfx::Matrix::MakeScale(scaleX, scaleY));
}
}  // namespace pag