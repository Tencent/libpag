
#include <glbinding/ContextHandle.h>

#ifdef WIN32
#include <windows.h>
#elif __APPLE__
#include <OpenGL/OpenGL.h>
#else
#include <GL/glx.h>
#endif

namespace glbinding
{

ContextHandle getCurrentContext()
{
    auto id = ContextHandle{0};

#ifdef WIN32
    const auto context = wglGetCurrentContext();
#elif __APPLE__
    const auto context = CGLGetCurrentContext();
#else
    const auto context = glXGetCurrentContext();
#endif
    id = reinterpret_cast<ContextHandle>(context);

    return id;
}

} // namespace glbinding
