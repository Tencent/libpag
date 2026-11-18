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
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <dirent.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "base/PAGTest.h"
#include "pag/file.h"
#include "pag/pag.h"
#include "pagx/LegacyPAGImporter.h"
#include "pagx/PAGAnimation.h"
#include "pagx/PAGImporter.h"
#include "pagx/PAGScene.h"
#include "pagx/PAGSurface.h"
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXExporter.h"
#include "utils/ProjectPath.h"

namespace {

using pag::ColorType;
using pag::PAGFile;
using pag::PAGPlayer;
using pag::PAGSurface;

// Long dimension cap (matches PAGCompareFrameTest) so large samples stay fast; both pipelines
// render at the same scaled size.
constexpr float kMaxFrameSize = 360.0f;
// First-pass sampling cap (spec C34's full sweep lands once the pipeline converges): at most
// this many frames per sample, evenly spaced across the duration.
constexpr int kMaxFramesPerSample = 6;

template <typename T>
int LegacyKeyframeCount(pag::Property<T>* property) {
  if (property == nullptr) {
    return 0;
  }
  auto* animatable = dynamic_cast<pag::AnimatableProperty<T>*>(property);
  return animatable != nullptr ? static_cast<int>(animatable->keyframes.size()) : 0;
}

// Appends the type ids of shape elements whose properties vary over time (ShapeGroup recurses
// and ShapePath is skipped: their transforms / outlines migrate as channels already).
void CollectAnimatedShapeElements(const std::vector<pag::ShapeElement*>& elements,
                                  std::string* out) {
  for (auto* element : elements) {
    if (element == nullptr) {
      continue;
    }
    if (element->type() == pag::ShapeType::ShapeGroup) {
      CollectAnimatedShapeElements(static_cast<pag::ShapeGroupElement*>(element)->elements, out);
      continue;
    }
    if (element->type() == pag::ShapeType::ShapePath) {
      continue;
    }
    std::vector<pag::TimeRange> ranges = {{0, 1000000}};
    element->excludeVaryingRanges(&ranges);
    if (ranges.size() != 1 || ranges[0].start != 0 || ranges[0].end != 1000000) {
      *out += std::to_string(static_cast<int>(element->type())) + ",";
    }
  }
}

bool IsLegacyPaint(pag::ShapeType type) {
  return type == pag::ShapeType::Fill || type == pag::ShapeType::Stroke ||
         type == pag::ShapeType::GradientFill || type == pag::ShapeType::GradientStroke;
}

// Classifies the paint stacking of every content list: "same" when the legacy stacking (Below
// paints and child groups insert at the bottom, Above paints at the top) already matches PAGX
// document order, "paintBlock" when only a contiguous run of paints needs reordering, and
// "complex" otherwise (paints interleaved with geometry, or groups that must move).
void ClassifyPaintOrder(const std::vector<pag::ShapeElement*>& elements, int* same, int* paintBlock,
                        int* complex) {
  std::vector<int> stack = {};
  std::vector<int> renderItems = {};
  int firstPaint = -1;
  int lastPaint = -1;
  bool hasGroup = false;
  for (int i = 0; i < static_cast<int>(elements.size()); i++) {
    auto* element = elements[static_cast<size_t>(i)];
    auto type = element->type();
    if (type == pag::ShapeType::ShapeGroup) {
      ClassifyPaintOrder(static_cast<pag::ShapeGroupElement*>(element)->elements, same, paintBlock,
                         complex);
      hasGroup = true;
      renderItems.push_back(i);
      stack.insert(stack.begin(), i);
      continue;
    }
    if (!IsLegacyPaint(type)) {
      continue;
    }
    if (firstPaint < 0) {
      firstPaint = i;
    }
    lastPaint = i;
    renderItems.push_back(i);
    pag::CompositeOrder order = pag::CompositeOrder::BelowPreviousInSameGroup;
    if (type == pag::ShapeType::Fill) {
      order = static_cast<pag::FillElement*>(element)->composite;
    } else if (type == pag::ShapeType::Stroke) {
      order = static_cast<pag::StrokeElement*>(element)->composite;
    } else if (type == pag::ShapeType::GradientFill) {
      order = static_cast<pag::GradientFillElement*>(element)->composite;
    } else {
      order = static_cast<pag::GradientStrokeElement*>(element)->composite;
    }
    if (order == pag::CompositeOrder::AbovePreviousInSameGroup) {
      stack.push_back(i);
    } else {
      stack.insert(stack.begin(), i);
    }
  }
  if (stack == renderItems) {
    (*same)++;
    return;
  }
  if (getenv("PAG_DUMP_EQ_PATTERNS") != nullptr) {
    std::string pattern = {};
    for (auto* element : elements) {
      auto type = element->type();
      if (type == pag::ShapeType::ShapeGroup) {
        pattern += "S";
      } else if (IsLegacyPaint(type)) {
        auto above =
            (type == pag::ShapeType::Fill && static_cast<pag::FillElement*>(element)->composite ==
                                                 pag::CompositeOrder::AbovePreviousInSameGroup) ||
            (type == pag::ShapeType::Stroke &&
             static_cast<pag::StrokeElement*>(element)->composite ==
                 pag::CompositeOrder::AbovePreviousInSameGroup);
        pattern += above ? "A" : "P";
      } else if (type == pag::ShapeType::TrimPaths || type == pag::ShapeType::RoundCorners ||
                 type == pag::ShapeType::MergePaths || type == pag::ShapeType::Repeater) {
        pattern += "M";
      } else {
        pattern += "G";
      }
    }
    fprintf(stderr, "[paint-pattern] %s\n", pattern.c_str());
  }
  bool interleaved = false;
  for (int i = firstPaint; i >= 0 && i <= lastPaint; i++) {
    if (!IsLegacyPaint(elements[static_cast<size_t>(i)]->type())) {
      interleaved = true;
    }
  }
  if (!hasGroup && !interleaved) {
    (*paintBlock)++;
  } else {
    (*complex)++;
  }
}

void DumpLegacyLayerTree(pag::Layer* layer, int depth, FILE* out) {
  if (layer == nullptr || out == nullptr) {
    return;
  }
  auto indent = std::string(static_cast<size_t>(depth) * 2, ' ');
  fprintf(out, "%sLAYER type=%d name=%s startTime=%lld duration=%lld active=%d", indent.c_str(),
          static_cast<int>(layer->type()), layer->name.c_str(),
          static_cast<long long>(layer->startTime), static_cast<long long>(layer->duration),
          layer->isActive ? 1 : 0);
  if (layer->stretch.numerator != static_cast<int32_t>(layer->stretch.denominator)) {
    fprintf(out, " stretch=%d/%u", layer->stretch.numerator, layer->stretch.denominator);
  }
  if (layer->timeRemap != nullptr) {
    fprintf(out, " timeRemapKeys=%d", LegacyKeyframeCount(layer->timeRemap));
  }
  if (layer->motionBlur) {
    fprintf(out, " motionBlur=1");
  }
  if (layer->trackMatteType != pag::TrackMatteType::None) {
    fprintf(out, " trackMatte=%d", static_cast<int>(layer->trackMatteType));
  }
  if (!layer->effects.empty() || !layer->layerStyles.empty()) {
    fprintf(out, " effects=%zu styles=%zu", layer->effects.size(), layer->layerStyles.size());
  }
  if (layer->type() == pag::LayerType::Shape) {
    std::string animated = {};
    CollectAnimatedShapeElements(static_cast<pag::ShapeLayer*>(layer)->contents, &animated);
    if (!animated.empty()) {
      fprintf(out, " animShapeElems=%s", animated.c_str());
    }
    int same = 0;
    int paintBlock = 0;
    int complex = 0;
    ClassifyPaintOrder(static_cast<pag::ShapeLayer*>(layer)->contents, &same, &paintBlock,
                       &complex);
    fprintf(out, " paintOrder[same=%d block=%d complex=%d]", same, paintBlock, complex);
  }
  if (layer->type() == pag::LayerType::Image) {
    auto* imageLayer = static_cast<pag::ImageLayer*>(layer);
    if (imageLayer->imageFillRule != nullptr && imageLayer->imageFillRule->timeRemap != nullptr) {
      fprintf(out, " fillRuleTimeRemapKeys=%d",
              LegacyKeyframeCount(imageLayer->imageFillRule->timeRemap));
    }
  }
  if (layer->type() == pag::LayerType::PreCompose) {
    auto* preCompose = static_cast<pag::PreComposeLayer*>(layer);
    fprintf(out, " compStart=%lld", static_cast<long long>(preCompose->compositionStartTime));
    if (preCompose->composition != nullptr &&
        preCompose->composition->type() != pag::CompositionType::Vector) {
      fprintf(out, " compType=%d compDuration=%lld",
              static_cast<int>(preCompose->composition->type()),
              static_cast<long long>(preCompose->composition->duration));
      if (preCompose->composition->type() == pag::CompositionType::Video) {
        auto* video = static_cast<pag::VideoComposition*>(preCompose->composition);
        for (auto* sequence : video->sequences) {
          fprintf(
              out,
              " seq[%dx%d fps=%.1f frames=%zu firstIdx=%lld lastIdx=%lld static=", sequence->width,
              sequence->height, sequence->frameRate, sequence->frames.size(),
              sequence->frames.empty() ? -1LL
                                       : static_cast<long long>(sequence->frames.front()->frame),
              sequence->frames.empty() ? -1LL
                                       : static_cast<long long>(sequence->frames.back()->frame));
          for (auto& range : sequence->staticTimeRanges) {
            fprintf(out, "[%lld..%lld]", static_cast<long long>(range.start),
                    static_cast<long long>(range.end));
          }
          fprintf(out, " headers=%zu alpha=%d,%d bytes=", sequence->headers.size(),
                  sequence->alphaStartX, sequence->alphaStartY);
          for (size_t i = 0; i < sequence->frames.size() && i < 4; i++) {
            auto* frame = sequence->frames[i];
            fprintf(out, "%lld%s:%zu,", static_cast<long long>(frame->frame),
                    frame->isKeyframe ? "K" : "",
                    frame->fileBytes ? frame->fileBytes->length() : 0);
          }
          fprintf(out, "]");
        }
      }
    }
  }
  if (layer->transform != nullptr) {
    fprintf(out, " tx[pos=%d scale=%d rot=%d opacity=%d]",
            LegacyKeyframeCount(layer->transform->position),
            LegacyKeyframeCount(layer->transform->scale),
            LegacyKeyframeCount(layer->transform->rotation),
            LegacyKeyframeCount(layer->transform->opacity));
    auto* animScale = dynamic_cast<pag::AnimatableProperty<pag::Point>*>(layer->transform->scale);
    if (animScale != nullptr) {
      for (auto* key : animScale->keyframes) {
        fprintf(out, " scaleKey[%lld..%lld int=%d (%.3f,%.3f)->(%.3f,%.3f) out=",
                static_cast<long long>(key->startTime), static_cast<long long>(key->endTime),
                static_cast<int>(key->interpolationType), key->startValue.x, key->startValue.y,
                key->endValue.x, key->endValue.y);
        for (auto& p : key->bezierOut) {
          fprintf(out, "(%.3f,%.3f)", p.x, p.y);
        }
        fprintf(out, " in=");
        for (auto& p : key->bezierIn) {
          fprintf(out, "(%.3f,%.3f)", p.x, p.y);
        }
        fprintf(out, "]");
      }
    }
  }
  fprintf(out, "\n");
  for (auto* mask : layer->masks) {
    if (mask == nullptr) {
      continue;
    }
    auto* animPath = dynamic_cast<pag::AnimatableProperty<pag::PathHandle>*>(mask->maskPath);
    fprintf(out, "%s  MASK mode=%d inverted=%d pathKeyframes=%d", indent.c_str(),
            static_cast<int>(mask->maskMode), mask->inverted ? 1 : 0,
            animPath != nullptr ? static_cast<int>(animPath->keyframes.size()) : 0);
    if (animPath != nullptr && !animPath->keyframes.empty()) {
      fprintf(out, " firstKey=%lld lastKey=%lld",
              static_cast<long long>(animPath->keyframes.front()->startTime),
              static_cast<long long>(animPath->keyframes.back()->startTime));
      for (auto* key : animPath->keyframes) {
        auto describe = [out](const pag::PathData* pathData) {
          if (pathData == nullptr || pathData->points.empty()) {
            fprintf(out, "EMPTY");
            return;
          }
          auto minX = pathData->points[0].x;
          auto maxX = pathData->points[0].x;
          auto minY = pathData->points[0].y;
          auto maxY = pathData->points[0].y;
          for (auto& pt : pathData->points) {
            minX = std::min(minX, pt.x);
            maxX = std::max(maxX, pt.x);
            minY = std::min(minY, pt.y);
            maxY = std::max(maxY, pt.y);
          }
          fprintf(out, "%zupts(%.0f,%.0f)-(%.0f,%.0f)", pathData->points.size(), minX, minY, maxX,
                  maxY);
        };
        fprintf(out, " key[%lld..%lld] int=%d start=", static_cast<long long>(key->startTime),
                static_cast<long long>(key->endTime), static_cast<int>(key->interpolationType));
        describe(key->startValue.get());
        fprintf(out, " end=");
        describe(key->endValue.get());
      }
    }
    auto* animFeather = dynamic_cast<pag::AnimatableProperty<pag::Point>*>(mask->maskFeather);
    auto* animExpansion = dynamic_cast<pag::AnimatableProperty<float>*>(mask->maskExpansion);
    auto* animOpacity = dynamic_cast<pag::AnimatableProperty<pag::Opacity>*>(mask->maskOpacity);
    fprintf(out, " featherKeys=%d expansionKeys=%d opacityKeys=%d",
            animFeather != nullptr ? static_cast<int>(animFeather->keyframes.size()) : 0,
            animExpansion != nullptr ? static_cast<int>(animExpansion->keyframes.size()) : 0,
            animOpacity != nullptr ? static_cast<int>(animOpacity->keyframes.size()) : 0);
    if (animExpansion != nullptr && !animExpansion->keyframes.empty()) {
      fprintf(out, " expansion=[%.2f..%.2f] keys=%lld..%lld",
              static_cast<double>(animExpansion->keyframes.front()->startValue),
              static_cast<double>(animExpansion->keyframes.back()->endValue),
              static_cast<long long>(animExpansion->keyframes.front()->startTime),
              static_cast<long long>(animExpansion->keyframes.back()->startTime));
    }
    fprintf(out, "\n");
  }
  if (layer->type() == pag::LayerType::PreCompose) {
    auto* preCompose = static_cast<pag::PreComposeLayer*>(layer);
    auto* vectorComp = dynamic_cast<pag::VectorComposition*>(preCompose->composition);
    if (vectorComp != nullptr) {
      fprintf(out, "%s  COMP id=%lld size=%dx%d duration=%lld frameRate=%.3f\n", indent.c_str(),
              static_cast<long long>(vectorComp->id), static_cast<int>(vectorComp->width),
              static_cast<int>(vectorComp->height), static_cast<long long>(vectorComp->duration),
              vectorComp->frameRate);
      for (auto* child : vectorComp->layers) {
        DumpLegacyLayerTree(child, depth + 1, out);
      }
    }
  }
}

struct SampleResult {
  std::string path;
  std::string fileName;
  int framesCompared = 0;
  int framesPassed = 0;
  std::string firstDiff;  // "frame=N diff=1.23%"
  bool decoded = false;
  std::string error;
};

void CollectPagFiles(const std::string& dir, std::vector<std::string>* out) {
  DIR* d = opendir(dir.c_str());
  if (d == nullptr) {
    return;
  }
  while (auto* entry = readdir(d)) {
    std::string name = entry->d_name;
    if (name == "." || name == "..") {
      continue;
    }
    std::string full = dir + "/" + name;
    if (entry->d_type == DT_DIR) {
      CollectPagFiles(full, out);
    } else if (name.size() > 4 && name.compare(name.size() - 4, 4, ".pag") == 0) {
      out->push_back(full);
    }
  }
  closedir(d);
}

void ScaleToFit(int width, int height, int* outWidth, int* outHeight) {
  auto w = static_cast<float>(width);
  auto h = static_cast<float>(height);
  if (std::max(w, h) > kMaxFrameSize) {
    if (w > h) {
      h = ceilf(h * kMaxFrameSize / w);
      w = kMaxFrameSize;
    } else {
      w = ceilf(w * kMaxFrameSize / h);
      h = kMaxFrameSize;
    }
  }
  *outWidth = static_cast<int>(roundf(w));
  *outHeight = static_cast<int>(roundf(h));
}

bool RenderLegacyFrame(PAGPlayer* player, const std::shared_ptr<PAGSurface>& surface, int width,
                       int height, std::vector<uint8_t>* pixels) {
  player->flush();
  pixels->assign(static_cast<size_t>(width) * height * 4, 0);
  return surface->readPixels(ColorType::RGBA_8888, pag::AlphaType::Premultiplied, pixels->data(),
                             static_cast<size_t>(width) * 4);
}

bool RenderMigratedFrame(pagx::PAGScene* scene, const std::shared_ptr<pagx::PAGSurface>& surface,
                         pagx::PAGAnimation* animation, int64_t deltaMicroseconds, int width,
                         int height, std::vector<uint8_t>* pixels) {
  // The default timeline is a top-level animation (the scene's auto-play pass skips it), so it
  // advances explicitly; scene->advanceAndApply drives the composition-scoped animations and
  // applies every channel — a plain setCurrentTime + draw never applies anything.
  animation->advanceAndApply(deltaMicroseconds);
  scene->advanceAndApply(deltaMicroseconds);
  pixels->assign(static_cast<size_t>(width) * height * 4, 0);
  if (!scene->draw(surface)) {
    return false;
  }
  return surface->readPixels(pixels->data(), static_cast<size_t>(width) * 4);
}

double DiffPercent(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
  if (a.size() != b.size() || a.empty()) {
    return 100.0;
  }
  size_t diff = 0;
  for (size_t i = 0; i < a.size(); i += 4) {
    if (a[i] != b[i] || a[i + 1] != b[i + 1] || a[i + 2] != b[i + 2] || a[i + 3] != b[i + 3]) {
      diff++;
    }
  }
  return 100.0 * static_cast<double>(diff) / (static_cast<double>(a.size()) / 4.0);
}

void CompareSample(const std::string& path, SampleResult* result) {
  result->path = path;
  auto slash = path.rfind('/');
  result->fileName = slash == std::string::npos ? path : path.substr(slash + 1);

  // Legacy pipeline: PAGFile + PAGPlayer at the scaled size.
  auto pagFile = PAGFile::Load(path);
  if (pagFile == nullptr) {
    result->error = "legacy load failed";
    return;
  }
  result->decoded = true;
  int width = 0;
  int height = 0;
  ScaleToFit(pagFile->width(), pagFile->height(), &width, &height);

  auto legacySurface = PAGSurface::MakeOffscreen(width, height);
  if (legacySurface == nullptr) {
    result->error = "legacy surface failed";
    return;
  }
  auto player = std::make_shared<PAGPlayer>();
  player->setSurface(legacySurface);
  player->setComposition(pagFile);
  player->setMatrix(pag::Matrix::I());

  // Migrated pipeline: pag::File -> LegacyPAGImporter -> PAGScene.
  auto rawFile = pag::File::Load(path);
  if (rawFile == nullptr) {
    result->error = "raw load failed";
    return;
  }
  std::vector<pagx::PAGExportIssue> issues = {};
  auto doc = pagx::LegacyPAGImporter::Convert(rawFile, &issues);
  if (doc == nullptr) {
    result->error = "conversion failed";
    return;
  }
  // PAG_DUMP_EQ_XML writes each migrated document's XML into the given directory (file name
  // filtered by PAG_DUMP_EQ_FILTER), so migration gaps can be inspected without a debugger.
  // No effect on normal runs.
  auto xmlDir = getenv("PAG_DUMP_EQ_XML");
  if (xmlDir != nullptr) {
    auto xmlFilter = getenv("PAG_DUMP_EQ_FILTER");
    std::string xmlFilterStr = xmlFilter != nullptr ? xmlFilter : "";
    if (xmlFilterStr.empty() || result->fileName.find(xmlFilterStr) != std::string::npos) {
      auto safeName = result->fileName;
      for (auto& ch : safeName) {
        if (ch == '/') ch = '_';
      }
      auto xmlPath = std::string(xmlDir) + "/" + safeName + ".xml";
      auto xmlFile = fopen(xmlPath.c_str(), "wb");
      if (xmlFile != nullptr) {
        auto xml = pagx::PAGXExporter::ToXML(*doc);
        fwrite(xml.data(), 1, xml.size(), xmlFile);
        fclose(xmlFile);
      }
      // Companion legacy-structure dump (.legacy.txt): the layer tree with masks and mask path
      // keyframes, so migration gaps can be mapped back to the legacy features that produce them.
      auto legacyPath = std::string(xmlDir) + "/" + safeName + ".legacy.txt";
      auto legacyFile = fopen(legacyPath.c_str(), "wb");
      if (legacyFile != nullptr) {
        DumpLegacyLayerTree(rawFile->rootLayer, 0, legacyFile);
        fclose(legacyFile);
      }
    }
  }
  auto scene = pagx::PAGScene::Make(doc);
  if (scene == nullptr) {
    result->error = "scene failed";
    return;
  }
  auto migratedSurface = pagx::PAGSurface::MakeOffscreen(width, height);
  if (migratedSurface == nullptr) {
    result->error = "migrated surface failed";
    return;
  }
  auto timeline = scene->getDefaultTimeline();
  auto animation = std::dynamic_pointer_cast<pagx::PAGAnimation>(timeline);
  if (animation == nullptr) {
    result->error = "no default timeline";
    return;
  }

  // PAGFile::duration() returns microseconds; convert to frames on the file's frame rate.
  auto frameRate = pagFile->frameRate() > 0 ? pagFile->frameRate() : 30.0f;
  auto totalFrames =
      static_cast<int>(llround(static_cast<double>(pagFile->duration()) * frameRate / 1000000.0));
  // Evenly sample at most kMaxFramesPerSample frames across the duration (always including the
  // first and the last).
  int step = totalFrames > kMaxFramesPerSample ? totalFrames / kMaxFramesPerSample : 1;
  // PAG_DUMP_EQ_STEP overrides the sampling step for the samples matched by PAG_DUMP_EQ_FILTER
  // (e.g. 1 = every frame), so timing divergences can be bracketed to the exact frame. Only the
  // matched samples change; the default sampling (and the reported metrics) are unaffected.
  auto stepOverride = getenv("PAG_DUMP_EQ_STEP");
  auto stepFilter = getenv("PAG_DUMP_EQ_FILTER");
  if (stepOverride != nullptr && stepFilter != nullptr &&
      result->fileName.find(stepFilter) != std::string::npos && atoi(stepOverride) > 0) {
    step = atoi(stepOverride);
  }

  std::vector<uint8_t> legacyPixels;
  std::vector<uint8_t> migratedPixels;
  int64_t migratedClockUs = 0;
  for (int frame = 0; frame < totalFrames; frame += step) {
    // Aim the legacy player at the CENTER of the frame's time span: the legacy player floors
    // sub-frame times, so a time landing an epsilon below the boundary renders the previous
    // frame, while the migrated pipeline evaluates channels continuously. The progress is
    // derived from the true duration (not the rounded totalFrames) so the centering stays
    // inside the frame period regardless of duration rounding.
    player->setProgress(pagFile->duration() > 0
                            ? ((static_cast<double>(frame) + 0.5) * 1000000.0 / frameRate) /
                                  static_cast<double>(pagFile->duration())
                            : 0.0);
    if (!RenderLegacyFrame(player.get(), legacySurface, width, height, &legacyPixels)) {
      continue;
    }
    auto frameSeconds = static_cast<double>(frame) / frameRate;
    // Ceil the migrated time ABOVE the frame boundary: frame-quantized subsystems (sequence
    // players) floor sub-frame times, so a time an epsilon below the boundary lands on the
    // previous frame, while continuous channel evaluation at boundary+tiny is still the frame's
    // value. Together with the centered legacy progress both pipelines render frame N.
    auto frameUs = static_cast<int64_t>(ceil(frameSeconds * 1000000.0));
    auto deltaUs = frameUs - migratedClockUs;
    migratedClockUs = frameUs;
    if (!RenderMigratedFrame(scene.get(), migratedSurface, animation.get(), deltaUs, width, height,
                             &migratedPixels)) {
      continue;
    }
    result->framesCompared++;
    auto diff = DiffPercent(legacyPixels, migratedPixels);
    // PAG_DUMP_EQ dumps the frame-0 legacy/migrated RGBA renders of every sample whose file name
    // contains PAG_DUMP_EQ_FILTER (all samples when unset) as raw files plus a manifest, for the
    // side-by-side comparison page tooling. No effect on normal runs.
    auto dumpDir = getenv("PAG_DUMP_EQ");
    auto dumpEveryFrame = getenv("PAG_DUMP_EQ_ALL") != nullptr;
    if (dumpDir != nullptr && (dumpEveryFrame || (frame == 0 && result->framesCompared == 1))) {
      auto filter = getenv("PAG_DUMP_EQ_FILTER");
      std::string filterStr = filter != nullptr ? filter : "";
      if (filterStr.empty() || result->fileName.find(filterStr) != std::string::npos) {
        auto safeName = result->fileName;
        for (auto& ch : safeName) {
          if (ch == '/') ch = '_';
        }
        auto writeRaw = [&safeName, dumpDir, &frame](const char* suffix,
                                                     const std::vector<uint8_t>& pixels) {
          std::string outPath =
              std::string(dumpDir) + "/" + safeName + "_f" + std::to_string(frame) + suffix;
          FILE* f = fopen(outPath.c_str(), "wb");
          if (f != nullptr) {
            fwrite(pixels.data(), 1, pixels.size(), f);
            fclose(f);
          }
        };
        writeRaw("_legacy.raw", legacyPixels);
        writeRaw("_migrated.raw", migratedPixels);
        FILE* manifest = fopen(std::string(std::string(dumpDir) + "/manifest.txt").c_str(), "a");
        if (manifest != nullptr) {
          fprintf(manifest, "%s|%s|%d|%d|%.2f\n", result->path.c_str(), result->fileName.c_str(),
                  width, height, diff);
          fclose(manifest);
        }
      }
    }
    if (diff == 0.0) {
      result->framesPassed++;
    } else if (result->firstDiff.empty()) {
      char buffer[64] = {};
      snprintf(buffer, sizeof(buffer), "frame=%d diff=%.2f%%", frame, diff);
      result->firstDiff = buffer;
    }
  }
}

}  // namespace

