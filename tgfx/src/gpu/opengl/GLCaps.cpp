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

#include "GLCaps.h"
#include "GLUtil.h"

namespace tgfx {
static GLStandard GetGLStandard(const char* versionString) {
  if (versionString == nullptr) {
    return GLStandard::None;
  }
  int major, minor;
  int n = sscanf(versionString, "%d.%d", &major, &minor);
  if (2 == n) {
    return GLStandard::GL;
  }
  int esMajor, esMinor;
  n = sscanf(versionString, "OpenGL ES %d.%d (WebGL %d.%d", &esMajor, &esMinor, &major, &minor);
  if (4 == n) {
    return GLStandard::WebGL;
  }

  // check for ES 1
  char profile[2];
  n = sscanf(versionString, "OpenGL ES-%c%c %d.%d", profile, profile + 1, &major, &minor);
  if (4 == n) {
    // we no longer support ES1.
    return GLStandard::None;
  }

  // check for ES2
  n = sscanf(versionString, "OpenGL ES %d.%d", &major, &minor);
  if (2 == n) {
    return GLStandard::GLES;
  }
  return GLStandard::None;
}

static GLVendor GetVendorFromString(const char* vendorString) {
  if (vendorString) {
    if (0 == strcmp(vendorString, "ARM")) {
      return GLVendor::ARM;
    }
    if (0 == strcmp(vendorString, "Google Inc.")) {
      return GLVendor::Google;
    }
    if (0 == strcmp(vendorString, "Imagination Technologies")) {
      return GLVendor::Imagination;
    }
    if (0 == strncmp(vendorString, "Intel ", 6) || 0 == strcmp(vendorString, "Intel")) {
      return GLVendor::Intel;
    }
    if (0 == strcmp(vendorString, "Qualcomm")) {
      return GLVendor::Qualcomm;
    }
    if (0 == strcmp(vendorString, "NVIDIA Corporation")) {
      return GLVendor::NVIDIA;
    }
    if (0 == strcmp(vendorString, "ATI Technologies Inc.")) {
      return GLVendor::ATI;
    }
  }
  return GLVendor::Other;
}

GLInfo::GLInfo(GLGetString* getString, GLGetStringi* getStringi, GLGetIntegerv* getIntegerv,
               GLGetInternalformativ* getInternalformativ,
               GLGetShaderPrecisionFormat getShaderPrecisionFormat)
    : getString(getString),
      getStringi(getStringi),
      getIntegerv(getIntegerv),
      getInternalformativ(getInternalformativ),
      getShaderPrecisionFormat(getShaderPrecisionFormat) {
  auto versionString = (const char*)getString(GL_VERSION);
  auto glVersion = GetGLVersion(versionString);
  version = GL_VER(glVersion.majorVersion, glVersion.minorVersion);
  standard = GetGLStandard(versionString);
  fetchExtensions();
}

static void eatSpaceSepStrings(std::vector<std::string>* out, const char text[]) {
  if (!text) {
    return;
  }
  while (true) {
    while (' ' == *text) {
      ++text;
    }
    if ('\0' == *text) {
      break;
    }
    size_t length = strcspn(text, " ");
    out->emplace_back(text, length);
    text += length;
  }
}

void GLInfo::fetchExtensions() {
  bool indexed = false;
  if (standard == GLStandard::GL || standard == GLStandard::GLES) {
    // glGetStringi and indexed extensions were added in version 3.0 of desktop GL and ES.
    indexed = version >= GL_VER(3, 0);
  } else if (standard == GLStandard::WebGL) {
    // WebGL (1.0 or 2.0) doesn't natively support glGetStringi, but emscripten adds it in
    // https://github.com/emscripten-core/emscripten/issues/3472
    indexed = version >= GL_VER(2, 0);
  }
  if (indexed) {
    if (getStringi) {
      int extensionCount = 0;
      getIntegerv(GL_NUM_EXTENSIONS, &extensionCount);
      for (int i = 0; i < extensionCount; ++i) {
        const char* ext = reinterpret_cast<const char*>(getStringi(GL_EXTENSIONS, i));
        extensions.emplace_back(ext);
      }
    }
  } else {
    auto text = reinterpret_cast<const char*>(getString(GL_EXTENSIONS));
    eatSpaceSepStrings(&extensions, text);
  }
}

bool GLInfo::hasExtension(const std::string& extension) const {
  auto result = std::find(extensions.begin(), extensions.end(), extension);
  return result != extensions.end();
}

static bool IsMediumFloatFp32(const GLInfo& ctxInfo) {
  if (ctxInfo.standard == GLStandard::GL && ctxInfo.version < GL_VER(4, 1) &&
      !ctxInfo.hasExtension("GL_ARB_ES2_compatibility")) {
    // We're on a desktop GL that doesn't have precision info. Assume they're all 32bit float.
    return true;
  }
  // glGetShaderPrecisionFormat doesn't accept GL_GEOMETRY_SHADER as a shader type. Hopefully the
  // geometry shaders don't have lower precision than vertex and fragment.
  for (unsigned shader : {GL_FRAGMENT_SHADER, GL_VERTEX_SHADER}) {
    int range[2];
    int bits;
    ctxInfo.getShaderPrecisionFormat(shader, GL_MEDIUM_FLOAT, range, &bits);
    if (range[0] < 127 || range[1] < 127 || bits < 23) {
      return false;
    }
  }
  return true;
}

const GLCaps* GLCaps::Get(Context* context) {
  return context ? static_cast<const GLCaps*>(context->caps()) : nullptr;
}

GLCaps::GLCaps(const GLInfo& info) {
  standard = info.standard;
  version = info.version;
  vendor = GetVendorFromString((const char*)info.getString(GL_VENDOR));
  floatIs32Bits = IsMediumFloatFp32(info);
  switch (standard) {
    case GLStandard::GL:
      initGLSupport(info);
      break;
    case GLStandard::GLES:
      initGLESSupport(info);
      break;
    case GLStandard::WebGL:
      initWebGLSupport(info);
      break;
    default:
      break;
  }
  info.getIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
  info.getIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxFragmentSamplers);
  initFSAASupport(info);
  initFormatMap(info);
}

