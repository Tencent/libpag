#pragma once

#include <glbinding/nogl.h>

#include <glbinding/gl/bitfield.h>

namespace gl32core
{

// import bitfields to namespace
using gl::GL_NONE_BIT;
using gl::GL_CONTEXT_CORE_PROFILE_BIT;
using gl::GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT;
using gl::GL_SYNC_FLUSH_COMMANDS_BIT;
using gl::GL_CONTEXT_COMPATIBILITY_PROFILE_BIT;
using gl::GL_DEPTH_BUFFER_BIT;
using gl::GL_STENCIL_BUFFER_BIT;
using gl::GL_COLOR_BUFFER_BIT;
using gl::GL_MAP_READ_BIT;
using gl::GL_MAP_WRITE_BIT;
using gl::GL_MAP_INVALIDATE_RANGE_BIT;
using gl::GL_MAP_INVALIDATE_BUFFER_BIT;
using gl::GL_MAP_FLUSH_EXPLICIT_BIT;
using gl::GL_MAP_UNSYNCHRONIZED_BIT;

} // namespace gl32core
