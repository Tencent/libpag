#pragma once

#include <glbinding/glbinding_api.h>

namespace glbinding 
{

using ContextHandle = long long;

GLBINDING_API ContextHandle getCurrentContext();

} // namespace glbinding