const TextureFormat& GLCaps::getTextureFormat(PixelFormat pixelFormat) const {
  return pixelFormatMap.at(pixelFormat).format;
}

const Swizzle& GLCaps::getSwizzle(PixelFormat pixelFormat) const {
  return pixelFormatMap.at(pixelFormat).swizzle;
}

const Swizzle& GLCaps::getTextureSwizzle(PixelFormat pixelFormat) const {
  return pixelFormatMap.at(pixelFormat).textureSwizzle;
}

const Swizzle& GLCaps::getOutputSwizzle(PixelFormat pixelFormat) const {
  return pixelFormatMap.at(pixelFormat).outputSwizzle;
}

int GLCaps::getSampleCount(int requestedCount, PixelFormat pixelFormat) const {
  if (requestedCount <= 1) {
    return 1;
  }
  auto result = pixelFormatMap.find(pixelFormat);
  if (result == pixelFormatMap.end() || result->second.colorSampleCounts.empty()) {
    return 1;
  }
  for (auto colorSampleCount : result->second.colorSampleCounts) {
    if (colorSampleCount >= requestedCount) {
      return colorSampleCount;
    }
  }
  return 1;
}

void GLCaps::initGLSupport(const GLInfo& info) {
  packRowLengthSupport = true;
  unpackRowLengthSupport = true;
  vertexArrayObjectSupport = version >= GL_VER(3, 0) ||
                             info.hasExtension("GL_ARB_vertex_array_object") ||
                             info.hasExtension("GL_APPLE_vertex_array_object");
  textureRedSupport = version >= GL_VER(3, 0) || info.hasExtension("GL_ARB_texture_rg");
  multisampleDisableSupport = true;
  textureBarrierSupport = version >= GL_VER(4, 5) || info.hasExtension("GL_ARB_texture_barrier") ||
                          info.hasExtension("GL_NV_texture_barrier");
  semaphoreSupport = version >= GL_VER(3, 2) || info.hasExtension("GL_ARB_sync");
  if (version < GL_VER(1, 3) && !info.hasExtension("GL_ARB_texture_border_clamp")) {
    clampToBorderSupport = false;
  }
}

