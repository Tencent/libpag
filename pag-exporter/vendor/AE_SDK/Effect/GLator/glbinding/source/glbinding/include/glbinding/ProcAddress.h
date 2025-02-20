#pragma once

#include <glbinding/glbinding_api.h>

namespace glbinding 
{

using ProcAddress = void(*)();

GLBINDING_API ProcAddress getProcAddress(const char * name);

} // namespace glbinding
