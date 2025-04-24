
#include <iostream>
#include <algorithm>
#include <thread>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glbinding/AbstractFunction.h>
#include <glbinding/callbacks.h>
#include <glbinding/Meta.h>
#include <glbinding/ContextInfo.h>
#include <glbinding/Version.h>

#include <glbinding/Binding.h>

#include <glbinding/gl32/gl.h>


using namespace gl32;
using namespace glbinding;

void error(int errnum, const char * errmsg)
{
    std::cerr << errnum << ": " << errmsg << std::endl;
}


#include "../comparison/gltest_data.inl"

void doGLStuff(GLFWwindow * window)
{
    glViewport(0, 0, 320, 240);

    #include "../comparison/gltest.inl"

    glfwSwapBuffers(window);
}

int main()
{
    if (!glfwInit())
        return 1;

    glfwSetErrorCallback(error);

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

    Binding::addContextSwitchCallback([](ContextHandle handle){
        std::cout << "Activating context " << handle << std::endl;
    });

    glfwMakeContextCurrent(window);

    // print some gl infos (query)

    Binding::initialize(false);

    std::cout << std::endl
        << "OpenGL Version:  " << ContextInfo::version() << std::endl
        << "OpenGL Vendor:   " << ContextInfo::vendor() << std::endl
        << "OpenGL Renderer: " << ContextInfo::renderer() << std::endl
        << "OpenGL Revision: " << Meta::glRevision() << " (gl.xml)" << std::endl << std::endl;

    setCallbackMask(CallbackMask::After | CallbackMask::ParametersAndReturnValue);

    setAfterCallback([](const FunctionCall & call) {
        std::cout << call.function->name() << "(";

        for (unsigned i = 0; i < call.parameters.size(); ++i)
        {
            std::cout << call.parameters[i]->asString();
            if (i < call.parameters.size() - 1)
                std::cout << ", ";
        }

        std::cout << ")";

        if (call.returnValue)
        {
            std::cout << " -> " << call.returnValue->asString();
        }

        std::cout << std::endl;
    });

    Binding::CreateProgram.setAfterCallback([](GLuint id) {
        std::cout << "Created Program: " << id << std::endl;
    });
    Binding::CreateShader.setAfterCallback([](GLuint id, GLenum /*type*/) {
        std::cout << "Created Shader: " << id << std::endl;
    });
    Binding::DeleteProgram.setAfterCallback([](GLuint id) {
        std::cout << "Deleted Program: " << id << std::endl;
    });
    Binding::DeleteShader.setAfterCallback([](GLuint id) {
        std::cout << "Deleted Shader: " << id << std::endl;
    });

    doGLStuff(window);

    std::cout << std::endl;

    glfwTerminate();
    return 0;
}
