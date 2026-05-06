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

#include "pagx/html/HTMLStaticImageRenderer.h"
#include <cmath>
#include <cstdint>
#include <fstream>
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/ColorStop.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/ConicGradient.h"
#include "renderer/LayerBuilder.h"
#include "tgfx/core/Bitmap.h"
#include "tgfx/core/ImageCodec.h"
#include "tgfx/core/Matrix.h"
#include "tgfx/core/Pixmap.h"
#include "tgfx/gpu/opengl/GLDevice.h"
#include "tgfx/layers/DisplayList.h"

namespace pagx {

namespace {

// Builds a fresh DiamondGradient inside `doc` whose coordinate space is the target tile's
// local space. The source may author its centre/radius in either the geometry's normalized
// 0..1 bounding box (when fitsToGeometry is true) or in the enclosing layer's absolute
// coordinate space. For the normalized case the gradient lives in the geometry's own frame,
// which after cloning becomes the new tile's frame 1:1 — no shift is needed. For the absolute
// case we must translate by (-left, -top) so the tile's top-left corresponds to the gradient
// origin. Previously the code always subtracted left/top, which in the common default
// (fitsToGeometry=true, centre=(0.5,0.5)) pushed the centre by tens of pixels — turning the
// entire tile into the gradient's last-stop colour (diamond_gradient symptom: flat #1E293B).
DiamondGradient* CloneDiamondGradientShifted(PAGXDocument* doc, const DiamondGradient* src,
                                             float left, float top) {
  auto clone = doc->makeNode<DiamondGradient>();
  clone->fitsToGeometry = src->fitsToGeometry;
  if (src->fitsToGeometry) {
    clone->center = src->center;
    clone->radius = src->radius;
  } else {
    clone->center = {src->center.x - left, src->center.y - top};
    clone->radius = src->radius;
  }
  clone->matrix = src->matrix;
  for (auto* srcStop : src->colorStops) {
    auto stop = doc->makeNode<ColorStop>();
    stop->offset = srcStop->offset;
    stop->color = srcStop->color;
    clone->colorStops.push_back(stop);
  }
  return clone;
}

// Builds a fresh ImagePattern inside `doc` whose matrix shifts along with the tile offset.
// ImagePattern's matrix maps from gradient-input coordinates to image-texture coordinates; we
// must prepend a translate(-left, -top) so that when the tile renderer feeds local pixels in,
// the sampler receives the same texels as the original full-layer render.
ImagePattern* CloneImagePatternShifted(PAGXDocument* doc, const ImagePattern* src, float left,
                                       float top) {
  auto clone = doc->makeNode<ImagePattern>();
  // Reuse the same Image node pointer: Image is owned by the *source* document, but LayerBuilder
  // only reads its data/filePath fields which remain valid for the lifetime of the original doc.
  // Since we render synchronously before returning, the source doc outlives this call.
  clone->image = src->image;
  clone->tileModeX = src->tileModeX;
  clone->tileModeY = src->tileModeY;
  clone->filterMode = src->filterMode;
  clone->mipmapMode = src->mipmapMode;
  // Preserve scaleMode so the offline PNG render honors the source's absolute-vs-fit intent.
  // Without this the clone inherits tgfx's new default (LetterBox), which stretches the image
  // to cover the geometry and silently breaks mirror/clamp tile tests that expect absolute
  // pattern coordinates.
  clone->scaleMode = src->scaleMode;
  // New matrix = src->matrix * translate(-left, -top)
  auto m = src->matrix;
  // Let T = [1 0 -left; 0 1 -top; 0 0 1]. Then src->matrix * T has translation components
  //   tx' = src.a*(-left) + src.b*(-top) + src.tx
  //   ty' = src.c*(-left) + src.d*(-top) + src.ty
  Matrix shifted = m;
  shifted.tx = m.a * (-left) + m.b * (-top) + m.tx;
  shifted.ty = m.c * (-left) + m.d * (-top) + m.ty;
  clone->matrix = shifted;
  return clone;
}

// Encodes a tgfx::Surface to a PNG file. Returns true on success.
bool WriteSurfaceAsPng(tgfx::Surface* surface, int pxWidth, int pxHeight,
                       const std::string& outputPath) {
  tgfx::Bitmap bitmap(pxWidth, pxHeight, false, false);
  if (bitmap.isEmpty()) {
    return false;
  }
  tgfx::Pixmap pixmap(bitmap);
  if (!surface->readPixels(pixmap.info(), pixmap.writablePixels())) {
    return false;
  }
  auto data = tgfx::ImageCodec::Encode(pixmap, tgfx::EncodedFormat::PNG, 100);
  if (!data) {
    return false;
  }
  std::ofstream f(outputPath, std::ios::binary);
  if (!f.is_open()) {
    return false;
  }
  f.write(reinterpret_cast<const char*>(data->data()), static_cast<std::streamsize>(data->size()));
  return f.good();
}

// Core render helper: builds a minimal PAGXDocument consisting of one geo element filled with
// `colorSourceInDoc` (already translated into the tile's local coordinate space), rasterizes it
// via tgfx, and writes the result to `outputPath`.
bool RenderTileToPng(PAGXDocument* doc, int cssWidth, int cssHeight, float pixelRatio,
                     const std::string& outputPath) {
  if (cssWidth <= 0 || cssHeight <= 0) {
    return false;
  }
  doc->applyLayout();

  auto device = tgfx::GLDevice::Make();
  if (!device) {
    return false;
  }
  auto context = device->lockContext();
  if (!context) {
    return false;
  }

  int pxWidth = static_cast<int>(std::ceil(static_cast<float>(cssWidth) * pixelRatio));
  int pxHeight = static_cast<int>(std::ceil(static_cast<float>(cssHeight) * pixelRatio));

  auto layer = LayerBuilder::Build(doc);
  if (!layer) {
    device->unlock();
    return false;
  }
  layer->setMatrix(tgfx::Matrix::MakeScale(pixelRatio));

  auto surface = tgfx::Surface::Make(context, pxWidth, pxHeight);
  if (!surface) {
    device->unlock();
    return false;
  }
  tgfx::DisplayList displayList;
  displayList.root()->addChild(layer);
  displayList.render(surface.get(), false);

  bool ok = WriteSurfaceAsPng(surface.get(), pxWidth, pxHeight, outputPath);
  device->unlock();
  return ok;
}

// Configures the shared portion of the minimal document for a rectangle or ellipse tile. The
// geo's `position` is set to (w/2, h/2) so the shape fills the whole temporary layer. Roundness
// is only honored for Rectangle and is clamped by the caller if needed.
Layer* BuildSingleElementLayer(PAGXDocument* doc, int width, int height) {
  auto layer = doc->makeNode<Layer>();
  layer->width = static_cast<float>(width);
  layer->height = static_cast<float>(height);
  doc->layers.push_back(layer);
  return layer;
}

}  // namespace

bool HTMLStaticImageRenderer::RenderDiamondToPng(float left, float top, float width, float height,
                                                 float roundness, const DiamondGradient* gradient,
                                                 float pixelRatio, const std::string& outputPath) {
  if (!gradient || width <= 0 || height <= 0) {
    return false;
  }
  int w = static_cast<int>(std::ceil(width));
  int h = static_cast<int>(std::ceil(height));
  auto doc = PAGXDocument::Make(static_cast<float>(w), static_cast<float>(h));
  auto layer = BuildSingleElementLayer(doc.get(), w, h);

  auto rect = doc->makeNode<Rectangle>();
  rect->position = {static_cast<float>(w) / 2.0f, static_cast<float>(h) / 2.0f};
  rect->size = {static_cast<float>(w), static_cast<float>(h)};
  rect->roundness = roundness;

  auto fill = doc->makeNode<Fill>();
  fill->color = CloneDiamondGradientShifted(doc.get(), gradient, left, top);
  layer->contents = {rect, fill};

  return RenderTileToPng(doc.get(), w, h, pixelRatio, outputPath);
}

bool HTMLStaticImageRenderer::RenderDiamondEllipseToPng(float left, float top, float width,
                                                        float height,
                                                        const DiamondGradient* gradient,
                                                        float pixelRatio,
                                                        const std::string& outputPath) {
  if (!gradient || width <= 0 || height <= 0) {
    return false;
  }
  int w = static_cast<int>(std::ceil(width));
  int h = static_cast<int>(std::ceil(height));
  auto doc = PAGXDocument::Make(static_cast<float>(w), static_cast<float>(h));
  auto layer = BuildSingleElementLayer(doc.get(), w, h);

  auto ellipse = doc->makeNode<Ellipse>();
  ellipse->position = {static_cast<float>(w) / 2.0f, static_cast<float>(h) / 2.0f};
  ellipse->size = {static_cast<float>(w), static_cast<float>(h)};

  auto fill = doc->makeNode<Fill>();
  fill->color = CloneDiamondGradientShifted(doc.get(), gradient, left, top);
  layer->contents = {ellipse, fill};

  return RenderTileToPng(doc.get(), w, h, pixelRatio, outputPath);
}

bool HTMLStaticImageRenderer::RenderImagePatternToPng(float left, float top, float width,
                                                      float height, float roundness,
                                                      const ImagePattern* pattern, float pixelRatio,
                                                      const std::string& outputPath) {
  if (!pattern || !pattern->image || width <= 0 || height <= 0) {
    return false;
  }
  int w = static_cast<int>(std::ceil(width));
  int h = static_cast<int>(std::ceil(height));
  auto doc = PAGXDocument::Make(static_cast<float>(w), static_cast<float>(h));
  auto layer = BuildSingleElementLayer(doc.get(), w, h);

  auto rect = doc->makeNode<Rectangle>();
  rect->position = {static_cast<float>(w) / 2.0f, static_cast<float>(h) / 2.0f};
  rect->size = {static_cast<float>(w), static_cast<float>(h)};
  rect->roundness = roundness;

  auto fill = doc->makeNode<Fill>();
  fill->color = CloneImagePatternShifted(doc.get(), pattern, left, top);
  layer->contents = {rect, fill};

  return RenderTileToPng(doc.get(), w, h, pixelRatio, outputPath);
}

bool HTMLStaticImageRenderer::RenderImagePatternEllipseToPng(float left, float top, float width,
                                                             float height,
                                                             const ImagePattern* pattern,
                                                             float pixelRatio,
                                                             const std::string& outputPath) {
  if (!pattern || !pattern->image || width <= 0 || height <= 0) {
    return false;
  }
  int w = static_cast<int>(std::ceil(width));
  int h = static_cast<int>(std::ceil(height));
  auto doc = PAGXDocument::Make(static_cast<float>(w), static_cast<float>(h));
  auto layer = BuildSingleElementLayer(doc.get(), w, h);

  auto ellipse = doc->makeNode<Ellipse>();
  ellipse->position = {static_cast<float>(w) / 2.0f, static_cast<float>(h) / 2.0f};
  ellipse->size = {static_cast<float>(w), static_cast<float>(h)};

  auto fill = doc->makeNode<Fill>();
  fill->color = CloneImagePatternShifted(doc.get(), pattern, left, top);
  layer->contents = {ellipse, fill};

  return RenderTileToPng(doc.get(), w, h, pixelRatio, outputPath);
}

bool HTMLStaticImageRenderer::RenderConicGradientToPng(float left, float top, float width,
                                                       float height, const ConicGradient* gradient,
                                                       float pixelRatio,
                                                       const std::string& outputPath) {
  if (!gradient || width <= 0 || height <= 0) {
    return false;
  }
  int w = static_cast<int>(std::ceil(width));
  int h = static_cast<int>(std::ceil(height));
  auto doc = PAGXDocument::Make(static_cast<float>(w), static_cast<float>(h));
  auto layer = BuildSingleElementLayer(doc.get(), w, h);

  auto rect = doc->makeNode<Rectangle>();
  rect->position = {static_cast<float>(w) / 2.0f, static_cast<float>(h) / 2.0f};
  rect->size = {static_cast<float>(w), static_cast<float>(h)};

  auto fill = doc->makeNode<Fill>();
  auto clone = doc->makeNode<ConicGradient>();
  clone->fitsToGeometry = gradient->fitsToGeometry;
  clone->startAngle = gradient->startAngle;
  clone->endAngle = gradient->endAngle;
  clone->matrix = gradient->matrix;
  if (gradient->fitsToGeometry) {
    // fitsToGeometry: centre is already normalized 0..1; pass through unchanged so the
    // renderer evaluates it against the tile bounding box.
    clone->center = gradient->center;
  } else {
    // Absolute pixel space: shift origin from the layer coordinate system to the tile's
    // top-left corner so the gradient evaluates correctly within the PNG tile.
    clone->center = {gradient->center.x - left, gradient->center.y - top};
  }
  for (auto* srcStop : gradient->colorStops) {
    auto stop = doc->makeNode<ColorStop>();
    stop->offset = srcStop->offset;
    stop->color = srcStop->color;
    clone->colorStops.push_back(stop);
  }
  fill->color = clone;
  layer->contents = {rect, fill};

  return RenderTileToPng(doc.get(), w, h, pixelRatio, outputPath);
}

}  // namespace pagx
