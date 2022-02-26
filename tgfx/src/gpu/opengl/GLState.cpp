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

#include "GLState.h"

#include "GLContext.h"

namespace tgfx {
#define PRIORITY_HIGH 3
#define PRIORITY_MEDIUM 2
#define PRIORITY_DEFAULT 1
#define PRIORITY_LOW 0

#ifdef DEBUG

#define UNSUPPORTED_STATE_WARNING() \
  LOGE("%s:%d: error: \"modifying unsupported GL state!\"\n", __FILE__, __LINE__);

#else

#define UNSUPPORTED_STATE_WARNING()

#endif

#define SAVE_DEFAULT_(state, Type)                                       \
  if ((state)->currentRecord) {                                          \
    auto& attributeMap = (state)->currentRecord->attributeMap;           \
    if (attributeMap.count(GLAttributeType::Type) == 0) {                \
      attributeMap[GLAttributeType::Type] =                              \
          (state)->insertAttribute(std::make_shared<Type>((state)->gl)); \
    }                                                                    \
  }

#define SAVE_DEFAULT(Type) SAVE_DEFAULT_(this, Type)

class ActiveTexture : public GLAttribute {
 public:
  explicit ActiveTexture(const GLInterface* gl) {
    gl->functions->getIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
  }

  GLAttributeType type() const override {
    return GLAttributeType::ActiveTexture;
  }

  int priority() const override {
    return PRIORITY_LOW;
  }

  void apply(GLState* state) const override {
    state->gl->functions->activeTexture(activeTexture);
    state->currentTextureUnit = activeTexture;
  }

  int activeTexture = 0;
};

class BlendEquationSeparate : public GLAttribute {
 public:
  explicit BlendEquationSeparate(const GLInterface* gl) {
    gl->functions->getIntegerv(GL_BLEND_EQUATION_RGB, &equationRGB);
    gl->functions->getIntegerv(GL_BLEND_EQUATION_ALPHA, &equationAlpha);
  }

  GLAttributeType type() const override {
    return GLAttributeType::BlendEquationSeparate;
  }

  int priority() const override {
    return PRIORITY_DEFAULT;
  }

  void apply(GLState* state) const override {
    state->gl->functions->blendEquationSeparate(equationRGB, equationAlpha);
  }

  int equationRGB = 0;
  int equationAlpha = 0;
};

class BlendFuncSeparate : public GLAttribute {
 public:
  explicit BlendFuncSeparate(const GLInterface* gl) {
    gl->functions->getIntegerv(GL_BLEND_SRC_RGB, &srcRGB);
    gl->functions->getIntegerv(GL_BLEND_DST_RGB, &dstRGB);
    gl->functions->getIntegerv(GL_BLEND_SRC_ALPHA, &srcAlpha);
    gl->functions->getIntegerv(GL_BLEND_DST_ALPHA, &dstAlpha);
  }

  GLAttributeType type() const override {
    return GLAttributeType::BlendFuncSeparate;
  }

  int priority() const override {
    return PRIORITY_DEFAULT;
  }

  void apply(GLState* state) const override {
    state->gl->functions->blendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
  }

  int srcRGB = 0;
  int dstRGB = 0;
  int srcAlpha = 0;
  int dstAlpha = 0;
};

class CurrentProgram : public GLAttribute {
 public:
  explicit CurrentProgram(const GLInterface* gl) {
    gl->functions->getIntegerv(GL_CURRENT_PROGRAM, &program);
  }

  GLAttributeType type() const override {
    return GLAttributeType::CurrentProgram;
  }

  int priority() const override {
    return PRIORITY_DEFAULT;
  }

  void apply(GLState* state) const override {
    state->gl->functions->useProgram(program);
  }

  int program = 0;
};

class EnableBlend : public GLAttribute {
 public:
  explicit EnableBlend(const GLInterface* gl) {
    enabled = gl->functions->isEnabled(GL_BLEND);
  }

  GLAttributeType type() const override {
    return GLAttributeType::EnableBlend;
  }

