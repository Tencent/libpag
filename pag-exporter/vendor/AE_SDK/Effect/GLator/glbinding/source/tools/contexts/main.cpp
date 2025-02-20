
#include <iostream>
#include <map>
#include <array>
#include <set>
#include <cstring>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glbinding/Meta.h>
#include <glbinding/AbstractFunction.h>
#include <glbinding/ContextInfo.h>
#include <glbinding/Version.h>

#include <glbinding/gl/gl.h>

#include <glbinding/Binding.h>


using namespace gl;
using namespace glbinding;

void print(
  const Version & version
, const bool forward
, const bool core
, const Version & result
, const bool isForward
, const bool isCore)
{
    std::cout << "  "
        << version << "  " << (forward ? "f" : "-") << " " << (core ? "c" : "-") << "  " << result << (isForward ? "f" : "") << (isCore ? "c" : "") << std::endl;
}

bool isCore(const Version & version)
{
    if (version<glbinding::Version(3,2))
    {
        return false;
    }

    GLint value = 0;

    glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &value);

    return (value & static_cast<unsigned>(GL_CONTEXT_CORE_PROFILE_BIT)) > 0;
}

Version printVersionOfContextRequest(
  const Version & version
, const bool forward
, const bool core)
{
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_VISIBLE, false);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, version.m_major);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, version.m_minor);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, forward);
    if (core)
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow * window = glfwCreateWindow(320, 240, "", nullptr, nullptr);
    if (!window)
    {
        print(version, forward, core, Version(), false, false);
        return Version();
    }

    glfwMakeContextCurrent(window);
    Binding::initialize();

    auto result = ContextInfo::version();
    glfwMakeContextCurrent(window);

    print(version, forward, core, result, forward, isCore(result));

    glfwMakeContextCurrent(nullptr);
    glfwDestroyWindow(window);

    return result;
}



int main(int argc, char * argv[])
{
    if (!glfwInit())
        return 1;

    //glfwSetErrorCallback(error);

    std::cout << std::endl << "test: requesting all context configurations ..." << std::endl
        << std::endl << "  scheme: <requested_version>  <forward> <core>  <created_version>" << std::endl << std::endl;

    std::map<Version, std::array<Version, 4>> markdown;

    for (const auto & version : Version::versions())
    {
        markdown[version][0] = printVersionOfContextRequest(version, false, false);
        markdown[version][1] = printVersionOfContextRequest(version, false, true);
        markdown[version][2] = printVersionOfContextRequest(version, true, false);
        markdown[version][3] = printVersionOfContextRequest(version, true, true);
        std::cout << std::endl;
    }

    auto printMarkdown = false;
    for (int i = 0; i < argc; ++i)
        printMarkdown |= (strcmp(argv[i], "--markdown") == 0);

    if (printMarkdown)
    {
        std::cout << "printing markdown formated results ..." << std::endl;

        std::cout << std::endl << "|";

        for (const auto & version : Version::versions())
            std::cout << version << (version != Version::latest() ? "<br>" : "");

        for (int i = 0; i < 4; ++i)
        {
            std::cout << "|";
            for (const auto & version : Version::versions())
                std::cout << markdown[version][i] << (version != Version::latest() ? "<br>" : "");
        }
        std::cout << "|" << std::endl << std::endl;
    }

    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_VISIBLE, false);

#ifdef __APPLE__
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, true);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif

    GLFWwindow * window = glfwCreateWindow(320, 240, "", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }
    glfwHideWindow(window);

    glfwMakeContextCurrent(window);

    Binding::initialize();

    // print some gl infos (query)

    std::cout
        << "OpenGL Version:  " << ContextInfo::version() << std::endl
        << "OpenGL Vendor:   " << ContextInfo::vendor() << std::endl
        << "OpenGL Renderer: " << ContextInfo::renderer() << std::endl
        << "OpenGL Revision: " << Meta::glRevision() << " (gl.xml)" << std::endl << std::endl;

    glfwTerminate();
    return 0;
}
