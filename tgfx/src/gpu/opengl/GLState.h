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

#include <list>
#include <unordered_map>
#include "GLInterface.h"

namespace tgfx {
enum class GLAttributeType {
  ActiveTexture,
  BlendEquationSeparate,
  BlendFuncSeparate,
  CurrentProgram,
  ElementBufferBinding,
  EnableBlend,
  EnableCullFace,
  EnableDepth,
  EnableDither,
  EnableFramebufferSRGB,
  EnableScissor,
  EnableStencil,
  EnableVertexProgramPointSize,
  EnableMultisample,
  EnableFetchPerSampleARM,
  EnablePolygonOffsetFill,
  FrameBufferBinding,
  ReadFrameBufferBinding,
  DrawFrameBufferBinding,
  RenderBufferBinding,
  PackAlignment,
  PackRowLength,
  ScissorBox,
  TextureBinding,
  UnpackAlignment,
  UnpackRowLength,
  VertexArrayBinding,
  VertexAttribute,
  VertexBufferBinding,
  DepthMask,
  Viewport
};

class GLAttribute {
 public:
  virtual ~GLAttribute() = default;

  virtual GLAttributeType type() const = 0;

  virtual int priority() const = 0;

  virtual void apply(GLState* state) const = 0;
};

class StateRecord {
 public:
  explicit StateRecord(unsigned currentVAO) : defaultVAO(currentVAO) {
  }

  unsigned defaultVAO = 0;
  std::list<std::shared_ptr<GLAttribute>> defaultAttributes = {};
  std::unordered_map<unsigned, std::shared_ptr<GLAttribute>> vertexMap = {};
  std::unordered_map<uint32_t, std::shared_ptr<GLAttribute>> textureMap = {};
  std::unordered_map<GLAttributeType, std::shared_ptr<GLAttribute>, EnumHasher> attributeMap = {};
};

class GLState {
 public:
  explicit GLState(const GLInterface* gl);

  void reset();

  void save();

  void restore();

  void activeTexture(unsigned textureUnit);

  void blendEquation(unsigned mode);

  void blendEquationSeparate(unsigned modeRGB, unsigned modeAlpha);

  void blendFunc(unsigned srcFactor, unsigned dstFactor);

  void blendFuncSeparate(unsigned srcRGB, unsigned dstRGB, unsigned srcAlpha, unsigned dstAlpha);

  void bindFramebuffer(unsigned target, unsigned framebuffer);

  void bindRenderbuffer(unsigned target, unsigned renderbuffer);

  void bindBuffer(unsigned target, unsigned buffer);

  void bindTexture(unsigned target, unsigned texture);

  void bindVertexArray(unsigned vertexArray);

  void disable(unsigned cap);

  void disableVertexAttribArray(unsigned index);

  void enable(unsigned cap);

  void enableVertexAttribArray(unsigned index);

  void pixelStorei(unsigned name, int param);

  void scissor(int x, int y, int width, int height);

  void viewport(int x, int y, int width, int height);

  void useProgram(unsigned program);

  void vertexAttribPointer(unsigned index, int size, unsigned type, unsigned char normalized,
                           int stride, const void* ptr);

  void depthMask(unsigned char flag);

  const GLInterface* gl = nullptr;
  unsigned currentVAO = 0;
  unsigned currentTextureUnit = 0;
  std::shared_ptr<StateRecord> currentRecord = nullptr;
  std::vector<std::shared_ptr<StateRecord>> recordList = {};
  void saveVertexAttribute(unsigned index);
  std::shared_ptr<GLAttribute> insertAttribute(std::shared_ptr<GLAttribute> attribute) const;
};

class Context;

class GLStateGuard {
 public:
  explicit GLStateGuard(Context* context);

  ~GLStateGuard();

 private:
  GLState* state = nullptr;
};
}  // namespace tgfx
