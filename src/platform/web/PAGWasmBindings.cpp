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

#include <emscripten/bind.h>
#include <emscripten/val.h>
#include "VideoInfoManager.h"
#include "base/utils/TGFXCast.h"
#include "pag/pag.h"
#include "pag/types.h"
#include "platform/web/GPUDrawable.h"
#include "platform/web/WebSoftwareDecoderFactory.h"
#include "rendering/editing/StillImage.h"
#include "tgfx/core/ImageInfo.h"
#include "tgfx/core/PathTypes.h"
#include "tgfx/gpu/opengl/GLDefines.h"

using namespace emscripten;

namespace pag {

std::unique_ptr<ByteData> CopyDataFromUint8Array(const val& emscriptenData) {
  if (!emscriptenData.as<bool>()) {
    return nullptr;
  }
  auto length = emscriptenData["length"].as<size_t>();
  if (length == 0) {
    return nullptr;
  }
  auto buffer = ByteData::Make(length);
  if (!buffer) {
    return nullptr;
  }
  auto memory = val::module_property("HEAPU8")["buffer"];
  auto memoryView =
      val::global("Uint8Array").new_(memory, reinterpret_cast<uintptr_t>(buffer->data()), length);
  memoryView.call<void>("set", emscriptenData);
  return buffer;
}

emscripten::val PAGLayerToJsObject(std::shared_ptr<PAGLayer> pagLayer) {
  emscripten::val layerObject = emscripten::val::null();
  if (!pagLayer) {
    return layerObject;
  }

  switch (pagLayer->layerType()) {
    case LayerType::Solid: {
      layerObject = emscripten::val(std::static_pointer_cast<PAGSolidLayer>(pagLayer));
      break;
    }
    case LayerType::Text: {
      layerObject = emscripten::val(std::static_pointer_cast<PAGTextLayer>(pagLayer));
      break;
    }
    case LayerType::Shape: {
      layerObject = emscripten::val(std::static_pointer_cast<PAGShapeLayer>(pagLayer));
      break;
    }
    case LayerType::Image: {
      layerObject = emscripten::val(std::static_pointer_cast<PAGImageLayer>(pagLayer));
      break;
    }
    case LayerType::PreCompose: {
      if (pagLayer->isPAGFile()) {
        layerObject = emscripten::val(std::static_pointer_cast<PAGFile>(pagLayer));
      } else {
        layerObject = emscripten::val(std::static_pointer_cast<PAGComposition>(pagLayer));
      }
      break;
    }
    default: {
      layerObject = emscripten::val(pagLayer);
      break;
    }
  }

  return layerObject;
}

bool PAGBindInit() {
  class_<PAGLayer>("_PAGLayer")
      .smart_ptr<std::shared_ptr<PAGLayer>>("_PAGLayer")
      .function("_uniqueID", optional_override([](PAGLayer& pagLayer) {
                  return static_cast<int>(pagLayer.uniqueID());
                }))
      .function("_layerType", optional_override([](PAGLayer& pagLayer) {
                  return static_cast<int>(pagLayer.layerType());
                }))
      .function("_layerName", &PAGLayer::layerName)
      .function("_matrix", &PAGLayer::matrix)
      .function("_setMatrix", &PAGLayer::setMatrix)
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
      .function("_getBounds", &PAGLayer::getBounds)
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
      .function("_getVideoRanges", optional_override([](PAGImageLayer& pagImageLayer) {
                  auto res = val::array();
                  for (auto videoRange : pagImageLayer.getVideoRanges()) {
                    auto videoRangeVal = val::object();
                    videoRangeVal.set("startTime", static_cast<float>(videoRange.startTime()));
                    videoRangeVal.set("endTime", static_cast<float>(videoRange.endTime()));
                    videoRangeVal.set("playDuration",
                                      static_cast<float>(videoRange.playDuration()));
                    videoRangeVal.set("reversed", videoRange.reversed());
                    res.call<int>("push", videoRangeVal);
                  }
                  return res;
                }))
      .function("_replaceImage", &PAGImageLayer::replaceImage)
      .function("_setImage", &PAGImageLayer::setImage)
      .function("_layerTimeToContent",
                optional_override([](PAGImageLayer& pagImageLayer, int layerTime) {
                  return static_cast<int>(pagImageLayer.layerTimeToContent(layerTime));
                }))
      .function("_contentTimeToLayer",
                optional_override([](PAGImageLayer& pagImageLayer, int contentTime) {
                  return static_cast<int>(pagImageLayer.contentTimeToLayer(contentTime));
                }))
      .function("_imageBytes", optional_override([](PAGImageLayer& pagImageLayer) {
                  ByteData* result = pagImageLayer.imageBytes();
                  if (!result || result->length() == 0) {
                    return val::null();
                  }
                  return val(typed_memory_view(result->length(), result->data()));
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
      .function("_getLayerAt",
                optional_override([](PAGComposition& pagComposition, int index) -> emscripten::val {
                  return PAGLayerToJsObject(pagComposition.getLayerAt(index));
                }))
      .function("_getLayerIndex", &PAGComposition::getLayerIndex)
      .function("_setLayerIndex", &PAGComposition::setLayerIndex)
      .function("_addLayer", &PAGComposition::addLayer)
      .function("_addLayerAt", &PAGComposition::addLayerAt)
      .function("_contains", &PAGComposition::contains)
      .function("_removeLayer",
                optional_override([](PAGComposition& pagComposition,
                                     std::shared_ptr<PAGLayer> pagLayer) -> emscripten::val {
                  return PAGLayerToJsObject(pagComposition.removeLayer(pagLayer));
                }))
      .function("_removeLayerAt",
                optional_override([](PAGComposition& pagComposition, int index) -> emscripten::val {
                  return PAGLayerToJsObject(pagComposition.removeLayerAt(index));
                }))
      .function("_removeAllLayers", &PAGComposition::removeAllLayers)
      .function("_swapLayer", &PAGComposition::swapLayer)
      .function("_swapLayerAt", &PAGComposition::swapLayerAt)
      .function("_audioBytes", optional_override([](PAGComposition& pagComposition) {
                  ByteData* result = pagComposition.audioBytes();
                  if (!result || result->length() == 0) {
                    return val::null();
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
      .function("_getLayersByName",
                optional_override([](PAGComposition& pagComposition, const std::string& layerName) {
                  auto layerVector = pagComposition.getLayersByName(layerName);
                  int size = layerVector.size();
                  emscripten::val jsArray = emscripten::val::array();
                  for (int i = 0; i < size; ++i) {
                    jsArray.call<void>("push", PAGLayerToJsObject(layerVector[i]));
                  }
                  return jsArray;
                }))
      .function("_getLayersUnderPoint",
                optional_override([](PAGComposition& pagComposition, float localX, float localY) {
                  auto layerVector = pagComposition.getLayersUnderPoint(localX, localY);
                  int size = layerVector.size();
                  emscripten::val jsArray = emscripten::val::array();
                  for (int i = 0; i < size; ++i) {
                    jsArray.call<void>("push", PAGLayerToJsObject(layerVector[i]));
                  }
                  return jsArray;
                }));

  class_<PAGFile, base<PAGComposition>>("_PAGFile")
      .smart_ptr<std::shared_ptr<PAGFile>>("_PAGFile")
      .class_function("_MaxSupportedTagLevel", PAGFile::MaxSupportedTagLevel)
      .class_function("_Load",
                      optional_override([](const val& emscriptenData) -> std::shared_ptr<PAGFile> {
                        auto data = CopyDataFromUint8Array(emscriptenData);
                        if (data == nullptr) {
                          return nullptr;
                        }
                        return PAGFile::Load(data->data(), data->length());
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
                  auto layerVector = pagFile.getLayersByEditableIndex(
                      editableIndex, static_cast<LayerType>(layerType));
                  int size = layerVector.size();
                  emscripten::val jsArray = emscripten::val::array();
                  for (int i = 0; i < size; ++i) {
                    jsArray.call<void>("push", PAGLayerToJsObject(layerVector[i]));
                  }
                  return jsArray;
                }))
      .function(
          "_getEditableIndices", optional_override([](PAGFile& pagFile, int layerType) {
            auto res = val::array();
            for (auto indices : pagFile.getEditableIndices(static_cast<LayerType>(layerType))) {
              res.call<int>("push", indices);
            }
            return res;
          }))
      .function("_timeStretchMode", optional_override([](PAGFile& pagFile) {
                  return static_cast<int>(pagFile.timeStretchMode());
                }))
      .function("_setTimeStretchMode", optional_override([](PAGFile& pagfile, int timeStretchMode) {
                  pagfile.setTimeStretchMode(static_cast<PAGTimeStretchMode>(timeStretchMode));
                }))
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
      .class_function("_FromRenderTarget",
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
      .function("_freeCache", &PAGSurface::freeCache)
      .function("_readPixels", optional_override([](PAGSurface& pagSurface, int colorType,
                                                    int alphaType, size_t dstRowBytes) -> val {
                  auto dataSize = dstRowBytes * pagSurface.height();
                  if (dataSize == 0) {
                    return val::null();
                  }
                  std::unique_ptr<uint8_t[]> uint8Array(new (std::nothrow) uint8_t[dataSize]);
                  if (uint8Array && pagSurface.readPixels(static_cast<ColorType>(colorType),
                                                          static_cast<AlphaType>(alphaType),
                                                          uint8Array.get(), dstRowBytes)) {
                    auto memory = val::module_property("HEAPU8")["buffer"];
                    auto memoryView =
                        val::global("Uint8Array")
                            .new_(memory, reinterpret_cast<uintptr_t>(uint8Array.get()), dataSize);
                    auto newArrayBuffer = val::global("ArrayBuffer").new_(dataSize);
                    auto newUint8Array = val::global("Uint8Array").new_(newArrayBuffer);
                    newUint8Array.call<void>("set", memoryView);
                    return newUint8Array;
                  }
                  return val::null();
                }));

  class_<PAGImage>("_PAGImage")
      .smart_ptr<std::shared_ptr<PAGImage>>("_PAGImage")
      .class_function("_FromBytes",
                      optional_override([](const val& emscriptenData) -> std::shared_ptr<PAGImage> {
                        auto data = CopyDataFromUint8Array(emscriptenData);
                        if (data == nullptr) {
                          return nullptr;
                        }
                        return PAGImage::FromBytes(reinterpret_cast<void*>(data->data()),
                                                   data->length());
                      }))
      .class_function("_FromNativeImage", optional_override([](val nativeImage) {
                        auto image = tgfx::Image::MakeFrom(nativeImage);
                        return std::static_pointer_cast<PAGImage>(StillImage::MakeFrom(image));
                      }))
      .class_function(
          "_FromPixels",
          optional_override([](const val& pixels, int width, int height, size_t rowBytes,
                               int colorType, int alphaType) -> std::shared_ptr<PAGImage> {
            auto data = CopyDataFromUint8Array(pixels);
            if (data == nullptr) {
              return nullptr;
            }
            return PAGImage::FromPixels(reinterpret_cast<void*>(data->data()), width, height,
                                        rowBytes, static_cast<ColorType>(colorType),
                                        static_cast<AlphaType>(alphaType));
          }))
      .class_function("_FromTexture",
                      optional_override([](int textureID, int width, int height, bool flipY) {
                        GLTextureInfo glInfo = {};
                        glInfo.target = GL_TEXTURE_2D;
                        glInfo.id = static_cast<unsigned>(textureID);
                        glInfo.format = GL_RGBA8;
                        BackendTexture glTexture(glInfo, width, height);
                        auto origin = flipY ? ImageOrigin::BottomLeft : ImageOrigin::TopLeft;
                        return PAGImage::FromTexture(glTexture, origin);
                      }))
      .function("_width", &PAGImage::width)
      .function("_height", &PAGImage::height)
      .function("_scaleMode", optional_override([](PAGImage& pagImage) {
                  return static_cast<int>(pagImage.scaleMode());
                }))
      .function("_setScaleMode", optional_override([](PAGImage& pagImage, int scaleMode) {
                  pagImage.setScaleMode(static_cast<PAGScaleMode>(scaleMode));
                }))
      .function("_matrix", &PAGImage::matrix)
      .function("_setMatrix", &PAGImage::setMatrix);

  class_<PAGPlayer>("_PAGPlayer")
      .smart_ptr_constructor("_PAGPlayer", &std::make_shared<PAGPlayer>)
      .function("_setProgress", &PAGPlayer::setProgress)
      .function("_flush", &PAGPlayer::flush)
      .function("_duration", optional_override([](PAGPlayer& pagPlayer) {
                  return static_cast<int>(pagPlayer.duration());
                }))
      .function("_getProgress", &PAGPlayer::getProgress)
      .function("_currentFrame", optional_override([](PAGPlayer& pagPlayer) {
                  return static_cast<int>(pagPlayer.currentFrame());
                }))
      .function("_videoEnabled", &PAGPlayer::videoEnabled)
      .function("_setVideoEnabled", &PAGPlayer::setVideoEnabled)
      .function("_cacheEnabled", &PAGPlayer::cacheEnabled)
      .function("_setCacheEnabled", &PAGPlayer::setCacheEnabled)
      .function("_cacheScale", &PAGPlayer::cacheScale)
      .function("_setCacheScale", &PAGPlayer::setCacheScale)
      .function("_maxFrameRate", &PAGPlayer::maxFrameRate)
      .function("_setMaxFrameRate", &PAGPlayer::setMaxFrameRate)
      .function("_scaleMode", optional_override([](PAGPlayer& pagPlayer) {
                  return static_cast<int>(pagPlayer.scaleMode());
                }))
      .function("_setScaleMode", optional_override([](PAGPlayer& pagPlayer, int scaleMode) {
                  pagPlayer.setScaleMode(static_cast<PAGScaleMode>(scaleMode));
                }))
      .function("_setSurface", &PAGPlayer::setSurface)
      .function("_getComposition", optional_override([](PAGPlayer& pagPlayer) -> emscripten::val {
                  auto composition = pagPlayer.getComposition();
                  return PAGLayerToJsObject(composition);
                }))
      .function("_setComposition", &PAGPlayer::setComposition)
      .function("_getSurface", &PAGPlayer::getSurface)
      .function("_matrix", &PAGPlayer::matrix)
      .function("_setMatrix", &PAGPlayer::setMatrix)
      .function("_nextFrame", &PAGPlayer::nextFrame)
      .function("_preFrame", &PAGPlayer::preFrame)
      .function("_autoClear", &PAGPlayer::autoClear)
      .function("_setAutoClear", &PAGPlayer::setAutoClear)
      .function("_getBounds", &PAGPlayer::getBounds)
      .function("_getLayersUnderPoint",
                optional_override([](PAGPlayer& pagPlayer, float surfaceX, float surfaceY) {
                  auto layerVector = pagPlayer.getLayersUnderPoint(surfaceX, surfaceY);
                  int size = layerVector.size();
                  emscripten::val jsArray = emscripten::val::array();
                  for (int i = 0; i < size; ++i) {
                    jsArray.call<void>("push", PAGLayerToJsObject(layerVector[i]));
                  }
                  return jsArray;
                }))
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
                }))
      .function("_prepare", &PAGPlayer::prepare);

  class_<PAGFont>("_PAGFont")
      .smart_ptr<std::shared_ptr<PAGFont>>("_PAGFont")
      .class_function("_create",
                      optional_override([](std::string fontFamily, std::string fontStyle) {
                        return pag::PAGFont(fontFamily, fontStyle);
                      }))
      .class_function("_SetFallbackFontNames", PAGFont::SetFallbackFontNames)
      .property("fontFamily", &PAGFont::fontFamily)
      .property("fontStyle", &PAGFont::fontStyle);

  class_<Matrix>("_Matrix")
      .class_function("_MakeAll", Matrix::MakeAll)
      .class_function("_MakeScale", optional_override([](float sx, float sy) {
                        return Matrix::MakeScale(sx, sy);
                      }))
      .class_function("_MakeScale",
                      optional_override([](float scale) { return Matrix::MakeScale(scale); }))
      .class_function("_MakeTrans", Matrix::MakeTrans)
      .function("_get", &Matrix::get)
      .function("_set", &Matrix::set)
      .function("_setAll", &Matrix::setAll)
      .function("_setAffine", &Matrix::setAffine)
      .function("_reset", &Matrix::reset)
      .function("_setTranslate", &Matrix::setTranslate)
      .function("_setScale",
                optional_override([](Matrix& matrix, float sx, float sy, float px, float py) {
                  return matrix.setScale(sx, sy, px, py);
                }))
      .function("_setRotate",
                optional_override([](Matrix& matrix, float degrees, float px, float py) {
                  return matrix.setRotate(degrees, px, py);
                }))
      .function("_setSinCos",
                optional_override([](Matrix& matrix, float sinV, float cosV, float px, float py) {
                  return matrix.setSinCos(sinV, cosV, px, py);
                }))
      .function("_setSkew",
                optional_override([](Matrix& matrix, float kx, float ky, float px, float py) {
                  return matrix.setSkew(kx, ky, px, py);
                }))
      .function("_setConcat", &Matrix::setConcat)
      .function("_preTranslate", &Matrix::preTranslate)
      .function("_preScale",
                optional_override([](Matrix& matrix, float sx, float sy, float px, float py) {
                  return matrix.preScale(sx, sy, px, py);
                }))
      .function("_preRotate",
                optional_override([](Matrix& matrix, float degrees, float px, float py) {
                  return matrix.preRotate(degrees, px, py);
                }))
      .function("_preSkew",
                optional_override([](Matrix& matrix, float kx, float ky, float px, float py) {
                  return matrix.preSkew(kx, ky, px, py);
                }))
      .function("_preConcat", &Matrix::preConcat)
      .function("_postTranslate", &Matrix::postTranslate)
      .function("_postScale",
                optional_override([](Matrix& matrix, float sx, float sy, float px, float py) {
                  return matrix.postScale(sx, sy, px, py);
                }))
      .function("_postRotate",
                optional_override([](Matrix& matrix, float degrees, float px, float py) {
                  return matrix.postRotate(degrees, px, py);
                }))
      .function("_postSkew",
                optional_override([](Matrix& matrix, float kx, float ky, float px, float py) {
                  return matrix.postSkew(kx, ky, px, py);
                }))
      .function("_postConcat", &Matrix::postConcat);

  class_<VideoInfoManager>("_videoInfoManager")
      .smart_ptr<std::shared_ptr<VideoInfoManager>>("_videoInfoManager")
      .class_function("_Make",
                      optional_override([](std::shared_ptr<PAGComposition> pagComposition) {
                        return std::make_shared<VideoInfoManager>(pagComposition);
                      }))
      .class_function("_HasVideo",
                      optional_override([](std::shared_ptr<PAGComposition> pagComposition) {
                        return VideoInfoManager::HasVideo(pagComposition);
                      }))
      .function("_getMp4DataByID", &VideoInfoManager::getMp4DataByID)
      .function("_getWidthByID", &VideoInfoManager::getWidthByID)
      .function("_getHeightByID", &VideoInfoManager::getHeightByID)
      .function("_getFrameRateByID", &VideoInfoManager::getFrameRateByID)
      .function("_getStaticTimeRangesByID", &VideoInfoManager::getStaticTimeRangesByID)
      .function("_getVideoIDs", &VideoInfoManager::getVideoIDs)
      .function("_getTargetFrameByID", &VideoInfoManager::getTargetFrameByID)
      .function("_getPlaybackRateByID", &VideoInfoManager::getPlaybackRateByID)
      .function("_hasTimeRangeOverlap", &VideoInfoManager::hasTimeRangeOverlap);
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
      .property("justification", optional_override([](const TextDocument& textDocument) {
                  return static_cast<uint8_t>(textDocument.justification);
                }),
                optional_override([](TextDocument& textDocument, uint8_t val) {
                  textDocument.justification = static_cast<ParagraphJustification>(val);
                }))
      .property("leading", &TextDocument::leading)
      .property("tracking", &TextDocument::tracking)
      .property("backgroundColor", &TextDocument::backgroundColor)
      .property("backgroundAlpha", &TextDocument::backgroundAlpha)
      .property("direction", optional_override([](const TextDocument& textDocument) {
                  return static_cast<uint8_t>(textDocument.direction);
                }),
                optional_override([](TextDocument& textDocument, uint8_t val) {
                  textDocument.direction = static_cast<TextDirection>(val);
                }));

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

  function("_registerSoftwareDecoderFactory", optional_override([](val factory) {
             static std::unique_ptr<SoftwareDecoderFactory> softwareDecoderFactory = nullptr;
             auto webFactory = WebSoftwareDecoderFactory::Make(factory);
             PAGVideoDecoder::RegisterSoftwareDecoderFactory(webFactory.get());
             softwareDecoderFactory = std::move(webFactory);
           }));

  function("_SDKVersion", &PAG::SDKVersion);

  register_vector<std::shared_ptr<PAGLayer>>("VectorPAGLayer");
  register_vector<Marker>("VectorMarker");
  return true;
}
}  // namespace pag