  int priority() const override {
    return PRIORITY_DEFAULT;
  }

  void apply(GLState* state) const override {
    if (enabled) {
      state->gl->functions->enable(GL_BLEND);
    } else {
      state->gl->functions->disable(GL_BLEND);
    }
  }

  bool enabled = false;
};

class EnableCullFace : public GLAttribute {
 public:
  explicit EnableCullFace(const GLInterface* gl) {
    enabled = gl->functions->isEnabled(GL_CULL_FACE);
  }

  GLAttributeType type() const override {
    return GLAttributeType::EnableCullFace;
  }

  int priority() const override {
    return PRIORITY_DEFAULT;
  }

  void apply(GLState* state) const override {
    if (enabled) {
      state->gl->functions->enable(GL_CULL_FACE);
    } else {
      state->gl->functions->disable(GL_CULL_FACE);
    }
  }

  bool enabled = false;
};

class EnableDepth : public GLAttribute {
 public:
  explicit EnableDepth(const GLInterface* gl) {
    enabled = gl->functions->isEnabled(GL_DEPTH_TEST);
  }

  GLAttributeType type() const override {
    return GLAttributeType::EnableDepth;
  }

  int priority() const override {
    return PRIORITY_DEFAULT;
  }

  void apply(GLState* state) const override {
    if (enabled) {
      state->gl->functions->enable(GL_DEPTH_TEST);
    } else {
      state->gl->functions->disable(GL_DEPTH_TEST);
    }
  }

  bool enabled = false;
};

class EnableDither : public GLAttribute {
 public:
  explicit EnableDither(const GLInterface* gl) {
    enabled = gl->functions->isEnabled(GL_DITHER);
  }

  GLAttributeType type() const override {
    return GLAttributeType::EnableDither;
  }

  int priority() const override {
    return PRIORITY_DEFAULT;
  }

  void apply(GLState* state) const override {
    if (enabled) {
      state->gl->functions->enable(GL_DITHER);
    } else {
      state->gl->functions->disable(GL_DITHER);
    }
  }

  bool enabled = false;
};

class EnableFramebufferSRGB : public GLAttribute {
 public:
  explicit EnableFramebufferSRGB(const GLInterface* gl) {
    enabled = gl->functions->isEnabled(GL_FRAMEBUFFER_SRGB);
  }

  GLAttributeType type() const override {
    return GLAttributeType::EnableFramebufferSRGB;
  }

  int priority() const override {
    return PRIORITY_DEFAULT;
  }

  void apply(GLState* state) const override {
    if (enabled) {
      state->gl->functions->enable(GL_FRAMEBUFFER_SRGB);
    } else {
      state->gl->functions->disable(GL_FRAMEBUFFER_SRGB);
    }
  }

  bool enabled = false;
};

class EnableScissor : public GLAttribute {
 public:
  explicit EnableScissor(const GLInterface* gl) {
    enabled = gl->functions->isEnabled(GL_SCISSOR_TEST);
  }

  GLAttributeType type() const override {
    return GLAttributeType::EnableScissor;
  }

  int priority() const override {
    return PRIORITY_DEFAULT;
  }

  void apply(GLState* state) const override {
    if (enabled) {
      state->gl->functions->enable(GL_SCISSOR_TEST);
    } else {
      state->gl->functions->disable(GL_SCISSOR_TEST);
    }
  }

  bool enabled = false;
};

class EnableStencil : public GLAttribute {
 public:
  explicit EnableStencil(const GLInterface* gl) {
    enabled = gl->functions->isEnabled(GL_STENCIL_TEST);
  }

  GLAttributeType type() const override {
    return GLAttributeType::EnableStencil;
  }

  int priority() const override {
    return PRIORITY_DEFAULT;
  }

  void apply(GLState* state) const override {
    if (enabled) {
      state->gl->functions->enable(GL_STENCIL_TEST);
    } else {
      state->gl->functions->disable(GL_STENCIL_TEST);
    }
  }

