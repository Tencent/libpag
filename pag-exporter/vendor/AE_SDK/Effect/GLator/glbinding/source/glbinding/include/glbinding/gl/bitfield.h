#pragma once

#include <glbinding/nogl.h>

#include <glbinding/SharedBitfield.h>

namespace gl
{

enum class AttribMask : unsigned int
{
    GL_NONE_BIT             = 0x0, // Generic GL_NONE_BIT
    GL_CURRENT_BIT          = 0x00000001,
    GL_POINT_BIT            = 0x00000002,
    GL_LINE_BIT             = 0x00000004,
    GL_POLYGON_BIT          = 0x00000008,
    GL_POLYGON_STIPPLE_BIT  = 0x00000010,
    GL_PIXEL_MODE_BIT       = 0x00000020,
    GL_LIGHTING_BIT         = 0x00000040,
    GL_FOG_BIT              = 0x00000080,
    GL_DEPTH_BUFFER_BIT     = 0x00000100,
    GL_ACCUM_BUFFER_BIT     = 0x00000200,
    GL_STENCIL_BUFFER_BIT   = 0x00000400,
    GL_VIEWPORT_BIT         = 0x00000800,
    GL_TRANSFORM_BIT        = 0x00001000,
    GL_ENABLE_BIT           = 0x00002000,
    GL_COLOR_BUFFER_BIT     = 0x00004000,
    GL_HINT_BIT             = 0x00008000,
    GL_EVAL_BIT             = 0x00010000,
    GL_LIST_BIT             = 0x00020000,
    GL_TEXTURE_BIT          = 0x00040000,
    GL_SCISSOR_BIT          = 0x00080000,
    GL_MULTISAMPLE_BIT      = 0x20000000,
    GL_MULTISAMPLE_BIT_3DFX = 0x20000000,
    GL_MULTISAMPLE_BIT_ARB  = 0x20000000,
    GL_MULTISAMPLE_BIT_EXT  = 0x20000000,
    GL_ALL_ATTRIB_BITS      = 0xFFFFFFFF,
};


enum class BufferAccessMask : unsigned int
{
    GL_NONE_BIT                  = 0x0, // Generic GL_NONE_BIT
    GL_MAP_READ_BIT              = 0x0001,
    GL_MAP_WRITE_BIT             = 0x0002,
    GL_MAP_INVALIDATE_RANGE_BIT  = 0x0004,
    GL_MAP_INVALIDATE_BUFFER_BIT = 0x0008,
    GL_MAP_FLUSH_EXPLICIT_BIT    = 0x0010,
    GL_MAP_UNSYNCHRONIZED_BIT    = 0x0020,
    GL_MAP_PERSISTENT_BIT        = 0x0040,
    GL_MAP_COHERENT_BIT          = 0x0080,
};


enum class BufferStorageMask : unsigned int
{
    GL_NONE_BIT            = 0x0, // Generic GL_NONE_BIT
    GL_MAP_READ_BIT        = 0x0001, // reuse from BufferAccessMask
    GL_MAP_WRITE_BIT       = 0x0002, // reuse from BufferAccessMask
    GL_MAP_PERSISTENT_BIT  = 0x0040, // reuse from BufferAccessMask
    GL_MAP_COHERENT_BIT    = 0x0080, // reuse from BufferAccessMask
    GL_DYNAMIC_STORAGE_BIT = 0x0100,
    GL_CLIENT_STORAGE_BIT  = 0x0200,
};


enum class ClearBufferMask : unsigned int
{
    GL_NONE_BIT               = 0x0, // Generic GL_NONE_BIT
    GL_DEPTH_BUFFER_BIT       = 0x00000100, // reuse from AttribMask
    GL_ACCUM_BUFFER_BIT       = 0x00000200, // reuse from AttribMask
    GL_STENCIL_BUFFER_BIT     = 0x00000400, // reuse from AttribMask
    GL_COLOR_BUFFER_BIT       = 0x00004000, // reuse from AttribMask
    GL_COVERAGE_BUFFER_BIT_NV = 0x00008000,
};


enum class ClientAttribMask : unsigned int
{
    GL_NONE_BIT                = 0x0, // Generic GL_NONE_BIT
    GL_CLIENT_PIXEL_STORE_BIT  = 0x00000001,
    GL_CLIENT_VERTEX_ARRAY_BIT = 0x00000002,
    GL_CLIENT_ALL_ATTRIB_BITS  = 0xFFFFFFFF,
};


enum class ContextFlagMask : unsigned int
{
    GL_NONE_BIT                            = 0x0, // Generic GL_NONE_BIT
    GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT = 0x00000001,
    GL_CONTEXT_FLAG_DEBUG_BIT              = 0x00000002,
    GL_CONTEXT_FLAG_ROBUST_ACCESS_BIT      = 0x00000004,
    GL_CONTEXT_FLAG_ROBUST_ACCESS_BIT_ARB  = 0x00000004,
    GL_CONTEXT_FLAG_NO_ERROR_BIT_KHR       = 0x00000008,
};


enum class ContextProfileMask : unsigned int
{
    GL_NONE_BIT                          = 0x0, // Generic GL_NONE_BIT
    GL_CONTEXT_CORE_PROFILE_BIT          = 0x00000001,
    GL_CONTEXT_COMPATIBILITY_PROFILE_BIT = 0x00000002,
};


enum class FfdMaskSGIX : unsigned int
{
    GL_NONE_BIT                      = 0x0, // Generic GL_NONE_BIT
    GL_TEXTURE_DEFORMATION_BIT_SGIX  = 0x00000001,
    GL_GEOMETRY_DEFORMATION_BIT_SGIX = 0x00000002,
};


enum class FragmentShaderColorModMaskATI : unsigned int
{
    GL_NONE_BIT       = 0x0, // Generic GL_NONE_BIT
    GL_COMP_BIT_ATI   = 0x00000002,
    GL_NEGATE_BIT_ATI = 0x00000004,
    GL_BIAS_BIT_ATI   = 0x00000008,
};


enum class FragmentShaderDestMaskATI : unsigned int
{
    GL_NONE_BIT      = 0x0, // Generic GL_NONE_BIT
    GL_RED_BIT_ATI   = 0x00000001,
    GL_GREEN_BIT_ATI = 0x00000002,
    GL_BLUE_BIT_ATI  = 0x00000004,
};


enum class FragmentShaderDestModMaskATI : unsigned int
{
    GL_NONE_BIT         = 0x0, // Generic GL_NONE_BIT
    GL_2X_BIT_ATI       = 0x00000001,
    GL_4X_BIT_ATI       = 0x00000002,
    GL_8X_BIT_ATI       = 0x00000004,
    GL_HALF_BIT_ATI     = 0x00000008,
    GL_QUARTER_BIT_ATI  = 0x00000010,
    GL_EIGHTH_BIT_ATI   = 0x00000020,
    GL_SATURATE_BIT_ATI = 0x00000040,
};


enum class MapBufferUsageMask : unsigned int
{
    GL_NONE_BIT                  = 0x0, // Generic GL_NONE_BIT
    GL_MAP_READ_BIT              = 0x0001, // reuse from BufferAccessMask
    GL_MAP_WRITE_BIT             = 0x0002, // reuse from BufferAccessMask
    GL_MAP_INVALIDATE_RANGE_BIT  = 0x0004, // reuse from BufferAccessMask
    GL_MAP_INVALIDATE_BUFFER_BIT = 0x0008, // reuse from BufferAccessMask
    GL_MAP_FLUSH_EXPLICIT_BIT    = 0x0010, // reuse from BufferAccessMask
    GL_MAP_UNSYNCHRONIZED_BIT    = 0x0020, // reuse from BufferAccessMask
    GL_MAP_PERSISTENT_BIT        = 0x0040, // reuse from BufferAccessMask
    GL_MAP_COHERENT_BIT          = 0x0080, // reuse from BufferAccessMask
    GL_DYNAMIC_STORAGE_BIT       = 0x0100, // reuse from BufferStorageMask
    GL_CLIENT_STORAGE_BIT        = 0x0200, // reuse from BufferStorageMask
    GL_SPARSE_STORAGE_BIT_ARB    = 0x0400,
};


enum class MemoryBarrierMask : unsigned int
{
    GL_NONE_BIT                            = 0x0, // Generic GL_NONE_BIT
    GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT     = 0x00000001,
    GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT_EXT = 0x00000001,
    GL_ELEMENT_ARRAY_BARRIER_BIT           = 0x00000002,
    GL_ELEMENT_ARRAY_BARRIER_BIT_EXT       = 0x00000002,
    GL_UNIFORM_BARRIER_BIT                 = 0x00000004,
    GL_UNIFORM_BARRIER_BIT_EXT             = 0x00000004,
    GL_TEXTURE_FETCH_BARRIER_BIT           = 0x00000008,
    GL_TEXTURE_FETCH_BARRIER_BIT_EXT       = 0x00000008,
    GL_SHADER_GLOBAL_ACCESS_BARRIER_BIT_NV = 0x00000010,
    GL_SHADER_IMAGE_ACCESS_BARRIER_BIT     = 0x00000020,
    GL_SHADER_IMAGE_ACCESS_BARRIER_BIT_EXT = 0x00000020,
    GL_COMMAND_BARRIER_BIT                 = 0x00000040,
    GL_COMMAND_BARRIER_BIT_EXT             = 0x00000040,
    GL_PIXEL_BUFFER_BARRIER_BIT            = 0x00000080,
    GL_PIXEL_BUFFER_BARRIER_BIT_EXT        = 0x00000080,
    GL_TEXTURE_UPDATE_BARRIER_BIT          = 0x00000100,
    GL_TEXTURE_UPDATE_BARRIER_BIT_EXT      = 0x00000100,
    GL_BUFFER_UPDATE_BARRIER_BIT           = 0x00000200,
    GL_BUFFER_UPDATE_BARRIER_BIT_EXT       = 0x00000200,
    GL_FRAMEBUFFER_BARRIER_BIT             = 0x00000400,
    GL_FRAMEBUFFER_BARRIER_BIT_EXT         = 0x00000400,
    GL_TRANSFORM_FEEDBACK_BARRIER_BIT      = 0x00000800,
    GL_TRANSFORM_FEEDBACK_BARRIER_BIT_EXT  = 0x00000800,
    GL_ATOMIC_COUNTER_BARRIER_BIT          = 0x00001000,
    GL_ATOMIC_COUNTER_BARRIER_BIT_EXT      = 0x00001000,
    GL_SHADER_STORAGE_BARRIER_BIT          = 0x00002000,
    GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT    = 0x00004000,
    GL_QUERY_BUFFER_BARRIER_BIT            = 0x00008000,
    GL_ALL_BARRIER_BITS                    = 0xFFFFFFFF,
    GL_ALL_BARRIER_BITS_EXT                = 0xFFFFFFFF,
};


enum class PathFontStyle : unsigned int
{
    GL_NONE_BIT      = 0x0, // Generic GL_NONE_BIT
    GL_BOLD_BIT_NV   = 0x01,
    GL_ITALIC_BIT_NV = 0x02,
};


enum class PathRenderingMaskNV : unsigned int
{
    GL_NONE_BIT                                = 0x0, // Generic GL_NONE_BIT
    GL_FONT_X_MIN_BOUNDS_BIT_NV                = 0x00010000,
    GL_FONT_Y_MIN_BOUNDS_BIT_NV                = 0x00020000,
    GL_FONT_X_MAX_BOUNDS_BIT_NV                = 0x00040000,
    GL_FONT_Y_MAX_BOUNDS_BIT_NV                = 0x00080000,
    GL_FONT_UNITS_PER_EM_BIT_NV                = 0x00100000,
    GL_FONT_ASCENDER_BIT_NV                    = 0x00200000,
    GL_FONT_DESCENDER_BIT_NV                   = 0x00400000,
    GL_FONT_HEIGHT_BIT_NV                      = 0x00800000,
    GL_BOLD_BIT_NV                             = 0x01, // reuse from PathFontStyle
    GL_GLYPH_WIDTH_BIT_NV                      = 0x01,
    GL_FONT_MAX_ADVANCE_WIDTH_BIT_NV           = 0x01000000,
    GL_GLYPH_HEIGHT_BIT_NV                     = 0x02,
    GL_ITALIC_BIT_NV                           = 0x02, // reuse from PathFontStyle
    GL_FONT_MAX_ADVANCE_HEIGHT_BIT_NV          = 0x02000000,
    GL_GLYPH_HORIZONTAL_BEARING_X_BIT_NV       = 0x04,
    GL_FONT_UNDERLINE_POSITION_BIT_NV          = 0x04000000,
    GL_GLYPH_HORIZONTAL_BEARING_Y_BIT_NV       = 0x08,
    GL_FONT_UNDERLINE_THICKNESS_BIT_NV         = 0x08000000,
    GL_GLYPH_HORIZONTAL_BEARING_ADVANCE_BIT_NV = 0x10,
    GL_GLYPH_HAS_KERNING_BIT_NV                = 0x100,
    GL_FONT_HAS_KERNING_BIT_NV                 = 0x10000000,
    GL_GLYPH_VERTICAL_BEARING_X_BIT_NV         = 0x20,
    GL_FONT_NUM_GLYPH_INDICES_BIT_NV           = 0x20000000,
    GL_GLYPH_VERTICAL_BEARING_Y_BIT_NV         = 0x40,
    GL_GLYPH_VERTICAL_BEARING_ADVANCE_BIT_NV   = 0x80,
};


enum class PerformanceQueryCapsMaskINTEL : unsigned int
{
    GL_NONE_BIT                       = 0x0, // Generic GL_NONE_BIT
    GL_PERFQUERY_SINGLE_CONTEXT_INTEL = 0x00000000,
    GL_PERFQUERY_GLOBAL_CONTEXT_INTEL = 0x00000001,
};


enum class SyncObjectMask : unsigned int
{
    GL_NONE_BIT                = 0x0, // Generic GL_NONE_BIT
    GL_SYNC_FLUSH_COMMANDS_BIT = 0x00000001,
};


enum class TextureStorageMaskAMD : unsigned int
{
    GL_NONE_BIT                       = 0x0, // Generic GL_NONE_BIT
    GL_TEXTURE_STORAGE_SPARSE_BIT_AMD = 0x00000001,
};


enum class UnusedMask : unsigned int
{
    GL_NONE_BIT   = 0x0, // Generic GL_NONE_BIT
    GL_UNUSED_BIT = 0x00000000,
};


enum class UseProgramStageMask : unsigned int
{
    GL_NONE_BIT                   = 0x0, // Generic GL_NONE_BIT
    GL_VERTEX_SHADER_BIT          = 0x00000001,
    GL_FRAGMENT_SHADER_BIT        = 0x00000002,
    GL_GEOMETRY_SHADER_BIT        = 0x00000004,
    GL_TESS_CONTROL_SHADER_BIT    = 0x00000008,
    GL_TESS_EVALUATION_SHADER_BIT = 0x00000010,
    GL_COMPUTE_SHADER_BIT         = 0x00000020,
    GL_ALL_SHADER_BITS            = 0xFFFFFFFF,
};


enum class VertexHintsMaskPGI : unsigned int
{
    GL_NONE_BIT                        = 0x0, // Generic GL_NONE_BIT
    GL_VERTEX23_BIT_PGI                = 0x00000004,
    GL_VERTEX4_BIT_PGI                 = 0x00000008,
    GL_COLOR3_BIT_PGI                  = 0x00010000,
    GL_COLOR4_BIT_PGI                  = 0x00020000,
    GL_EDGEFLAG_BIT_PGI                = 0x00040000,
    GL_INDEX_BIT_PGI                   = 0x00080000,
    GL_MAT_AMBIENT_BIT_PGI             = 0x00100000,
    GL_MAT_AMBIENT_AND_DIFFUSE_BIT_PGI = 0x00200000,
    GL_MAT_DIFFUSE_BIT_PGI             = 0x00400000,
    GL_MAT_EMISSION_BIT_PGI            = 0x00800000,
    GL_MAT_COLOR_INDEXES_BIT_PGI       = 0x01000000,
    GL_MAT_SHININESS_BIT_PGI           = 0x02000000,
    GL_MAT_SPECULAR_BIT_PGI            = 0x04000000,
    GL_NORMAL_BIT_PGI                  = 0x08000000,
    GL_TEXCOORD1_BIT_PGI               = 0x10000000,
    GL_TEXCOORD2_BIT_PGI               = 0x20000000,
    GL_TEXCOORD3_BIT_PGI               = 0x40000000,
    GL_TEXCOORD4_BIT_PGI               = 0x80000000,
};



// import bitfields to namespace

static const glbinding::SharedBitfield<gl::AttribMask, gl::BufferAccessMask, gl::BufferStorageMask, gl::ClearBufferMask, gl::ClientAttribMask, gl::ContextFlagMask, gl::ContextProfileMask, gl::FfdMaskSGIX, gl::FragmentShaderColorModMaskATI, gl::FragmentShaderDestMaskATI, gl::FragmentShaderDestModMaskATI, gl::MapBufferUsageMask, gl::MemoryBarrierMask, gl::PathFontStyle, gl::PathRenderingMaskNV, gl::PerformanceQueryCapsMaskINTEL, gl::SyncObjectMask, gl::TextureStorageMaskAMD, gl::UnusedMask, gl::UseProgramStageMask, gl::VertexHintsMaskPGI> GL_NONE_BIT = gl::AttribMask::GL_NONE_BIT;
static const PerformanceQueryCapsMaskINTEL GL_PERFQUERY_SINGLE_CONTEXT_INTEL = PerformanceQueryCapsMaskINTEL::GL_PERFQUERY_SINGLE_CONTEXT_INTEL;
static const UnusedMask GL_UNUSED_BIT = UnusedMask::GL_UNUSED_BIT;
static const FragmentShaderDestModMaskATI GL_2X_BIT_ATI = FragmentShaderDestModMaskATI::GL_2X_BIT_ATI;
static const ClientAttribMask GL_CLIENT_PIXEL_STORE_BIT = ClientAttribMask::GL_CLIENT_PIXEL_STORE_BIT;
static const ContextProfileMask GL_CONTEXT_CORE_PROFILE_BIT = ContextProfileMask::GL_CONTEXT_CORE_PROFILE_BIT;
static const ContextFlagMask GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT = ContextFlagMask::GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT;
static const AttribMask GL_CURRENT_BIT = AttribMask::GL_CURRENT_BIT;
static const PerformanceQueryCapsMaskINTEL GL_PERFQUERY_GLOBAL_CONTEXT_INTEL = PerformanceQueryCapsMaskINTEL::GL_PERFQUERY_GLOBAL_CONTEXT_INTEL;
static const FragmentShaderDestMaskATI GL_RED_BIT_ATI = FragmentShaderDestMaskATI::GL_RED_BIT_ATI;
static const SyncObjectMask GL_SYNC_FLUSH_COMMANDS_BIT = SyncObjectMask::GL_SYNC_FLUSH_COMMANDS_BIT;
static const FfdMaskSGIX GL_TEXTURE_DEFORMATION_BIT_SGIX = FfdMaskSGIX::GL_TEXTURE_DEFORMATION_BIT_SGIX;
static const TextureStorageMaskAMD GL_TEXTURE_STORAGE_SPARSE_BIT_AMD = TextureStorageMaskAMD::GL_TEXTURE_STORAGE_SPARSE_BIT_AMD;
static const MemoryBarrierMask GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT = MemoryBarrierMask::GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT;
static const MemoryBarrierMask GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT_EXT = MemoryBarrierMask::GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT_EXT;
static const UseProgramStageMask GL_VERTEX_SHADER_BIT = UseProgramStageMask::GL_VERTEX_SHADER_BIT;
static const FragmentShaderDestModMaskATI GL_4X_BIT_ATI = FragmentShaderDestModMaskATI::GL_4X_BIT_ATI;
static const ClientAttribMask GL_CLIENT_VERTEX_ARRAY_BIT = ClientAttribMask::GL_CLIENT_VERTEX_ARRAY_BIT;
static const FragmentShaderColorModMaskATI GL_COMP_BIT_ATI = FragmentShaderColorModMaskATI::GL_COMP_BIT_ATI;
static const ContextProfileMask GL_CONTEXT_COMPATIBILITY_PROFILE_BIT = ContextProfileMask::GL_CONTEXT_COMPATIBILITY_PROFILE_BIT;
static const ContextFlagMask GL_CONTEXT_FLAG_DEBUG_BIT = ContextFlagMask::GL_CONTEXT_FLAG_DEBUG_BIT;
static const MemoryBarrierMask GL_ELEMENT_ARRAY_BARRIER_BIT = MemoryBarrierMask::GL_ELEMENT_ARRAY_BARRIER_BIT;
static const MemoryBarrierMask GL_ELEMENT_ARRAY_BARRIER_BIT_EXT = MemoryBarrierMask::GL_ELEMENT_ARRAY_BARRIER_BIT_EXT;
static const UseProgramStageMask GL_FRAGMENT_SHADER_BIT = UseProgramStageMask::GL_FRAGMENT_SHADER_BIT;
static const FfdMaskSGIX GL_GEOMETRY_DEFORMATION_BIT_SGIX = FfdMaskSGIX::GL_GEOMETRY_DEFORMATION_BIT_SGIX;
static const FragmentShaderDestMaskATI GL_GREEN_BIT_ATI = FragmentShaderDestMaskATI::GL_GREEN_BIT_ATI;
static const AttribMask GL_POINT_BIT = AttribMask::GL_POINT_BIT;
static const FragmentShaderDestModMaskATI GL_8X_BIT_ATI = FragmentShaderDestModMaskATI::GL_8X_BIT_ATI;
static const FragmentShaderDestMaskATI GL_BLUE_BIT_ATI = FragmentShaderDestMaskATI::GL_BLUE_BIT_ATI;
static const ContextFlagMask GL_CONTEXT_FLAG_ROBUST_ACCESS_BIT = ContextFlagMask::GL_CONTEXT_FLAG_ROBUST_ACCESS_BIT;
static const ContextFlagMask GL_CONTEXT_FLAG_ROBUST_ACCESS_BIT_ARB = ContextFlagMask::GL_CONTEXT_FLAG_ROBUST_ACCESS_BIT_ARB;
static const UseProgramStageMask GL_GEOMETRY_SHADER_BIT = UseProgramStageMask::GL_GEOMETRY_SHADER_BIT;
static const AttribMask GL_LINE_BIT = AttribMask::GL_LINE_BIT;
static const FragmentShaderColorModMaskATI GL_NEGATE_BIT_ATI = FragmentShaderColorModMaskATI::GL_NEGATE_BIT_ATI;
static const MemoryBarrierMask GL_UNIFORM_BARRIER_BIT = MemoryBarrierMask::GL_UNIFORM_BARRIER_BIT;
static const MemoryBarrierMask GL_UNIFORM_BARRIER_BIT_EXT = MemoryBarrierMask::GL_UNIFORM_BARRIER_BIT_EXT;
static const VertexHintsMaskPGI GL_VERTEX23_BIT_PGI = VertexHintsMaskPGI::GL_VERTEX23_BIT_PGI;
static const FragmentShaderColorModMaskATI GL_BIAS_BIT_ATI = FragmentShaderColorModMaskATI::GL_BIAS_BIT_ATI;
static const ContextFlagMask GL_CONTEXT_FLAG_NO_ERROR_BIT_KHR = ContextFlagMask::GL_CONTEXT_FLAG_NO_ERROR_BIT_KHR;
static const FragmentShaderDestModMaskATI GL_HALF_BIT_ATI = FragmentShaderDestModMaskATI::GL_HALF_BIT_ATI;
static const AttribMask GL_POLYGON_BIT = AttribMask::GL_POLYGON_BIT;
static const UseProgramStageMask GL_TESS_CONTROL_SHADER_BIT = UseProgramStageMask::GL_TESS_CONTROL_SHADER_BIT;
static const MemoryBarrierMask GL_TEXTURE_FETCH_BARRIER_BIT = MemoryBarrierMask::GL_TEXTURE_FETCH_BARRIER_BIT;
static const MemoryBarrierMask GL_TEXTURE_FETCH_BARRIER_BIT_EXT = MemoryBarrierMask::GL_TEXTURE_FETCH_BARRIER_BIT_EXT;
static const VertexHintsMaskPGI GL_VERTEX4_BIT_PGI = VertexHintsMaskPGI::GL_VERTEX4_BIT_PGI;
static const AttribMask GL_POLYGON_STIPPLE_BIT = AttribMask::GL_POLYGON_STIPPLE_BIT;
static const FragmentShaderDestModMaskATI GL_QUARTER_BIT_ATI = FragmentShaderDestModMaskATI::GL_QUARTER_BIT_ATI;
static const MemoryBarrierMask GL_SHADER_GLOBAL_ACCESS_BARRIER_BIT_NV = MemoryBarrierMask::GL_SHADER_GLOBAL_ACCESS_BARRIER_BIT_NV;
static const UseProgramStageMask GL_TESS_EVALUATION_SHADER_BIT = UseProgramStageMask::GL_TESS_EVALUATION_SHADER_BIT;
static const UseProgramStageMask GL_COMPUTE_SHADER_BIT = UseProgramStageMask::GL_COMPUTE_SHADER_BIT;
static const FragmentShaderDestModMaskATI GL_EIGHTH_BIT_ATI = FragmentShaderDestModMaskATI::GL_EIGHTH_BIT_ATI;
static const AttribMask GL_PIXEL_MODE_BIT = AttribMask::GL_PIXEL_MODE_BIT;
static const MemoryBarrierMask GL_SHADER_IMAGE_ACCESS_BARRIER_BIT = MemoryBarrierMask::GL_SHADER_IMAGE_ACCESS_BARRIER_BIT;
static const MemoryBarrierMask GL_SHADER_IMAGE_ACCESS_BARRIER_BIT_EXT = MemoryBarrierMask::GL_SHADER_IMAGE_ACCESS_BARRIER_BIT_EXT;
static const MemoryBarrierMask GL_COMMAND_BARRIER_BIT = MemoryBarrierMask::GL_COMMAND_BARRIER_BIT;
static const MemoryBarrierMask GL_COMMAND_BARRIER_BIT_EXT = MemoryBarrierMask::GL_COMMAND_BARRIER_BIT_EXT;
static const AttribMask GL_LIGHTING_BIT = AttribMask::GL_LIGHTING_BIT;
static const FragmentShaderDestModMaskATI GL_SATURATE_BIT_ATI = FragmentShaderDestModMaskATI::GL_SATURATE_BIT_ATI;
static const AttribMask GL_FOG_BIT = AttribMask::GL_FOG_BIT;
static const MemoryBarrierMask GL_PIXEL_BUFFER_BARRIER_BIT = MemoryBarrierMask::GL_PIXEL_BUFFER_BARRIER_BIT;
static const MemoryBarrierMask GL_PIXEL_BUFFER_BARRIER_BIT_EXT = MemoryBarrierMask::GL_PIXEL_BUFFER_BARRIER_BIT_EXT;
static const glbinding::SharedBitfield<AttribMask, ClearBufferMask> GL_DEPTH_BUFFER_BIT = AttribMask::GL_DEPTH_BUFFER_BIT;
static const MemoryBarrierMask GL_TEXTURE_UPDATE_BARRIER_BIT = MemoryBarrierMask::GL_TEXTURE_UPDATE_BARRIER_BIT;
static const MemoryBarrierMask GL_TEXTURE_UPDATE_BARRIER_BIT_EXT = MemoryBarrierMask::GL_TEXTURE_UPDATE_BARRIER_BIT_EXT;
static const glbinding::SharedBitfield<AttribMask, ClearBufferMask> GL_ACCUM_BUFFER_BIT = AttribMask::GL_ACCUM_BUFFER_BIT;
static const MemoryBarrierMask GL_BUFFER_UPDATE_BARRIER_BIT = MemoryBarrierMask::GL_BUFFER_UPDATE_BARRIER_BIT;
static const MemoryBarrierMask GL_BUFFER_UPDATE_BARRIER_BIT_EXT = MemoryBarrierMask::GL_BUFFER_UPDATE_BARRIER_BIT_EXT;
static const MemoryBarrierMask GL_FRAMEBUFFER_BARRIER_BIT = MemoryBarrierMask::GL_FRAMEBUFFER_BARRIER_BIT;
static const MemoryBarrierMask GL_FRAMEBUFFER_BARRIER_BIT_EXT = MemoryBarrierMask::GL_FRAMEBUFFER_BARRIER_BIT_EXT;
static const glbinding::SharedBitfield<AttribMask, ClearBufferMask> GL_STENCIL_BUFFER_BIT = AttribMask::GL_STENCIL_BUFFER_BIT;
static const MemoryBarrierMask GL_TRANSFORM_FEEDBACK_BARRIER_BIT = MemoryBarrierMask::GL_TRANSFORM_FEEDBACK_BARRIER_BIT;
static const MemoryBarrierMask GL_TRANSFORM_FEEDBACK_BARRIER_BIT_EXT = MemoryBarrierMask::GL_TRANSFORM_FEEDBACK_BARRIER_BIT_EXT;
static const AttribMask GL_VIEWPORT_BIT = AttribMask::GL_VIEWPORT_BIT;
static const MemoryBarrierMask GL_ATOMIC_COUNTER_BARRIER_BIT = MemoryBarrierMask::GL_ATOMIC_COUNTER_BARRIER_BIT;
static const MemoryBarrierMask GL_ATOMIC_COUNTER_BARRIER_BIT_EXT = MemoryBarrierMask::GL_ATOMIC_COUNTER_BARRIER_BIT_EXT;
static const AttribMask GL_TRANSFORM_BIT = AttribMask::GL_TRANSFORM_BIT;
static const AttribMask GL_ENABLE_BIT = AttribMask::GL_ENABLE_BIT;
static const MemoryBarrierMask GL_SHADER_STORAGE_BARRIER_BIT = MemoryBarrierMask::GL_SHADER_STORAGE_BARRIER_BIT;
static const MemoryBarrierMask GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT = MemoryBarrierMask::GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT;
static const glbinding::SharedBitfield<AttribMask, ClearBufferMask> GL_COLOR_BUFFER_BIT = AttribMask::GL_COLOR_BUFFER_BIT;
static const ClearBufferMask GL_COVERAGE_BUFFER_BIT_NV = ClearBufferMask::GL_COVERAGE_BUFFER_BIT_NV;
static const AttribMask GL_HINT_BIT = AttribMask::GL_HINT_BIT;
static const MemoryBarrierMask GL_QUERY_BUFFER_BARRIER_BIT = MemoryBarrierMask::GL_QUERY_BUFFER_BARRIER_BIT;
static const glbinding::SharedBitfield<BufferAccessMask, BufferStorageMask, MapBufferUsageMask> GL_MAP_READ_BIT = BufferAccessMask::GL_MAP_READ_BIT;
static const VertexHintsMaskPGI GL_COLOR3_BIT_PGI = VertexHintsMaskPGI::GL_COLOR3_BIT_PGI;
static const AttribMask GL_EVAL_BIT = AttribMask::GL_EVAL_BIT;
static const PathRenderingMaskNV GL_FONT_X_MIN_BOUNDS_BIT_NV = PathRenderingMaskNV::GL_FONT_X_MIN_BOUNDS_BIT_NV;
static const glbinding::SharedBitfield<BufferAccessMask, BufferStorageMask, MapBufferUsageMask> GL_MAP_WRITE_BIT = BufferAccessMask::GL_MAP_WRITE_BIT;
static const VertexHintsMaskPGI GL_COLOR4_BIT_PGI = VertexHintsMaskPGI::GL_COLOR4_BIT_PGI;
static const PathRenderingMaskNV GL_FONT_Y_MIN_BOUNDS_BIT_NV = PathRenderingMaskNV::GL_FONT_Y_MIN_BOUNDS_BIT_NV;
static const AttribMask GL_LIST_BIT = AttribMask::GL_LIST_BIT;
static const glbinding::SharedBitfield<BufferAccessMask, MapBufferUsageMask> GL_MAP_INVALIDATE_RANGE_BIT = BufferAccessMask::GL_MAP_INVALIDATE_RANGE_BIT;
static const VertexHintsMaskPGI GL_EDGEFLAG_BIT_PGI = VertexHintsMaskPGI::GL_EDGEFLAG_BIT_PGI;
static const PathRenderingMaskNV GL_FONT_X_MAX_BOUNDS_BIT_NV = PathRenderingMaskNV::GL_FONT_X_MAX_BOUNDS_BIT_NV;
static const AttribMask GL_TEXTURE_BIT = AttribMask::GL_TEXTURE_BIT;
static const glbinding::SharedBitfield<BufferAccessMask, MapBufferUsageMask> GL_MAP_INVALIDATE_BUFFER_BIT = BufferAccessMask::GL_MAP_INVALIDATE_BUFFER_BIT;
static const PathRenderingMaskNV GL_FONT_Y_MAX_BOUNDS_BIT_NV = PathRenderingMaskNV::GL_FONT_Y_MAX_BOUNDS_BIT_NV;
static const VertexHintsMaskPGI GL_INDEX_BIT_PGI = VertexHintsMaskPGI::GL_INDEX_BIT_PGI;
static const AttribMask GL_SCISSOR_BIT = AttribMask::GL_SCISSOR_BIT;
static const glbinding::SharedBitfield<BufferAccessMask, MapBufferUsageMask> GL_MAP_FLUSH_EXPLICIT_BIT = BufferAccessMask::GL_MAP_FLUSH_EXPLICIT_BIT;
static const PathRenderingMaskNV GL_FONT_UNITS_PER_EM_BIT_NV = PathRenderingMaskNV::GL_FONT_UNITS_PER_EM_BIT_NV;
static const VertexHintsMaskPGI GL_MAT_AMBIENT_BIT_PGI = VertexHintsMaskPGI::GL_MAT_AMBIENT_BIT_PGI;
static const glbinding::SharedBitfield<BufferAccessMask, MapBufferUsageMask> GL_MAP_UNSYNCHRONIZED_BIT = BufferAccessMask::GL_MAP_UNSYNCHRONIZED_BIT;
static const PathRenderingMaskNV GL_FONT_ASCENDER_BIT_NV = PathRenderingMaskNV::GL_FONT_ASCENDER_BIT_NV;
static const VertexHintsMaskPGI GL_MAT_AMBIENT_AND_DIFFUSE_BIT_PGI = VertexHintsMaskPGI::GL_MAT_AMBIENT_AND_DIFFUSE_BIT_PGI;
static const glbinding::SharedBitfield<BufferAccessMask, BufferStorageMask, MapBufferUsageMask> GL_MAP_PERSISTENT_BIT = BufferAccessMask::GL_MAP_PERSISTENT_BIT;
static const PathRenderingMaskNV GL_FONT_DESCENDER_BIT_NV = PathRenderingMaskNV::GL_FONT_DESCENDER_BIT_NV;
static const VertexHintsMaskPGI GL_MAT_DIFFUSE_BIT_PGI = VertexHintsMaskPGI::GL_MAT_DIFFUSE_BIT_PGI;
static const glbinding::SharedBitfield<BufferAccessMask, BufferStorageMask, MapBufferUsageMask> GL_MAP_COHERENT_BIT = BufferAccessMask::GL_MAP_COHERENT_BIT;
static const PathRenderingMaskNV GL_FONT_HEIGHT_BIT_NV = PathRenderingMaskNV::GL_FONT_HEIGHT_BIT_NV;
static const VertexHintsMaskPGI GL_MAT_EMISSION_BIT_PGI = VertexHintsMaskPGI::GL_MAT_EMISSION_BIT_PGI;
static const glbinding::SharedBitfield<PathFontStyle, PathRenderingMaskNV> GL_BOLD_BIT_NV = PathFontStyle::GL_BOLD_BIT_NV;
static const PathRenderingMaskNV GL_GLYPH_WIDTH_BIT_NV = PathRenderingMaskNV::GL_GLYPH_WIDTH_BIT_NV;
static const glbinding::SharedBitfield<BufferStorageMask, MapBufferUsageMask> GL_DYNAMIC_STORAGE_BIT = BufferStorageMask::GL_DYNAMIC_STORAGE_BIT;
static const PathRenderingMaskNV GL_FONT_MAX_ADVANCE_WIDTH_BIT_NV = PathRenderingMaskNV::GL_FONT_MAX_ADVANCE_WIDTH_BIT_NV;
static const VertexHintsMaskPGI GL_MAT_COLOR_INDEXES_BIT_PGI = VertexHintsMaskPGI::GL_MAT_COLOR_INDEXES_BIT_PGI;
static const PathRenderingMaskNV GL_GLYPH_HEIGHT_BIT_NV = PathRenderingMaskNV::GL_GLYPH_HEIGHT_BIT_NV;
static const glbinding::SharedBitfield<PathFontStyle, PathRenderingMaskNV> GL_ITALIC_BIT_NV = PathFontStyle::GL_ITALIC_BIT_NV;
static const glbinding::SharedBitfield<BufferStorageMask, MapBufferUsageMask> GL_CLIENT_STORAGE_BIT = BufferStorageMask::GL_CLIENT_STORAGE_BIT;
static const PathRenderingMaskNV GL_FONT_MAX_ADVANCE_HEIGHT_BIT_NV = PathRenderingMaskNV::GL_FONT_MAX_ADVANCE_HEIGHT_BIT_NV;
static const VertexHintsMaskPGI GL_MAT_SHININESS_BIT_PGI = VertexHintsMaskPGI::GL_MAT_SHININESS_BIT_PGI;
static const PathRenderingMaskNV GL_GLYPH_HORIZONTAL_BEARING_X_BIT_NV = PathRenderingMaskNV::GL_GLYPH_HORIZONTAL_BEARING_X_BIT_NV;
static const MapBufferUsageMask GL_SPARSE_STORAGE_BIT_ARB = MapBufferUsageMask::GL_SPARSE_STORAGE_BIT_ARB;
static const PathRenderingMaskNV GL_FONT_UNDERLINE_POSITION_BIT_NV = PathRenderingMaskNV::GL_FONT_UNDERLINE_POSITION_BIT_NV;
static const VertexHintsMaskPGI GL_MAT_SPECULAR_BIT_PGI = VertexHintsMaskPGI::GL_MAT_SPECULAR_BIT_PGI;
static const PathRenderingMaskNV GL_GLYPH_HORIZONTAL_BEARING_Y_BIT_NV = PathRenderingMaskNV::GL_GLYPH_HORIZONTAL_BEARING_Y_BIT_NV;
static const PathRenderingMaskNV GL_FONT_UNDERLINE_THICKNESS_BIT_NV = PathRenderingMaskNV::GL_FONT_UNDERLINE_THICKNESS_BIT_NV;
static const VertexHintsMaskPGI GL_NORMAL_BIT_PGI = VertexHintsMaskPGI::GL_NORMAL_BIT_PGI;
static const PathRenderingMaskNV GL_GLYPH_HORIZONTAL_BEARING_ADVANCE_BIT_NV = PathRenderingMaskNV::GL_GLYPH_HORIZONTAL_BEARING_ADVANCE_BIT_NV;
static const PathRenderingMaskNV GL_GLYPH_HAS_KERNING_BIT_NV = PathRenderingMaskNV::GL_GLYPH_HAS_KERNING_BIT_NV;
static const PathRenderingMaskNV GL_FONT_HAS_KERNING_BIT_NV = PathRenderingMaskNV::GL_FONT_HAS_KERNING_BIT_NV;
static const VertexHintsMaskPGI GL_TEXCOORD1_BIT_PGI = VertexHintsMaskPGI::GL_TEXCOORD1_BIT_PGI;
static const PathRenderingMaskNV GL_GLYPH_VERTICAL_BEARING_X_BIT_NV = PathRenderingMaskNV::GL_GLYPH_VERTICAL_BEARING_X_BIT_NV;
static const PathRenderingMaskNV GL_FONT_NUM_GLYPH_INDICES_BIT_NV = PathRenderingMaskNV::GL_FONT_NUM_GLYPH_INDICES_BIT_NV;
static const AttribMask GL_MULTISAMPLE_BIT = AttribMask::GL_MULTISAMPLE_BIT;
static const AttribMask GL_MULTISAMPLE_BIT_3DFX = AttribMask::GL_MULTISAMPLE_BIT_3DFX;
static const AttribMask GL_MULTISAMPLE_BIT_ARB = AttribMask::GL_MULTISAMPLE_BIT_ARB;
static const AttribMask GL_MULTISAMPLE_BIT_EXT = AttribMask::GL_MULTISAMPLE_BIT_EXT;
static const VertexHintsMaskPGI GL_TEXCOORD2_BIT_PGI = VertexHintsMaskPGI::GL_TEXCOORD2_BIT_PGI;
static const PathRenderingMaskNV GL_GLYPH_VERTICAL_BEARING_Y_BIT_NV = PathRenderingMaskNV::GL_GLYPH_VERTICAL_BEARING_Y_BIT_NV;
static const VertexHintsMaskPGI GL_TEXCOORD3_BIT_PGI = VertexHintsMaskPGI::GL_TEXCOORD3_BIT_PGI;
static const PathRenderingMaskNV GL_GLYPH_VERTICAL_BEARING_ADVANCE_BIT_NV = PathRenderingMaskNV::GL_GLYPH_VERTICAL_BEARING_ADVANCE_BIT_NV;
static const VertexHintsMaskPGI GL_TEXCOORD4_BIT_PGI = VertexHintsMaskPGI::GL_TEXCOORD4_BIT_PGI;
static const AttribMask GL_ALL_ATTRIB_BITS = AttribMask::GL_ALL_ATTRIB_BITS;
static const MemoryBarrierMask GL_ALL_BARRIER_BITS = MemoryBarrierMask::GL_ALL_BARRIER_BITS;
static const MemoryBarrierMask GL_ALL_BARRIER_BITS_EXT = MemoryBarrierMask::GL_ALL_BARRIER_BITS_EXT;
static const UseProgramStageMask GL_ALL_SHADER_BITS = UseProgramStageMask::GL_ALL_SHADER_BITS;
static const ClientAttribMask GL_CLIENT_ALL_ATTRIB_BITS = ClientAttribMask::GL_CLIENT_ALL_ATTRIB_BITS;

} // namespace gl
