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

#include <emscripten/bind.h>
#include "core/FontMetrics.h"
#include "core/ImageInfo.h"
#include "core/PathTypes.h"
#include "gpu/opengl/GLDefines.h"
#include "pag/pag.h"
#include "pag/types.h"
#include "platform/web/GPUDrawable.h"
#include "platform/web/NativeImage.h"
#include "rendering/editing/StillImage.h"

using namespace emscripten;
using namespace pag;

EMSCRIPTEN_BINDINGS(pag) {
  class_<PAGLayer>("_PAGLayer")
      .smart_ptr<std::shared_ptr<PAGLayer>>("_PAGLayer")
      .function("_uniqueID", optional_override([](PAGLayer& pagLayer) {
                  return static_cast<int>(pagLayer.uniqueID());
                }))
      .function("_layerType", optional_override([](PAGLayer& pagLayer) {
                  return static_cast<LayerType>(pagLayer.layerType());
                }))
      .function("_layerName", &PAGLayer::layerName)
      .function("_alpha", &PAGLayer::alpha)
      .function("_setAlpha", &PAGLayer::setAlpha)
      .function("_visible", &PAGLayer::visible)
      .function("_setVisible", &PAGLayer::setVisible)
      .function("_editableIndex", &PAGLayer::editableIndex)
      .function("_frameRate", &PAGLayer::frameRate)
      .function("_duration", optional_override([](PAGLayer& pagLayer) {
                  return static_cast<int>(pagLayer.duration());
                }))
      .function("_startTime", optional_override([](PAGLayer& pagLayer) {
                  return static_cast<int>(pagLayer.startTime());
                }))
      .function("_localTimeToGlobal", optional_override([](PAGLayer& pagLayer, int localTime) {
                  return static_cast<int>(pagLayer.localTimeToGlobal(localTime));
                }))
      .function("_globalToLocalTime", optional_override([](PAGLayer& pagLayer, int globalTime) {
                  return static_cast<int>(pagLayer.globalToLocalTime(globalTime));
                }));
  class_<PAGComposition, base<PAGLayer>>("_PAGComposition")
      .smart_ptr<std::shared_ptr<PAGComposition>>("_PAGComposition")
      .function("_width", &PAGComposition::width)
      .function("_height", &PAGComposition::height);
  class_<PAGFile, base<PAGComposition>>("_PAGFile")
      .smart_ptr<std::shared_ptr<PAGFile>>("_PAGFile")
      .class_function("_Load", optional_override([](uintptr_t bytes, size_t length) {
                        return PAGFile::Load(reinterpret_cast<void*>(bytes), length);
                      }))
      .function("_numImages", &PAGFile::numImages)
      .function("_numVideos", &PAGFile::numVideos)
      .function("_numTexts", &PAGFile::numTexts)
      .function("_getTextData", &PAGFile::getTextData)
      .function("_replaceText", &PAGFile::replaceText)
      .function("_replaceImage", &PAGFile::replaceImage)
      .function("_getLayersByEditableIndex",
                optional_override([](PAGFile& pagFile, int editableIndex, LayerType layerType) {
                  return pagFile.getLayersByEditableIndex(editableIndex, layerType);
                }))
      .function("_timeStretchMode", &PAGFile::timeStretchMode)
      .function("_setTimeStretchMode", &PAGFile::setTimeStretchMode)
      .function("_setDuration", optional_override([](PAGFile& pagFile, int duration) {
                  return pagFile.setDuration(static_cast<int64_t>(duration));
                }));

  class_<PAGSurface>("_PAGSurface")
      .smart_ptr<std::shared_ptr<PAGSurface>>("_PAGSurface")
      .class_function("_FromCanvas", optional_override([](const std::string& canvasID) {
                        return PAGSurface::MakeFrom(GPUDrawable::FromCanvasID(canvasID));
                      }))
      .class_function("_FromTexture",
                      optional_override([](int textureID, int width, int height, bool flipY) {
                        GLTextureInfo glInfo = {};
                        glInfo.target = GL::TEXTURE_2D;
                        glInfo.id = static_cast<unsigned>(textureID);
                        glInfo.format = GL::RGBA8;
                        BackendTexture glTexture(glInfo, width, height);
                        auto origin = flipY ? ImageOrigin::BottomLeft : ImageOrigin::TopLeft;
                        return PAGSurface::MakeFrom(glTexture, origin);
                      }))
      .class_function("_FromFrameBuffer",
                      optional_override([](int frameBufferID, int width, int height, bool flipY) {
                        GLFrameBufferInfo glFrameBufferInfo = {};
                        glFrameBufferInfo.id = static_cast<unsigned>(frameBufferID);
                        glFrameBufferInfo.format = GL::RGBA8;
                        BackendRenderTarget glRenderTarget(glFrameBufferInfo, width, height);
                        auto origin = flipY ? ImageOrigin::BottomLeft : ImageOrigin::TopLeft;
                        return PAGSurface::MakeFrom(glRenderTarget, origin);
                      }))
      .function("_width", &PAGSurface::width)
      .function("_height", &PAGSurface::height)
      .function("_updateSize", &PAGSurface::updateSize);

  class_<PAGImage>("_PAGImage")
      .smart_ptr<std::shared_ptr<PAGImage>>("_PAGImage")
      .class_function("_FromBytes", optional_override([](uintptr_t bytes, size_t length) {
                        return PAGImage::FromBytes(reinterpret_cast<void*>(bytes), length);
                      }))
      .class_function("_FromNativeImage", optional_override([](val nativeImage) {
                        return std::static_pointer_cast<PAGImage>(
                            StillImage::FromImage(NativeImage::MakeFrom(nativeImage)));
                      }))
      .function("_width", &PAGImage::width)
      .function("_height", &PAGImage::height);

  class_<PAGPlayer>("_PAGPlayer")
      .smart_ptr_constructor("_PAGPlayer", &std::make_shared<PAGPlayer>)
      .function("_setProgress", &PAGPlayer::setProgress)
      .function("_flush", &PAGPlayer::flush)
      .function("_duration", optional_override([](PAGPlayer& pagPlayer) {
                  return static_cast<int>(pagPlayer.duration());
                }))
      .function("_getProgress", &PAGPlayer::getProgress)
      .function("_videoEnabled", &PAGPlayer::videoEnabled)
      .function("_setVideoEnabled", &PAGPlayer::setVideoEnabled)
      .function("_cacheEnabled", &PAGPlayer::cacheEnabled)
      .function("_setCacheEnabled", &PAGPlayer::setCacheEnabled)
      .function("_cacheScale", &PAGPlayer::cacheScale)
      .function("_setCacheScale", &PAGPlayer::setCacheScale)
      .function("_maxFrameRate", &PAGPlayer::maxFrameRate)
      .function("_setMaxFrameRate", &PAGPlayer::setMaxFrameRate)
      .function("_scaleMode", &PAGPlayer::scaleMode)
      .function("_setScaleMode", &PAGPlayer::setScaleMode)
      .function("_setSurface", &PAGPlayer::setSurface)
      .function("_getComposition", optional_override([](PAGPlayer& pagPlayer) {
                  return std::static_pointer_cast<PAGFile>(pagPlayer.getComposition());
                }))
      .function("_setComposition",
                optional_override([](PAGPlayer& pagPlayer, std::shared_ptr<PAGFile>& pagFile) {
                  pagPlayer.setComposition(std::move(pagFile));
                }));

  class_<ImageInfo>("ImageInfo")
      .property("width", &ImageInfo::width)
      .property("height", &ImageInfo::height)
      .property("rowBytes", &ImageInfo::rowBytes)
      .property("colorType", &ImageInfo::colorType);

  class_<Matrix>("Matrix")
      .property("a", &Matrix::getScaleX)
      .property("b", &Matrix::getSkewY)
      .property("c", &Matrix::getSkewX)
      .property("d", &Matrix::getScaleY)
      .property("tx", &Matrix::getTranslateX)
      .property("ty", &Matrix::getTranslateY);

  class_<TextDocument>("TextDocument")
      .smart_ptr<std::shared_ptr<TextDocument>>("TextDocument")
      .property("applyFill", &TextDocument::applyFill)
      .property("applyStroke", &TextDocument::applyStroke)
      .property("baselineShift", &TextDocument::baselineShift)
      .property("boxText", &TextDocument::boxText)
      .property("boxTextPos", &TextDocument::boxTextPos)
      .property("boxTextSize", &TextDocument::boxTextSize)
      .property("firstBaseLine", &TextDocument::firstBaseLine)
      .property("fauxBold", &TextDocument::fauxBold)
      .property("fauxItalic", &TextDocument::fauxItalic)
      .property("fillColor", &TextDocument::fillColor)
      .property("fontFamily", &TextDocument::fontFamily)
      .property("fontStyle", &TextDocument::fontStyle)
      .property("fontSize", &TextDocument::fontSize)
      .property("strokeColor", &TextDocument::strokeColor)
      .property("strokeOverFill", &TextDocument::strokeOverFill)
      .property("strokeWidth", &TextDocument::strokeWidth)
      .property("text", &TextDocument::text)
      .property("justification", &TextDocument::justification)
      .property("leading", &TextDocument::leading)
      .property("tracking", &TextDocument::tracking)
      .property("backgroundColor", &TextDocument::backgroundColor)
      .property("backgroundAlpha", &TextDocument::backgroundAlpha)
      .property("direction", &TextDocument::direction);

  class_<Stroke>("Stroke")
      .property("width", &Stroke::width)
      .property("cap", &Stroke::cap)
      .property("join", &Stroke::join)
      .property("miterLimit", &Stroke::miterLimit);

  value_object<FontMetrics>("FontMetrics")
      .field("ascent", &FontMetrics::ascent)
      .field("descent", &FontMetrics::descent)
      .field("xHeight", &FontMetrics::xHeight)
      .field("capHeight", &FontMetrics::capHeight);

  value_object<Rect>("Rect")
      .field("left", &Rect::left)
      .field("top", &Rect::top)
      .field("right", &Rect::right)
      .field("bottom", &Rect::bottom);

  value_object<Point>("Point").field("x", &Point::x).field("y", &Point::y);

  value_object<Color>("Color")
      .field("red", &Color::red)
      .field("green", &Color::green)
      .field("blue", &Color::blue);

  enum_<PathFillType>("PathFillType")
      .value("WINDING", PathFillType::Winding)
      .value("EVEN_ODD", PathFillType::EvenOdd)
      .value("INVERSE_WINDING", PathFillType::InverseWinding)
      .value("INVERSE_EVEN_ODD", PathFillType::InverseEvenOdd);

  enum_<ColorType>("ColorType")
      .value("Unknown", ColorType::Unknown)
      .value("ALPHA_8", ColorType::ALPHA_8)
      .value("RGBA_8888", ColorType::RGBA_8888)
      .value("BGRA_8888", ColorType::BGRA_8888);

  enum_<LayerType>("LayerType")
      .value("Unknown", LayerType::Unknown)
      .value("Null", LayerType::Null)
      .value("Solid", LayerType::Solid)
      .value("Text", LayerType::Text)
      .value("Shape", LayerType::Shape)
      .value("Image", LayerType::Image)
      .value("PreCompose", LayerType::PreCompose);

  enum_<Stroke::Cap>("Cap")
      .value("Miter", Stroke::Cap::Butt)
      .value("Round", Stroke::Cap::Round)
      .value("Bevel", Stroke::Cap::Square);

  enum_<Stroke::Join>("Join")
      .value("Miter", Stroke::Join::Miter)
      .value("Round", Stroke::Join::Round)
      .value("Bevel", Stroke::Join::Bevel);

  register_vector<std::shared_ptr<PAGLayer>>("VectorPAGLayer");
  register_vector<std::string>("VectorString");
  register_vector<Point>("VectorPoint");

  function("_SetFallbackFontNames", optional_override([](std::vector<std::string> fontNames) {
             PAGFont::SetFallbackFontNames(fontNames);
           }));
}