  bool enabled = false;
};

class EnableVertexProgramPointSize : public GLAttribute {
 public:
  explicit EnableVertexProgramPointSize(const GLInterface* gl) {
    enabled = gl->functions->isEnabled(GL_VERTEX_PROGRAM_POINT_SIZE);
  }

  GLAttributeType type() const override {
    return GLAttributeType::EnableVertexProgramPointSize;
  }

  int priority() const override {
    return PRIORITY_DEFAULT;
  }

  void apply(GLState* state) const override {
    if (enabled) {
      state->gl->functions->enable(GL_VERTEX_PROGRAM_POINT_SIZE);
    } else {
      state->gl->functions->disable(GL_VERTEX_PROGRAM_POINT_SIZE);
    }
  }

  bool enabled = false;
};

class EnableMultisample : public GLAttribute {
 public:
  explicit EnableMultisample(const GLInterface* gl) {
    enabled = gl->functions->isEnabled(GL_MULTISAMPLE);
  }

  GLAttributeType type() const override {
    return GLAttributeType::EnableMultisample;
  }

  int priority() const override {
    return PRIORITY_DEFAULT;
  }

  void apply(GLState* state) const override {
    if (enabled) {
      state->gl->functions->enable(GL_MULTISAMPLE);
    } else {
      state->gl->functions->disable(GL_MULTISAMPLE);
    }
  }

  bool enabled = false;
};

class EnableFetchPerSampleARM : public GLAttribute {
 public:
  explicit EnableFetchPerSampleARM(const GLInterface* gl) {
    enabled = gl->functions->isEnabled(GL_FETCH_PER_SAMPLE_ARM);
  }

  GLAttributeType type() const override {
    return GLAttributeType::EnableFetchPerSampleARM;
  }

  int priority() const override {
    return PRIORITY_DEFAULT;
  }

  void apply(GLState* state) const override {
    if (enabled) {
      state->gl->functions->enable(GL_FETCH_PER_SAMPLE_ARM);
    } else {
      state->gl->functions->disable(GL_FETCH_PER_SAMPLE_ARM);
    }
  }

  bool enabled = false;
};

class EnablePolygonOffsetFill : public GLAttribute {
 public:
  explicit EnablePolygonOffsetFill(const GLInterface* gl) {
    enabled = gl->functions->isEnabled(GL_POLYGON_OFFSET_FILL);
  }

  GLAttributeType type() const override {
    return GLAttributeType::EnablePolygonOffsetFill;
  }

  int priority() const override {
    return PRIORITY_DEFAULT;
  }

  void apply(GLState* state) const override {
    if (enabled) {
      state->gl->functions->enable(GL_POLYGON_OFFSET_FILL);
    } else {
      state->gl->functions->disable(GL_POLYGON_OFFSET_FILL);
    }
  }

  bool enabled = false;
};

class ElementBufferBinding : public GLAttribute {
 public:
  explicit ElementBufferBinding(const GLInterface* gl) {
    gl->functions->getIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &buffer);
  }

  GLAttributeType type() const override {
    return GLAttributeType::ElementBufferBinding;
  }

  int priority() const override {
    return PRIORITY_MEDIUM;
  }

  void apply(GLState* state) const override {
    state->gl->functions->bindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer);
  }

  int buffer = 0;
};

class FrameBufferBinding : public GLAttribute {
 public:
  explicit FrameBufferBinding(const GLInterface* gl) {
    gl->functions->getIntegerv(GL_FRAMEBUFFER_BINDING, &buffer);
  }

  GLAttributeType type() const override {
    return GLAttributeType::FrameBufferBinding;
  }

  int priority() const override {
    return PRIORITY_DEFAULT;
  }

  void apply(GLState* state) const override {
    state->gl->functions->bindFramebuffer(GL_FRAMEBUFFER, buffer);
  }

  int buffer = 0;
};

class ReadFrameBufferBinding : public GLAttribute {
 public:
  explicit ReadFrameBufferBinding(const GLInterface* gl) {
    gl->functions->getIntegerv(GL_READ_FRAMEBUFFER_BINDING, &buffer);
  }