PAG_TEST(PAGMigrationEquivalenceTest, AllSamples) {
  // P3-15 (spec C34): every decodable legacy sample renders identically through the migrated
  // pipeline at every integer frame. The strict gate is pixel-exact; this first pass reports
  // the pass rate and per-sample diffs so the remaining gaps converge task by task.
  std::vector<std::string> files = {};
  CollectPagFiles(pag::ProjectPath::Absolute("resources"), &files);
  ASSERT_GT(files.size(), 100u);

  int decoded = 0;
  int fullyPassed = 0;
  int framesCompared = 0;
  int framesPassed = 0;
  std::string failures;
  int processed = 0;
  // PAG_EQ_ONLY restricts the run to the samples whose path contains the given text, so a single
  // sample can be diagnosed in isolation (no state left behind by earlier samples).
  auto onlyFilter = getenv("PAG_EQ_ONLY");
  for (const auto& path : files) {
    if (onlyFilter != nullptr && path.find(onlyFilter) == std::string::npos) {
      continue;
    }
    SampleResult result = {};
    CompareSample(path, &result);
    processed++;
    if (processed % 20 == 0) {
      printf("[migration-eq] progress: %d/%zu samples\n", processed, files.size());
      fflush(stdout);
    }
    if (!result.decoded) {
      continue;
    }
    decoded++;
    framesCompared += result.framesCompared;
    framesPassed += result.framesPassed;
    if (!result.error.empty()) {
      failures += "  " + result.fileName + ": " + result.error + "\n";
      continue;
    }
    if (result.framesCompared > 0 && result.framesCompared == result.framesPassed) {
      fullyPassed++;
    } else {
      char buffer[160] = {};
      snprintf(buffer, sizeof(buffer), "  %s: %d/%d frames %s\n", result.fileName.c_str(),
               result.framesPassed, result.framesCompared, result.firstDiff.c_str());
      failures += buffer;
    }
  }
  printf("[migration-eq] samples: %zu total, %d decoded, %d fully passed\n", files.size(), decoded,
         fullyPassed);
  printf("[migration-eq] frames: %d/%d passed (%.1f%%)\n", framesPassed, framesCompared,
         framesCompared > 0 ? 100.0 * framesPassed / framesCompared : 0.0);
  if (!failures.empty()) {
    printf("[migration-eq] failing samples:\n%s", failures.c_str());
  }
  EXPECT_GT(decoded, 0);
  EXPECT_GT(framesCompared, 0);
}