void GLCaps::initGLESSupport(const GLInfo& info) {
  packRowLengthSupport = version >= GL_VER(3, 0) || info.hasExtension("GL_NV_pack_subimage");
  unpackRowLengthSupport = version >= GL_VER(3, 0) || info.hasExtension("GL_EXT_unpack_subimage");
  vertexArrayObjectSupport =
      version >= GL_VER(3, 0) || info.hasExtension("GL_OES_vertex_array_object");
  textureRedSupport = version >= GL_VER(3, 0) || info.hasExtension("GL_EXT_texture_rg");
  multisampleDisableSupport = info.hasExtension("GL_EXT_multisample_compatibility");
  textureBarrierSupport = info.hasExtension("GL_NV_texture_barrier");
  if (info.hasExtension("GL_EXT_shader_framebuffer_fetch")) {
    frameBufferFetchSupport = true;
    frameBufferFetchColorName = "gl_LastFragData[0]";
    frameBufferFetchExtensionString = "GL_EXT_shader_framebuffer_fetch";
    frameBufferFetchRequiresEnablePerSample = false;
  } else if (info.hasExtension("GL_NV_shader_framebuffer_fetch")) {
    // Actually, we haven't seen an ES3.0 device with this extension yet, so we don't know.
    frameBufferFetchSupport = true;
    frameBufferFetchColorName = "gl_LastFragData[0]";
    frameBufferFetchExtensionString = "GL_NV_shader_framebuffer_fetch";
    frameBufferFetchRequiresEnablePerSample = false;
  } else if (info.hasExtension("GL_ARM_shader_framebuffer_fetch")) {
    frameBufferFetchSupport = true;
    frameBufferFetchColorName = "gl_LastFragColorARM";
    frameBufferFetchExtensionString = "GL_ARM_shader_framebuffer_fetch";
    // The arm extension requires specifically enabling MSAA fetching per sample.
    // On some devices this may have a perf hit. Also multiple render targets are disabled
    frameBufferFetchRequiresEnablePerSample = true;
  }
  semaphoreSupport = version >= GL_VER(3, 0) || info.hasExtension("GL_APPLE_sync");
  if (version < GL_VER(3, 2) && !info.hasExtension("GL_EXT_texture_border_clamp") &&
      !info.hasExtension("GL_NV_texture_border_clamp") &&
      !info.hasExtension("GL_OES_texture_border_clamp")) {
    clampToBorderSupport = false;
  }
  npotTextureTileSupport = version >= GL_VER(3, 0) || info.hasExtension("GL_OES_texture_npot");
}

void GLCaps::initWebGLSupport(const GLInfo& info) {
  packRowLengthSupport = version >= GL_VER(2, 0);
  unpackRowLengthSupport = version >= GL_VER(2, 0);
  vertexArrayObjectSupport = version >= GL_VER(2, 0) ||
                             info.hasExtension("GL_OES_vertex_array_object") ||
                             info.hasExtension("OES_vertex_array_object");
  textureRedSupport = false;
  multisampleDisableSupport = false;  // no WebGL support
  textureBarrierSupport = false;
  semaphoreSupport = version >= GL_VER(2, 0);
  clampToBorderSupport = false;
  npotTextureTileSupport = version >= GL_VER(2, 0);
}

