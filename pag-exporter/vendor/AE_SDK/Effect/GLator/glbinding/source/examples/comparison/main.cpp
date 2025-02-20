
#include <iostream>
#include <ratio>
#include <thread>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glbinding/ContextInfo.h>
#include <glbinding/logging.h>
#include <glbinding/Version.h>

#include "Timer.h"
#include <thread>
#include <fstream>

#include "glbinding.h"
#include "glew.h"



void compare()
{
    const int ITERATIONS = 8192;
    const int ITERATIONS_WARMUP = ITERATIONS / 32;


    Timer timer;

    std::cout <<  std::endl << "test: initialize bindings ..." << std::endl;

    timer.start("      glbinding ");
    glbinding_init();

    timer.restart("      glew      ");
    glew_init();

    timer.stop();


    std::cout << std::endl
        << "OpenGL Version:  " << glbinding::ContextInfo::version() << std::endl
        << "OpenGL Vendor:   " << glbinding::ContextInfo::vendor() << std::endl
        << "OpenGL Renderer: " << glbinding::ContextInfo::renderer() << std::endl;


    std::cout << std::endl << "prep: warm-up ..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::cout << "      glew      " << std::endl;
    for (int i = 0; i < ITERATIONS_WARMUP; ++i)
        glew_test();

    std::cout << "      glbinding " << std::endl;
    for (int i = 0; i < ITERATIONS_WARMUP; ++i)
        glbinding_test();

    timer.setSteps(24 * ITERATIONS);
    

    std::cout << std::endl << "test: average call times for " << ITERATIONS << " x 24 calls ..." << std::endl;

    timer.start("      glew      ");
    for (int i = 0; i < ITERATIONS; ++i)
        glew_test();

    long double glew_avg = timer.restart("      glbinding ");

    for (int i = 0; i < ITERATIONS; ++i)
        glbinding_test();

    long double glbinding_avg = timer.stop();

 
    std::cout << std::endl << "test: again, now with error checking ..." << std::endl;

    glew_error(true);
    glbinding_error(true);

    timer.start("      glew      ");
    for (int i = 0; i < ITERATIONS; ++i)
        glew_test();

    long double glew_avg_err = timer.restart("      glbinding ");
    for (int i = 0; i < ITERATIONS; ++i)
        glbinding_test();

    long double glbinding_avg_err = timer.stop();
    glbinding_error(false);

    std::cout << std::endl << "test: again, now with logging ..." << std::endl;
    glbinding::logging::start("logs/comparison");
    timer.start("      glbinding ");

    for (int i = 0; i < ITERATIONS; ++i)
        glbinding_test();
    
    long double glbinding_avg_log = timer.stop();
    glbinding::logging::stop();


    std::cout << std::endl << "glbinding/glew decrease:                 " << (glbinding_avg / glew_avg - 1.0) * 100.0 << "%" << std::endl;
    std::cout << std::endl << "glbinding/glew decrease (error checks):  " << (glbinding_avg_err / glew_avg_err - 1.0) * 100.0 << "%" << std::endl;
    std::cout << std::endl << "glbinding decrease with logging:         " << (glbinding_avg_log / glbinding_avg  - 1.0) * 100.0 << "%" << std::endl;

    std::cout << std::endl << "finalizing ..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));
}

void errorfun(int errnum, const char * errmsg)
{
    std::cerr << errnum << ": " << errmsg << std::endl;
}

int main(int /*argc*/, char* /*argv*/[])
{
    if (!glfwInit())
        return 1;

    glfwSetErrorCallback(errorfun);

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

    glfwMakeContextCurrent(window);

    compare();

    glfwTerminate();
    return 0;
}
