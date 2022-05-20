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

#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include "core/utils/EnumHasher.h"
#include "core/utils/Log.h"
#include "gpu/Swizzle.h"
#include "tgfx/gpu/Caps.h"
#include "tgfx/gpu/PixelFormat.h"
#include "tgfx/gpu/opengl/GLDefines.h"
#include "tgfx/gpu/opengl/GLFunctions.h"

#define GL_VER(major, minor) ((static_cast<uint32_t>(major) << 16) | static_cast<uint32_t>(minor))

namespace tgfx {
enum class GLStandard { None, GL, GLES, WebGL };

struct TextureFormat {
  unsigned sizedFormat = 0;
  unsigned internalFormatTexImage = 0;
  unsigned internalFormatRenderBuffer = 0;
  unsigned externalFormat = 0;
};

struct ConfigInfo {
  TextureFormat format;
  std::vector<int> colorSampleCounts;
  Swizzle swizzle = Swizzle::RGBA();
  Swizzle textureSwizzle = Swizzle::RGBA();
  Swizzle outputSwizzle = Swizzle::RGBA();
};

enum class GLVendor { ARM, Google, Imagination, Intel, Qualcomm, NVIDIA, ATI, Other };

/**
 * The type of MSAA for FBOs supported. Different extensions have different
 * semantics of how / when a resolve is performed.
 */
enum class MSFBOType {
  /**
   * no support for MSAA FBOs
   */
  None,
  /**
   * OpenGL 3.0+, OpenGL ES 3.0+, GL_ARB_framebuffer_object,
   * GL_CHROMIUM_framebuffer_multisample, GL_ANGLE_framebuffer_multisample,
   * or GL_EXT_framebuffer_multisample
   */
  Standard,
  /**
   * GL_APPLE_framebuffer_multisample ES extension
   */
  ES_Apple,
  /**
   * GL_IMG_multisampled_render_to_texture. This variation does not have MSAA renderbuffers.
   * Instead the texture is multisampled when bound to the FBO and then resolved automatically
   * when read. It also defines an alternate value for GL_MAX_SAMPLES (which we call
   * GL_MAX_SAMPLES_IMG).
   */
  ES_IMG_MsToTexture,
  /**
   * GL_EXT_multisampled_render_to_texture. Same as the IMG one above but uses the standard
   * GL_MAX_SAMPLES value.
   */
  ES_EXT_MsToTexture
};

class GLInfo {
 public:
  GLStandard standard = GLStandard::None;
  uint32_t version = 0;
  GLGetString* getString = nullptr;
  GLGetStringi* getStringi = nullptr;
  GLGetIntegerv* getIntegerv = nullptr;
  GLGetInternalformativ* getInternalformativ = nullptr;
  GLGetShaderPrecisionFormat* getShaderPrecisionFormat = nullptr;

  GLInfo(GLGetString* getString, GLGetStringi* getStringi, GLGetIntegerv* getIntegerv,
         GLGetInternalformativ* getInternalformativ,
         GLGetShaderPrecisionFormat* getShaderPrecisionFormat);

  bool hasExtension(const std::string& extension) const;

 private:
  void fetchExtensions();

  std::vector<std::string> extensions = {};
};

static const int kMaxSaneSamplers = 32;

class GLCaps : public Caps {
 public:
  GLStandard standard = GLStandard::None;
  uint32_t version = 0;
  GLVendor vendor = GLVendor::Other;
  bool vertexArrayObjectSupport = false;
  bool packRowLengthSupport = false;
  bool unpackRowLengthSupport = false;
  bool textureRedSupport = false;
  MSFBOType msFBOType = MSFBOType::None;
  bool frameBufferFetchSupport = false;
  bool frameBufferFetchRequiresEnablePerSample = false;
  std::string frameBufferFetchColorName;
  std::string frameBufferFetchExtensionString;
  bool textureBarrierSupport = false;
  int maxFragmentSamplers = kMaxSaneSamplers;
  bool semaphoreSupport = false;

  static const GLCaps* Get(Context* context);

  explicit GLCaps(const GLInfo& info);

  const TextureFormat& getTextureFormat(PixelFormat pixelFormat) const;

  const Swizzle& getSwizzle(PixelFormat pixelFormat) const;

  const Swizzle& getTextureSwizzle(PixelFormat pixelFormat) const;

  const Swizzle& getOutputSwizzle(PixelFormat pixelFormat) const;

  int getSampleCount(int requestedCount, PixelFormat pixelFormat) const;

  /**
   * Does the preferred MSAA FBO extension have MSAA renderBuffers?
   */
  bool usesMSAARenderBuffers() const;

  /**
   * Is the MSAA FBO extension one where the texture is multisampled when bound to an FBO and
   * then implicitly resolved when read.
   */
  bool usesImplicitMSAAResolve() const;

 private:
  std::unordered_map<PixelFormat, ConfigInfo, EnumHasher> pixelFormatMap = {};

  void initFormatMap(const GLInfo& info);
  void initColorSampleCount(const GLInfo& info);
  void initGLSupport(const GLInfo& info);
  void initGLESSupport(const GLInfo& info);
  void initWebGLSupport(const GLInfo& info);
  void initFSAASupport(const GLInfo& info);
};
}  // namespace tgfx
