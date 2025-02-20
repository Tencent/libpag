#pragma once

#include <glbinding/glbinding_api.h>

#include <set>
#include <string>

namespace gl
{
    enum class GLextension;
}


namespace glbinding
{

class Version;


class GLBINDING_API ContextInfo
{
public:
    ContextInfo() = delete;

    static std::set<gl::GLextension> extensions(std::set<std::string> * unknown = nullptr);

    static std::string renderer();
    static std::string vendor();

    static Version version();
};

} // namespace glbinding