void GLCaps::initFormatMap(const GLInfo& info) {
  pixelFormatMap[PixelFormat::RGBA_8888].format.sizedFormat = GL_RGBA8;
  pixelFormatMap[PixelFormat::RGBA_8888].format.externalFormat = GL_RGBA;
  pixelFormatMap[PixelFormat::RGBA_8888].swizzle = Swizzle::RGBA();
  if (textureRedSupport) {
    pixelFormatMap[PixelFormat::ALPHA_8].format.sizedFormat = GL_R8;
    pixelFormatMap[PixelFormat::ALPHA_8].format.externalFormat = GL_RED;
    pixelFormatMap[PixelFormat::ALPHA_8].swizzle = Swizzle::RRRR();
    // Shader output swizzles will default to RGBA. When we've use GL_RED instead of GL_ALPHA to
    // implement PixelFormat::ALPHA_8 we need to swizzle the shader outputs so the alpha channel
    // gets written to the single component.
    pixelFormatMap[PixelFormat::ALPHA_8].outputSwizzle = Swizzle::AAAA();
    pixelFormatMap[PixelFormat::GRAY_8].format.sizedFormat = GL_R8;
    pixelFormatMap[PixelFormat::GRAY_8].format.externalFormat = GL_RED;
    pixelFormatMap[PixelFormat::GRAY_8].swizzle = Swizzle::RRRA();
    pixelFormatMap[PixelFormat::RG_88].format.sizedFormat = GL_RG8;
    pixelFormatMap[PixelFormat::RG_88].format.externalFormat = GL_RG;
    pixelFormatMap[PixelFormat::RG_88].swizzle = Swizzle::RGRG();
  } else {
    pixelFormatMap[PixelFormat::ALPHA_8].format.sizedFormat = GL_ALPHA8;
    pixelFormatMap[PixelFormat::ALPHA_8].format.externalFormat = GL_ALPHA;
    pixelFormatMap[PixelFormat::ALPHA_8].swizzle = Swizzle::AAAA();
    pixelFormatMap[PixelFormat::GRAY_8].format.sizedFormat = GL_LUMINANCE8;
    pixelFormatMap[PixelFormat::GRAY_8].format.externalFormat = GL_LUMINANCE;
    pixelFormatMap[PixelFormat::GRAY_8].swizzle = Swizzle::RGBA();
    pixelFormatMap[PixelFormat::RG_88].format.sizedFormat = GL_LUMINANCE8_ALPHA8;
    pixelFormatMap[PixelFormat::RG_88].format.externalFormat = GL_LUMINANCE_ALPHA;
    pixelFormatMap[PixelFormat::RG_88].swizzle = Swizzle::RARA();
  }
  // Texture swizzle doesn't work when using CVPixelBuffer on iPhone 5s and iPhone 6.
  // If we don't have texture swizzle support then the shader generator must insert the
  // swizzle into shader code.
  for (auto& item : pixelFormatMap) {
    item.second.textureSwizzle = item.second.swizzle;
  }
  // ES 2.0 requires that the internal/external formats match.
  bool useSizedTexFormats =
      (standard == GLStandard::GL || (standard == GLStandard::GLES && version >= GL_VER(3, 0)));
  bool useSizedRbFormats = standard == GLStandard::GLES || standard == GLStandard::WebGL;

  for (auto& item : pixelFormatMap) {
    auto& format = item.second.format;
    format.internalFormatTexImage = useSizedTexFormats ? format.sizedFormat : format.externalFormat;
    format.internalFormatRenderBuffer =
        useSizedRbFormats ? format.sizedFormat : format.externalFormat;
  }
  initColorSampleCount(info);
}

static bool UsesInternalformatQuery(GLStandard standard, const GLInfo& glInterface,
                                    uint32_t version) {
  return (standard == GLStandard::GL &&
          ((version >= GL_VER(4, 2)) || glInterface.hasExtension("GL_ARB_internalformat_query"))) ||
         (standard == GLStandard::GLES && version >= GL_VER(3, 0));
}

