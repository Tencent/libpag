#include "GLRestorer.h"

namespace pag {
GLRestorer::GLRestorer(const tgfx::GLFunctions* gl) : gl(gl) {
  if (gl == nullptr) {
    return;
  }
  gl->getIntegerv(GL_VIEWPORT, viewport);
  scissorEnabled = gl->isEnabled(GL_SCISSOR_TEST);
  if (scissorEnabled) {
    gl->getIntegerv(GL_SCISSOR_BOX, scissorBox);
  }
  gl->getIntegerv(GL_CURRENT_PROGRAM, &program);
  gl->getIntegerv(GL_FRAMEBUFFER_BINDING, &frameBuffer);
  gl->getIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
  gl->getIntegerv(GL_TEXTURE_BINDING_2D, &textureID);
  gl->getIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBuffer);
  gl->getIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &elementArrayBuffer);
  if (gl->bindVertexArray != nullptr) {
    gl->getIntegerv(GL_VERTEX_ARRAY_BINDING, &vertexArray);
  }
  gl->bindBuffer(GL_ARRAY_BUFFER, 0);
  gl->bindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  blendEnabled = gl->isEnabled(GL_BLEND);
  if (blendEnabled) {
    gl->getIntegerv(GL_BLEND_EQUATION, &blendEquation);
    gl->getIntegerv(GL_BLEND_EQUATION_RGB, &equationRGB);
    gl->getIntegerv(GL_BLEND_EQUATION_ALPHA, &equationAlpha);
    gl->getIntegerv(GL_BLEND_SRC_RGB, &blendSrcRGB);
    gl->getIntegerv(GL_BLEND_DST_RGB, &blendDstRGB);
    gl->getIntegerv(GL_BLEND_SRC_ALPHA, &blendSrcAlpha);
    gl->getIntegerv(GL_BLEND_DST_ALPHA, &blendDstAlpha);
  }
  if (vertexArray > 0) {
    gl->bindVertexArray(0);
  }
}

GLRestorer::~GLRestorer() {
  if (gl == nullptr) {
    return;
  }
  gl->viewport(viewport[0], viewport[1], viewport[2], viewport[3]);
  if (scissorEnabled) {
    gl->enable(GL_SCISSOR_TEST);
    gl->scissor(scissorBox[0], scissorBox[1], scissorBox[2], scissorBox[3]);
  } else {
    gl->disable(GL_SCISSOR_TEST);
  }
  gl->useProgram(static_cast<unsigned>(program));
  gl->bindFramebuffer(GL_FRAMEBUFFER, static_cast<unsigned>(frameBuffer));
  gl->activeTexture(static_cast<unsigned>(activeTexture));
  gl->bindTexture(GL_TEXTURE_2D, static_cast<unsigned>(textureID));
  if (vertexArray > 0) {
    gl->bindVertexArray(vertexArray);
  }
  gl->bindBuffer(GL_ARRAY_BUFFER, static_cast<unsigned>(arrayBuffer));
  gl->bindBuffer(GL_ELEMENT_ARRAY_BUFFER, static_cast<unsigned>(elementArrayBuffer));
  if (blendEnabled) {
    gl->enable(GL_BLEND);
    gl->blendEquation(static_cast<unsigned>(blendEquation));
    gl->blendEquationSeparate(equationRGB, equationAlpha);
    gl->blendFuncSeparate(blendSrcRGB, blendDstRGB, blendSrcAlpha, blendDstAlpha);
  } else {
    gl->disable(GL_BLEND);
  }
}
}  // namespace pag
