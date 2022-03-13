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
#include <emscripten/val.h>
#include "base/utils/TGFXCast.h"
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
                  return static_cast<int>(pagLayer.layerType());
                }))
      .function("_layerName", &PAGLayer::layerName)
      .function("_matrix",
                optional_override([](PAGLayer& pagLayer) { return ToTGFX(pagLayer.matrix()); }))
      .function("_setMatrix", optional_override([](PAGLayer& pagLayer, tgfx::Matrix matrix) {
                  pagLayer.setMatrix(ToPAG(matrix));
                }))
      .function("_resetMatrix", &PAGLayer::resetMatrix)
      .function("_getTotalMatrix", &PAGLayer::getTotalMatrix)
      .function("_alpha", &PAGLayer::alpha)
      .function("_setAlpha", &PAGLayer::setAlpha)
      .function("_visible", &PAGLayer::visible)
      .function("_setVisible", &PAGLayer::setVisible)
      .function("_editableIndex", &PAGLayer::editableIndex)
      .function("_parent", &PAGLayer::parent)
      .function("_markers", optional_override([](PAGLayer& pagLayer) {
                  std::vector<Marker> result = {};
                  for (auto marker_ptr : pagLayer.markers()) {
                    Marker marker;
                    marker.startTime = marker_ptr->startTime;
                    marker.duration = marker_ptr->duration;
                    marker.comment = marker_ptr->comment;
                    result.push_back(marker);
                  }
                  return result;
                }))
      .function("_globalToLocalTime", optional_override([](PAGLayer& pagLayer, int globalTime) {
                  return static_cast<int>(pagLayer.globalToLocalTime(globalTime));
                }))
      .function("_localTimeToGlobal", optional_override([](PAGLayer& pagLayer, int localTime) {
                  return static_cast<int>(pagLayer.localTimeToGlobal(localTime));
                }))
      .function("_duration", optional_override([](PAGLayer& pagLayer) {
                  return static_cast<int>(pagLayer.duration());
                }))
      .function("_frameRate", &PAGLayer::frameRate)
      .function("_startTime", optional_override([](PAGLayer& pagLayer) {
                  return static_cast<int>(pagLayer.startTime());
                }))
      .function("_startTime", optional_override([](PAGLayer& pagLayer) {
                  return static_cast<int>(pagLayer.startTime());
                }))
      .function("_setStartTime", optional_override([](PAGLayer& pagLayer, int time) {
                  return pagLayer.setStartTime(static_cast<int64_t>(time));
                }))
      .function("_currentTime", optional_override([](PAGLayer& pagLayer) {
                  return static_cast<int>(pagLayer.currentTime());
                }))
      .function("_setCurrentTime", optional_override([](PAGLayer& pagLayer, int time) {
                  return pagLayer.setCurrentTime(static_cast<int64_t>(time));
                }))
      .function("_getProgress", &PAGLayer::getProgress)
      .function("_setProgress", &PAGLayer::setProgress)
      .function("_preFrame", &PAGLayer::preFrame)
      .function("_nextFrame", &PAGLayer::nextFrame)
      .function("_getBounds",
                optional_override([](PAGLayer& pagLayer) { return ToTGFX(pagLayer.getBounds()); }))
      .function("_trackMatteLayer", &PAGLayer::trackMatteLayer)
      .function("_excludedFromTimeline", &PAGLayer::excludedFromTimeline)
      .function("_setExcludedFromTimeline", &PAGLayer::setExcludedFromTimeline)
      .function("_isPAGFile", &PAGLayer::isPAGFile);

  class_<PAGSolidLayer, base<PAGLayer>>("_PAGSolidLayer")
      .smart_ptr<std::shared_ptr<PAGSolidLayer>>("_PAGSolidLayer")
      .class_function("_Make", optional_override([](int duration, int width, int height,
                                                    Color solidColor, int opacity) {
                        return PAGSolidLayer::Make(static_cast<int64_t>(duration), width, height,
                                                   solidColor, opacity);
                      }))
      .function("_solidColor", &PAGSolidLayer::solidColor)
      .function("_setSolidColor", &PAGSolidLayer::setSolidColor);

  class_<PAGImageLayer, base<PAGLayer>>("_PAGImageLayer")
      .smart_ptr<std::shared_ptr<PAGImageLayer>>("_PAGImageLayer")
      .class_function("_Make", optional_override([](int width, int height, int duration) {
                        return PAGImageLayer::Make(width, height, static_cast<int64_t>(duration));
                      }))
      .function("_contentDuration", optional_override([](PAGImageLayer& pagImageLayer) {
                  return static_cast<int>(pagImageLayer.contentDuration());
                }))
      .function("_getVideoRanges", &PAGImageLayer::getVideoRanges)
      .function("_replaceImage", &PAGImageLayer::replaceImage)
      .function("_layerTimeToContent",
                optional_override([](PAGImageLayer& pagImageLayer, int layerTime) {
                  return static_cast<int>(pagImageLayer.layerTimeToContent(layerTime));
                }))
      .function("_contentTimeToLayer",
                optional_override([](PAGImageLayer& pagImageLayer, int contentTime) {
                  return static_cast<int>(pagImageLayer.contentTimeToLayer(contentTime));
                }));

  class_<PAGTextLayer, base<PAGLayer>>("_PAGTextLayer")
      .smart_ptr<std::shared_ptr<PAGTextLayer>>("_PAGTextLayer")
      .class_function("_Make", optional_override([](int duration, std::string text, float fontSize,
                                                    std::string fontFamily, std::string fontStyle) {
                        return PAGTextLayer::Make(static_cast<int64_t>(duration), text, fontSize,
                                                  fontFamily, fontStyle);
                      }))
      .class_function(
          "_Make",
          optional_override([](int duration, std::shared_ptr<TextDocument> textDocumentHandle) {
            return PAGTextLayer::Make(static_cast<int64_t>(duration), textDocumentHandle);
          }))
      .function("_fillColor", &PAGTextLayer::fillColor)
      .function("_setFillColor", &PAGTextLayer::setFillColor)
      .function("_font", &PAGTextLayer::font)
      .function("_setFont", &PAGTextLayer::setFont)
      .function("_fontSize", &PAGTextLayer::fontSize)
      .function("_setFontSize", &PAGTextLayer::setFontSize)
      .function("_strokeColor", &PAGTextLayer::strokeColor)
      .function("_setStrokeColor", &PAGTextLayer::setStrokeColor)
      .function("_text", &PAGTextLayer::text)
      .function("_setText", &PAGTextLayer::setText)
      .function("_reset", &PAGTextLayer::reset);

  class_<PAGComposition, base<PAGLayer>>("_PAGComposition")
      .smart_ptr<std::shared_ptr<PAGComposition>>("_PAGComposition")
      .class_function("_Make", PAGComposition::Make)
      .function("_width", &PAGComposition::width)
      .function("_height", &PAGComposition::height)
      .function("_setContentSize", &PAGComposition::setContentSize)
      .function("_numChildren", &PAGComposition::numChildren)
      .function("_getLayerAt", &PAGComposition::getLayerAt)
      .function("_getLayerIndex", &PAGComposition::getLayerIndex)
      .function("_setLayerIndex", &PAGComposition::setLayerIndex)
      .function("_addLayer", &PAGComposition::addLayer)
      .function("_addLayerAt", &PAGComposition::addLayerAt)
      .function("_contains", &PAGComposition::contains)
      .function("_removeLayer", &PAGComposition::removeLayer)
      .function("_removeLayerAt", &PAGComposition::removeLayerAt)
      .function("_removeAllLayers", &PAGComposition::removeAllLayers)
      .function("_swapLayer", &PAGComposition::swapLayer)
      .function("_swapLayerAt", &PAGComposition::swapLayerAt)
      .function("_audioBytes", optional_override([](PAGComposition& pagComposition) {
                  ByteData* result = pagComposition.audioBytes();
                  if (result->length() == 0) {
                    uint8_t empty_arr[] = {};
                    return val(typed_memory_view(0, empty_arr));
                  }
                  return val(typed_memory_view(result->length(), result->data()));
                }))
      .function("_audioMarkers", optional_override([](PAGComposition& pagComposition) {
                  std::vector<Marker> result = {};
                  for (auto marker_ptr : pagComposition.audioMarkers()) {
                    Marker marker;
                    marker.startTime = marker_ptr->startTime;
                    marker.duration = marker_ptr->duration;
                    marker.comment = marker_ptr->comment;
                    result.push_back(marker);
                  }
                  return result;
                }))
      .function("_audioStartTime", optional_override([](PAGComposition& pagComposition) {
                  return static_cast<int>(pagComposition.audioStartTime());
                }))
      .function("_getLayersByName", &PAGComposition::getLayersByName)
      .function("_getLayersUnderPoint", &PAGComposition::getLayersUnderPoint);

  class_<PAGFile, base<PAGComposition>>("_PAGFile")
      .smart_ptr<std::shared_ptr<PAGFile>>("_PAGFile")
      .class_function("_MaxSupportedTagLevel", PAGFile::MaxSupportedTagLevel)
      .class_function("_Load", optional_override([](uintptr_t bytes, size_t length) {
                        return PAGFile::Load(reinterpret_cast<void*>(bytes), length);
                      }))
      .function("_tagLevel", &PAGFile::tagLevel)
      .function("_numTexts", &PAGFile::numTexts)
      .function("_numImages", &PAGFile::numImages)
      .function("_numVideos", &PAGFile::numVideos)
      .function("_getTextData", &PAGFile::getTextData)
      .function("_replaceText", &PAGFile::replaceText)
      .function("_replaceImage", &PAGFile::replaceImage)
      .function("_getLayersByEditableIndex",
                optional_override([](PAGFile& pagFile, int editableIndex, int layerType) {
                  return pagFile.getLayersByEditableIndex(editableIndex,
                                                          static_cast<LayerType>(layerType));
                }))
      .function("_timeStretchMode", &PAGFile::timeStretchMode)
      .function("_setTimeStretchMode", &PAGFile::setTimeStretchMode)
      .function("_setDuration", optional_override([](PAGFile& pagFile, int duration) {
                  return pagFile.setDuration(static_cast<int64_t>(duration));
                }))
      .function("_copyOriginal", &PAGFile::copyOriginal);

  class_<PAGSurface>("_PAGSurface")
      .smart_ptr<std::shared_ptr<PAGSurface>>("_PAGSurface")
      .class_function("_FromCanvas", optional_override([](const std::string& canvasID) {
                        return PAGSurface::MakeFrom(GPUDrawable::FromCanvasID(canvasID));
                      }))
      .class_function("_FromTexture",
                      optional_override([](int textureID, int width, int height, bool flipY) {
                        GLTextureInfo glInfo = {};
                        glInfo.target = GL_TEXTURE_2D;
                        glInfo.id = static_cast<unsigned>(textureID);
                        glInfo.format = GL_RGBA8;
                        BackendTexture glTexture(glInfo, width, height);
                        auto origin = flipY ? ImageOrigin::BottomLeft : ImageOrigin::TopLeft;
                        return PAGSurface::MakeFrom(glTexture, origin);
                      }))
      .class_function("_FromFrameBuffer",
                      optional_override([](int frameBufferID, int width, int height, bool flipY) {
                        GLFrameBufferInfo glFrameBufferInfo = {};
                        glFrameBufferInfo.id = static_cast<unsigned>(frameBufferID);
                        glFrameBufferInfo.format = GL_RGBA8;
                        BackendRenderTarget glRenderTarget(glFrameBufferInfo, width, height);
                        auto origin = flipY ? ImageOrigin::BottomLeft : ImageOrigin::TopLeft;
                        return PAGSurface::MakeFrom(glRenderTarget, origin);
                      }))
      .function("_width", &PAGSurface::width)
      .function("_height", &PAGSurface::height)
      .function("_updateSize", &PAGSurface::updateSize)
      .function("_clearAll", &PAGSurface::clearAll)
      .function("_freeCache", &PAGSurface::freeCache);

  class_<PAGImage>("_PAGImage")
      .smart_ptr<std::shared_ptr<PAGImage>>("_PAGImage")
      .class_function("_FromBytes", optional_override([](uintptr_t bytes, size_t length) {
                        return PAGImage::FromBytes(reinterpret_cast<void*>(bytes), length);
                      }))
      .class_function("_FromNativeImage", optional_override([](val nativeImage) {
                        return std::static_pointer_cast<PAGImage>(
                            StillImage::FromImage(tgfx::NativeImage::MakeFrom(nativeImage)));
                      }))
      .function("_width", &PAGImage::width)
      .function("_height", &PAGImage::height)
      .function("_scaleMode", &PAGImage::scaleMode)
      .function("_setScaleMode", &PAGImage::setScaleMode)
      .function("_matrix",
                optional_override([](PAGImage& pagImage) { return ToTGFX(pagImage.matrix()); }))
      .function("_setMatrix", optional_override([](PAGImage& pagImage, tgfx::Matrix matrix) {
                  pagImage.setMatrix(ToPAG(matrix));
                }));

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
      .function("_getComposition", &PAGPlayer::getComposition)
      .function("_setComposition", &PAGPlayer::setComposition)
      .function("_getSurface", &PAGPlayer::getSurface)
      .function("_matrix",
                optional_override([](PAGPlayer& pagPlayer) { return ToTGFX(pagPlayer.matrix()); }))
      .function("_setMatrix", optional_override([](PAGPlayer& pagPlayer, tgfx::Matrix matrix) {
                  pagPlayer.setMatrix(ToPAG(matrix));
                }))
      .function("_nextFrame", &PAGPlayer::nextFrame)
      .function("_preFrame", &PAGPlayer::preFrame)
      .function("_autoClear", &PAGPlayer::autoClear)
      .function("_setAutoClear", &PAGPlayer::setAutoClear)
      .function("_getBounds",
                optional_override([](PAGPlayer& pagPlayer, std::shared_ptr<PAGLayer> pagLayer) {
                  return ToTGFX(pagPlayer.getBounds(pagLayer));
                }))
      .function("_getLayersUnderPoint", &PAGPlayer::getLayersUnderPoint)
      .function("_hitTestPoint", &PAGPlayer::hitTestPoint)
      .function("_renderingTime", optional_override([](PAGPlayer& pagPlayer) {
                  return static_cast<int>(pagPlayer.renderingTime());
                }))
      .function("_imageDecodingTime", optional_override([](PAGPlayer& pagPlayer) {
                  return static_cast<int>(pagPlayer.imageDecodingTime());
                }))
      .function("_presentingTime", optional_override([](PAGPlayer& pagPlayer) {
                  return static_cast<int>(pagPlayer.presentingTime());
                }))
      .function("_graphicsMemory", optional_override([](PAGPlayer& pagPlayer) {
                  return static_cast<int>(pagPlayer.graphicsMemory());
                }));

  class_<PAGFont>("_PAGFont")
      .smart_ptr<std::shared_ptr<PAGFont>>("_PAGFont")
      .class_function("_create",
                      optional_override([](std::string fontFamily, std::string fontStyle) {
                        return pag::PAGFont(fontFamily, fontStyle);
                      }))
      .class_function("_SetFallbackFontNames", PAGFont::SetFallbackFontNames)
      .property("fontFamily", &PAGFont::fontFamily)
      .property("fontStyle", &PAGFont::fontStyle);

  class_<tgfx::ImageInfo>("ImageInfo")
      .property("width", &tgfx::ImageInfo::width)
      .property("height", &tgfx::ImageInfo::height)
      .property("rowBytes", &tgfx::ImageInfo::rowBytes)
      .property("colorType", &tgfx::ImageInfo::colorType);

  class_<tgfx::Matrix>("Matrix")
      .property("a", &tgfx::Matrix::getScaleX)
      .property("b", &tgfx::Matrix::getSkewY)
      .property("c", &tgfx::Matrix::getSkewX)
      .property("d", &tgfx::Matrix::getScaleY)
      .property("tx", &tgfx::Matrix::getTranslateX)
      .property("ty", &tgfx::Matrix::getTranslateY)
      .function("set", &tgfx::Matrix::set)
      .function("setAffine", &tgfx::Matrix::setAffine);

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

  class_<tgfx::Stroke>("Stroke")
      .property("width", &tgfx::Stroke::width)
      .property("cap", &tgfx::Stroke::cap)
      .property("join", &tgfx::Stroke::join)
      .property("miterLimit", &tgfx::Stroke::miterLimit);

  class_<PAGVideoRange>("PAGVideoRange")
      .function("startTime", optional_override([](PAGVideoRange& pagVideoRange) {
                  return static_cast<int>(pagVideoRange.startTime());
                }))
      .function("endTime", optional_override([](PAGVideoRange& pagVideoRange) {
                  return static_cast<int>(pagVideoRange.endTime());
                }))
      .function("playDuration", optional_override([](PAGVideoRange& pagVideoRange) {
                  return static_cast<int>(pagVideoRange.playDuration());
                }))
      .function("reversed", &PAGVideoRange::reversed);

  value_object<tgfx::FontMetrics>("FontMetrics")
      .field("ascent", &tgfx::FontMetrics::ascent)
      .field("descent", &tgfx::FontMetrics::descent)
      .field("xHeight", &tgfx::FontMetrics::xHeight)
      .field("capHeight", &tgfx::FontMetrics::capHeight);

  value_object<tgfx::Rect>("Rect")
      .field("left", &tgfx::Rect::left)
      .field("top", &tgfx::Rect::top)
      .field("right", &tgfx::Rect::right)
      .field("bottom", &tgfx::Rect::bottom);

  value_object<tgfx::Point>("Point").field("x", &tgfx::Point::x).field("y", &tgfx::Point::y);

  value_object<Color>("Color")
      .field("red", &Color::red)
      .field("green", &Color::green)
      .field("blue", &Color::blue);

  value_object<Marker>("Marker")
      .field("startTime", optional_override([](const Marker& marker) {
               return static_cast<int>(marker.startTime);
             }),
             optional_override(
                 [](Marker& marker, int value) { marker.startTime = static_cast<int64_t>(value); }))
      .field("duration", optional_override([](const Marker& marker) {
               return static_cast<int>(marker.duration);
             }),
             optional_override(
                 [](Marker& marker, int value) { marker.duration = static_cast<int64_t>(value); }))
      .field("comment", &Marker::comment);

  enum_<tgfx::PathFillType>("PathFillType")
      .value("Winding", tgfx::PathFillType::Winding)
      .value("EvenOdd", tgfx::PathFillType::EvenOdd)
      .value("InverseWinding", tgfx::PathFillType::InverseWinding)
      .value("InverseEvenOdd", tgfx::PathFillType::InverseEvenOdd);

  enum_<ColorType>("ColorType")
      .value("Unknown", ColorType::Unknown)
      .value("ALPHA_8", ColorType::ALPHA_8)
      .value("RGBA_8888", ColorType::RGBA_8888)
      .value("BGRA_8888", ColorType::BGRA_8888);

  enum_<tgfx::LineCap>("Cap")
      .value("Miter", tgfx::LineCap::Butt)
      .value("Round", tgfx::LineCap::Round)
      .value("Bevel", tgfx::LineCap::Square);

  enum_<tgfx::LineJoin>("Join")
      .value("Miter", tgfx::LineJoin::Miter)
      .value("Round", tgfx::LineJoin::Round)
      .value("Bevel", tgfx::LineJoin::Bevel);

  register_vector<std::shared_ptr<PAGLayer>>("VectorPAGLayer");
  register_vector<std::string>("VectorString");
  register_vector<tgfx::Point>("VectorPoint");
  register_vector<Marker>("VectorMarker");
  register_vector<PAGVideoRange>("VectorPAGVideoRange");
}