  GLAttributeType type() const override {
    return GLAttributeType::ReadFrameBufferBinding;
  }

  int priority() const override {
    return PRIORITY_DEFAULT;
  }

  void apply(GLState* state) const override {
    state->gl->functions->bindFramebuffer(GL_READ_FRAMEBUFFER, buffer);
  }

  int buffer = 0;
};

class DrawFrameBufferBinding : public GLAttribute {
 public:
  explicit DrawFrameBufferBinding(const GLInterface* gl) {
    gl->functions->getIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &buffer);
  }

  GLAttributeType type() const override {
    return GLAttributeType::DrawFrameBufferBinding;
  }

  int priority() const override {
    return PRIORITY_DEFAULT;
  }

  void apply(GLState* state) const override {
    state->gl->functions->bindFramebuffer(GL_DRAW_FRAMEBUFFER, buffer);
  }

  int buffer = 0;
};

class RenderBufferBinding : public GLAttribute {
 public:
  explicit RenderBufferBinding(const GLInterface* gl) {
    gl->functions->getIntegerv(GL_RENDERBUFFER_BINDING, &buffer);
  }

  GLAttributeType type() const override {
    return GLAttributeType::RenderBufferBinding;
  }

  int priority() const override {
    return PRIORITY_DEFAULT;
  }

  void apply(GLState* state) const override {
    state->gl->functions->bindRenderbuffer(GL_RENDERBUFFER, buffer);
  }

  int buffer = 0;
};

class PackAlignment : public GLAttribute {
 public:
  explicit PackAlignment(const GLInterface* gl) {
    gl->functions->getIntegerv(GL_PACK_ALIGNMENT, &alignment);
  }

  GLAttributeType type() const override {
    return GLAttributeType::PackAlignment;
  }

  int priority() const override {
    return PRIORITY_DEFAULT;
  }

  void apply(GLState* state) const override {
    state->gl->functions->pixelStorei(GL_PACK_ALIGNMENT, alignment);
  }

  int alignment = 0;
};

class PackRowLength : public GLAttribute {
 public:
  explicit PackRowLength(const GLInterface* gl) {
    gl->functions->getIntegerv(GL_PACK_ROW_LENGTH, &rowLength);
  }

  GLAttributeType type() const override {
    return GLAttributeType::PackRowLength;
  }

  int priority() const override {
    return PRIORITY_DEFAULT;
  }

  void apply(GLState* state) const override {
    state->gl->functions->pixelStorei(GL_PACK_ROW_LENGTH, rowLength);
  }

  int rowLength = 0;
};

class ScissorBox : public GLAttribute {
 public:
  explicit ScissorBox(const GLInterface* gl) {
    gl->functions->getIntegerv(GL_SCISSOR_BOX, box);
  }

  GLAttributeType type() const override {
    return GLAttributeType::ScissorBox;
  }

  int priority() const override {
    return PRIORITY_DEFAULT;
  }

  void apply(GLState* state) const override {
    state->gl->functions->scissor(box[0], box[1], box[2], box[3]);
  }

  int box[4] = {};
};

class TextureBinding : public GLAttribute {
 public:
  TextureBinding(const GLInterface* gl, unsigned textureUnit, unsigned textureTarget)
      : textureUnit(textureUnit), textureTarget(textureTarget) {
    switch (textureTarget) {
      case GL_TEXTURE_2D:
        gl->functions->getIntegerv(GL_TEXTURE_BINDING_2D, &textureID);
        break;
      case GL_TEXTURE_EXTERNAL_OES:
        gl->functions->getIntegerv(GL_TEXTURE_BINDING_EXTERNAL_OES, &textureID);
        break;
      case GL_TEXTURE_RECTANGLE:
        gl->functions->getIntegerv(GL_TEXTURE_BINDING_RECTANGLE, &textureID);
        break;
      default:
        UNSUPPORTED_STATE_WARNING()
        break;
    }
  }

