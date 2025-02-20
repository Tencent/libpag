#pragma once

#include <string>

#include <unordered_map>
#include <vector>
#include <set>

#include <glbinding/gl/types.h>
#include <glbinding/gl/extension.h>


namespace glbinding
{

class Version;

extern const std::unordered_map<std::string, gl::GLbitfield> Meta_BitfieldsByString;
extern const std::unordered_map<gl::GLbitfield, std::string> Meta_StringsByBitfield;

extern const std::unordered_map<std::string, gl::GLboolean> Meta_BooleansByString;
extern const std::unordered_map<gl::GLboolean, std::string> Meta_StringsByBoolean;

extern const std::unordered_map<std::string, gl::GLenum> Meta_EnumsByString;
extern const std::unordered_map<gl::GLenum, std::string> Meta_StringsByEnum;

extern const std::unordered_map<std::string, gl::GLextension> Meta_ExtensionsByString;
extern const std::unordered_map<gl::GLextension, std::string> Meta_StringsByExtension;

extern const std::unordered_map<std::string, std::set<gl::GLextension>> Meta_ExtensionsByFunctionString;
extern const std::unordered_map<gl::GLextension, std::set<std::string>> Meta_FunctionStringsByExtension;

extern const std::unordered_map<gl::GLextension, Version> Meta_ReqVersionsByExtension;

extern const std::unordered_map<gl::AttribMask, std::string> Meta_StringsByAttribMask;
extern const std::unordered_map<gl::ClearBufferMask, std::string> Meta_StringsByClearBufferMask;
extern const std::unordered_map<gl::ClientAttribMask, std::string> Meta_StringsByClientAttribMask;
extern const std::unordered_map<gl::ContextFlagMask, std::string> Meta_StringsByContextFlagMask;
extern const std::unordered_map<gl::ContextProfileMask, std::string> Meta_StringsByContextProfileMask;
extern const std::unordered_map<gl::FfdMaskSGIX, std::string> Meta_StringsByFfdMaskSGIX;
extern const std::unordered_map<gl::FragmentShaderColorModMaskATI, std::string> Meta_StringsByFragmentShaderColorModMaskATI;
extern const std::unordered_map<gl::FragmentShaderDestMaskATI, std::string> Meta_StringsByFragmentShaderDestMaskATI;
extern const std::unordered_map<gl::FragmentShaderDestModMaskATI, std::string> Meta_StringsByFragmentShaderDestModMaskATI;
extern const std::unordered_map<gl::MapBufferUsageMask, std::string> Meta_StringsByMapBufferUsageMask;
extern const std::unordered_map<gl::MemoryBarrierMask, std::string> Meta_StringsByMemoryBarrierMask;
extern const std::unordered_map<gl::PathRenderingMaskNV, std::string> Meta_StringsByPathRenderingMaskNV;
extern const std::unordered_map<gl::PerformanceQueryCapsMaskINTEL, std::string> Meta_StringsByPerformanceQueryCapsMaskINTEL;
extern const std::unordered_map<gl::SyncObjectMask, std::string> Meta_StringsBySyncObjectMask;
extern const std::unordered_map<gl::TextureStorageMaskAMD, std::string> Meta_StringsByTextureStorageMaskAMD;
extern const std::unordered_map<gl::UseProgramStageMask, std::string> Meta_StringsByUseProgramStageMask;
extern const std::unordered_map<gl::VertexHintsMaskPGI, std::string> Meta_StringsByVertexHintsMaskPGI;
extern const std::unordered_map<gl::UnusedMask, std::string> Meta_StringsByUnusedMask;
extern const std::unordered_map<gl::BufferAccessMask, std::string> Meta_StringsByBufferAccessMask;
extern const std::unordered_map<gl::BufferStorageMask, std::string> Meta_StringsByBufferStorageMask;
extern const std::unordered_map<gl::PathFontStyle, std::string> Meta_StringsByPathFontStyle;

} // namespace glbinding
