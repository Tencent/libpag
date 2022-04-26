#ifndef PAG_GLRESTORER_H
#define PAG_GLRESTORER_H

#include "tgfx/gpu/opengl/GLFunctions.h"

namespace pag {
class GLRestorer {
 public:
  GLRestorer(const tgfx::GLFunctions* gl);

  ~GLRestorer();

 private:
  const tgfx::GLFunctions* gl = nullptr;
  int viewport[4] = {};
  unsigned scissorEnabled = GL_FALSE;
  int scissorBox[4] = {};
  int frameBuffer = 0;
  int program = 0;
  int activeTexture = 0;
  int textureID = 0;
  int arrayBuffer = 0;
  int elementArrayBuffer = 0;
  int vertexArray = 0;

  unsigned blendEnabled = GL_FALSE;
  int blendEquation = 0;
  int equationRGB = 0;
  int equationAlpha = 0;
  int blendSrcRGB = 0;
  int blendDstRGB = 0;
  int blendSrcAlpha = 0;
  int blendDstAlpha = 0;
};
}  // namespace pag

#endif  //PAG_GLRESTORER_H