  GLAttributeType type() const override {
    return GLAttributeType::TextureBinding;
  }

  int priority() const override {
    return PRIORITY_DEFAULT;
  }

  void apply(GLState* state) const override {
    state->gl->functions->activeTexture(textureUnit);
    state->gl->functions->bindTexture(textureTarget, textureID);
  }

 private:
  unsigned textureUnit = 0;
  unsigned textureTarget = 0;
  int textureID = 0;
};

class UnpackAlignment : public GLAttribute {
 public:
  explicit UnpackAlignment(const GLInterface* gl) {
    gl->functions->getIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
  }

  GLAttributeType type() const override {
    return GLAttributeType::UnpackAlignment;
  }

  int priority() const override {
    return PRIORITY_DEFAULT;
  }

  void apply(GLState* state) const override {
    state->gl->functions->pixelStorei(GL_UNPACK_ALIGNMENT, alignment);
  }

  int alignment = 0;
};

class UnpackRowLength : public GLAttribute {
 public:
  explicit UnpackRowLength(const GLInterface* gl) {
    gl->functions->getIntegerv(GL_UNPACK_ROW_LENGTH, &rowLength);
  }

  GLAttributeType type() const override {
    return GLAttributeType::UnpackRowLength;
  }

  int priority() const override {
    return PRIORITY_DEFAULT;
  }

  void apply(GLState* state) const override {
    state->gl->functions->pixelStorei(GL_UNPACK_ROW_LENGTH, rowLength);
  }

  int rowLength = 0;
};

class VertexArrayBinding : public GLAttribute {
 public:
  explicit VertexArrayBinding(const GLInterface* gl) {
    gl->functions->getIntegerv(GL_VERTEX_ARRAY_BINDING, &vertexArray);
  }

  GLAttributeType type() const override {
    return GLAttributeType::VertexArrayBinding;
  }

  int priority() const override {
    return PRIORITY_HIGH;
  }

  void apply(GLState* state) const override {
    state->gl->functions->bindVertexArray(vertexArray);
    state->currentVAO = vertexArray;
  }

  int vertexArray = 0;
};

class VertexAttribute : public GLAttribute {
 public:
  explicit VertexAttribute(const GLInterface* gl, unsigned index) : index(index) {
    gl->functions->getVertexAttribiv(index, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &vbo);
    gl->functions->getVertexAttribiv(index, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enabled);
    gl->functions->getVertexAttribiv(index, GL_VERTEX_ATTRIB_ARRAY_SIZE, &size);
    gl->functions->getVertexAttribiv(index, GL_VERTEX_ATTRIB_ARRAY_TYPE, &dataType);
    gl->functions->getVertexAttribiv(index, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &normalized);
    gl->functions->getVertexAttribiv(index, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &stride);
    gl->functions->getVertexAttribPointerv(index, GL_VERTEX_ATTRIB_ARRAY_POINTER, &pointer);
  }

  GLAttributeType type() const override {
    return GLAttributeType::VertexAttribute;
  }

  int priority() const override {
    return PRIORITY_DEFAULT;
  }

  void apply(GLState* state) const override {
    state->gl->functions->bindBuffer(GL_ARRAY_BUFFER, vbo);
    state->gl->functions->vertexAttribPointer(index, size, dataType, normalized, stride, pointer);
    if (enabled == GL_TRUE) {
      state->gl->functions->enableVertexAttribArray(index);
    } else {
      state->gl->functions->disableVertexAttribArray(index);
    }
  }

 private:
  unsigned index = 0;
  int vbo = 0;
  int enabled = GL_FALSE;
  int size = 0;
  int dataType = 0;
  int normalized = 0;
  int stride = 0;
  void* pointer = nullptr;
};

class VertexBufferBinding : public GLAttribute {
 public:
  explicit VertexBufferBinding(const GLInterface* gl) {
    gl->functions->getIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer);
  }

  GLAttributeType type() const override {
    return GLAttributeType::VertexBufferBinding;
  }

  int priority() const override {
    return PRIORITY_LOW;
  }

  void apply(GLState* state) const override {
    state->gl->functions->bindBuffer(GL_ARRAY_BUFFER, buffer);
  }

  int buffer = 0;
};

