
#include "glew.h"

#include <GL/glew.h>

#include <iostream>


namespace
{
    #include "gltest_data.inl"

    bool errors = false;
}


void glew_init()
{
    glewExperimental = GL_TRUE;
    glewInit();
    glGetError();
}


inline void glError()
{
    GLenum error = glGetError();
    if (error != GL_NO_ERROR)
        std::cout << "Error: " << error << std::endl;
}

void glew_test()
{
    if (errors)
    {
        #include "gltest_error.inl"
    }
    else
    {
        #include "gltest.inl"
    }
}

void glew_error(bool enable)
{
    errors = enable;
}