void GLCaps::initColorSampleCount(const GLInfo& info) {
  std::vector<PixelFormat> pixelFormats = {PixelFormat::RGBA_8888};
  for (auto pixelFormat : pixelFormats) {
    if (UsesInternalformatQuery(standard, info, version)) {
      int count = 0;
      unsigned format = pixelFormatMap[pixelFormat].format.internalFormatRenderBuffer;
      info.getInternalformativ(GL_RENDERBUFFER, format, GL_NUM_SAMPLE_COUNTS, 1, &count);
      if (count) {
        int* temp = new int[count];
        info.getInternalformativ(GL_RENDERBUFFER, format, GL_SAMPLES, count, temp);
        // GL has a concept of MSAA rasterization with a single sample but we do not.
        if (temp[count - 1] == 1) {
          --count;
        }
        // We initialize our supported values with 1 (no msaa) and reverse the order
        // returned by GL so that the array is ascending.
        pixelFormatMap[pixelFormat].colorSampleCounts.push_back(1);
        for (int j = 0; j < count; ++j) {
          pixelFormatMap[pixelFormat].colorSampleCounts.push_back(temp[count - j - 1]);
        }
        delete[] temp;
      }
    } else {
      // Fake out the table using some semi-standard counts up to the max allowed sample
      // count.
      int maxSampleCnt = 1;
      if (MSFBOType::ES_IMG_MsToTexture == msFBOType) {
        info.getIntegerv(GL_MAX_SAMPLES_IMG, &maxSampleCnt);
      } else if (MSFBOType::None != msFBOType) {
        info.getIntegerv(GL_MAX_SAMPLES, &maxSampleCnt);
      }
      // Chrome has a mock GL implementation that returns 0.
      maxSampleCnt = std::max(1, maxSampleCnt);

      std::vector<int> defaultSamples{1, 2, 4, 8};
      for (auto samples : defaultSamples) {
        if (samples > maxSampleCnt) {
          break;
        }
        pixelFormatMap[pixelFormat].colorSampleCounts.push_back(samples);
      }
    }
  }
}

static MSFBOType GetMSFBOType_GLES(uint32_t version, const GLInfo& glInterface) {
  // We prefer multisampled-render-to-texture extensions over ES3 MSAA because we've observed
  // ES3 driver bugs on at least one device with a tiled GPU (N10).
  if (glInterface.hasExtension("GL_EXT_multisampled_render_to_texture")) {
    return MSFBOType::ES_EXT_MsToTexture;
  } else if (glInterface.hasExtension("GL_IMG_multisampled_render_to_texture")) {
    return MSFBOType::ES_IMG_MsToTexture;
  } else if (version >= GL_VER(3, 0) ||
             glInterface.hasExtension("GL_CHROMIUM_framebuffer_multisample") ||
             glInterface.hasExtension("GL_ANGLE_framebuffer_multisample")) {
    return MSFBOType::Standard;
  } else if (glInterface.hasExtension("GL_APPLE_framebuffer_multisample")) {
    return MSFBOType::ES_Apple;
  }
  return MSFBOType::None;
}

void GLCaps::initFSAASupport(const GLInfo& info) {
  if (standard == GLStandard::GL) {
    if (version >= GL_VER(3, 0) || info.hasExtension("GL_ARB_framebuffer_object") ||
        (info.hasExtension("GL_EXT_framebuffer_multisample") &&
         info.hasExtension("GL_EXT_framebuffer_blit"))) {
      msFBOType = MSFBOType::Standard;
    }
  } else if (standard == GLStandard::GLES) {
    msFBOType = GetMSFBOType_GLES(version, info);
  } else if (standard == GLStandard::WebGL) {
    // No support in WebGL
    msFBOType = MSFBOType::None;
  }

  // We disable MSAA across the board for Intel GPUs for performance reasons.
  if (GLVendor::Intel == vendor) {
    msFBOType = MSFBOType::None;
  }
}
bool GLCaps::usesMSAARenderBuffers() const {
  return MSFBOType::None != msFBOType && MSFBOType::ES_IMG_MsToTexture != msFBOType &&
         MSFBOType::ES_EXT_MsToTexture != msFBOType;
}

bool GLCaps::usesImplicitMSAAResolve() const {
  return MSFBOType::ES_IMG_MsToTexture == msFBOType || MSFBOType::ES_EXT_MsToTexture == msFBOType;
}
}  // namespace tgfx