class DepthMask : public GLAttribute {
 public:
  explicit DepthMask(const GLInterface* gl) {
    gl->functions->getBooleanv(GL_DEPTH_WRITEMASK, &flag);
  }

  GLAttributeType type() const override {
    return GLAttributeType::DepthMask;
  }

  int priority() const override {
    return PRIORITY_LOW;
  }

  void apply(GLState* state) const override {
    state->gl->functions->depthMask(flag);
  }

  unsigned char flag = false;
};

class Viewport : public GLAttribute {
 public:
  explicit Viewport(const GLInterface* gl) {
    gl->functions->getIntegerv(GL_VIEWPORT, viewport);
  }

  GLAttributeType type() const override {
    return GLAttributeType::Viewport;
  }

  int priority() const override {
    return PRIORITY_DEFAULT;
  }

  void apply(GLState* state) const override {
    state->gl->functions->viewport(viewport[0], viewport[1], viewport[2], viewport[3]);
  }

  int viewport[4] = {};
};

GLState::GLState(const GLInterface* gl) : gl(gl) {
  reset();
}

void GLState::reset() {
  if (gl->caps->vertexArrayObjectSupport) {
    int vao = 0;
    gl->functions->getIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    currentVAO = vao;
  }
  int texture = 0;
  gl->functions->getIntegerv(GL_ACTIVE_TEXTURE, &texture);
  currentTextureUnit = texture;
  currentRecord = nullptr;
  recordList = {};
}

void GLState::save() {
  if (currentRecord) {
    recordList.push_back(currentRecord);
  }
  currentRecord = std::make_shared<StateRecord>(currentVAO);
}

void GLState::restore() {
  if (currentRecord == nullptr) {
    return;
  }
  for (auto& attribute : currentRecord->defaultAttributes) {
    attribute->apply(this);
  }
  if (recordList.empty()) {
    currentRecord = nullptr;
  } else {
    currentRecord = recordList.back();
    recordList.pop_back();
  }
}

void GLState::activeTexture(unsigned textureUnit) {
  if (textureUnit == currentTextureUnit) {
    return;
  }
  SAVE_DEFAULT(ActiveTexture)
  currentTextureUnit = textureUnit;
  gl->functions->activeTexture(textureUnit);
}

void GLState::blendEquation(unsigned mode) {
  SAVE_DEFAULT(BlendEquationSeparate)
  gl->functions->blendEquation(mode);
}

void GLState::blendEquationSeparate(unsigned modeRGB, unsigned modeAlpha) {
  SAVE_DEFAULT(BlendEquationSeparate)
  gl->functions->blendEquationSeparate(modeRGB, modeAlpha);
}

void GLState::blendFunc(unsigned int srcFactor, unsigned int dstFactor) {
  SAVE_DEFAULT(BlendFuncSeparate)
  gl->functions->blendFunc(srcFactor, dstFactor);
}

