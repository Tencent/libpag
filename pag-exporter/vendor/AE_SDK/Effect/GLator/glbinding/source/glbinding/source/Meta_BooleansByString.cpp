
#include "Meta_Maps.h"

#include <glbinding/gl/boolean.h>


using namespace gl; // ToDo: multiple APIs?

namespace glbinding
{

const std::unordered_map<std::string, GLboolean> Meta_BooleansByString
{
#ifdef GL_BY_STRINGS
    { "GL_FALSE", GLboolean::GL_FALSE },
    { "GL_TRUE", GLboolean::GL_TRUE }
#endif
};

} // namespace glbinding
