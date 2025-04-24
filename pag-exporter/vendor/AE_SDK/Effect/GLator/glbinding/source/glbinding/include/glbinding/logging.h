#pragma once

#include <glbinding/callbacks.h>


namespace glbinding 
{

namespace logging
{
    GLBINDING_API void resize(const unsigned int);

    GLBINDING_API void start();
    GLBINDING_API void start(const std::string & filepath);
    GLBINDING_API void startExcept(const std::set<std::string> & blackList);
    GLBINDING_API void startExcept(const std::string & filepath, const std::set<std::string> & blackList);
    GLBINDING_API void stop();
    GLBINDING_API void pause();
    GLBINDING_API void resume();

    using BufferType = FunctionCall*;
    GLBINDING_API void log(BufferType  call);
}


} // namespace glbinding
