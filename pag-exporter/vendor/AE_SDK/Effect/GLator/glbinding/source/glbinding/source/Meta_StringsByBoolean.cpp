
#include "Meta_Maps.h"

#include <glbinding/gl/boolean.h>


using namespace gl;

namespace glbinding
{

const std::unordered_map<GLboolean, std::string> Meta_StringsByBoolean
{
#ifdef STRINGS_BY_GL
    { GLboolean::GL_FALSE, "GL_FALSE" },
    { GLboolean::GL_TRUE, "GL_TRUE" }
#endif
};

} // namespace glbinding