void GLState::blendFuncSeparate(unsigned srcRGB, unsigned dstRGB, unsigned srcAlpha,
                                unsigned dstAlpha) {
  SAVE_DEFAULT(BlendFuncSeparate)
  gl->functions->blendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void GLState::bindFramebuffer(unsigned target, unsigned framebuffer) {
  switch (target) {
    case GL_FRAMEBUFFER:
      SAVE_DEFAULT(FrameBufferBinding)
      break;
    case GL_READ_FRAMEBUFFER:
      SAVE_DEFAULT(ReadFrameBufferBinding)
      break;
    case GL_DRAW_FRAMEBUFFER:
      SAVE_DEFAULT(DrawFrameBufferBinding)
      break;
    default:
      UNSUPPORTED_STATE_WARNING()
      break;
  }
  gl->functions->bindFramebuffer(target, framebuffer);
}

void GLState::bindRenderbuffer(unsigned int target, unsigned int renderbuffer) {
  if (target == GL_RENDERBUFFER) {
    SAVE_DEFAULT(RenderBufferBinding)
  } else {
    UNSUPPORTED_STATE_WARNING()
  }
  gl->functions->bindRenderbuffer(target, renderbuffer);
}

void GLState::bindBuffer(unsigned target, unsigned buffer) {
  switch (target) {
    case GL_ARRAY_BUFFER:
      SAVE_DEFAULT(VertexBufferBinding)
      break;
    case GL_ELEMENT_ARRAY_BUFFER:
      if (currentRecord && currentRecord->defaultVAO == currentVAO) {
        // EBO is stored on VAO, and we save the default value only if the current VAO is the
        // default.
        SAVE_DEFAULT(ElementBufferBinding)
      }
      break;
    default:
      UNSUPPORTED_STATE_WARNING()
      break;
  }
  gl->functions->bindBuffer(target, buffer);
}

void GLState::bindTexture(unsigned target, unsigned texture) {
  if (currentRecord) {
    auto& textureMap = currentRecord->textureMap;
    uint32_t uint = currentTextureUnit - GL_TEXTURE0;
    auto key = (uint << 28) | target;
    if (textureMap.count(key) == 0) {
      textureMap[key] =
          insertAttribute(std::make_shared<TextureBinding>(gl, currentTextureUnit, target));
    }
  }
  gl->functions->bindTexture(target, texture);
}

void GLState::bindVertexArray(unsigned vertexArray) {
  if (vertexArray == currentVAO) {
    return;
  }
  SAVE_DEFAULT(VertexArrayBinding)
  currentVAO = vertexArray;
  gl->functions->bindVertexArray(vertexArray);
}

static void SaveDefaultBlend(GLState* state) {
  SAVE_DEFAULT_(state, EnableBlend)
}

static void SaveDefaultCullFace(GLState* state) {
  SAVE_DEFAULT_(state, EnableCullFace)
}
static void SaveDefaultDepth(GLState* state) {
  SAVE_DEFAULT_(state, EnableDepth)
}
static void SaveDefaultDither(GLState* state) {
  SAVE_DEFAULT_(state, EnableDither)
}
static void SaveDefaultFramebufferSRGB(GLState* state) {
  SAVE_DEFAULT_(state, EnableFramebufferSRGB)
}
static void SaveDefaultStencil(GLState* state) {
  SAVE_DEFAULT_(state, EnableStencil)
}
static void SaveDefaultScissor(GLState* state) {
  SAVE_DEFAULT_(state, EnableScissor)
}
static void SaveDefaultVertexProgramPointSize(GLState* state) {
  SAVE_DEFAULT_(state, EnableVertexProgramPointSize)
}
static void SaveDefaultMultisample(GLState* state) {
  SAVE_DEFAULT_(state, EnableMultisample)
}
static void SaveDefaultFetchPerSampleARM(GLState* state) {
  SAVE_DEFAULT_(state, EnableFetchPerSampleARM)
}
static void SaveDefaultPolygonOffsetFill(GLState* state) {
  SAVE_DEFAULT_(state, EnablePolygonOffsetFill);
}

static constexpr std::pair<unsigned, void (*)(GLState*)> StateMap[] = {
    {GL_BLEND, SaveDefaultBlend},
    {GL_CULL_FACE, SaveDefaultCullFace},
    {GL_DEPTH_TEST, SaveDefaultDepth},
    {GL_DITHER, SaveDefaultDither},
    {GL_SCISSOR_TEST, SaveDefaultScissor},
    {GL_FRAMEBUFFER_SRGB, SaveDefaultFramebufferSRGB},
    {GL_STENCIL_TEST, SaveDefaultStencil},
    {GL_VERTEX_PROGRAM_POINT_SIZE, SaveDefaultVertexProgramPointSize},
    {GL_MULTISAMPLE, SaveDefaultMultisample},
    {GL_FETCH_PER_SAMPLE_ARM, SaveDefaultFetchPerSampleARM},
    {GL_POLYGON_OFFSET_FILL, SaveDefaultPolygonOffsetFill},
};

void GLState::disable(unsigned cap) {
  auto found = false;
  for (const auto& pair : StateMap) {
    if (pair.first == cap) {
      pair.second(this);
      found = true;
      break;
    }
  }
  if (!found) {
    // UNSUPPORTED_STATE_WARNING()
  }
  gl->functions->disable(cap);
}

void GLState::disableVertexAttribArray(unsigned index) {
  saveVertexAttribute(index);
  gl->functions->disableVertexAttribArray(index);
}

void GLState::enable(unsigned cap) {
  auto found = false;
  for (const auto& pair : StateMap) {
    if (pair.first == cap) {
      pair.second(this);
      found = true;
      break;
    }
  }
  if (!found) {
    UNSUPPORTED_STATE_WARNING()
  }
  gl->functions->enable(cap);
}

void GLState::enableVertexAttribArray(unsigned index) {
  saveVertexAttribute(index);
  gl->functions->enableVertexAttribArray(index);
}

void GLState::pixelStorei(unsigned int name, int param) {
  switch (name) {
    case GL_UNPACK_ROW_LENGTH:
      SAVE_DEFAULT(UnpackRowLength)
      break;
    case GL_UNPACK_ALIGNMENT:
      SAVE_DEFAULT(UnpackAlignment)
      break;
    case GL_PACK_ROW_LENGTH:
      SAVE_DEFAULT(PackRowLength)
      break;
    case GL_PACK_ALIGNMENT:
      SAVE_DEFAULT(PackAlignment)
      break;
    default:
      UNSUPPORTED_STATE_WARNING()
      break;
  }
  gl->functions->pixelStorei(name, param);
}

void GLState::scissor(int x, int y, int width, int height) {
  SAVE_DEFAULT(ScissorBox)
  gl->functions->scissor(x, y, width, height);
}

void GLState::viewport(int x, int y, int width, int height) {
  SAVE_DEFAULT(Viewport)
  gl->functions->viewport(x, y, width, height);
}

void GLState::useProgram(unsigned program) {
  SAVE_DEFAULT(CurrentProgram)
  gl->functions->useProgram(program);
}

void GLState::vertexAttribPointer(unsigned index, int size, unsigned type, unsigned char normalized,
                                  int stride, const void* ptr) {
  saveVertexAttribute(index);
  gl->functions->vertexAttribPointer(index, size, type, normalized, stride, ptr);
}

void GLState::depthMask(unsigned char flag) {
  SAVE_DEFAULT(DepthMask);
  gl->functions->depthMask(flag);
}

void GLState::saveVertexAttribute(unsigned int index) {
  if (currentRecord && currentRecord->defaultVAO == currentVAO) {
    auto& vertexMap = currentRecord->vertexMap;
    if (vertexMap.count(index) == 0) {
      vertexMap[index] = insertAttribute(std::make_shared<VertexAttribute>(gl, index));
    }
  }
}

std::shared_ptr<GLAttribute> GLState::insertAttribute(
    std::shared_ptr<GLAttribute> attribute) const {
  auto& defaultAttributes = currentRecord->defaultAttributes;
  switch (attribute->priority()) {
    case PRIORITY_HIGH:
      defaultAttributes.push_front(attribute);
      break;
    case PRIORITY_LOW:
      defaultAttributes.push_back(attribute);
      break;
    default:
      auto position = defaultAttributes.begin();
      for (auto& item : defaultAttributes) {
        if (item->priority() <= attribute->priority()) {
          break;
        }
        position++;
      }
      defaultAttributes.insert(position, attribute);
      break;
  }
  return attribute;
}

GLStateGuard::GLStateGuard(Context* context) {
  state = static_cast<GLContext*>(context)->glState.get();
  state->save();
}

GLStateGuard::~GLStateGuard() {
  state->restore();
}

}  // namespace tgfx
