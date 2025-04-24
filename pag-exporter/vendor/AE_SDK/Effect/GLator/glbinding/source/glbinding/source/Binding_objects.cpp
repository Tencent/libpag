#include <glbinding/Binding.h>

#include <glbinding/gl/bitfield.h>
#include <glbinding/gl/boolean.h>
#include <glbinding/gl/enum.h>
#include <glbinding/gl/values.h>


using namespace gl; // ToDo: multiple APIs?

namespace glbinding 
{

/*
Binding::iterator Binding::begin()
{
	return iterator(&Accum);
}

Binding::iterator Binding::end()
{
	return iterator(&WriteMaskEXT);
}

Binding::const_iterator Binding::begin() const
{
	return const_iterator(&Accum);
}

Binding::const_iterator Binding::end() const
{
    return const_iterator(&WriteMaskEXT);
}
*/

Function<void, GLenum, GLfloat> Binding::Accum("glAccum");
Function<void, GLenum, GLfixed> Binding::AccumxOES("glAccumxOES");
Function<void, GLuint> Binding::ActiveProgramEXT("glActiveProgramEXT");
Function<void, GLuint, GLuint> Binding::ActiveShaderProgram("glActiveShaderProgram");
Function<void, GLenum> Binding::ActiveStencilFaceEXT("glActiveStencilFaceEXT");
Function<void, GLenum> Binding::ActiveTexture("glActiveTexture");
Function<void, GLenum> Binding::ActiveTextureARB("glActiveTextureARB");
Function<void, GLuint, const GLchar *> Binding::ActiveVaryingNV("glActiveVaryingNV");
Function<void, GLenum, GLuint, GLuint, GLuint, GLuint, GLuint> Binding::AlphaFragmentOp1ATI("glAlphaFragmentOp1ATI");
Function<void, GLenum, GLuint, GLuint, GLuint, GLuint, GLuint, GLuint, GLuint, GLuint> Binding::AlphaFragmentOp2ATI("glAlphaFragmentOp2ATI");
Function<void, GLenum, GLuint, GLuint, GLuint, GLuint, GLuint, GLuint, GLuint, GLuint, GLuint, GLuint, GLuint> Binding::AlphaFragmentOp3ATI("glAlphaFragmentOp3ATI");
Function<void, GLenum, GLfloat> Binding::AlphaFunc("glAlphaFunc");
Function<void, GLenum, GLfixed> Binding::AlphaFuncxOES("glAlphaFuncxOES");
Function<void> Binding::ApplyFramebufferAttachmentCMAAINTEL("glApplyFramebufferAttachmentCMAAINTEL");
Function<void, GLenum> Binding::ApplyTextureEXT("glApplyTextureEXT");
Function<GLboolean, GLsizei, const GLuint *, GLboolean *> Binding::AreProgramsResidentNV("glAreProgramsResidentNV");
Function<GLboolean, GLsizei, const GLuint *, GLboolean *> Binding::AreTexturesResident("glAreTexturesResident");
Function<GLboolean, GLsizei, const GLuint *, GLboolean *> Binding::AreTexturesResidentEXT("glAreTexturesResidentEXT");
Function<void, GLint> Binding::ArrayElement("glArrayElement");
Function<void, GLint> Binding::ArrayElementEXT("glArrayElementEXT");
Function<void, GLenum, GLint, GLenum, GLsizei, GLuint, GLuint> Binding::ArrayObjectATI("glArrayObjectATI");
Function<void, GLuint> Binding::AsyncMarkerSGIX("glAsyncMarkerSGIX");
Function<void, GLhandleARB, GLhandleARB> Binding::AttachObjectARB("glAttachObjectARB");
Function<void, GLuint, GLuint> Binding::AttachShader("glAttachShader");
Function<void, GLenum> Binding::Begin("glBegin");
Function<void, GLuint, GLenum> Binding::BeginConditionalRender("glBeginConditionalRender");
Function<void, GLuint, GLenum> Binding::BeginConditionalRenderNV("glBeginConditionalRenderNV");
Function<void, GLuint> Binding::BeginConditionalRenderNVX("glBeginConditionalRenderNVX");
Function<void> Binding::BeginFragmentShaderATI("glBeginFragmentShaderATI");
Function<void, GLuint> Binding::BeginOcclusionQueryNV("glBeginOcclusionQueryNV");
Function<void, GLuint> Binding::BeginPerfMonitorAMD("glBeginPerfMonitorAMD");
Function<void, GLuint> Binding::BeginPerfQueryINTEL("glBeginPerfQueryINTEL");
Function<void, GLenum, GLuint> Binding::BeginQuery("glBeginQuery");
Function<void, GLenum, GLuint> Binding::BeginQueryARB("glBeginQueryARB");
Function<void, GLenum, GLuint, GLuint> Binding::BeginQueryIndexed("glBeginQueryIndexed");
Function<void, GLenum> Binding::BeginTransformFeedback("glBeginTransformFeedback");
Function<void, GLenum> Binding::BeginTransformFeedbackEXT("glBeginTransformFeedbackEXT");
Function<void, GLenum> Binding::BeginTransformFeedbackNV("glBeginTransformFeedbackNV");
Function<void> Binding::BeginVertexShaderEXT("glBeginVertexShaderEXT");
Function<void, GLuint> Binding::BeginVideoCaptureNV("glBeginVideoCaptureNV");
Function<void, GLuint, GLuint, const GLchar *> Binding::BindAttribLocation("glBindAttribLocation");
Function<void, GLhandleARB, GLuint, const GLcharARB *> Binding::BindAttribLocationARB("glBindAttribLocationARB");
Function<void, GLenum, GLuint> Binding::BindBuffer("glBindBuffer");
Function<void, GLenum, GLuint> Binding::BindBufferARB("glBindBufferARB");
Function<void, GLenum, GLuint, GLuint> Binding::BindBufferBase("glBindBufferBase");
Function<void, GLenum, GLuint, GLuint> Binding::BindBufferBaseEXT("glBindBufferBaseEXT");
Function<void, GLenum, GLuint, GLuint> Binding::BindBufferBaseNV("glBindBufferBaseNV");
Function<void, GLenum, GLuint, GLuint, GLintptr> Binding::BindBufferOffsetEXT("glBindBufferOffsetEXT");
Function<void, GLenum, GLuint, GLuint, GLintptr> Binding::BindBufferOffsetNV("glBindBufferOffsetNV");
Function<void, GLenum, GLuint, GLuint, GLintptr, GLsizeiptr> Binding::BindBufferRange("glBindBufferRange");
Function<void, GLenum, GLuint, GLuint, GLintptr, GLsizeiptr> Binding::BindBufferRangeEXT("glBindBufferRangeEXT");
Function<void, GLenum, GLuint, GLuint, GLintptr, GLsizeiptr> Binding::BindBufferRangeNV("glBindBufferRangeNV");
Function<void, GLenum, GLuint, GLsizei, const GLuint *> Binding::BindBuffersBase("glBindBuffersBase");
Function<void, GLenum, GLuint, GLsizei, const GLuint *, const GLintptr *, const GLsizeiptr *> Binding::BindBuffersRange("glBindBuffersRange");
Function<void, GLuint, GLuint, const GLchar *> Binding::BindFragDataLocation("glBindFragDataLocation");
Function<void, GLuint, GLuint, const GLchar *> Binding::BindFragDataLocationEXT("glBindFragDataLocationEXT");
Function<void, GLuint, GLuint, GLuint, const GLchar *> Binding::BindFragDataLocationIndexed("glBindFragDataLocationIndexed");
Function<void, GLuint> Binding::BindFragmentShaderATI("glBindFragmentShaderATI");
Function<void, GLenum, GLuint> Binding::BindFramebuffer("glBindFramebuffer");
Function<void, GLenum, GLuint> Binding::BindFramebufferEXT("glBindFramebufferEXT");
Function<void, GLuint, GLuint, GLint, GLboolean, GLint, GLenum, GLenum> Binding::BindImageTexture("glBindImageTexture");
Function<void, GLuint, GLuint, GLint, GLboolean, GLint, GLenum, GLint> Binding::BindImageTextureEXT("glBindImageTextureEXT");
Function<void, GLuint, GLsizei, const GLuint *> Binding::BindImageTextures("glBindImageTextures");
Function<GLuint, GLenum, GLenum> Binding::BindLightParameterEXT("glBindLightParameterEXT");
Function<GLuint, GLenum, GLenum> Binding::BindMaterialParameterEXT("glBindMaterialParameterEXT");
Function<void, GLenum, GLenum, GLuint> Binding::BindMultiTextureEXT("glBindMultiTextureEXT");
Function<GLuint, GLenum> Binding::BindParameterEXT("glBindParameterEXT");
Function<void, GLenum, GLuint> Binding::BindProgramARB("glBindProgramARB");
Function<void, GLenum, GLuint> Binding::BindProgramNV("glBindProgramNV");
Function<void, GLuint> Binding::BindProgramPipeline("glBindProgramPipeline");
Function<void, GLenum, GLuint> Binding::BindRenderbuffer("glBindRenderbuffer");
Function<void, GLenum, GLuint> Binding::BindRenderbufferEXT("glBindRenderbufferEXT");
Function<void, GLuint, GLuint> Binding::BindSampler("glBindSampler");
Function<void, GLuint, GLsizei, const GLuint *> Binding::BindSamplers("glBindSamplers");
Function<GLuint, GLenum, GLenum, GLenum> Binding::BindTexGenParameterEXT("glBindTexGenParameterEXT");
Function<void, GLenum, GLuint> Binding::BindTexture("glBindTexture");
Function<void, GLenum, GLuint> Binding::BindTextureEXT("glBindTextureEXT");
Function<void, GLuint, GLuint> Binding::BindTextureUnit("glBindTextureUnit");
Function<GLuint, GLenum, GLenum> Binding::BindTextureUnitParameterEXT("glBindTextureUnitParameterEXT");
Function<void, GLuint, GLsizei, const GLuint *> Binding::BindTextures("glBindTextures");
Function<void, GLenum, GLuint> Binding::BindTransformFeedback("glBindTransformFeedback");
Function<void, GLenum, GLuint> Binding::BindTransformFeedbackNV("glBindTransformFeedbackNV");
Function<void, GLuint> Binding::BindVertexArray("glBindVertexArray");
Function<void, GLuint> Binding::BindVertexArrayAPPLE("glBindVertexArrayAPPLE");
Function<void, GLuint, GLuint, GLintptr, GLsizei> Binding::BindVertexBuffer("glBindVertexBuffer");
Function<void, GLuint, GLsizei, const GLuint *, const GLintptr *, const GLsizei *> Binding::BindVertexBuffers("glBindVertexBuffers");
Function<void, GLuint> Binding::BindVertexShaderEXT("glBindVertexShaderEXT");
Function<void, GLuint, GLuint, GLenum, GLintptrARB> Binding::BindVideoCaptureStreamBufferNV("glBindVideoCaptureStreamBufferNV");
Function<void, GLuint, GLuint, GLenum, GLenum, GLuint> Binding::BindVideoCaptureStreamTextureNV("glBindVideoCaptureStreamTextureNV");
Function<void, GLbyte, GLbyte, GLbyte> Binding::Binormal3bEXT("glBinormal3bEXT");
Function<void, const GLbyte *> Binding::Binormal3bvEXT("glBinormal3bvEXT");
Function<void, GLdouble, GLdouble, GLdouble> Binding::Binormal3dEXT("glBinormal3dEXT");
Function<void, const GLdouble *> Binding::Binormal3dvEXT("glBinormal3dvEXT");
Function<void, GLfloat, GLfloat, GLfloat> Binding::Binormal3fEXT("glBinormal3fEXT");
Function<void, const GLfloat *> Binding::Binormal3fvEXT("glBinormal3fvEXT");
Function<void, GLint, GLint, GLint> Binding::Binormal3iEXT("glBinormal3iEXT");
Function<void, const GLint *> Binding::Binormal3ivEXT("glBinormal3ivEXT");
Function<void, GLshort, GLshort, GLshort> Binding::Binormal3sEXT("glBinormal3sEXT");
Function<void, const GLshort *> Binding::Binormal3svEXT("glBinormal3svEXT");
Function<void, GLenum, GLsizei, const void *> Binding::BinormalPointerEXT("glBinormalPointerEXT");
Function<void, GLsizei, GLsizei, GLfloat, GLfloat, GLfloat, GLfloat, const GLubyte *> Binding::Bitmap("glBitmap");
Function<void, GLsizei, GLsizei, GLfixed, GLfixed, GLfixed, GLfixed, const GLubyte *> Binding::BitmapxOES("glBitmapxOES");
Function<void> Binding::BlendBarrierKHR("glBlendBarrierKHR");
Function<void> Binding::BlendBarrierNV("glBlendBarrierNV");
Function<void, GLfloat, GLfloat, GLfloat, GLfloat> Binding::BlendColor("glBlendColor");
Function<void, GLfloat, GLfloat, GLfloat, GLfloat> Binding::BlendColorEXT("glBlendColorEXT");
Function<void, GLfixed, GLfixed, GLfixed, GLfixed> Binding::BlendColorxOES("glBlendColorxOES");
Function<void, GLenum> Binding::BlendEquation("glBlendEquation");
Function<void, GLenum> Binding::BlendEquationEXT("glBlendEquationEXT");
Function<void, GLuint, GLenum> Binding::BlendEquationIndexedAMD("glBlendEquationIndexedAMD");
Function<void, GLenum, GLenum> Binding::BlendEquationSeparate("glBlendEquationSeparate");
Function<void, GLenum, GLenum> Binding::BlendEquationSeparateEXT("glBlendEquationSeparateEXT");
Function<void, GLuint, GLenum, GLenum> Binding::BlendEquationSeparateIndexedAMD("glBlendEquationSeparateIndexedAMD");
Function<void, GLuint, GLenum, GLenum> Binding::BlendEquationSeparatei("glBlendEquationSeparatei");
Function<void, GLuint, GLenum, GLenum> Binding::BlendEquationSeparateiARB("glBlendEquationSeparateiARB");
Function<void, GLuint, GLenum> Binding::BlendEquationi("glBlendEquationi");
Function<void, GLuint, GLenum> Binding::BlendEquationiARB("glBlendEquationiARB");
Function<void, GLenum, GLenum> Binding::BlendFunc("glBlendFunc");
Function<void, GLuint, GLenum, GLenum> Binding::BlendFuncIndexedAMD("glBlendFuncIndexedAMD");
Function<void, GLenum, GLenum, GLenum, GLenum> Binding::BlendFuncSeparate("glBlendFuncSeparate");
Function<void, GLenum, GLenum, GLenum, GLenum> Binding::BlendFuncSeparateEXT("glBlendFuncSeparateEXT");
Function<void, GLenum, GLenum, GLenum, GLenum> Binding::BlendFuncSeparateINGR("glBlendFuncSeparateINGR");
Function<void, GLuint, GLenum, GLenum, GLenum, GLenum> Binding::BlendFuncSeparateIndexedAMD("glBlendFuncSeparateIndexedAMD");
Function<void, GLuint, GLenum, GLenum, GLenum, GLenum> Binding::BlendFuncSeparatei("glBlendFuncSeparatei");
Function<void, GLuint, GLenum, GLenum, GLenum, GLenum> Binding::BlendFuncSeparateiARB("glBlendFuncSeparateiARB");
Function<void, GLuint, GLenum, GLenum> Binding::BlendFunci("glBlendFunci");
Function<void, GLuint, GLenum, GLenum> Binding::BlendFunciARB("glBlendFunciARB");
Function<void, GLenum, GLint> Binding::BlendParameteriNV("glBlendParameteriNV");
Function<void, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, ClearBufferMask, GLenum> Binding::BlitFramebuffer("glBlitFramebuffer");
Function<void, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, ClearBufferMask, GLenum> Binding::BlitFramebufferEXT("glBlitFramebufferEXT");
Function<void, GLuint, GLuint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, ClearBufferMask, GLenum> Binding::BlitNamedFramebuffer("glBlitNamedFramebuffer");
Function<void, GLenum, GLuint, GLuint64EXT, GLsizeiptr> Binding::BufferAddressRangeNV("glBufferAddressRangeNV");
Function<void, GLenum, GLsizeiptr, const void *, GLenum> Binding::BufferData("glBufferData");
Function<void, GLenum, GLsizeiptrARB, const void *, GLenum> Binding::BufferDataARB("glBufferDataARB");
Function<void, GLenum, GLintptr, GLsizeiptr, GLboolean> Binding::BufferPageCommitmentARB("glBufferPageCommitmentARB");
Function<void, GLenum, GLenum, GLint> Binding::BufferParameteriAPPLE("glBufferParameteriAPPLE");
Function<void, GLenum, GLsizeiptr, const void *, BufferStorageMask> Binding::BufferStorage("glBufferStorage");
Function<void, GLenum, GLintptr, GLsizeiptr, const void *> Binding::BufferSubData("glBufferSubData");
Function<void, GLenum, GLintptrARB, GLsizeiptrARB, const void *> Binding::BufferSubDataARB("glBufferSubDataARB");
Function<void, GLuint> Binding::CallCommandListNV("glCallCommandListNV");
Function<void, GLuint> Binding::CallList("glCallList");
Function<void, GLsizei, GLenum, const void *> Binding::CallLists("glCallLists");
Function<GLenum, GLenum> Binding::CheckFramebufferStatus("glCheckFramebufferStatus");
Function<GLenum, GLenum> Binding::CheckFramebufferStatusEXT("glCheckFramebufferStatusEXT");
Function<GLenum, GLuint, GLenum> Binding::CheckNamedFramebufferStatus("glCheckNamedFramebufferStatus");
Function<GLenum, GLuint, GLenum> Binding::CheckNamedFramebufferStatusEXT("glCheckNamedFramebufferStatusEXT");
Function<void, GLenum, GLenum> Binding::ClampColor("glClampColor");
Function<void, GLenum, GLenum> Binding::ClampColorARB("glClampColorARB");
Function<void, ClearBufferMask> Binding::Clear("glClear");
Function<void, GLfloat, GLfloat, GLfloat, GLfloat> Binding::ClearAccum("glClearAccum");
Function<void, GLfixed, GLfixed, GLfixed, GLfixed> Binding::ClearAccumxOES("glClearAccumxOES");
Function<void, GLenum, GLenum, GLenum, GLenum, const void *> Binding::ClearBufferData("glClearBufferData");
Function<void, GLenum, GLenum, GLintptr, GLsizeiptr, GLenum, GLenum, const void *> Binding::ClearBufferSubData("glClearBufferSubData");
Function<void, GLenum, GLint, GLfloat, GLint> Binding::ClearBufferfi("glClearBufferfi");
Function<void, GLenum, GLint, const GLfloat *> Binding::ClearBufferfv("glClearBufferfv");
Function<void, GLenum, GLint, const GLint *> Binding::ClearBufferiv("glClearBufferiv");
Function<void, GLenum, GLint, const GLuint *> Binding::ClearBufferuiv("glClearBufferuiv");
Function<void, GLfloat, GLfloat, GLfloat, GLfloat> Binding::ClearColor("glClearColor");
Function<void, GLint, GLint, GLint, GLint> Binding::ClearColorIiEXT("glClearColorIiEXT");
Function<void, GLuint, GLuint, GLuint, GLuint> Binding::ClearColorIuiEXT("glClearColorIuiEXT");
Function<void, GLfixed, GLfixed, GLfixed, GLfixed> Binding::ClearColorxOES("glClearColorxOES");
Function<void, GLdouble> Binding::ClearDepth("glClearDepth");
Function<void, GLdouble> Binding::ClearDepthdNV("glClearDepthdNV");
Function<void, GLfloat> Binding::ClearDepthf("glClearDepthf");
Function<void, GLclampf> Binding::ClearDepthfOES("glClearDepthfOES");
Function<void, GLfixed> Binding::ClearDepthxOES("glClearDepthxOES");
Function<void, GLfloat> Binding::ClearIndex("glClearIndex");
Function<void, GLuint, GLenum, GLenum, GLenum, const void *> Binding::ClearNamedBufferData("glClearNamedBufferData");
Function<void, GLuint, GLenum, GLenum, GLenum, const void *> Binding::ClearNamedBufferDataEXT("glClearNamedBufferDataEXT");
Function<void, GLuint, GLenum, GLintptr, GLsizeiptr, GLenum, GLenum, const void *> Binding::ClearNamedBufferSubData("glClearNamedBufferSubData");
Function<void, GLuint, GLenum, GLsizeiptr, GLsizeiptr, GLenum, GLenum, const void *> Binding::ClearNamedBufferSubDataEXT("glClearNamedBufferSubDataEXT");
Function<void, GLuint, GLenum, const GLfloat, GLint> Binding::ClearNamedFramebufferfi("glClearNamedFramebufferfi");
Function<void, GLuint, GLenum, GLint, const GLfloat *> Binding::ClearNamedFramebufferfv("glClearNamedFramebufferfv");
Function<void, GLuint, GLenum, GLint, const GLint *> Binding::ClearNamedFramebufferiv("glClearNamedFramebufferiv");
Function<void, GLuint, GLenum, GLint, const GLuint *> Binding::ClearNamedFramebufferuiv("glClearNamedFramebufferuiv");
Function<void, GLint> Binding::ClearStencil("glClearStencil");
Function<void, GLuint, GLint, GLenum, GLenum, const void *> Binding::ClearTexImage("glClearTexImage");
Function<void, GLuint, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLenum, const void *> Binding::ClearTexSubImage("glClearTexSubImage");
Function<void, GLenum> Binding::ClientActiveTexture("glClientActiveTexture");
Function<void, GLenum> Binding::ClientActiveTextureARB("glClientActiveTextureARB");
Function<void, GLenum> Binding::ClientActiveVertexStreamATI("glClientActiveVertexStreamATI");
Function<void, ClientAttribMask> Binding::ClientAttribDefaultEXT("glClientAttribDefaultEXT");
Function<GLenum, GLsync, SyncObjectMask, GLuint64> Binding::ClientWaitSync("glClientWaitSync");
Function<void, GLenum, GLenum> Binding::ClipControl("glClipControl");
Function<void, GLenum, const GLdouble *> Binding::ClipPlane("glClipPlane");
Function<void, GLenum, const GLfloat *> Binding::ClipPlanefOES("glClipPlanefOES");
Function<void, GLenum, const GLfixed *> Binding::ClipPlanexOES("glClipPlanexOES");
Function<void, GLbyte, GLbyte, GLbyte> Binding::Color3b("glColor3b");
Function<void, const GLbyte *> Binding::Color3bv("glColor3bv");
Function<void, GLdouble, GLdouble, GLdouble> Binding::Color3d("glColor3d");
Function<void, const GLdouble *> Binding::Color3dv("glColor3dv");
Function<void, GLfloat, GLfloat, GLfloat> Binding::Color3f("glColor3f");
Function<void, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat> Binding::Color3fVertex3fSUN("glColor3fVertex3fSUN");
Function<void, const GLfloat *, const GLfloat *> Binding::Color3fVertex3fvSUN("glColor3fVertex3fvSUN");
Function<void, const GLfloat *> Binding::Color3fv("glColor3fv");
Function<void, GLhalfNV, GLhalfNV, GLhalfNV> Binding::Color3hNV("glColor3hNV");
Function<void, const GLhalfNV *> Binding::Color3hvNV("glColor3hvNV");
Function<void, GLint, GLint, GLint> Binding::Color3i("glColor3i");
Function<void, const GLint *> Binding::Color3iv("glColor3iv");
Function<void, GLshort, GLshort, GLshort> Binding::Color3s("glColor3s");
Function<void, const GLshort *> Binding::Color3sv("glColor3sv");
Function<void, GLubyte, GLubyte, GLubyte> Binding::Color3ub("glColor3ub");
Function<void, const GLubyte *> Binding::Color3ubv("glColor3ubv");
Function<void, GLuint, GLuint, GLuint> Binding::Color3ui("glColor3ui");
Function<void, const GLuint *> Binding::Color3uiv("glColor3uiv");
Function<void, GLushort, GLushort, GLushort> Binding::Color3us("glColor3us");
Function<void, const GLushort *> Binding::Color3usv("glColor3usv");
Function<void, GLfixed, GLfixed, GLfixed> Binding::Color3xOES("glColor3xOES");
Function<void, const GLfixed *> Binding::Color3xvOES("glColor3xvOES");
Function<void, GLbyte, GLbyte, GLbyte, GLbyte> Binding::Color4b("glColor4b");
Function<void, const GLbyte *> Binding::Color4bv("glColor4bv");
Function<void, GLdouble, GLdouble, GLdouble, GLdouble> Binding::Color4d("glColor4d");
Function<void, const GLdouble *> Binding::Color4dv("glColor4dv");
Function<void, GLfloat, GLfloat, GLfloat, GLfloat> Binding::Color4f("glColor4f");
Function<void, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat> Binding::Color4fNormal3fVertex3fSUN("glColor4fNormal3fVertex3fSUN");
Function<void, const GLfloat *, const GLfloat *, const GLfloat *> Binding::Color4fNormal3fVertex3fvSUN("glColor4fNormal3fVertex3fvSUN");
Function<void, const GLfloat *> Binding::Color4fv("glColor4fv");
Function<void, GLhalfNV, GLhalfNV, GLhalfNV, GLhalfNV> Binding::Color4hNV("glColor4hNV");
Function<void, const GLhalfNV *> Binding::Color4hvNV("glColor4hvNV");
Function<void, GLint, GLint, GLint, GLint> Binding::Color4i("glColor4i");
Function<void, const GLint *> Binding::Color4iv("glColor4iv");
Function<void, GLshort, GLshort, GLshort, GLshort> Binding::Color4s("glColor4s");
Function<void, const GLshort *> Binding::Color4sv("glColor4sv");
Function<void, GLubyte, GLubyte, GLubyte, GLubyte> Binding::Color4ub("glColor4ub");
Function<void, GLubyte, GLubyte, GLubyte, GLubyte, GLfloat, GLfloat> Binding::Color4ubVertex2fSUN("glColor4ubVertex2fSUN");
Function<void, const GLubyte *, const GLfloat *> Binding::Color4ubVertex2fvSUN("glColor4ubVertex2fvSUN");
Function<void, GLubyte, GLubyte, GLubyte, GLubyte, GLfloat, GLfloat, GLfloat> Binding::Color4ubVertex3fSUN("glColor4ubVertex3fSUN");
Function<void, const GLubyte *, const GLfloat *> Binding::Color4ubVertex3fvSUN("glColor4ubVertex3fvSUN");
Function<void, const GLubyte *> Binding::Color4ubv("glColor4ubv");
Function<void, GLuint, GLuint, GLuint, GLuint> Binding::Color4ui("glColor4ui");
Function<void, const GLuint *> Binding::Color4uiv("glColor4uiv");
Function<void, GLushort, GLushort, GLushort, GLushort> Binding::Color4us("glColor4us");
Function<void, const GLushort *> Binding::Color4usv("glColor4usv");
Function<void, GLfixed, GLfixed, GLfixed, GLfixed> Binding::Color4xOES("glColor4xOES");
Function<void, const GLfixed *> Binding::Color4xvOES("glColor4xvOES");
Function<void, GLint, GLenum, GLsizei> Binding::ColorFormatNV("glColorFormatNV");
Function<void, GLenum, GLuint, GLuint, GLuint, GLuint, GLuint, GLuint> Binding::ColorFragmentOp1ATI("glColorFragmentOp1ATI");
Function<void, GLenum, GLuint, GLuint, GLuint, GLuint, GLuint, GLuint, GLuint, GLuint, GLuint> Binding::ColorFragmentOp2ATI("glColorFragmentOp2ATI");
Function<void, GLenum, GLuint, GLuint, GLuint, GLuint, GLuint, GLuint, GLuint, GLuint, GLuint, GLuint, GLuint, GLuint> Binding::ColorFragmentOp3ATI("glColorFragmentOp3ATI");
Function<void, GLboolean, GLboolean, GLboolean, GLboolean> Binding::ColorMask("glColorMask");
Function<void, GLuint, GLboolean, GLboolean, GLboolean, GLboolean> Binding::ColorMaskIndexedEXT("glColorMaskIndexedEXT");
Function<void, GLuint, GLboolean, GLboolean, GLboolean, GLboolean> Binding::ColorMaski("glColorMaski");
Function<void, GLenum, GLenum> Binding::ColorMaterial("glColorMaterial");
Function<void, GLenum, GLuint> Binding::ColorP3ui("glColorP3ui");
Function<void, GLenum, const GLuint *> Binding::ColorP3uiv("glColorP3uiv");
Function<void, GLenum, GLuint> Binding::ColorP4ui("glColorP4ui");
Function<void, GLenum, const GLuint *> Binding::ColorP4uiv("glColorP4uiv");
Function<void, GLint, GLenum, GLsizei, const void *> Binding::ColorPointer("glColorPointer");
Function<void, GLint, GLenum, GLsizei, GLsizei, const void *> Binding::ColorPointerEXT("glColorPointerEXT");
Function<void, GLint, GLenum, GLint, const void **, GLint> Binding::ColorPointerListIBM("glColorPointerListIBM");
Function<void, GLint, GLenum, const void **> Binding::ColorPointervINTEL("glColorPointervINTEL");
Function<void, GLenum, GLsizei, GLsizei, GLenum, GLenum, const void *> Binding::ColorSubTable("glColorSubTable");
Function<void, GLenum, GLsizei, GLsizei, GLenum, GLenum, const void *> Binding::ColorSubTableEXT("glColorSubTableEXT");
Function<void, GLenum, GLenum, GLsizei, GLenum, GLenum, const void *> Binding::ColorTable("glColorTable");
Function<void, GLenum, GLenum, GLsizei, GLenum, GLenum, const void *> Binding::ColorTableEXT("glColorTableEXT");
Function<void, GLenum, GLenum, const GLfloat *> Binding::ColorTableParameterfv("glColorTableParameterfv");
Function<void, GLenum, GLenum, const GLfloat *> Binding::ColorTableParameterfvSGI("glColorTableParameterfvSGI");
Function<void, GLenum, GLenum, const GLint *> Binding::ColorTableParameteriv("glColorTableParameteriv");
Function<void, GLenum, GLenum, const GLint *> Binding::ColorTableParameterivSGI("glColorTableParameterivSGI");
Function<void, GLenum, GLenum, GLsizei, GLenum, GLenum, const void *> Binding::ColorTableSGI("glColorTableSGI");
Function<void, GLenum, GLenum, GLenum, GLenum, GLenum, GLenum> Binding::CombinerInputNV("glCombinerInputNV");
Function<void, GLenum, GLenum, GLenum, GLenum, GLenum, GLenum, GLenum, GLboolean, GLboolean, GLboolean> Binding::CombinerOutputNV("glCombinerOutputNV");
Function<void, GLenum, GLfloat> Binding::CombinerParameterfNV("glCombinerParameterfNV");
Function<void, GLenum, const GLfloat *> Binding::CombinerParameterfvNV("glCombinerParameterfvNV");
Function<void, GLenum, GLint> Binding::CombinerParameteriNV("glCombinerParameteriNV");
Function<void, GLenum, const GLint *> Binding::CombinerParameterivNV("glCombinerParameterivNV");
Function<void, GLenum, GLenum, const GLfloat *> Binding::CombinerStageParameterfvNV("glCombinerStageParameterfvNV");
Function<void, GLuint, GLuint> Binding::CommandListSegmentsNV("glCommandListSegmentsNV");
Function<void, GLuint> Binding::CompileCommandListNV("glCompileCommandListNV");
Function<void, GLuint> Binding::CompileShader("glCompileShader");
Function<void, GLhandleARB> Binding::CompileShaderARB("glCompileShaderARB");
Function<void, GLuint, GLsizei, const GLchar *const*, const GLint *> Binding::CompileShaderIncludeARB("glCompileShaderIncludeARB");
Function<void, GLenum, GLenum, GLint, GLenum, GLsizei, GLint, GLsizei, const void *> Binding::CompressedMultiTexImage1DEXT("glCompressedMultiTexImage1DEXT");
Function<void, GLenum, GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const void *> Binding::CompressedMultiTexImage2DEXT("glCompressedMultiTexImage2DEXT");
Function<void, GLenum, GLenum, GLint, GLenum, GLsizei, GLsizei, GLsizei, GLint, GLsizei, const void *> Binding::CompressedMultiTexImage3DEXT("glCompressedMultiTexImage3DEXT");
Function<void, GLenum, GLenum, GLint, GLint, GLsizei, GLenum, GLsizei, const void *> Binding::CompressedMultiTexSubImage1DEXT("glCompressedMultiTexSubImage1DEXT");
Function<void, GLenum, GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLsizei, const void *> Binding::CompressedMultiTexSubImage2DEXT("glCompressedMultiTexSubImage2DEXT");
Function<void, GLenum, GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLsizei, const void *> Binding::CompressedMultiTexSubImage3DEXT("glCompressedMultiTexSubImage3DEXT");
Function<void, GLenum, GLint, GLenum, GLsizei, GLint, GLsizei, const void *> Binding::CompressedTexImage1D("glCompressedTexImage1D");
Function<void, GLenum, GLint, GLenum, GLsizei, GLint, GLsizei, const void *> Binding::CompressedTexImage1DARB("glCompressedTexImage1DARB");
Function<void, GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const void *> Binding::CompressedTexImage2D("glCompressedTexImage2D");
Function<void, GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const void *> Binding::CompressedTexImage2DARB("glCompressedTexImage2DARB");
Function<void, GLenum, GLint, GLenum, GLsizei, GLsizei, GLsizei, GLint, GLsizei, const void *> Binding::CompressedTexImage3D("glCompressedTexImage3D");
Function<void, GLenum, GLint, GLenum, GLsizei, GLsizei, GLsizei, GLint, GLsizei, const void *> Binding::CompressedTexImage3DARB("glCompressedTexImage3DARB");
Function<void, GLenum, GLint, GLint, GLsizei, GLenum, GLsizei, const void *> Binding::CompressedTexSubImage1D("glCompressedTexSubImage1D");
Function<void, GLenum, GLint, GLint, GLsizei, GLenum, GLsizei, const void *> Binding::CompressedTexSubImage1DARB("glCompressedTexSubImage1DARB");
Function<void, GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLsizei, const void *> Binding::CompressedTexSubImage2D("glCompressedTexSubImage2D");
Function<void, GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLsizei, const void *> Binding::CompressedTexSubImage2DARB("glCompressedTexSubImage2DARB");
Function<void, GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLsizei, const void *> Binding::CompressedTexSubImage3D("glCompressedTexSubImage3D");
Function<void, GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLsizei, const void *> Binding::CompressedTexSubImage3DARB("glCompressedTexSubImage3DARB");
Function<void, GLuint, GLenum, GLint, GLenum, GLsizei, GLint, GLsizei, const void *> Binding::CompressedTextureImage1DEXT("glCompressedTextureImage1DEXT");
Function<void, GLuint, GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const void *> Binding::CompressedTextureImage2DEXT("glCompressedTextureImage2DEXT");
Function<void, GLuint, GLenum, GLint, GLenum, GLsizei, GLsizei, GLsizei, GLint, GLsizei, const void *> Binding::CompressedTextureImage3DEXT("glCompressedTextureImage3DEXT");
Function<void, GLuint, GLint, GLint, GLsizei, GLenum, GLsizei, const void *> Binding::CompressedTextureSubImage1D("glCompressedTextureSubImage1D");
Function<void, GLuint, GLenum, GLint, GLint, GLsizei, GLenum, GLsizei, const void *> Binding::CompressedTextureSubImage1DEXT("glCompressedTextureSubImage1DEXT");
Function<void, GLuint, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLsizei, const void *> Binding::CompressedTextureSubImage2D("glCompressedTextureSubImage2D");
Function<void, GLuint, GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLsizei, const void *> Binding::CompressedTextureSubImage2DEXT("glCompressedTextureSubImage2DEXT");
Function<void, GLuint, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLsizei, const void *> Binding::CompressedTextureSubImage3D("glCompressedTextureSubImage3D");
Function<void, GLuint, GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLsizei, const void *> Binding::CompressedTextureSubImage3DEXT("glCompressedTextureSubImage3DEXT");
Function<void, GLenum, GLfloat> Binding::ConservativeRasterParameterfNV("glConservativeRasterParameterfNV");
Function<void, GLenum, GLenum, GLsizei, GLenum, GLenum, const void *> Binding::ConvolutionFilter1D("glConvolutionFilter1D");
Function<void, GLenum, GLenum, GLsizei, GLenum, GLenum, const void *> Binding::ConvolutionFilter1DEXT("glConvolutionFilter1DEXT");
Function<void, GLenum, GLenum, GLsizei, GLsizei, GLenum, GLenum, const void *> Binding::ConvolutionFilter2D("glConvolutionFilter2D");
Function<void, GLenum, GLenum, GLsizei, GLsizei, GLenum, GLenum, const void *> Binding::ConvolutionFilter2DEXT("glConvolutionFilter2DEXT");
Function<void, GLenum, GLenum, GLfloat> Binding::ConvolutionParameterf("glConvolutionParameterf");
Function<void, GLenum, GLenum, GLfloat> Binding::ConvolutionParameterfEXT("glConvolutionParameterfEXT");
Function<void, GLenum, GLenum, const GLfloat *> Binding::ConvolutionParameterfv("glConvolutionParameterfv");
Function<void, GLenum, GLenum, const GLfloat *> Binding::ConvolutionParameterfvEXT("glConvolutionParameterfvEXT");
Function<void, GLenum, GLenum, GLint> Binding::ConvolutionParameteri("glConvolutionParameteri");
Function<void, GLenum, GLenum, GLint> Binding::ConvolutionParameteriEXT("glConvolutionParameteriEXT");
Function<void, GLenum, GLenum, const GLint *> Binding::ConvolutionParameteriv("glConvolutionParameteriv");
Function<void, GLenum, GLenum, const GLint *> Binding::ConvolutionParameterivEXT("glConvolutionParameterivEXT");
Function<void, GLenum, GLenum, GLfixed> Binding::ConvolutionParameterxOES("glConvolutionParameterxOES");
Function<void, GLenum, GLenum, const GLfixed *> Binding::ConvolutionParameterxvOES("glConvolutionParameterxvOES");
Function<void, GLenum, GLenum, GLintptr, GLintptr, GLsizeiptr> Binding::CopyBufferSubData("glCopyBufferSubData");
Function<void, GLenum, GLsizei, GLint, GLint, GLsizei> Binding::CopyColorSubTable("glCopyColorSubTable");
Function<void, GLenum, GLsizei, GLint, GLint, GLsizei> Binding::CopyColorSubTableEXT("glCopyColorSubTableEXT");
Function<void, GLenum, GLenum, GLint, GLint, GLsizei> Binding::CopyColorTable("glCopyColorTable");
Function<void, GLenum, GLenum, GLint, GLint, GLsizei> Binding::CopyColorTableSGI("glCopyColorTableSGI");
Function<void, GLenum, GLenum, GLint, GLint, GLsizei> Binding::CopyConvolutionFilter1D("glCopyConvolutionFilter1D");
Function<void, GLenum, GLenum, GLint, GLint, GLsizei> Binding::CopyConvolutionFilter1DEXT("glCopyConvolutionFilter1DEXT");
Function<void, GLenum, GLenum, GLint, GLint, GLsizei, GLsizei> Binding::CopyConvolutionFilter2D("glCopyConvolutionFilter2D");
Function<void, GLenum, GLenum, GLint, GLint, GLsizei, GLsizei> Binding::CopyConvolutionFilter2DEXT("glCopyConvolutionFilter2DEXT");
Function<void, GLuint, GLenum, GLint, GLint, GLint, GLint, GLuint, GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei> Binding::CopyImageSubData("glCopyImageSubData");
Function<void, GLuint, GLenum, GLint, GLint, GLint, GLint, GLuint, GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei> Binding::CopyImageSubDataNV("glCopyImageSubDataNV");
Function<void, GLenum, GLenum, GLint, GLenum, GLint, GLint, GLsizei, GLint> Binding::CopyMultiTexImage1DEXT("glCopyMultiTexImage1DEXT");
Function<void, GLenum, GLenum, GLint, GLenum, GLint, GLint, GLsizei, GLsizei, GLint> Binding::CopyMultiTexImage2DEXT("glCopyMultiTexImage2DEXT");
Function<void, GLenum, GLenum, GLint, GLint, GLint, GLint, GLsizei> Binding::CopyMultiTexSubImage1DEXT("glCopyMultiTexSubImage1DEXT");
Function<void, GLenum, GLenum, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei> Binding::CopyMultiTexSubImage2DEXT("glCopyMultiTexSubImage2DEXT");
Function<void, GLenum, GLenum, GLint, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei> Binding::CopyMultiTexSubImage3DEXT("glCopyMultiTexSubImage3DEXT");
Function<void, GLuint, GLuint, GLintptr, GLintptr, GLsizeiptr> Binding::CopyNamedBufferSubData("glCopyNamedBufferSubData");
Function<void, GLuint, GLuint> Binding::CopyPathNV("glCopyPathNV");
Function<void, GLint, GLint, GLsizei, GLsizei, GLenum> Binding::CopyPixels("glCopyPixels");
Function<void, GLenum, GLint, GLenum, GLint, GLint, GLsizei, GLint> Binding::CopyTexImage1D("glCopyTexImage1D");
Function<void, GLenum, GLint, GLenum, GLint, GLint, GLsizei, GLint> Binding::CopyTexImage1DEXT("glCopyTexImage1DEXT");
Function<void, GLenum, GLint, GLenum, GLint, GLint, GLsizei, GLsizei, GLint> Binding::CopyTexImage2D("glCopyTexImage2D");
Function<void, GLenum, GLint, GLenum, GLint, GLint, GLsizei, GLsizei, GLint> Binding::CopyTexImage2DEXT("glCopyTexImage2DEXT");
Function<void, GLenum, GLint, GLint, GLint, GLint, GLsizei> Binding::CopyTexSubImage1D("glCopyTexSubImage1D");
Function<void, GLenum, GLint, GLint, GLint, GLint, GLsizei> Binding::CopyTexSubImage1DEXT("glCopyTexSubImage1DEXT");
Function<void, GLenum, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei> Binding::CopyTexSubImage2D("glCopyTexSubImage2D");
Function<void, GLenum, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei> Binding::CopyTexSubImage2DEXT("glCopyTexSubImage2DEXT");
Function<void, GLenum, GLint, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei> Binding::CopyTexSubImage3D("glCopyTexSubImage3D");
Function<void, GLenum, GLint, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei> Binding::CopyTexSubImage3DEXT("glCopyTexSubImage3DEXT");
Function<void, GLuint, GLenum, GLint, GLenum, GLint, GLint, GLsizei, GLint> Binding::CopyTextureImage1DEXT("glCopyTextureImage1DEXT");
Function<void, GLuint, GLenum, GLint, GLenum, GLint, GLint, GLsizei, GLsizei, GLint> Binding::CopyTextureImage2DEXT("glCopyTextureImage2DEXT");
Function<void, GLuint, GLint, GLint, GLint, GLint, GLsizei> Binding::CopyTextureSubImage1D("glCopyTextureSubImage1D");
Function<void, GLuint, GLenum, GLint, GLint, GLint, GLint, GLsizei> Binding::CopyTextureSubImage1DEXT("glCopyTextureSubImage1DEXT");
Function<void, GLuint, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei> Binding::CopyTextureSubImage2D("glCopyTextureSubImage2D");
Function<void, GLuint, GLenum, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei> Binding::CopyTextureSubImage2DEXT("glCopyTextureSubImage2DEXT");
Function<void, GLuint, GLint, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei> Binding::CopyTextureSubImage3D("glCopyTextureSubImage3D");
Function<void, GLuint, GLenum, GLint, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei> Binding::CopyTextureSubImage3DEXT("glCopyTextureSubImage3DEXT");
Function<void, GLsizei, GLenum, const void *, GLuint, GLenum, GLenum, const GLfloat *> Binding::CoverFillPathInstancedNV("glCoverFillPathInstancedNV");
Function<void, GLuint, GLenum> Binding::CoverFillPathNV("glCoverFillPathNV");
Function<void, GLsizei, GLenum, const void *, GLuint, GLenum, GLenum, const GLfloat *> Binding::CoverStrokePathInstancedNV("glCoverStrokePathInstancedNV");
Function<void, GLuint, GLenum> Binding::CoverStrokePathNV("glCoverStrokePathNV");
Function<void, GLenum> Binding::CoverageModulationNV("glCoverageModulationNV");
Function<void, GLsizei, const GLfloat *> Binding::CoverageModulationTableNV("glCoverageModulationTableNV");
Function<void, GLsizei, GLuint *> Binding::CreateBuffers("glCreateBuffers");
Function<void, GLsizei, GLuint *> Binding::CreateCommandListsNV("glCreateCommandListsNV");
Function<void, GLsizei, GLuint *> Binding::CreateFramebuffers("glCreateFramebuffers");
Function<void, GLuint, GLuint *> Binding::CreatePerfQueryINTEL("glCreatePerfQueryINTEL");
Function<GLuint> Binding::CreateProgram("glCreateProgram");
Function<GLhandleARB> Binding::CreateProgramObjectARB("glCreateProgramObjectARB");
Function<void, GLsizei, GLuint *> Binding::CreateProgramPipelines("glCreateProgramPipelines");
Function<void, GLenum, GLsizei, GLuint *> Binding::CreateQueries("glCreateQueries");
Function<void, GLsizei, GLuint *> Binding::CreateRenderbuffers("glCreateRenderbuffers");
Function<void, GLsizei, GLuint *> Binding::CreateSamplers("glCreateSamplers");
Function<GLuint, GLenum> Binding::CreateShader("glCreateShader");
Function<GLhandleARB, GLenum> Binding::CreateShaderObjectARB("glCreateShaderObjectARB");
Function<GLuint, GLenum, const GLchar *> Binding::CreateShaderProgramEXT("glCreateShaderProgramEXT");
Function<GLuint, GLenum, GLsizei, const GLchar *const*> Binding::CreateShaderProgramv("glCreateShaderProgramv");
Function<void, GLsizei, GLuint *> Binding::CreateStatesNV("glCreateStatesNV");
Function<GLsync, _cl_context *, _cl_event *, UnusedMask> Binding::CreateSyncFromCLeventARB("glCreateSyncFromCLeventARB");
Function<void, GLenum, GLsizei, GLuint *> Binding::CreateTextures("glCreateTextures");
Function<void, GLsizei, GLuint *> Binding::CreateTransformFeedbacks("glCreateTransformFeedbacks");
Function<void, GLsizei, GLuint *> Binding::CreateVertexArrays("glCreateVertexArrays");
Function<void, GLenum> Binding::CullFace("glCullFace");
Function<void, GLenum, GLdouble *> Binding::CullParameterdvEXT("glCullParameterdvEXT");
Function<void, GLenum, GLfloat *> Binding::CullParameterfvEXT("glCullParameterfvEXT");
Function<void, GLint> Binding::CurrentPaletteMatrixARB("glCurrentPaletteMatrixARB");
Function<void, GLDEBUGPROC, const void *> Binding::DebugMessageCallback("glDebugMessageCallback");
Function<void, GLDEBUGPROCAMD, void *> Binding::DebugMessageCallbackAMD("glDebugMessageCallbackAMD");
Function<void, GLDEBUGPROCARB, const void *> Binding::DebugMessageCallbackARB("glDebugMessageCallbackARB");
Function<void, GLenum, GLenum, GLenum, GLsizei, const GLuint *, GLboolean> Binding::DebugMessageControl("glDebugMessageControl");
Function<void, GLenum, GLenum, GLenum, GLsizei, const GLuint *, GLboolean> Binding::DebugMessageControlARB("glDebugMessageControlARB");
Function<void, GLenum, GLenum, GLsizei, const GLuint *, GLboolean> Binding::DebugMessageEnableAMD("glDebugMessageEnableAMD");
Function<void, GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar *> Binding::DebugMessageInsert("glDebugMessageInsert");
Function<void, GLenum, GLenum, GLuint, GLsizei, const GLchar *> Binding::DebugMessageInsertAMD("glDebugMessageInsertAMD");
Function<void, GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar *> Binding::DebugMessageInsertARB("glDebugMessageInsertARB");
Function<void, FfdMaskSGIX> Binding::DeformSGIX("glDeformSGIX");
Function<void, GLenum, GLdouble, GLdouble, GLint, GLint, GLdouble, GLdouble, GLint, GLint, GLdouble, GLdouble, GLint, GLint, const GLdouble *> Binding::DeformationMap3dSGIX("glDeformationMap3dSGIX");
Function<void, GLenum, GLfloat, GLfloat, GLint, GLint, GLfloat, GLfloat, GLint, GLint, GLfloat, GLfloat, GLint, GLint, const GLfloat *> Binding::DeformationMap3fSGIX("glDeformationMap3fSGIX");
Function<void, GLuint, GLsizei> Binding::DeleteAsyncMarkersSGIX("glDeleteAsyncMarkersSGIX");
Function<void, GLsizei, const GLuint *> Binding::DeleteBuffers("glDeleteBuffers");
Function<void, GLsizei, const GLuint *> Binding::DeleteBuffersARB("glDeleteBuffersARB");
Function<void, GLsizei, const GLuint *> Binding::DeleteCommandListsNV("glDeleteCommandListsNV");
Function<void, GLsizei, const GLuint *> Binding::DeleteFencesAPPLE("glDeleteFencesAPPLE");
Function<void, GLsizei, const GLuint *> Binding::DeleteFencesNV("glDeleteFencesNV");
Function<void, GLuint> Binding::DeleteFragmentShaderATI("glDeleteFragmentShaderATI");
Function<void, GLsizei, const GLuint *> Binding::DeleteFramebuffers("glDeleteFramebuffers");
Function<void, GLsizei, const GLuint *> Binding::DeleteFramebuffersEXT("glDeleteFramebuffersEXT");
Function<void, GLuint, GLsizei> Binding::DeleteLists("glDeleteLists");
Function<void, GLint, const GLchar *> Binding::DeleteNamedStringARB("glDeleteNamedStringARB");
Function<void, GLenum, GLuint, const GLuint *> Binding::DeleteNamesAMD("glDeleteNamesAMD");
Function<void, GLhandleARB> Binding::DeleteObjectARB("glDeleteObjectARB");
Function<void, GLsizei, const GLuint *> Binding::DeleteOcclusionQueriesNV("glDeleteOcclusionQueriesNV");
Function<void, GLuint, GLsizei> Binding::DeletePathsNV("glDeletePathsNV");
Function<void, GLsizei, GLuint *> Binding::DeletePerfMonitorsAMD("glDeletePerfMonitorsAMD");
Function<void, GLuint> Binding::DeletePerfQueryINTEL("glDeletePerfQueryINTEL");
Function<void, GLuint> Binding::DeleteProgram("glDeleteProgram");
Function<void, GLsizei, const GLuint *> Binding::DeleteProgramPipelines("glDeleteProgramPipelines");
Function<void, GLsizei, const GLuint *> Binding::DeleteProgramsARB("glDeleteProgramsARB");
Function<void, GLsizei, const GLuint *> Binding::DeleteProgramsNV("glDeleteProgramsNV");
Function<void, GLsizei, const GLuint *> Binding::DeleteQueries("glDeleteQueries");
Function<void, GLsizei, const GLuint *> Binding::DeleteQueriesARB("glDeleteQueriesARB");
Function<void, GLsizei, const GLuint *> Binding::DeleteRenderbuffers("glDeleteRenderbuffers");
Function<void, GLsizei, const GLuint *> Binding::DeleteRenderbuffersEXT("glDeleteRenderbuffersEXT");
Function<void, GLsizei, const GLuint *> Binding::DeleteSamplers("glDeleteSamplers");
Function<void, GLuint> Binding::DeleteShader("glDeleteShader");
Function<void, GLsizei, const GLuint *> Binding::DeleteStatesNV("glDeleteStatesNV");
Function<void, GLsync> Binding::DeleteSync("glDeleteSync");
Function<void, GLsizei, const GLuint *> Binding::DeleteTextures("glDeleteTextures");
Function<void, GLsizei, const GLuint *> Binding::DeleteTexturesEXT("glDeleteTexturesEXT");
Function<void, GLsizei, const GLuint *> Binding::DeleteTransformFeedbacks("glDeleteTransformFeedbacks");
Function<void, GLsizei, const GLuint *> Binding::DeleteTransformFeedbacksNV("glDeleteTransformFeedbacksNV");
Function<void, GLsizei, const GLuint *> Binding::DeleteVertexArrays("glDeleteVertexArrays");
Function<void, GLsizei, const GLuint *> Binding::DeleteVertexArraysAPPLE("glDeleteVertexArraysAPPLE");
Function<void, GLuint> Binding::DeleteVertexShaderEXT("glDeleteVertexShaderEXT");
Function<void, GLclampd, GLclampd> Binding::DepthBoundsEXT("glDepthBoundsEXT");
Function<void, GLdouble, GLdouble> Binding::DepthBoundsdNV("glDepthBoundsdNV");
Function<void, GLenum> Binding::DepthFunc("glDepthFunc");
Function<void, GLboolean> Binding::DepthMask("glDepthMask");
Function<void, GLdouble, GLdouble> Binding::DepthRange("glDepthRange");
Function<void, GLuint, GLsizei, const GLdouble *> Binding::DepthRangeArrayv("glDepthRangeArrayv");
Function<void, GLuint, GLdouble, GLdouble> Binding::DepthRangeIndexed("glDepthRangeIndexed");
Function<void, GLdouble, GLdouble> Binding::DepthRangedNV("glDepthRangedNV");
Function<void, GLfloat, GLfloat> Binding::DepthRangef("glDepthRangef");
Function<void, GLclampf, GLclampf> Binding::DepthRangefOES("glDepthRangefOES");
Function<void, GLfixed, GLfixed> Binding::DepthRangexOES("glDepthRangexOES");
Function<void, GLhandleARB, GLhandleARB> Binding::DetachObjectARB("glDetachObjectARB");
Function<void, GLuint, GLuint> Binding::DetachShader("glDetachShader");
Function<void, GLenum, GLsizei, const GLfloat *> Binding::DetailTexFuncSGIS("glDetailTexFuncSGIS");
Function<void, GLenum> Binding::Disable("glDisable");
Function<void, GLenum> Binding::DisableClientState("glDisableClientState");
Function<void, GLenum, GLuint> Binding::DisableClientStateIndexedEXT("glDisableClientStateIndexedEXT");
Function<void, GLenum, GLuint> Binding::DisableClientStateiEXT("glDisableClientStateiEXT");
Function<void, GLenum, GLuint> Binding::DisableIndexedEXT("glDisableIndexedEXT");
Function<void, GLuint> Binding::DisableVariantClientStateEXT("glDisableVariantClientStateEXT");
Function<void, GLuint, GLuint> Binding::DisableVertexArrayAttrib("glDisableVertexArrayAttrib");
Function<void, GLuint, GLuint> Binding::DisableVertexArrayAttribEXT("glDisableVertexArrayAttribEXT");
Function<void, GLuint, GLenum> Binding::DisableVertexArrayEXT("glDisableVertexArrayEXT");
Function<void, GLuint, GLenum> Binding::DisableVertexAttribAPPLE("glDisableVertexAttribAPPLE");
Function<void, GLuint> Binding::DisableVertexAttribArray("glDisableVertexAttribArray");
Function<void, GLuint> Binding::DisableVertexAttribArrayARB("glDisableVertexAttribArrayARB");
Function<void, GLenum, GLuint> Binding::Disablei("glDisablei");
Function<void, GLuint, GLuint, GLuint> Binding::DispatchCompute("glDispatchCompute");
Function<void, GLuint, GLuint, GLuint, GLuint, GLuint, GLuint> Binding::DispatchComputeGroupSizeARB("glDispatchComputeGroupSizeARB");
Function<void, GLintptr> Binding::DispatchComputeIndirect("glDispatchComputeIndirect");
Function<void, GLenum, GLint, GLsizei> Binding::DrawArrays("glDrawArrays");
Function<void, GLenum, GLint, GLsizei> Binding::DrawArraysEXT("glDrawArraysEXT");
Function<void, GLenum, const void *> Binding::DrawArraysIndirect("glDrawArraysIndirect");
Function<void, GLenum, GLint, GLsizei, GLsizei> Binding::DrawArraysInstanced("glDrawArraysInstanced");
Function<void, GLenum, GLint, GLsizei, GLsizei> Binding::DrawArraysInstancedARB("glDrawArraysInstancedARB");
Function<void, GLenum, GLint, GLsizei, GLsizei, GLuint> Binding::DrawArraysInstancedBaseInstance("glDrawArraysInstancedBaseInstance");
Function<void, GLenum, GLint, GLsizei, GLsizei> Binding::DrawArraysInstancedEXT("glDrawArraysInstancedEXT");
Function<void, GLenum> Binding::DrawBuffer("glDrawBuffer");
Function<void, GLsizei, const GLenum *> Binding::DrawBuffers("glDrawBuffers");
Function<void, GLsizei, const GLenum *> Binding::DrawBuffersARB("glDrawBuffersARB");
Function<void, GLsizei, const GLenum *> Binding::DrawBuffersATI("glDrawBuffersATI");
Function<void, GLenum, const GLuint64 *, const GLsizei *, GLuint> Binding::DrawCommandsAddressNV("glDrawCommandsAddressNV");
Function<void, GLenum, GLuint, const GLintptr *, const GLsizei *, GLuint> Binding::DrawCommandsNV("glDrawCommandsNV");
Function<void, const GLuint64 *, const GLsizei *, const GLuint *, const GLuint *, GLuint> Binding::DrawCommandsStatesAddressNV("glDrawCommandsStatesAddressNV");
Function<void, GLuint, const GLintptr *, const GLsizei *, const GLuint *, const GLuint *, GLuint> Binding::DrawCommandsStatesNV("glDrawCommandsStatesNV");
Function<void, GLenum, GLint, GLsizei> Binding::DrawElementArrayAPPLE("glDrawElementArrayAPPLE");
Function<void, GLenum, GLsizei> Binding::DrawElementArrayATI("glDrawElementArrayATI");
Function<void, GLenum, GLsizei, GLenum, const void *> Binding::DrawElements("glDrawElements");
Function<void, GLenum, GLsizei, GLenum, const void *, GLint> Binding::DrawElementsBaseVertex("glDrawElementsBaseVertex");
Function<void, GLenum, GLenum, const void *> Binding::DrawElementsIndirect("glDrawElementsIndirect");
Function<void, GLenum, GLsizei, GLenum, const void *, GLsizei> Binding::DrawElementsInstanced("glDrawElementsInstanced");
Function<void, GLenum, GLsizei, GLenum, const void *, GLsizei> Binding::DrawElementsInstancedARB("glDrawElementsInstancedARB");
Function<void, GLenum, GLsizei, GLenum, const void *, GLsizei, GLuint> Binding::DrawElementsInstancedBaseInstance("glDrawElementsInstancedBaseInstance");
Function<void, GLenum, GLsizei, GLenum, const void *, GLsizei, GLint> Binding::DrawElementsInstancedBaseVertex("glDrawElementsInstancedBaseVertex");
Function<void, GLenum, GLsizei, GLenum, const void *, GLsizei, GLint, GLuint> Binding::DrawElementsInstancedBaseVertexBaseInstance("glDrawElementsInstancedBaseVertexBaseInstance");
Function<void, GLenum, GLsizei, GLenum, const void *, GLsizei> Binding::DrawElementsInstancedEXT("glDrawElementsInstancedEXT");
Function<void, GLenum, GLint, GLsizei, GLsizei> Binding::DrawMeshArraysSUN("glDrawMeshArraysSUN");
Function<void, GLsizei, GLsizei, GLenum, GLenum, const void *> Binding::DrawPixels("glDrawPixels");
Function<void, GLenum, GLuint, GLuint, GLint, GLsizei> Binding::DrawRangeElementArrayAPPLE("glDrawRangeElementArrayAPPLE");
Function<void, GLenum, GLuint, GLuint, GLsizei> Binding::DrawRangeElementArrayATI("glDrawRangeElementArrayATI");
Function<void, GLenum, GLuint, GLuint, GLsizei, GLenum, const void *> Binding::DrawRangeElements("glDrawRangeElements");
Function<void, GLenum, GLuint, GLuint, GLsizei, GLenum, const void *, GLint> Binding::DrawRangeElementsBaseVertex("glDrawRangeElementsBaseVertex");
Function<void, GLenum, GLuint, GLuint, GLsizei, GLenum, const void *> Binding::DrawRangeElementsEXT("glDrawRangeElementsEXT");
Function<void, GLuint, GLuint, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat> Binding::DrawTextureNV("glDrawTextureNV");
Function<void, GLenum, GLuint> Binding::DrawTransformFeedback("glDrawTransformFeedback");
Function<void, GLenum, GLuint, GLsizei> Binding::DrawTransformFeedbackInstanced("glDrawTransformFeedbackInstanced");
Function<void, GLenum, GLuint> Binding::DrawTransformFeedbackNV("glDrawTransformFeedbackNV");
Function<void, GLenum, GLuint, GLuint> Binding::DrawTransformFeedbackStream("glDrawTransformFeedbackStream");
Function<void, GLenum, GLuint, GLuint, GLsizei> Binding::DrawTransformFeedbackStreamInstanced("glDrawTransformFeedbackStreamInstanced");
Function<void, GLboolean> Binding::EdgeFlag("glEdgeFlag");
Function<void, GLsizei> Binding::EdgeFlagFormatNV("glEdgeFlagFormatNV");
Function<void, GLsizei, const void *> Binding::EdgeFlagPointer("glEdgeFlagPointer");
Function<void, GLsizei, GLsizei, const GLboolean *> Binding::EdgeFlagPointerEXT("glEdgeFlagPointerEXT");
Function<void, GLint, const GLboolean **, GLint> Binding::EdgeFlagPointerListIBM("glEdgeFlagPointerListIBM");
Function<void, const GLboolean *> Binding::EdgeFlagv("glEdgeFlagv");
Function<void, GLenum, const void *> Binding::ElementPointerAPPLE("glElementPointerAPPLE");
Function<void, GLenum, const void *> Binding::ElementPointerATI("glElementPointerATI");
Function<void, GLenum> Binding::Enable("glEnable");
Function<void, GLenum> Binding::EnableClientState("glEnableClientState");
Function<void, GLenum, GLuint> Binding::EnableClientStateIndexedEXT("glEnableClientStateIndexedEXT");
Function<void, GLenum, GLuint> Binding::EnableClientStateiEXT("glEnableClientStateiEXT");
Function<void, GLenum, GLuint> Binding::EnableIndexedEXT("glEnableIndexedEXT");
Function<void, GLuint> Binding::EnableVariantClientStateEXT("glEnableVariantClientStateEXT");
Function<void, GLuint, GLuint> Binding::EnableVertexArrayAttrib("glEnableVertexArrayAttrib");
Function<void, GLuint, GLuint> Binding::EnableVertexArrayAttribEXT("glEnableVertexArrayAttribEXT");
Function<void, GLuint, GLenum> Binding::EnableVertexArrayEXT("glEnableVertexArrayEXT");
Function<void, GLuint, GLenum> Binding::EnableVertexAttribAPPLE("glEnableVertexAttribAPPLE");
Function<void, GLuint> Binding::EnableVertexAttribArray("glEnableVertexAttribArray");
Function<void, GLuint> Binding::EnableVertexAttribArrayARB("glEnableVertexAttribArrayARB");
Function<void, GLenum, GLuint> Binding::Enablei("glEnablei");
Function<void> Binding::End("glEnd");
Function<void> Binding::EndConditionalRender("glEndConditionalRender");
Function<void> Binding::EndConditionalRenderNV("glEndConditionalRenderNV");
Function<void> Binding::EndConditionalRenderNVX("glEndConditionalRenderNVX");
Function<void> Binding::EndFragmentShaderATI("glEndFragmentShaderATI");
Function<void> Binding::EndList("glEndList");
Function<void> Binding::EndOcclusionQueryNV("glEndOcclusionQueryNV");
Function<void, GLuint> Binding::EndPerfMonitorAMD("glEndPerfMonitorAMD");
Function<void, GLuint> Binding::EndPerfQueryINTEL("glEndPerfQueryINTEL");
Function<void, GLenum> Binding::EndQuery("glEndQuery");
Function<void, GLenum> Binding::EndQueryARB("glEndQueryARB");
Function<void, GLenum, GLuint> Binding::EndQueryIndexed("glEndQueryIndexed");
Function<void> Binding::EndTransformFeedback("glEndTransformFeedback");
Function<void> Binding::EndTransformFeedbackEXT("glEndTransformFeedbackEXT");
Function<void> Binding::EndTransformFeedbackNV("glEndTransformFeedbackNV");
Function<void> Binding::EndVertexShaderEXT("glEndVertexShaderEXT");
Function<void, GLuint> Binding::EndVideoCaptureNV("glEndVideoCaptureNV");
Function<void, GLdouble> Binding::EvalCoord1d("glEvalCoord1d");
Function<void, const GLdouble *> Binding::EvalCoord1dv("glEvalCoord1dv");
Function<void, GLfloat> Binding::EvalCoord1f("glEvalCoord1f");
Function<void, const GLfloat *> Binding::EvalCoord1fv("glEvalCoord1fv");
Function<void, GLfixed> Binding::EvalCoord1xOES("glEvalCoord1xOES");
Function<void, const GLfixed *> Binding::EvalCoord1xvOES("glEvalCoord1xvOES");
Function<void, GLdouble, GLdouble> Binding::EvalCoord2d("glEvalCoord2d");
Function<void, const GLdouble *> Binding::EvalCoord2dv("glEvalCoord2dv");
Function<void, GLfloat, GLfloat> Binding::EvalCoord2f("glEvalCoord2f");
Function<void, const GLfloat *> Binding::EvalCoord2fv("glEvalCoord2fv");
Function<void, GLfixed, GLfixed> Binding::EvalCoord2xOES("glEvalCoord2xOES");
Function<void, const GLfixed *> Binding::EvalCoord2xvOES("glEvalCoord2xvOES");
Function<void, GLenum, GLenum> Binding::EvalMapsNV("glEvalMapsNV");
Function<void, GLenum, GLint, GLint> Binding::EvalMesh1("glEvalMesh1");
Function<void, GLenum, GLint, GLint, GLint, GLint> Binding::EvalMesh2("glEvalMesh2");
Function<void, GLint> Binding::EvalPoint1("glEvalPoint1");
Function<void, GLint, GLint> Binding::EvalPoint2("glEvalPoint2");
Function<void> Binding::EvaluateDepthValuesARB("glEvaluateDepthValuesARB");
Function<void, GLenum, GLuint, const GLfloat *> Binding::ExecuteProgramNV("glExecuteProgramNV");
Function<void, GLuint, GLuint, GLuint> Binding::ExtractComponentEXT("glExtractComponentEXT");
Function<void, GLsizei, GLenum, GLfloat *> Binding::FeedbackBuffer("glFeedbackBuffer");
Function<void, GLsizei, GLenum, const GLfixed *> Binding::FeedbackBufferxOES("glFeedbackBufferxOES");
Function<GLsync, GLenum, UnusedMask> Binding::FenceSync("glFenceSync");
Function<void, GLenum, GLenum, GLenum, GLenum> Binding::FinalCombinerInputNV("glFinalCombinerInputNV");
Function<void> Binding::Finish("glFinish");
Function<GLint, GLuint *> Binding::FinishAsyncSGIX("glFinishAsyncSGIX");
Function<void, GLuint> Binding::FinishFenceAPPLE("glFinishFenceAPPLE");
Function<void, GLuint> Binding::FinishFenceNV("glFinishFenceNV");
Function<void, GLenum, GLint> Binding::FinishObjectAPPLE("glFinishObjectAPPLE");
Function<void> Binding::FinishTextureSUNX("glFinishTextureSUNX");
Function<void> Binding::Flush("glFlush");
Function<void, GLenum, GLintptr, GLsizeiptr> Binding::FlushMappedBufferRange("glFlushMappedBufferRange");
Function<void, GLenum, GLintptr, GLsizeiptr> Binding::FlushMappedBufferRangeAPPLE("glFlushMappedBufferRangeAPPLE");
Function<void, GLuint, GLintptr, GLsizeiptr> Binding::FlushMappedNamedBufferRange("glFlushMappedNamedBufferRange");
Function<void, GLuint, GLintptr, GLsizeiptr> Binding::FlushMappedNamedBufferRangeEXT("glFlushMappedNamedBufferRangeEXT");
Function<void, GLenum> Binding::FlushPixelDataRangeNV("glFlushPixelDataRangeNV");
Function<void> Binding::FlushRasterSGIX("glFlushRasterSGIX");
Function<void, GLenum> Binding::FlushStaticDataIBM("glFlushStaticDataIBM");
Function<void, GLsizei, void *> Binding::FlushVertexArrayRangeAPPLE("glFlushVertexArrayRangeAPPLE");
Function<void> Binding::FlushVertexArrayRangeNV("glFlushVertexArrayRangeNV");
Function<void, GLenum, GLsizei> Binding::FogCoordFormatNV("glFogCoordFormatNV");
Function<void, GLenum, GLsizei, const void *> Binding::FogCoordPointer("glFogCoordPointer");
Function<void, GLenum, GLsizei, const void *> Binding::FogCoordPointerEXT("glFogCoordPointerEXT");
Function<void, GLenum, GLint, const void **, GLint> Binding::FogCoordPointerListIBM("glFogCoordPointerListIBM");
Function<void, GLdouble> Binding::FogCoordd("glFogCoordd");
Function<void, GLdouble> Binding::FogCoorddEXT("glFogCoorddEXT");
Function<void, const GLdouble *> Binding::FogCoorddv("glFogCoorddv");
Function<void, const GLdouble *> Binding::FogCoorddvEXT("glFogCoorddvEXT");
Function<void, GLfloat> Binding::FogCoordf("glFogCoordf");
Function<void, GLfloat> Binding::FogCoordfEXT("glFogCoordfEXT");
Function<void, const GLfloat *> Binding::FogCoordfv("glFogCoordfv");
Function<void, const GLfloat *> Binding::FogCoordfvEXT("glFogCoordfvEXT");
Function<void, GLhalfNV> Binding::FogCoordhNV("glFogCoordhNV");
Function<void, const GLhalfNV *> Binding::FogCoordhvNV("glFogCoordhvNV");
Function<void, GLsizei, const GLfloat *> Binding::FogFuncSGIS("glFogFuncSGIS");
Function<void, GLenum, GLfloat> Binding::Fogf("glFogf");
Function<void, GLenum, const GLfloat *> Binding::Fogfv("glFogfv");
Function<void, GLenum, GLint> Binding::Fogi("glFogi");
Function<void, GLenum, const GLint *> Binding::Fogiv("glFogiv");
Function<void, GLenum, GLfixed> Binding::FogxOES("glFogxOES");
Function<void, GLenum, const GLfixed *> Binding::FogxvOES("glFogxvOES");
Function<void, GLenum, GLenum> Binding::FragmentColorMaterialSGIX("glFragmentColorMaterialSGIX");
Function<void, GLuint> Binding::FragmentCoverageColorNV("glFragmentCoverageColorNV");
Function<void, GLenum, GLfloat> Binding::FragmentLightModelfSGIX("glFragmentLightModelfSGIX");
Function<void, GLenum, const GLfloat *> Binding::FragmentLightModelfvSGIX("glFragmentLightModelfvSGIX");
Function<void, GLenum, GLint> Binding::FragmentLightModeliSGIX("glFragmentLightModeliSGIX");
Function<void, GLenum, const GLint *> Binding::FragmentLightModelivSGIX("glFragmentLightModelivSGIX");
Function<void, GLenum, GLenum, GLfloat> Binding::FragmentLightfSGIX("glFragmentLightfSGIX");
Function<void, GLenum, GLenum, const GLfloat *> Binding::FragmentLightfvSGIX("glFragmentLightfvSGIX");
Function<void, GLenum, GLenum, GLint> Binding::FragmentLightiSGIX("glFragmentLightiSGIX");
Function<void, GLenum, GLenum, const GLint *> Binding::FragmentLightivSGIX("glFragmentLightivSGIX");
Function<void, GLenum, GLenum, GLfloat> Binding::FragmentMaterialfSGIX("glFragmentMaterialfSGIX");
Function<void, GLenum, GLenum, const GLfloat *> Binding::FragmentMaterialfvSGIX("glFragmentMaterialfvSGIX");
Function<void, GLenum, GLenum, GLint> Binding::FragmentMaterialiSGIX("glFragmentMaterialiSGIX");
Function<void, GLenum, GLenum, const GLint *> Binding::FragmentMaterialivSGIX("glFragmentMaterialivSGIX");
Function<void> Binding::FrameTerminatorGREMEDY("glFrameTerminatorGREMEDY");
Function<void, GLint> Binding::FrameZoomSGIX("glFrameZoomSGIX");
Function<void, GLuint, GLenum> Binding::FramebufferDrawBufferEXT("glFramebufferDrawBufferEXT");
Function<void, GLuint, GLsizei, const GLenum *> Binding::FramebufferDrawBuffersEXT("glFramebufferDrawBuffersEXT");
Function<void, GLenum, GLenum, GLint> Binding::FramebufferParameteri("glFramebufferParameteri");
Function<void, GLuint, GLenum> Binding::FramebufferReadBufferEXT("glFramebufferReadBufferEXT");
Function<void, GLenum, GLenum, GLenum, GLuint> Binding::FramebufferRenderbuffer("glFramebufferRenderbuffer");
Function<void, GLenum, GLenum, GLenum, GLuint> Binding::FramebufferRenderbufferEXT("glFramebufferRenderbufferEXT");
Function<void, GLenum, GLuint, GLsizei, const GLfloat *> Binding::FramebufferSampleLocationsfvARB("glFramebufferSampleLocationsfvARB");
Function<void, GLenum, GLuint, GLsizei, const GLfloat *> Binding::FramebufferSampleLocationsfvNV("glFramebufferSampleLocationsfvNV");
Function<void, GLenum, GLenum, GLuint, GLint> Binding::FramebufferTexture("glFramebufferTexture");
Function<void, GLenum, GLenum, GLenum, GLuint, GLint> Binding::FramebufferTexture1D("glFramebufferTexture1D");
Function<void, GLenum, GLenum, GLenum, GLuint, GLint> Binding::FramebufferTexture1DEXT("glFramebufferTexture1DEXT");
Function<void, GLenum, GLenum, GLenum, GLuint, GLint> Binding::FramebufferTexture2D("glFramebufferTexture2D");
Function<void, GLenum, GLenum, GLenum, GLuint, GLint> Binding::FramebufferTexture2DEXT("glFramebufferTexture2DEXT");
Function<void, GLenum, GLenum, GLenum, GLuint, GLint, GLint> Binding::FramebufferTexture3D("glFramebufferTexture3D");
Function<void, GLenum, GLenum, GLenum, GLuint, GLint, GLint> Binding::FramebufferTexture3DEXT("glFramebufferTexture3DEXT");
Function<void, GLenum, GLenum, GLuint, GLint> Binding::FramebufferTextureARB("glFramebufferTextureARB");
Function<void, GLenum, GLenum, GLuint, GLint> Binding::FramebufferTextureEXT("glFramebufferTextureEXT");
Function<void, GLenum, GLenum, GLuint, GLint, GLenum> Binding::FramebufferTextureFaceARB("glFramebufferTextureFaceARB");
Function<void, GLenum, GLenum, GLuint, GLint, GLenum> Binding::FramebufferTextureFaceEXT("glFramebufferTextureFaceEXT");
Function<void, GLenum, GLenum, GLuint, GLint, GLint> Binding::FramebufferTextureLayer("glFramebufferTextureLayer");
Function<void, GLenum, GLenum, GLuint, GLint, GLint> Binding::FramebufferTextureLayerARB("glFramebufferTextureLayerARB");
Function<void, GLenum, GLenum, GLuint, GLint, GLint> Binding::FramebufferTextureLayerEXT("glFramebufferTextureLayerEXT");
Function<void, GLenum, GLenum, GLuint, GLint, GLint, GLsizei> Binding::FramebufferTextureMultiviewOVR("glFramebufferTextureMultiviewOVR");
Function<void, GLuint> Binding::FreeObjectBufferATI("glFreeObjectBufferATI");
Function<void, GLenum> Binding::FrontFace("glFrontFace");
Function<void, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble> Binding::Frustum("glFrustum");
Function<void, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat> Binding::FrustumfOES("glFrustumfOES");
Function<void, GLfixed, GLfixed, GLfixed, GLfixed, GLfixed, GLfixed> Binding::FrustumxOES("glFrustumxOES");
Function<GLuint, GLsizei> Binding::GenAsyncMarkersSGIX("glGenAsyncMarkersSGIX");
Function<void, GLsizei, GLuint *> Binding::GenBuffers("glGenBuffers");
Function<void, GLsizei, GLuint *> Binding::GenBuffersARB("glGenBuffersARB");
Function<void, GLsizei, GLuint *> Binding::GenFencesAPPLE("glGenFencesAPPLE");
Function<void, GLsizei, GLuint *> Binding::GenFencesNV("glGenFencesNV");
Function<GLuint, GLuint> Binding::GenFragmentShadersATI("glGenFragmentShadersATI");
Function<void, GLsizei, GLuint *> Binding::GenFramebuffers("glGenFramebuffers");
Function<void, GLsizei, GLuint *> Binding::GenFramebuffersEXT("glGenFramebuffersEXT");
Function<GLuint, GLsizei> Binding::GenLists("glGenLists");
Function<void, GLenum, GLuint, GLuint *> Binding::GenNamesAMD("glGenNamesAMD");
Function<void, GLsizei, GLuint *> Binding::GenOcclusionQueriesNV("glGenOcclusionQueriesNV");
Function<GLuint, GLsizei> Binding::GenPathsNV("glGenPathsNV");
Function<void, GLsizei, GLuint *> Binding::GenPerfMonitorsAMD("glGenPerfMonitorsAMD");
Function<void, GLsizei, GLuint *> Binding::GenProgramPipelines("glGenProgramPipelines");
Function<void, GLsizei, GLuint *> Binding::GenProgramsARB("glGenProgramsARB");
Function<void, GLsizei, GLuint *> Binding::GenProgramsNV("glGenProgramsNV");
Function<void, GLsizei, GLuint *> Binding::GenQueries("glGenQueries");
Function<void, GLsizei, GLuint *> Binding::GenQueriesARB("glGenQueriesARB");
Function<void, GLsizei, GLuint *> Binding::GenRenderbuffers("glGenRenderbuffers");
Function<void, GLsizei, GLuint *> Binding::GenRenderbuffersEXT("glGenRenderbuffersEXT");
Function<void, GLsizei, GLuint *> Binding::GenSamplers("glGenSamplers");
Function<GLuint, GLenum, GLenum, GLenum, GLuint> Binding::GenSymbolsEXT("glGenSymbolsEXT");
Function<void, GLsizei, GLuint *> Binding::GenTextures("glGenTextures");
Function<void, GLsizei, GLuint *> Binding::GenTexturesEXT("glGenTexturesEXT");
Function<void, GLsizei, GLuint *> Binding::GenTransformFeedbacks("glGenTransformFeedbacks");
Function<void, GLsizei, GLuint *> Binding::GenTransformFeedbacksNV("glGenTransformFeedbacksNV");
Function<void, GLsizei, GLuint *> Binding::GenVertexArrays("glGenVertexArrays");
Function<void, GLsizei, GLuint *> Binding::GenVertexArraysAPPLE("glGenVertexArraysAPPLE");
Function<GLuint, GLuint> Binding::GenVertexShadersEXT("glGenVertexShadersEXT");
Function<void, GLenum> Binding::GenerateMipmap("glGenerateMipmap");
Function<void, GLenum> Binding::GenerateMipmapEXT("glGenerateMipmapEXT");
Function<void, GLenum, GLenum> Binding::GenerateMultiTexMipmapEXT("glGenerateMultiTexMipmapEXT");
Function<void, GLuint> Binding::GenerateTextureMipmap("glGenerateTextureMipmap");
Function<void, GLuint, GLenum> Binding::GenerateTextureMipmapEXT("glGenerateTextureMipmapEXT");
Function<void, GLuint, GLuint, GLenum, GLint *> Binding::GetActiveAtomicCounterBufferiv("glGetActiveAtomicCounterBufferiv");
Function<void, GLuint, GLuint, GLsizei, GLsizei *, GLint *, GLenum *, GLchar *> Binding::GetActiveAttrib("glGetActiveAttrib");
Function<void, GLhandleARB, GLuint, GLsizei, GLsizei *, GLint *, GLenum *, GLcharARB *> Binding::GetActiveAttribARB("glGetActiveAttribARB");
Function<void, GLuint, GLenum, GLuint, GLsizei, GLsizei *, GLchar *> Binding::GetActiveSubroutineName("glGetActiveSubroutineName");
Function<void, GLuint, GLenum, GLuint, GLsizei, GLsizei *, GLchar *> Binding::GetActiveSubroutineUniformName("glGetActiveSubroutineUniformName");
Function<void, GLuint, GLenum, GLuint, GLenum, GLint *> Binding::GetActiveSubroutineUniformiv("glGetActiveSubroutineUniformiv");
Function<void, GLuint, GLuint, GLsizei, GLsizei *, GLint *, GLenum *, GLchar *> Binding::GetActiveUniform("glGetActiveUniform");
Function<void, GLhandleARB, GLuint, GLsizei, GLsizei *, GLint *, GLenum *, GLcharARB *> Binding::GetActiveUniformARB("glGetActiveUniformARB");
Function<void, GLuint, GLuint, GLsizei, GLsizei *, GLchar *> Binding::GetActiveUniformBlockName("glGetActiveUniformBlockName");
Function<void, GLuint, GLuint, GLenum, GLint *> Binding::GetActiveUniformBlockiv("glGetActiveUniformBlockiv");
Function<void, GLuint, GLuint, GLsizei, GLsizei *, GLchar *> Binding::GetActiveUniformName("glGetActiveUniformName");
Function<void, GLuint, GLsizei, const GLuint *, GLenum, GLint *> Binding::GetActiveUniformsiv("glGetActiveUniformsiv");
Function<void, GLuint, GLuint, GLsizei, GLsizei *, GLsizei *, GLenum *, GLchar *> Binding::GetActiveVaryingNV("glGetActiveVaryingNV");
Function<void, GLenum, GLenum, GLfloat *> Binding::GetArrayObjectfvATI("glGetArrayObjectfvATI");
Function<void, GLenum, GLenum, GLint *> Binding::GetArrayObjectivATI("glGetArrayObjectivATI");
Function<void, GLhandleARB, GLsizei, GLsizei *, GLhandleARB *> Binding::GetAttachedObjectsARB("glGetAttachedObjectsARB");
Function<void, GLuint, GLsizei, GLsizei *, GLuint *> Binding::GetAttachedShaders("glGetAttachedShaders");
Function<GLint, GLuint, const GLchar *> Binding::GetAttribLocation("glGetAttribLocation");
Function<GLint, GLhandleARB, const GLcharARB *> Binding::GetAttribLocationARB("glGetAttribLocationARB");
Function<void, GLenum, GLuint, GLboolean *> Binding::GetBooleanIndexedvEXT("glGetBooleanIndexedvEXT");
Function<void, GLenum, GLuint, GLboolean *> Binding::GetBooleani_v("glGetBooleani_v");
Function<void, GLenum, GLboolean *> Binding::GetBooleanv("glGetBooleanv");
Function<void, GLenum, GLenum, GLint64 *> Binding::GetBufferParameteri64v("glGetBufferParameteri64v");
Function<void, GLenum, GLenum, GLint *> Binding::GetBufferParameteriv("glGetBufferParameteriv");
Function<void, GLenum, GLenum, GLint *> Binding::GetBufferParameterivARB("glGetBufferParameterivARB");
Function<void, GLenum, GLenum, GLuint64EXT *> Binding::GetBufferParameterui64vNV("glGetBufferParameterui64vNV");
Function<void, GLenum, GLenum, void **> Binding::GetBufferPointerv("glGetBufferPointerv");
Function<void, GLenum, GLenum, void **> Binding::GetBufferPointervARB("glGetBufferPointervARB");
Function<void, GLenum, GLintptr, GLsizeiptr, void *> Binding::GetBufferSubData("glGetBufferSubData");
Function<void, GLenum, GLintptrARB, GLsizeiptrARB, void *> Binding::GetBufferSubDataARB("glGetBufferSubDataARB");
Function<void, GLenum, GLdouble *> Binding::GetClipPlane("glGetClipPlane");
Function<void, GLenum, GLfloat *> Binding::GetClipPlanefOES("glGetClipPlanefOES");
Function<void, GLenum, GLfixed *> Binding::GetClipPlanexOES("glGetClipPlanexOES");
Function<void, GLenum, GLenum, GLenum, void *> Binding::GetColorTable("glGetColorTable");
Function<void, GLenum, GLenum, GLenum, void *> Binding::GetColorTableEXT("glGetColorTableEXT");
Function<void, GLenum, GLenum, GLfloat *> Binding::GetColorTableParameterfv("glGetColorTableParameterfv");
Function<void, GLenum, GLenum, GLfloat *> Binding::GetColorTableParameterfvEXT("glGetColorTableParameterfvEXT");
Function<void, GLenum, GLenum, GLfloat *> Binding::GetColorTableParameterfvSGI("glGetColorTableParameterfvSGI");
Function<void, GLenum, GLenum, GLint *> Binding::GetColorTableParameteriv("glGetColorTableParameteriv");
Function<void, GLenum, GLenum, GLint *> Binding::GetColorTableParameterivEXT("glGetColorTableParameterivEXT");
Function<void, GLenum, GLenum, GLint *> Binding::GetColorTableParameterivSGI("glGetColorTableParameterivSGI");
Function<void, GLenum, GLenum, GLenum, void *> Binding::GetColorTableSGI("glGetColorTableSGI");
Function<void, GLenum, GLenum, GLenum, GLenum, GLfloat *> Binding::GetCombinerInputParameterfvNV("glGetCombinerInputParameterfvNV");
Function<void, GLenum, GLenum, GLenum, GLenum, GLint *> Binding::GetCombinerInputParameterivNV("glGetCombinerInputParameterivNV");
Function<void, GLenum, GLenum, GLenum, GLfloat *> Binding::GetCombinerOutputParameterfvNV("glGetCombinerOutputParameterfvNV");
Function<void, GLenum, GLenum, GLenum, GLint *> Binding::GetCombinerOutputParameterivNV("glGetCombinerOutputParameterivNV");
Function<void, GLenum, GLenum, GLfloat *> Binding::GetCombinerStageParameterfvNV("glGetCombinerStageParameterfvNV");
Function<GLuint, GLenum, GLuint> Binding::GetCommandHeaderNV("glGetCommandHeaderNV");
Function<void, GLenum, GLenum, GLint, void *> Binding::GetCompressedMultiTexImageEXT("glGetCompressedMultiTexImageEXT");
Function<void, GLenum, GLint, void *> Binding::GetCompressedTexImage("glGetCompressedTexImage");
Function<void, GLenum, GLint, void *> Binding::GetCompressedTexImageARB("glGetCompressedTexImageARB");
Function<void, GLuint, GLint, GLsizei, void *> Binding::GetCompressedTextureImage("glGetCompressedTextureImage");
Function<void, GLuint, GLenum, GLint, void *> Binding::GetCompressedTextureImageEXT("glGetCompressedTextureImageEXT");
Function<void, GLuint, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLsizei, void *> Binding::GetCompressedTextureSubImage("glGetCompressedTextureSubImage");
Function<void, GLenum, GLenum, GLenum, void *> Binding::GetConvolutionFilter("glGetConvolutionFilter");
Function<void, GLenum, GLenum, GLenum, void *> Binding::GetConvolutionFilterEXT("glGetConvolutionFilterEXT");
Function<void, GLenum, GLenum, GLfloat *> Binding::GetConvolutionParameterfv("glGetConvolutionParameterfv");
Function<void, GLenum, GLenum, GLfloat *> Binding::GetConvolutionParameterfvEXT("glGetConvolutionParameterfvEXT");
Function<void, GLenum, GLenum, GLint *> Binding::GetConvolutionParameteriv("glGetConvolutionParameteriv");
Function<void, GLenum, GLenum, GLint *> Binding::GetConvolutionParameterivEXT("glGetConvolutionParameterivEXT");
Function<void, GLenum, GLenum, GLfixed *> Binding::GetConvolutionParameterxvOES("glGetConvolutionParameterxvOES");
Function<void, GLsizei, GLfloat *> Binding::GetCoverageModulationTableNV("glGetCoverageModulationTableNV");
Function<GLuint, GLuint, GLsizei, GLenum *, GLenum *, GLuint *, GLenum *, GLsizei *, GLchar *> Binding::GetDebugMessageLog("glGetDebugMessageLog");
Function<GLuint, GLuint, GLsizei, GLenum *, GLuint *, GLuint *, GLsizei *, GLchar *> Binding::GetDebugMessageLogAMD("glGetDebugMessageLogAMD");
Function<GLuint, GLuint, GLsizei, GLenum *, GLenum *, GLuint *, GLenum *, GLsizei *, GLchar *> Binding::GetDebugMessageLogARB("glGetDebugMessageLogARB");
Function<void, GLenum, GLfloat *> Binding::GetDetailTexFuncSGIS("glGetDetailTexFuncSGIS");
Function<void, GLenum, GLuint, GLdouble *> Binding::GetDoubleIndexedvEXT("glGetDoubleIndexedvEXT");
Function<void, GLenum, GLuint, GLdouble *> Binding::GetDoublei_v("glGetDoublei_v");
Function<void, GLenum, GLuint, GLdouble *> Binding::GetDoublei_vEXT("glGetDoublei_vEXT");
Function<void, GLenum, GLdouble *> Binding::GetDoublev("glGetDoublev");
Function<GLenum> Binding::GetError("glGetError");
Function<void, GLuint, GLenum, GLint *> Binding::GetFenceivNV("glGetFenceivNV");
Function<void, GLenum, GLenum, GLfloat *> Binding::GetFinalCombinerInputParameterfvNV("glGetFinalCombinerInputParameterfvNV");
Function<void, GLenum, GLenum, GLint *> Binding::GetFinalCombinerInputParameterivNV("glGetFinalCombinerInputParameterivNV");
Function<void, GLuint *> Binding::GetFirstPerfQueryIdINTEL("glGetFirstPerfQueryIdINTEL");
Function<void, GLenum, GLfixed *> Binding::GetFixedvOES("glGetFixedvOES");
Function<void, GLenum, GLuint, GLfloat *> Binding::GetFloatIndexedvEXT("glGetFloatIndexedvEXT");
Function<void, GLenum, GLuint, GLfloat *> Binding::GetFloati_v("glGetFloati_v");
Function<void, GLenum, GLuint, GLfloat *> Binding::GetFloati_vEXT("glGetFloati_vEXT");
Function<void, GLenum, GLfloat *> Binding::GetFloatv("glGetFloatv");
Function<void, GLfloat *> Binding::GetFogFuncSGIS("glGetFogFuncSGIS");
Function<GLint, GLuint, const GLchar *> Binding::GetFragDataIndex("glGetFragDataIndex");
Function<GLint, GLuint, const GLchar *> Binding::GetFragDataLocation("glGetFragDataLocation");
Function<GLint, GLuint, const GLchar *> Binding::GetFragDataLocationEXT("glGetFragDataLocationEXT");
Function<void, GLenum, GLenum, GLfloat *> Binding::GetFragmentLightfvSGIX("glGetFragmentLightfvSGIX");
Function<void, GLenum, GLenum, GLint *> Binding::GetFragmentLightivSGIX("glGetFragmentLightivSGIX");
Function<void, GLenum, GLenum, GLfloat *> Binding::GetFragmentMaterialfvSGIX("glGetFragmentMaterialfvSGIX");
Function<void, GLenum, GLenum, GLint *> Binding::GetFragmentMaterialivSGIX("glGetFragmentMaterialivSGIX");
Function<void, GLenum, GLenum, GLenum, GLint *> Binding::GetFramebufferAttachmentParameteriv("glGetFramebufferAttachmentParameteriv");
Function<void, GLenum, GLenum, GLenum, GLint *> Binding::GetFramebufferAttachmentParameterivEXT("glGetFramebufferAttachmentParameterivEXT");
Function<void, GLenum, GLenum, GLint *> Binding::GetFramebufferParameteriv("glGetFramebufferParameteriv");
Function<void, GLuint, GLenum, GLint *> Binding::GetFramebufferParameterivEXT("glGetFramebufferParameterivEXT");
Function<GLenum> Binding::GetGraphicsResetStatus("glGetGraphicsResetStatus");
Function<GLenum> Binding::GetGraphicsResetStatusARB("glGetGraphicsResetStatusARB");
Function<GLhandleARB, GLenum> Binding::GetHandleARB("glGetHandleARB");
Function<void, GLenum, GLboolean, GLenum, GLenum, void *> Binding::GetHistogram("glGetHistogram");
Function<void, GLenum, GLboolean, GLenum, GLenum, void *> Binding::GetHistogramEXT("glGetHistogramEXT");
Function<void, GLenum, GLenum, GLfloat *> Binding::GetHistogramParameterfv("glGetHistogramParameterfv");
Function<void, GLenum, GLenum, GLfloat *> Binding::GetHistogramParameterfvEXT("glGetHistogramParameterfvEXT");
Function<void, GLenum, GLenum, GLint *> Binding::GetHistogramParameteriv("glGetHistogramParameteriv");
Function<void, GLenum, GLenum, GLint *> Binding::GetHistogramParameterivEXT("glGetHistogramParameterivEXT");
Function<void, GLenum, GLenum, GLfixed *> Binding::GetHistogramParameterxvOES("glGetHistogramParameterxvOES");
Function<GLuint64, GLuint, GLint, GLboolean, GLint, GLenum> Binding::GetImageHandleARB("glGetImageHandleARB");
Function<GLuint64, GLuint, GLint, GLboolean, GLint, GLenum> Binding::GetImageHandleNV("glGetImageHandleNV");
Function<void, GLenum, GLenum, GLfloat *> Binding::GetImageTransformParameterfvHP("glGetImageTransformParameterfvHP");
Function<void, GLenum, GLenum, GLint *> Binding::GetImageTransformParameterivHP("glGetImageTransformParameterivHP");
Function<void, GLhandleARB, GLsizei, GLsizei *, GLcharARB *> Binding::GetInfoLogARB("glGetInfoLogARB");
Function<GLint> Binding::GetInstrumentsSGIX("glGetInstrumentsSGIX");
Function<void, GLenum, GLuint, GLint64 *> Binding::GetInteger64i_v("glGetInteger64i_v");
Function<void, GLenum, GLint64 *> Binding::GetInteger64v("glGetInteger64v");
Function<void, GLenum, GLuint, GLint *> Binding::GetIntegerIndexedvEXT("glGetIntegerIndexedvEXT");
Function<void, GLenum, GLuint, GLint *> Binding::GetIntegeri_v("glGetIntegeri_v");
Function<void, GLenum, GLuint, GLuint64EXT *> Binding::GetIntegerui64i_vNV("glGetIntegerui64i_vNV");
Function<void, GLenum, GLuint64EXT *> Binding::GetIntegerui64vNV("glGetIntegerui64vNV");
Function<void, GLenum, GLint *> Binding::GetIntegerv("glGetIntegerv");
Function<void, GLenum, GLenum, GLsizei, GLenum, GLsizei, GLint *> Binding::GetInternalformatSampleivNV("glGetInternalformatSampleivNV");
Function<void, GLenum, GLenum, GLenum, GLsizei, GLint64 *> Binding::GetInternalformati64v("glGetInternalformati64v");
Function<void, GLenum, GLenum, GLenum, GLsizei, GLint *> Binding::GetInternalformativ("glGetInternalformativ");
Function<void, GLuint, GLenum, GLboolean *> Binding::GetInvariantBooleanvEXT("glGetInvariantBooleanvEXT");
Function<void, GLuint, GLenum, GLfloat *> Binding::GetInvariantFloatvEXT("glGetInvariantFloatvEXT");
Function<void, GLuint, GLenum, GLint *> Binding::GetInvariantIntegervEXT("glGetInvariantIntegervEXT");
Function<void, GLenum, GLenum, GLfloat *> Binding::GetLightfv("glGetLightfv");
Function<void, GLenum, GLenum, GLint *> Binding::GetLightiv("glGetLightiv");
Function<void, GLenum, GLenum, GLfixed *> Binding::GetLightxOES("glGetLightxOES");
Function<void, GLuint, GLenum, GLfloat *> Binding::GetListParameterfvSGIX("glGetListParameterfvSGIX");
Function<void, GLuint, GLenum, GLint *> Binding::GetListParameterivSGIX("glGetListParameterivSGIX");
Function<void, GLuint, GLenum, GLboolean *> Binding::GetLocalConstantBooleanvEXT("glGetLocalConstantBooleanvEXT");
Function<void, GLuint, GLenum, GLfloat *> Binding::GetLocalConstantFloatvEXT("glGetLocalConstantFloatvEXT");
Function<void, GLuint, GLenum, GLint *> Binding::GetLocalConstantIntegervEXT("glGetLocalConstantIntegervEXT");
Function<void, GLenum, GLuint, GLenum, GLfloat *> Binding::GetMapAttribParameterfvNV("glGetMapAttribParameterfvNV");
Function<void, GLenum, GLuint, GLenum, GLint *> Binding::GetMapAttribParameterivNV("glGetMapAttribParameterivNV");
Function<void, GLenum, GLuint, GLenum, GLsizei, GLsizei, GLboolean, void *> Binding::GetMapControlPointsNV("glGetMapControlPointsNV");
Function<void, GLenum, GLenum, GLfloat *> Binding::GetMapParameterfvNV("glGetMapParameterfvNV");
Function<void, GLenum, GLenum, GLint *> Binding::GetMapParameterivNV("glGetMapParameterivNV");
Function<void, GLenum, GLenum, GLdouble *> Binding::GetMapdv("glGetMapdv");
Function<void, GLenum, GLenum, GLfloat *> Binding::GetMapfv("glGetMapfv");
Function<void, GLenum, GLenum, GLint *> Binding::GetMapiv("glGetMapiv");
Function<void, GLenum, GLenum, GLfixed *> Binding::GetMapxvOES("glGetMapxvOES");
Function<void, GLenum, GLenum, GLfloat *> Binding::GetMaterialfv("glGetMaterialfv");
Function<void, GLenum, GLenum, GLint *> Binding::GetMaterialiv("glGetMaterialiv");
Function<void, GLenum, GLenum, GLfixed> Binding::GetMaterialxOES("glGetMaterialxOES");
Function<void, GLenum, GLboolean, GLenum, GLenum, void *> Binding::GetMinmax("glGetMinmax");
Function<void, GLenum, GLboolean, GLenum, GLenum, void *> Binding::GetMinmaxEXT("glGetMinmaxEXT");
Function<void, GLenum, GLenum, GLfloat *> Binding::GetMinmaxParameterfv("glGetMinmaxParameterfv");
Function<void, GLenum, GLenum, GLfloat *> Binding::GetMinmaxParameterfvEXT("glGetMinmaxParameterfvEXT");
Function<void, GLenum, GLenum, GLint *> Binding::GetMinmaxParameteriv("glGetMinmaxParameteriv");
Function<void, GLenum, GLenum, GLint *> Binding::GetMinmaxParameterivEXT("glGetMinmaxParameterivEXT");
Function<void, GLenum, GLenum, GLenum, GLfloat *> Binding::GetMultiTexEnvfvEXT("glGetMultiTexEnvfvEXT");
Function<void, GLenum, GLenum, GLenum, GLint *> Binding::GetMultiTexEnvivEXT("glGetMultiTexEnvivEXT");
Function<void, GLenum, GLenum, GLenum, GLdouble *> Binding::GetMultiTexGendvEXT("glGetMultiTexGendvEXT");
Function<void, GLenum, GLenum, GLenum, GLfloat *> Binding::GetMultiTexGenfvEXT("glGetMultiTexGenfvEXT");
Function<void, GLenum, GLenum, GLenum, GLint *> Binding::GetMultiTexGenivEXT("glGetMultiTexGenivEXT");
Function<void, GLenum, GLenum, GLint, GLenum, GLenum, void *> Binding::GetMultiTexImageEXT("glGetMultiTexImageEXT");
Function<void, GLenum, GLenum, GLint, GLenum, GLfloat *> Binding::GetMultiTexLevelParameterfvEXT("glGetMultiTexLevelParameterfvEXT");
Function<void, GLenum, GLenum, GLint, GLenum, GLint *> Binding::GetMultiTexLevelParameterivEXT("glGetMultiTexLevelParameterivEXT");
Function<void, GLenum, GLenum, GLenum, GLint *> Binding::GetMultiTexParameterIivEXT("glGetMultiTexParameterIivEXT");
Function<void, GLenum, GLenum, GLenum, GLuint *> Binding::GetMultiTexParameterIuivEXT("glGetMultiTexParameterIuivEXT");
Function<void, GLenum, GLenum, GLenum, GLfloat *> Binding::GetMultiTexParameterfvEXT("glGetMultiTexParameterfvEXT");
Function<void, GLenum, GLenum, GLenum, GLint *> Binding::GetMultiTexParameterivEXT("glGetMultiTexParameterivEXT");
Function<void, GLenum, GLuint, GLfloat *> Binding::GetMultisamplefv("glGetMultisamplefv");
Function<void, GLenum, GLuint, GLfloat *> Binding::GetMultisamplefvNV("glGetMultisamplefvNV");
Function<void, GLuint, GLenum, GLint64 *> Binding::GetNamedBufferParameteri64v("glGetNamedBufferParameteri64v");
Function<void, GLuint, GLenum, GLint *> Binding::GetNamedBufferParameteriv("glGetNamedBufferParameteriv");
Function<void, GLuint, GLenum, GLint *> Binding::GetNamedBufferParameterivEXT("glGetNamedBufferParameterivEXT");
Function<void, GLuint, GLenum, GLuint64EXT *> Binding::GetNamedBufferParameterui64vNV("glGetNamedBufferParameterui64vNV");
Function<void, GLuint, GLenum, void **> Binding::GetNamedBufferPointerv("glGetNamedBufferPointerv");
Function<void, GLuint, GLenum, void **> Binding::GetNamedBufferPointervEXT("glGetNamedBufferPointervEXT");
Function<void, GLuint, GLintptr, GLsizeiptr, void *> Binding::GetNamedBufferSubData("glGetNamedBufferSubData");
Function<void, GLuint, GLintptr, GLsizeiptr, void *> Binding::GetNamedBufferSubDataEXT("glGetNamedBufferSubDataEXT");
Function<void, GLuint, GLenum, GLenum, GLint *> Binding::GetNamedFramebufferAttachmentParameteriv("glGetNamedFramebufferAttachmentParameteriv");
Function<void, GLuint, GLenum, GLenum, GLint *> Binding::GetNamedFramebufferAttachmentParameterivEXT("glGetNamedFramebufferAttachmentParameterivEXT");
Function<void, GLuint, GLenum, GLint *> Binding::GetNamedFramebufferParameteriv("glGetNamedFramebufferParameteriv");
Function<void, GLuint, GLenum, GLint *> Binding::GetNamedFramebufferParameterivEXT("glGetNamedFramebufferParameterivEXT");
Function<void, GLuint, GLenum, GLuint, GLint *> Binding::GetNamedProgramLocalParameterIivEXT("glGetNamedProgramLocalParameterIivEXT");
Function<void, GLuint, GLenum, GLuint, GLuint *> Binding::GetNamedProgramLocalParameterIuivEXT("glGetNamedProgramLocalParameterIuivEXT");
Function<void, GLuint, GLenum, GLuint, GLdouble *> Binding::GetNamedProgramLocalParameterdvEXT("glGetNamedProgramLocalParameterdvEXT");
Function<void, GLuint, GLenum, GLuint, GLfloat *> Binding::GetNamedProgramLocalParameterfvEXT("glGetNamedProgramLocalParameterfvEXT");
Function<void, GLuint, GLenum, GLenum, void *> Binding::GetNamedProgramStringEXT("glGetNamedProgramStringEXT");
Function<void, GLuint, GLenum, GLenum, GLint *> Binding::GetNamedProgramivEXT("glGetNamedProgramivEXT");
Function<void, GLuint, GLenum, GLint *> Binding::GetNamedRenderbufferParameteriv("glGetNamedRenderbufferParameteriv");
Function<void, GLuint, GLenum, GLint *> Binding::GetNamedRenderbufferParameterivEXT("glGetNamedRenderbufferParameterivEXT");
Function<void, GLint, const GLchar *, GLsizei, GLint *, GLchar *> Binding::GetNamedStringARB("glGetNamedStringARB");
Function<void, GLint, const GLchar *, GLenum, GLint *> Binding::GetNamedStringivARB("glGetNamedStringivARB");
Function<void, GLuint, GLuint *> Binding::GetNextPerfQueryIdINTEL("glGetNextPerfQueryIdINTEL");
Function<void, GLuint, GLenum, GLfloat *> Binding::GetObjectBufferfvATI("glGetObjectBufferfvATI");
Function<void, GLuint, GLenum, GLint *> Binding::GetObjectBufferivATI("glGetObjectBufferivATI");
Function<void, GLenum, GLuint, GLsizei, GLsizei *, GLchar *> Binding::GetObjectLabel("glGetObjectLabel");
Function<void, GLenum, GLuint, GLsizei, GLsizei *, GLchar *> Binding::GetObjectLabelEXT("glGetObjectLabelEXT");
Function<void, GLhandleARB, GLenum, GLfloat *> Binding::GetObjectParameterfvARB("glGetObjectParameterfvARB");
Function<void, GLenum, GLuint, GLenum, GLint *> Binding::GetObjectParameterivAPPLE("glGetObjectParameterivAPPLE");
Function<void, GLhandleARB, GLenum, GLint *> Binding::GetObjectParameterivARB("glGetObjectParameterivARB");
Function<void, const void *, GLsizei, GLsizei *, GLchar *> Binding::GetObjectPtrLabel("glGetObjectPtrLabel");
Function<void, GLuint, GLenum, GLint *> Binding::GetOcclusionQueryivNV("glGetOcclusionQueryivNV");
Function<void, GLuint, GLenum, GLuint *> Binding::GetOcclusionQueryuivNV("glGetOcclusionQueryuivNV");
Function<void, GLenum, GLenum, GLfloat *> Binding::GetPathColorGenfvNV("glGetPathColorGenfvNV");
Function<void, GLenum, GLenum, GLint *> Binding::GetPathColorGenivNV("glGetPathColorGenivNV");
Function<void, GLuint, GLubyte *> Binding::GetPathCommandsNV("glGetPathCommandsNV");
Function<void, GLuint, GLfloat *> Binding::GetPathCoordsNV("glGetPathCoordsNV");
Function<void, GLuint, GLfloat *> Binding::GetPathDashArrayNV("glGetPathDashArrayNV");
Function<GLfloat, GLuint, GLsizei, GLsizei> Binding::GetPathLengthNV("glGetPathLengthNV");
Function<void, PathRenderingMaskNV, GLuint, GLsizei, GLsizei, GLfloat *> Binding::GetPathMetricRangeNV("glGetPathMetricRangeNV");
Function<void, PathRenderingMaskNV, GLsizei, GLenum, const void *, GLuint, GLsizei, GLfloat *> Binding::GetPathMetricsNV("glGetPathMetricsNV");
Function<void, GLuint, GLenum, GLfloat *> Binding::GetPathParameterfvNV("glGetPathParameterfvNV");
Function<void, GLuint, GLenum, GLint *> Binding::GetPathParameterivNV("glGetPathParameterivNV");
Function<void, GLenum, GLsizei, GLenum, const void *, GLuint, GLfloat, GLfloat, GLenum, GLfloat *> Binding::GetPathSpacingNV("glGetPathSpacingNV");
Function<void, GLenum, GLenum, GLfloat *> Binding::GetPathTexGenfvNV("glGetPathTexGenfvNV");
Function<void, GLenum, GLenum, GLint *> Binding::GetPathTexGenivNV("glGetPathTexGenivNV");
Function<void, GLuint, GLuint, GLuint, GLchar *, GLuint, GLchar *, GLuint *, GLuint *, GLuint *, GLuint *, GLuint64 *> Binding::GetPerfCounterInfoINTEL("glGetPerfCounterInfoINTEL");
Function<void, GLuint, GLenum, GLsizei, GLuint *, GLint *> Binding::GetPerfMonitorCounterDataAMD("glGetPerfMonitorCounterDataAMD");
Function<void, GLuint, GLuint, GLenum, void *> Binding::GetPerfMonitorCounterInfoAMD("glGetPerfMonitorCounterInfoAMD");
Function<void, GLuint, GLuint, GLsizei, GLsizei *, GLchar *> Binding::GetPerfMonitorCounterStringAMD("glGetPerfMonitorCounterStringAMD");
Function<void, GLuint, GLint *, GLint *, GLsizei, GLuint *> Binding::GetPerfMonitorCountersAMD("glGetPerfMonitorCountersAMD");
Function<void, GLuint, GLsizei, GLsizei *, GLchar *> Binding::GetPerfMonitorGroupStringAMD("glGetPerfMonitorGroupStringAMD");
Function<void, GLint *, GLsizei, GLuint *> Binding::GetPerfMonitorGroupsAMD("glGetPerfMonitorGroupsAMD");
Function<void, GLuint, GLuint, GLsizei, GLvoid *, GLuint *> Binding::GetPerfQueryDataINTEL("glGetPerfQueryDataINTEL");
Function<void, GLchar *, GLuint *> Binding::GetPerfQueryIdByNameINTEL("glGetPerfQueryIdByNameINTEL");
Function<void, GLuint, GLuint, GLchar *, GLuint *, GLuint *, GLuint *, GLuint *> Binding::GetPerfQueryInfoINTEL("glGetPerfQueryInfoINTEL");
Function<void, GLenum, GLfloat *> Binding::GetPixelMapfv("glGetPixelMapfv");
Function<void, GLenum, GLuint *> Binding::GetPixelMapuiv("glGetPixelMapuiv");
Function<void, GLenum, GLushort *> Binding::GetPixelMapusv("glGetPixelMapusv");
Function<void, GLenum, GLint, GLfixed *> Binding::GetPixelMapxv("glGetPixelMapxv");
Function<void, GLenum, GLfloat *> Binding::GetPixelTexGenParameterfvSGIS("glGetPixelTexGenParameterfvSGIS");
Function<void, GLenum, GLint *> Binding::GetPixelTexGenParameterivSGIS("glGetPixelTexGenParameterivSGIS");
Function<void, GLenum, GLenum, GLfloat *> Binding::GetPixelTransformParameterfvEXT("glGetPixelTransformParameterfvEXT");
Function<void, GLenum, GLenum, GLint *> Binding::GetPixelTransformParameterivEXT("glGetPixelTransformParameterivEXT");
Function<void, GLenum, GLuint, void **> Binding::GetPointerIndexedvEXT("glGetPointerIndexedvEXT");
Function<void, GLenum, GLuint, void **> Binding::GetPointeri_vEXT("glGetPointeri_vEXT");
Function<void, GLenum, void **> Binding::GetPointerv("glGetPointerv");
Function<void, GLenum, void **> Binding::GetPointervEXT("glGetPointervEXT");
Function<void, GLubyte *> Binding::GetPolygonStipple("glGetPolygonStipple");
Function<void, GLuint, GLsizei, GLsizei *, GLenum *, void *> Binding::GetProgramBinary("glGetProgramBinary");
Function<void, GLenum, GLuint, GLint *> Binding::GetProgramEnvParameterIivNV("glGetProgramEnvParameterIivNV");
Function<void, GLenum, GLuint, GLuint *> Binding::GetProgramEnvParameterIuivNV("glGetProgramEnvParameterIuivNV");
Function<void, GLenum, GLuint, GLdouble *> Binding::GetProgramEnvParameterdvARB("glGetProgramEnvParameterdvARB");
Function<void, GLenum, GLuint, GLfloat *> Binding::GetProgramEnvParameterfvARB("glGetProgramEnvParameterfvARB");
Function<void, GLuint, GLsizei, GLsizei *, GLchar *> Binding::GetProgramInfoLog("glGetProgramInfoLog");
Function<void, GLuint, GLenum, GLenum, GLint *> Binding::GetProgramInterfaceiv("glGetProgramInterfaceiv");
Function<void, GLenum, GLuint, GLint *> Binding::GetProgramLocalParameterIivNV("glGetProgramLocalParameterIivNV");
Function<void, GLenum, GLuint, GLuint *> Binding::GetProgramLocalParameterIuivNV("glGetProgramLocalParameterIuivNV");
Function<void, GLenum, GLuint, GLdouble *> Binding::GetProgramLocalParameterdvARB("glGetProgramLocalParameterdvARB");
Function<void, GLenum, GLuint, GLfloat *> Binding::GetProgramLocalParameterfvARB("glGetProgramLocalParameterfvARB");
Function<void, GLuint, GLsizei, const GLubyte *, GLdouble *> Binding::GetProgramNamedParameterdvNV("glGetProgramNamedParameterdvNV");
Function<void, GLuint, GLsizei, const GLubyte *, GLfloat *> Binding::GetProgramNamedParameterfvNV("glGetProgramNamedParameterfvNV");
Function<void, GLenum, GLuint, GLenum, GLdouble *> Binding::GetProgramParameterdvNV("glGetProgramParameterdvNV");
Function<void, GLenum, GLuint, GLenum, GLfloat *> Binding::GetProgramParameterfvNV("glGetProgramParameterfvNV");
Function<void, GLuint, GLsizei, GLsizei *, GLchar *> Binding::GetProgramPipelineInfoLog("glGetProgramPipelineInfoLog");
Function<void, GLuint, GLenum, GLint *> Binding::GetProgramPipelineiv("glGetProgramPipelineiv");
Function<GLuint, GLuint, GLenum, const GLchar *> Binding::GetProgramResourceIndex("glGetProgramResourceIndex");
Function<GLint, GLuint, GLenum, const GLchar *> Binding::GetProgramResourceLocation("glGetProgramResourceLocation");
Function<GLint, GLuint, GLenum, const GLchar *> Binding::GetProgramResourceLocationIndex("glGetProgramResourceLocationIndex");
Function<void, GLuint, GLenum, GLuint, GLsizei, GLsizei *, GLchar *> Binding::GetProgramResourceName("glGetProgramResourceName");
Function<void, GLuint, GLenum, GLuint, GLsizei, const GLenum *, GLsizei, GLsizei *, GLfloat *> Binding::GetProgramResourcefvNV("glGetProgramResourcefvNV");
Function<void, GLuint, GLenum, GLuint, GLsizei, const GLenum *, GLsizei, GLsizei *, GLint *> Binding::GetProgramResourceiv("glGetProgramResourceiv");
Function<void, GLuint, GLenum, GLenum, GLint *> Binding::GetProgramStageiv("glGetProgramStageiv");
Function<void, GLenum, GLenum, void *> Binding::GetProgramStringARB("glGetProgramStringARB");
Function<void, GLuint, GLenum, GLubyte *> Binding::GetProgramStringNV("glGetProgramStringNV");
Function<void, GLenum, GLuint, GLuint *> Binding::GetProgramSubroutineParameteruivNV("glGetProgramSubroutineParameteruivNV");
Function<void, GLuint, GLenum, GLint *> Binding::GetProgramiv("glGetProgramiv");
Function<void, GLenum, GLenum, GLint *> Binding::GetProgramivARB("glGetProgramivARB");
Function<void, GLuint, GLenum, GLint *> Binding::GetProgramivNV("glGetProgramivNV");
Function<void, GLuint, GLuint, GLenum, GLintptr> Binding::GetQueryBufferObjecti64v("glGetQueryBufferObjecti64v");
Function<void, GLuint, GLuint, GLenum, GLintptr> Binding::GetQueryBufferObjectiv("glGetQueryBufferObjectiv");
Function<void, GLuint, GLuint, GLenum, GLintptr> Binding::GetQueryBufferObjectui64v("glGetQueryBufferObjectui64v");
Function<void, GLuint, GLuint, GLenum, GLintptr> Binding::GetQueryBufferObjectuiv("glGetQueryBufferObjectuiv");
Function<void, GLenum, GLuint, GLenum, GLint *> Binding::GetQueryIndexediv("glGetQueryIndexediv");
Function<void, GLuint, GLenum, GLint64 *> Binding::GetQueryObjecti64v("glGetQueryObjecti64v");
Function<void, GLuint, GLenum, GLint64 *> Binding::GetQueryObjecti64vEXT("glGetQueryObjecti64vEXT");
Function<void, GLuint, GLenum, GLint *> Binding::GetQueryObjectiv("glGetQueryObjectiv");
Function<void, GLuint, GLenum, GLint *> Binding::GetQueryObjectivARB("glGetQueryObjectivARB");
Function<void, GLuint, GLenum, GLuint64 *> Binding::GetQueryObjectui64v("glGetQueryObjectui64v");
Function<void, GLuint, GLenum, GLuint64 *> Binding::GetQueryObjectui64vEXT("glGetQueryObjectui64vEXT");
Function<void, GLuint, GLenum, GLuint *> Binding::GetQueryObjectuiv("glGetQueryObjectuiv");
Function<void, GLuint, GLenum, GLuint *> Binding::GetQueryObjectuivARB("glGetQueryObjectuivARB");
Function<void, GLenum, GLenum, GLint *> Binding::GetQueryiv("glGetQueryiv");
Function<void, GLenum, GLenum, GLint *> Binding::GetQueryivARB("glGetQueryivARB");
Function<void, GLenum, GLenum, GLint *> Binding::GetRenderbufferParameteriv("glGetRenderbufferParameteriv");
Function<void, GLenum, GLenum, GLint *> Binding::GetRenderbufferParameterivEXT("glGetRenderbufferParameterivEXT");
Function<void, GLuint, GLenum, GLint *> Binding::GetSamplerParameterIiv("glGetSamplerParameterIiv");
Function<void, GLuint, GLenum, GLuint *> Binding::GetSamplerParameterIuiv("glGetSamplerParameterIuiv");
Function<void, GLuint, GLenum, GLfloat *> Binding::GetSamplerParameterfv("glGetSamplerParameterfv");
Function<void, GLuint, GLenum, GLint *> Binding::GetSamplerParameteriv("glGetSamplerParameteriv");
Function<void, GLenum, GLenum, GLenum, void *, void *, void *> Binding::GetSeparableFilter("glGetSeparableFilter");
Function<void, GLenum, GLenum, GLenum, void *, void *, void *> Binding::GetSeparableFilterEXT("glGetSeparableFilterEXT");
Function<void, GLuint, GLsizei, GLsizei *, GLchar *> Binding::GetShaderInfoLog("glGetShaderInfoLog");
Function<void, GLenum, GLenum, GLint *, GLint *> Binding::GetShaderPrecisionFormat("glGetShaderPrecisionFormat");
Function<void, GLuint, GLsizei, GLsizei *, GLchar *> Binding::GetShaderSource("glGetShaderSource");
Function<void, GLhandleARB, GLsizei, GLsizei *, GLcharARB *> Binding::GetShaderSourceARB("glGetShaderSourceARB");
Function<void, GLuint, GLenum, GLint *> Binding::GetShaderiv("glGetShaderiv");
Function<void, GLenum, GLfloat *> Binding::GetSharpenTexFuncSGIS("glGetSharpenTexFuncSGIS");
Function<GLushort, GLenum> Binding::GetStageIndexNV("glGetStageIndexNV");
Function<const GLubyte *, GLenum> Binding::GetString("glGetString");
Function<const GLubyte *, GLenum, GLuint> Binding::GetStringi("glGetStringi");
Function<GLuint, GLuint, GLenum, const GLchar *> Binding::GetSubroutineIndex("glGetSubroutineIndex");
Function<GLint, GLuint, GLenum, const GLchar *> Binding::GetSubroutineUniformLocation("glGetSubroutineUniformLocation");
Function<void, GLsync, GLenum, GLsizei, GLsizei *, GLint *> Binding::GetSynciv("glGetSynciv");
Function<void, GLenum, GLfloat *> Binding::GetTexBumpParameterfvATI("glGetTexBumpParameterfvATI");
Function<void, GLenum, GLint *> Binding::GetTexBumpParameterivATI("glGetTexBumpParameterivATI");
Function<void, GLenum, GLenum, GLfloat *> Binding::GetTexEnvfv("glGetTexEnvfv");
Function<void, GLenum, GLenum, GLint *> Binding::GetTexEnviv("glGetTexEnviv");
Function<void, GLenum, GLenum, GLfixed *> Binding::GetTexEnvxvOES("glGetTexEnvxvOES");
Function<void, GLenum, GLenum, GLfloat *> Binding::GetTexFilterFuncSGIS("glGetTexFilterFuncSGIS");
Function<void, GLenum, GLenum, GLdouble *> Binding::GetTexGendv("glGetTexGendv");
Function<void, GLenum, GLenum, GLfloat *> Binding::GetTexGenfv("glGetTexGenfv");
Function<void, GLenum, GLenum, GLint *> Binding::GetTexGeniv("glGetTexGeniv");
Function<void, GLenum, GLenum, GLfixed *> Binding::GetTexGenxvOES("glGetTexGenxvOES");
Function<void, GLenum, GLint, GLenum, GLenum, void *> Binding::GetTexImage("glGetTexImage");
Function<void, GLenum, GLint, GLenum, GLfloat *> Binding::GetTexLevelParameterfv("glGetTexLevelParameterfv");
Function<void, GLenum, GLint, GLenum, GLint *> Binding::GetTexLevelParameteriv("glGetTexLevelParameteriv");
Function<void, GLenum, GLint, GLenum, GLfixed *> Binding::GetTexLevelParameterxvOES("glGetTexLevelParameterxvOES");
Function<void, GLenum, GLenum, GLint *> Binding::GetTexParameterIiv("glGetTexParameterIiv");
Function<void, GLenum, GLenum, GLint *> Binding::GetTexParameterIivEXT("glGetTexParameterIivEXT");
Function<void, GLenum, GLenum, GLuint *> Binding::GetTexParameterIuiv("glGetTexParameterIuiv");
Function<void, GLenum, GLenum, GLuint *> Binding::GetTexParameterIuivEXT("glGetTexParameterIuivEXT");
Function<void, GLenum, GLenum, void **> Binding::GetTexParameterPointervAPPLE("glGetTexParameterPointervAPPLE");
Function<void, GLenum, GLenum, GLfloat *> Binding::GetTexParameterfv("glGetTexParameterfv");
Function<void, GLenum, GLenum, GLint *> Binding::GetTexParameteriv("glGetTexParameteriv");
Function<void, GLenum, GLenum, GLfixed *> Binding::GetTexParameterxvOES("glGetTexParameterxvOES");
Function<GLuint64, GLuint> Binding::GetTextureHandleARB("glGetTextureHandleARB");
Function<GLuint64, GLuint> Binding::GetTextureHandleNV("glGetTextureHandleNV");
Function<void, GLuint, GLint, GLenum, GLenum, GLsizei, void *> Binding::GetTextureImage("glGetTextureImage");
Function<void, GLuint, GLenum, GLint, GLenum, GLenum, void *> Binding::GetTextureImageEXT("glGetTextureImageEXT");
Function<void, GLuint, GLint, GLenum, GLfloat *> Binding::GetTextureLevelParameterfv("glGetTextureLevelParameterfv");
Function<void, GLuint, GLenum, GLint, GLenum, GLfloat *> Binding::GetTextureLevelParameterfvEXT("glGetTextureLevelParameterfvEXT");
Function<void, GLuint, GLint, GLenum, GLint *> Binding::GetTextureLevelParameteriv("glGetTextureLevelParameteriv");
Function<void, GLuint, GLenum, GLint, GLenum, GLint *> Binding::GetTextureLevelParameterivEXT("glGetTextureLevelParameterivEXT");
Function<void, GLuint, GLenum, GLint *> Binding::GetTextureParameterIiv("glGetTextureParameterIiv");
Function<void, GLuint, GLenum, GLenum, GLint *> Binding::GetTextureParameterIivEXT("glGetTextureParameterIivEXT");
Function<void, GLuint, GLenum, GLuint *> Binding::GetTextureParameterIuiv("glGetTextureParameterIuiv");
Function<void, GLuint, GLenum, GLenum, GLuint *> Binding::GetTextureParameterIuivEXT("glGetTextureParameterIuivEXT");
Function<void, GLuint, GLenum, GLfloat *> Binding::GetTextureParameterfv("glGetTextureParameterfv");
Function<void, GLuint, GLenum, GLenum, GLfloat *> Binding::GetTextureParameterfvEXT("glGetTextureParameterfvEXT");
Function<void, GLuint, GLenum, GLint *> Binding::GetTextureParameteriv("glGetTextureParameteriv");
Function<void, GLuint, GLenum, GLenum, GLint *> Binding::GetTextureParameterivEXT("glGetTextureParameterivEXT");
Function<GLuint64, GLuint, GLuint> Binding::GetTextureSamplerHandleARB("glGetTextureSamplerHandleARB");
Function<GLuint64, GLuint, GLuint> Binding::GetTextureSamplerHandleNV("glGetTextureSamplerHandleNV");
Function<void, GLuint, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLenum, GLsizei, void *> Binding::GetTextureSubImage("glGetTextureSubImage");
Function<void, GLenum, GLuint, GLenum, GLint *> Binding::GetTrackMatrixivNV("glGetTrackMatrixivNV");
Function<void, GLuint, GLuint, GLsizei, GLsizei *, GLsizei *, GLenum *, GLchar *> Binding::GetTransformFeedbackVarying("glGetTransformFeedbackVarying");
Function<void, GLuint, GLuint, GLsizei, GLsizei *, GLsizei *, GLenum *, GLchar *> Binding::GetTransformFeedbackVaryingEXT("glGetTransformFeedbackVaryingEXT");
Function<void, GLuint, GLuint, GLint *> Binding::GetTransformFeedbackVaryingNV("glGetTransformFeedbackVaryingNV");
Function<void, GLuint, GLenum, GLuint, GLint64 *> Binding::GetTransformFeedbacki64_v("glGetTransformFeedbacki64_v");
Function<void, GLuint, GLenum, GLuint, GLint *> Binding::GetTransformFeedbacki_v("glGetTransformFeedbacki_v");
Function<void, GLuint, GLenum, GLint *> Binding::GetTransformFeedbackiv("glGetTransformFeedbackiv");
Function<GLuint, GLuint, const GLchar *> Binding::GetUniformBlockIndex("glGetUniformBlockIndex");
Function<GLint, GLuint, GLint> Binding::GetUniformBufferSizeEXT("glGetUniformBufferSizeEXT");
Function<void, GLuint, GLsizei, const GLchar *const*, GLuint *> Binding::GetUniformIndices("glGetUniformIndices");
Function<GLint, GLuint, const GLchar *> Binding::GetUniformLocation("glGetUniformLocation");
Function<GLint, GLhandleARB, const GLcharARB *> Binding::GetUniformLocationARB("glGetUniformLocationARB");
Function<GLintptr, GLuint, GLint> Binding::GetUniformOffsetEXT("glGetUniformOffsetEXT");
Function<void, GLenum, GLint, GLuint *> Binding::GetUniformSubroutineuiv("glGetUniformSubroutineuiv");
Function<void, GLuint, GLint, GLdouble *> Binding::GetUniformdv("glGetUniformdv");
Function<void, GLuint, GLint, GLfloat *> Binding::GetUniformfv("glGetUniformfv");
Function<void, GLhandleARB, GLint, GLfloat *> Binding::GetUniformfvARB("glGetUniformfvARB");
Function<void, GLuint, GLint, GLint64 *> Binding::GetUniformi64vARB("glGetUniformi64vARB");
Function<void, GLuint, GLint, GLint64EXT *> Binding::GetUniformi64vNV("glGetUniformi64vNV");
Function<void, GLuint, GLint, GLint *> Binding::GetUniformiv("glGetUniformiv");
Function<void, GLhandleARB, GLint, GLint *> Binding::GetUniformivARB("glGetUniformivARB");
Function<void, GLuint, GLint, GLuint64 *> Binding::GetUniformui64vARB("glGetUniformui64vARB");
Function<void, GLuint, GLint, GLuint64EXT *> Binding::GetUniformui64vNV("glGetUniformui64vNV");
Function<void, GLuint, GLint, GLuint *> Binding::GetUniformuiv("glGetUniformuiv");
Function<void, GLuint, GLint, GLuint *> Binding::GetUniformuivEXT("glGetUniformuivEXT");
Function<void, GLuint, GLenum, GLfloat *> Binding::GetVariantArrayObjectfvATI("glGetVariantArrayObjectfvATI");
Function<void, GLuint, GLenum, GLint *> Binding::GetVariantArrayObjectivATI("glGetVariantArrayObjectivATI");
Function<void, GLuint, GLenum, GLboolean *> Binding::GetVariantBooleanvEXT("glGetVariantBooleanvEXT");
Function<void, GLuint, GLenum, GLfloat *> Binding::GetVariantFloatvEXT("glGetVariantFloatvEXT");
Function<void, GLuint, GLenum, GLint *> Binding::GetVariantIntegervEXT("glGetVariantIntegervEXT");
Function<void, GLuint, GLenum, void **> Binding::GetVariantPointervEXT("glGetVariantPointervEXT");
Function<GLint, GLuint, const GLchar *> Binding::GetVaryingLocationNV("glGetVaryingLocationNV");
Function<void, GLuint, GLuint, GLenum, GLint64 *> Binding::GetVertexArrayIndexed64iv("glGetVertexArrayIndexed64iv");
Function<void, GLuint, GLuint, GLenum, GLint *> Binding::GetVertexArrayIndexediv("glGetVertexArrayIndexediv");
Function<void, GLuint, GLuint, GLenum, GLint *> Binding::GetVertexArrayIntegeri_vEXT("glGetVertexArrayIntegeri_vEXT");
Function<void, GLuint, GLenum, GLint *> Binding::GetVertexArrayIntegervEXT("glGetVertexArrayIntegervEXT");
Function<void, GLuint, GLuint, GLenum, void **> Binding::GetVertexArrayPointeri_vEXT("glGetVertexArrayPointeri_vEXT");
Function<void, GLuint, GLenum, void **> Binding::GetVertexArrayPointervEXT("glGetVertexArrayPointervEXT");
Function<void, GLuint, GLenum, GLint *> Binding::GetVertexArrayiv("glGetVertexArrayiv");
Function<void, GLuint, GLenum, GLfloat *> Binding::GetVertexAttribArrayObjectfvATI("glGetVertexAttribArrayObjectfvATI");
Function<void, GLuint, GLenum, GLint *> Binding::GetVertexAttribArrayObjectivATI("glGetVertexAttribArrayObjectivATI");
Function<void, GLuint, GLenum, GLint *> Binding::GetVertexAttribIiv("glGetVertexAttribIiv");
Function<void, GLuint, GLenum, GLint *> Binding::GetVertexAttribIivEXT("glGetVertexAttribIivEXT");
Function<void, GLuint, GLenum, GLuint *> Binding::GetVertexAttribIuiv("glGetVertexAttribIuiv");
Function<void, GLuint, GLenum, GLuint *> Binding::GetVertexAttribIuivEXT("glGetVertexAttribIuivEXT");
Function<void, GLuint, GLenum, GLdouble *> Binding::GetVertexAttribLdv("glGetVertexAttribLdv");
Function<void, GLuint, GLenum, GLdouble *> Binding::GetVertexAttribLdvEXT("glGetVertexAttribLdvEXT");
Function<void, GLuint, GLenum, GLint64EXT *> Binding::GetVertexAttribLi64vNV("glGetVertexAttribLi64vNV");
Function<void, GLuint, GLenum, GLuint64EXT *> Binding::GetVertexAttribLui64vARB("glGetVertexAttribLui64vARB");
Function<void, GLuint, GLenum, GLuint64EXT *> Binding::GetVertexAttribLui64vNV("glGetVertexAttribLui64vNV");
Function<void, GLuint, GLenum, void **> Binding::GetVertexAttribPointerv("glGetVertexAttribPointerv");
Function<void, GLuint, GLenum, void **> Binding::GetVertexAttribPointervARB("glGetVertexAttribPointervARB");
Function<void, GLuint, GLenum, void **> Binding::GetVertexAttribPointervNV("glGetVertexAttribPointervNV");
Function<void, GLuint, GLenum, GLdouble *> Binding::GetVertexAttribdv("glGetVertexAttribdv");
Function<void, GLuint, GLenum, GLdouble *> Binding::GetVertexAttribdvARB("glGetVertexAttribdvARB");
Function<void, GLuint, GLenum, GLdouble *> Binding::GetVertexAttribdvNV("glGetVertexAttribdvNV");
Function<void, GLuint, GLenum, GLfloat *> Binding::GetVertexAttribfv("glGetVertexAttribfv");
Function<void, GLuint, GLenum, GLfloat *> Binding::GetVertexAttribfvARB("glGetVertexAttribfvARB");
Function<void, GLuint, GLenum, GLfloat *> Binding::GetVertexAttribfvNV("glGetVertexAttribfvNV");
Function<void, GLuint, GLenum, GLint *> Binding::GetVertexAttribiv("glGetVertexAttribiv");
Function<void, GLuint, GLenum, GLint *> Binding::GetVertexAttribivARB("glGetVertexAttribivARB");
Function<void, GLuint, GLenum, GLint *> Binding::GetVertexAttribivNV("glGetVertexAttribivNV");
Function<void, GLuint, GLuint, GLenum, GLdouble *> Binding::GetVideoCaptureStreamdvNV("glGetVideoCaptureStreamdvNV");
Function<void, GLuint, GLuint, GLenum, GLfloat *> Binding::GetVideoCaptureStreamfvNV("glGetVideoCaptureStreamfvNV");
Function<void, GLuint, GLuint, GLenum, GLint *> Binding::GetVideoCaptureStreamivNV("glGetVideoCaptureStreamivNV");
Function<void, GLuint, GLenum, GLint *> Binding::GetVideoCaptureivNV("glGetVideoCaptureivNV");
Function<void, GLuint, GLenum, GLint64EXT *> Binding::GetVideoi64vNV("glGetVideoi64vNV");
Function<void, GLuint, GLenum, GLint *> Binding::GetVideoivNV("glGetVideoivNV");
Function<void, GLuint, GLenum, GLuint64EXT *> Binding::GetVideoui64vNV("glGetVideoui64vNV");
Function<void, GLuint, GLenum, GLuint *> Binding::GetVideouivNV("glGetVideouivNV");
Function<void, GLenum, GLenum, GLenum, GLsizei, void *> Binding::GetnColorTable("glGetnColorTable");
Function<void, GLenum, GLenum, GLenum, GLsizei, void *> Binding::GetnColorTableARB("glGetnColorTableARB");
Function<void, GLenum, GLint, GLsizei, void *> Binding::GetnCompressedTexImage("glGetnCompressedTexImage");
Function<void, GLenum, GLint, GLsizei, void *> Binding::GetnCompressedTexImageARB("glGetnCompressedTexImageARB");
Function<void, GLenum, GLenum, GLenum, GLsizei, void *> Binding::GetnConvolutionFilter("glGetnConvolutionFilter");
Function<void, GLenum, GLenum, GLenum, GLsizei, void *> Binding::GetnConvolutionFilterARB("glGetnConvolutionFilterARB");
Function<void, GLenum, GLboolean, GLenum, GLenum, GLsizei, void *> Binding::GetnHistogram("glGetnHistogram");
Function<void, GLenum, GLboolean, GLenum, GLenum, GLsizei, void *> Binding::GetnHistogramARB("glGetnHistogramARB");
Function<void, GLenum, GLenum, GLsizei, GLdouble *> Binding::GetnMapdv("glGetnMapdv");
Function<void, GLenum, GLenum, GLsizei, GLdouble *> Binding::GetnMapdvARB("glGetnMapdvARB");
Function<void, GLenum, GLenum, GLsizei, GLfloat *> Binding::GetnMapfv("glGetnMapfv");
Function<void, GLenum, GLenum, GLsizei, GLfloat *> Binding::GetnMapfvARB("glGetnMapfvARB");
Function<void, GLenum, GLenum, GLsizei, GLint *> Binding::GetnMapiv("glGetnMapiv");
Function<void, GLenum, GLenum, GLsizei, GLint *> Binding::GetnMapivARB("glGetnMapivARB");
Function<void, GLenum, GLboolean, GLenum, GLenum, GLsizei, void *> Binding::GetnMinmax("glGetnMinmax");
Function<void, GLenum, GLboolean, GLenum, GLenum, GLsizei, void *> Binding::GetnMinmaxARB("glGetnMinmaxARB");
Function<void, GLenum, GLsizei, GLfloat *> Binding::GetnPixelMapfv("glGetnPixelMapfv");
Function<void, GLenum, GLsizei, GLfloat *> Binding::GetnPixelMapfvARB("glGetnPixelMapfvARB");
Function<void, GLenum, GLsizei, GLuint *> Binding::GetnPixelMapuiv("glGetnPixelMapuiv");
Function<void, GLenum, GLsizei, GLuint *> Binding::GetnPixelMapuivARB("glGetnPixelMapuivARB");
Function<void, GLenum, GLsizei, GLushort *> Binding::GetnPixelMapusv("glGetnPixelMapusv");
Function<void, GLenum, GLsizei, GLushort *> Binding::GetnPixelMapusvARB("glGetnPixelMapusvARB");
Function<void, GLsizei, GLubyte *> Binding::GetnPolygonStipple("glGetnPolygonStipple");
Function<void, GLsizei, GLubyte *> Binding::GetnPolygonStippleARB("glGetnPolygonStippleARB");
Function<void, GLenum, GLenum, GLenum, GLsizei, void *, GLsizei, void *, void *> Binding::GetnSeparableFilter("glGetnSeparableFilter");
Function<void, GLenum, GLenum, GLenum, GLsizei, void *, GLsizei, void *, void *> Binding::GetnSeparableFilterARB("glGetnSeparableFilterARB");
Function<void, GLenum, GLint, GLenum, GLenum, GLsizei, void *> Binding::GetnTexImage("glGetnTexImage");
Function<void, GLenum, GLint, GLenum, GLenum, GLsizei, void *> Binding::GetnTexImageARB("glGetnTexImageARB");
Function<void, GLuint, GLint, GLsizei, GLdouble *> Binding::GetnUniformdv("glGetnUniformdv");
Function<void, GLuint, GLint, GLsizei, GLdouble *> Binding::GetnUniformdvARB("glGetnUniformdvARB");
Function<void, GLuint, GLint, GLsizei, GLfloat *> Binding::GetnUniformfv("glGetnUniformfv");
Function<void, GLuint, GLint, GLsizei, GLfloat *> Binding::GetnUniformfvARB("glGetnUniformfvARB");
Function<void, GLuint, GLint, GLsizei, GLint64 *> Binding::GetnUniformi64vARB("glGetnUniformi64vARB");
Function<void, GLuint, GLint, GLsizei, GLint *> Binding::GetnUniformiv("glGetnUniformiv");
Function<void, GLuint, GLint, GLsizei, GLint *> Binding::GetnUniformivARB("glGetnUniformivARB");
Function<void, GLuint, GLint, GLsizei, GLuint64 *> Binding::GetnUniformui64vARB("glGetnUniformui64vARB");
Function<void, GLuint, GLint, GLsizei, GLuint *> Binding::GetnUniformuiv("glGetnUniformuiv");
Function<void, GLuint, GLint, GLsizei, GLuint *> Binding::GetnUniformuivARB("glGetnUniformuivARB");
Function<void, GLbyte> Binding::GlobalAlphaFactorbSUN("glGlobalAlphaFactorbSUN");
Function<void, GLdouble> Binding::GlobalAlphaFactordSUN("glGlobalAlphaFactordSUN");
Function<void, GLfloat> Binding::GlobalAlphaFactorfSUN("glGlobalAlphaFactorfSUN");
Function<void, GLint> Binding::GlobalAlphaFactoriSUN("glGlobalAlphaFactoriSUN");
Function<void, GLshort> Binding::GlobalAlphaFactorsSUN("glGlobalAlphaFactorsSUN");
Function<void, GLubyte> Binding::GlobalAlphaFactorubSUN("glGlobalAlphaFactorubSUN");
Function<void, GLuint> Binding::GlobalAlphaFactoruiSUN("glGlobalAlphaFactoruiSUN");
Function<void, GLushort> Binding::GlobalAlphaFactorusSUN("glGlobalAlphaFactorusSUN");
Function<void, GLenum, GLenum> Binding::Hint("glHint");
Function<void, GLenum, GLint> Binding::HintPGI("glHintPGI");
Function<void, GLenum, GLsizei, GLenum, GLboolean> Binding::Histogram("glHistogram");
Function<void, GLenum, GLsizei, GLenum, GLboolean> Binding::HistogramEXT("glHistogramEXT");
Function<void, GLenum, const void *> Binding::IglooInterfaceSGIX("glIglooInterfaceSGIX");
Function<void, GLenum, GLenum, GLfloat> Binding::ImageTransformParameterfHP("glImageTransformParameterfHP");
Function<void, GLenum, GLenum, const GLfloat *> Binding::ImageTransformParameterfvHP("glImageTransformParameterfvHP");
Function<void, GLenum, GLenum, GLint> Binding::ImageTransformParameteriHP("glImageTransformParameteriHP");
Function<void, GLenum, GLenum, const GLint *> Binding::ImageTransformParameterivHP("glImageTransformParameterivHP");
Function<GLsync, GLenum, GLintptr, UnusedMask> Binding::ImportSyncEXT("glImportSyncEXT");
Function<void, GLenum, GLsizei> Binding::IndexFormatNV("glIndexFormatNV");
Function<void, GLenum, GLclampf> Binding::IndexFuncEXT("glIndexFuncEXT");
Function<void, GLuint> Binding::IndexMask("glIndexMask");
Function<void, GLenum, GLenum> Binding::IndexMaterialEXT("glIndexMaterialEXT");
Function<void, GLenum, GLsizei, const void *> Binding::IndexPointer("glIndexPointer");
Function<void, GLenum, GLsizei, GLsizei, const void *> Binding::IndexPointerEXT("glIndexPointerEXT");
Function<void, GLenum, GLint, const void **, GLint> Binding::IndexPointerListIBM("glIndexPointerListIBM");
Function<void, GLdouble> Binding::Indexd("glIndexd");
Function<void, const GLdouble *> Binding::Indexdv("glIndexdv");
Function<void, GLfloat> Binding::Indexf("glIndexf");
Function<void, const GLfloat *> Binding::Indexfv("glIndexfv");
Function<void, GLint> Binding::Indexi("glIndexi");
Function<void, const GLint *> Binding::Indexiv("glIndexiv");
Function<void, GLshort> Binding::Indexs("glIndexs");
Function<void, const GLshort *> Binding::Indexsv("glIndexsv");
Function<void, GLubyte> Binding::Indexub("glIndexub");
Function<void, const GLubyte *> Binding::Indexubv("glIndexubv");
Function<void, GLfixed> Binding::IndexxOES("glIndexxOES");
Function<void, const GLfixed *> Binding::IndexxvOES("glIndexxvOES");
Function<void> Binding::InitNames("glInitNames");
Function<void, GLuint, GLuint, GLuint> Binding::InsertComponentEXT("glInsertComponentEXT");
Function<void, GLsizei, const GLchar *> Binding::InsertEventMarkerEXT("glInsertEventMarkerEXT");
Function<void, GLsizei, GLint *> Binding::InstrumentsBufferSGIX("glInstrumentsBufferSGIX");
Function<void, GLenum, GLsizei, const void *> Binding::InterleavedArrays("glInterleavedArrays");
Function<void, GLuint, GLuint, GLuint, GLfloat> Binding::InterpolatePathsNV("glInterpolatePathsNV");
Function<void, GLuint> Binding::InvalidateBufferData("glInvalidateBufferData");
Function<void, GLuint, GLintptr, GLsizeiptr> Binding::InvalidateBufferSubData("glInvalidateBufferSubData");
Function<void, GLenum, GLsizei, const GLenum *> Binding::InvalidateFramebuffer("glInvalidateFramebuffer");
Function<void, GLuint, GLsizei, const GLenum *> Binding::InvalidateNamedFramebufferData("glInvalidateNamedFramebufferData");
Function<void, GLuint, GLsizei, const GLenum *, GLint, GLint, GLsizei, GLsizei> Binding::InvalidateNamedFramebufferSubData("glInvalidateNamedFramebufferSubData");
Function<void, GLenum, GLsizei, const GLenum *, GLint, GLint, GLsizei, GLsizei> Binding::InvalidateSubFramebuffer("glInvalidateSubFramebuffer");
Function<void, GLuint, GLint> Binding::InvalidateTexImage("glInvalidateTexImage");
Function<void, GLuint, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei> Binding::InvalidateTexSubImage("glInvalidateTexSubImage");
Function<GLboolean, GLuint> Binding::IsAsyncMarkerSGIX("glIsAsyncMarkerSGIX");
Function<GLboolean, GLuint> Binding::IsBuffer("glIsBuffer");
Function<GLboolean, GLuint> Binding::IsBufferARB("glIsBufferARB");
Function<GLboolean, GLenum> Binding::IsBufferResidentNV("glIsBufferResidentNV");
Function<GLboolean, GLuint> Binding::IsCommandListNV("glIsCommandListNV");
Function<GLboolean, GLenum> Binding::IsEnabled("glIsEnabled");
Function<GLboolean, GLenum, GLuint> Binding::IsEnabledIndexedEXT("glIsEnabledIndexedEXT");
Function<GLboolean, GLenum, GLuint> Binding::IsEnabledi("glIsEnabledi");
Function<GLboolean, GLuint> Binding::IsFenceAPPLE("glIsFenceAPPLE");
Function<GLboolean, GLuint> Binding::IsFenceNV("glIsFenceNV");
Function<GLboolean, GLuint> Binding::IsFramebuffer("glIsFramebuffer");
Function<GLboolean, GLuint> Binding::IsFramebufferEXT("glIsFramebufferEXT");
Function<GLboolean, GLuint64> Binding::IsImageHandleResidentARB("glIsImageHandleResidentARB");
Function<GLboolean, GLuint64> Binding::IsImageHandleResidentNV("glIsImageHandleResidentNV");
Function<GLboolean, GLuint> Binding::IsList("glIsList");
Function<GLboolean, GLenum, GLuint> Binding::IsNameAMD("glIsNameAMD");
Function<GLboolean, GLuint> Binding::IsNamedBufferResidentNV("glIsNamedBufferResidentNV");
Function<GLboolean, GLint, const GLchar *> Binding::IsNamedStringARB("glIsNamedStringARB");
Function<GLboolean, GLuint> Binding::IsObjectBufferATI("glIsObjectBufferATI");
Function<GLboolean, GLuint> Binding::IsOcclusionQueryNV("glIsOcclusionQueryNV");
Function<GLboolean, GLuint> Binding::IsPathNV("glIsPathNV");
Function<GLboolean, GLuint, GLuint, GLfloat, GLfloat> Binding::IsPointInFillPathNV("glIsPointInFillPathNV");
Function<GLboolean, GLuint, GLfloat, GLfloat> Binding::IsPointInStrokePathNV("glIsPointInStrokePathNV");
Function<GLboolean, GLuint> Binding::IsProgram("glIsProgram");
Function<GLboolean, GLuint> Binding::IsProgramARB("glIsProgramARB");
Function<GLboolean, GLuint> Binding::IsProgramNV("glIsProgramNV");
Function<GLboolean, GLuint> Binding::IsProgramPipeline("glIsProgramPipeline");
Function<GLboolean, GLuint> Binding::IsQuery("glIsQuery");
Function<GLboolean, GLuint> Binding::IsQueryARB("glIsQueryARB");
Function<GLboolean, GLuint> Binding::IsRenderbuffer("glIsRenderbuffer");
Function<GLboolean, GLuint> Binding::IsRenderbufferEXT("glIsRenderbufferEXT");
Function<GLboolean, GLuint> Binding::IsSampler("glIsSampler");
Function<GLboolean, GLuint> Binding::IsShader("glIsShader");
Function<GLboolean, GLuint> Binding::IsStateNV("glIsStateNV");
Function<GLboolean, GLsync> Binding::IsSync("glIsSync");
Function<GLboolean, GLuint> Binding::IsTexture("glIsTexture");
Function<GLboolean, GLuint> Binding::IsTextureEXT("glIsTextureEXT");
Function<GLboolean, GLuint64> Binding::IsTextureHandleResidentARB("glIsTextureHandleResidentARB");
Function<GLboolean, GLuint64> Binding::IsTextureHandleResidentNV("glIsTextureHandleResidentNV");
Function<GLboolean, GLuint> Binding::IsTransformFeedback("glIsTransformFeedback");
Function<GLboolean, GLuint> Binding::IsTransformFeedbackNV("glIsTransformFeedbackNV");
Function<GLboolean, GLuint, GLenum> Binding::IsVariantEnabledEXT("glIsVariantEnabledEXT");
Function<GLboolean, GLuint> Binding::IsVertexArray("glIsVertexArray");
Function<GLboolean, GLuint> Binding::IsVertexArrayAPPLE("glIsVertexArrayAPPLE");
Function<GLboolean, GLuint, GLenum> Binding::IsVertexAttribEnabledAPPLE("glIsVertexAttribEnabledAPPLE");
Function<void, GLenum, GLuint, GLsizei, const GLchar *> Binding::LabelObjectEXT("glLabelObjectEXT");
Function<void, GLenum, GLint> Binding::LightEnviSGIX("glLightEnviSGIX");
Function<void, GLenum, GLfloat> Binding::LightModelf("glLightModelf");
Function<void, GLenum, const GLfloat *> Binding::LightModelfv("glLightModelfv");
Function<void, GLenum, GLint> Binding::LightModeli("glLightModeli");
Function<void, GLenum, const GLint *> Binding::LightModeliv("glLightModeliv");
Function<void, GLenum, GLfixed> Binding::LightModelxOES("glLightModelxOES");
Function<void, GLenum, const GLfixed *> Binding::LightModelxvOES("glLightModelxvOES");
Function<void, GLenum, GLenum, GLfloat> Binding::Lightf("glLightf");
Function<void, GLenum, GLenum, const GLfloat *> Binding::Lightfv("glLightfv");
Function<void, GLenum, GLenum, GLint> Binding::Lighti("glLighti");
Function<void, GLenum, GLenum, const GLint *> Binding::Lightiv("glLightiv");
Function<void, GLenum, GLenum, GLfixed> Binding::LightxOES("glLightxOES");
Function<void, GLenum, GLenum, const GLfixed *> Binding::LightxvOES("glLightxvOES");
Function<void, GLint, GLushort> Binding::LineStipple("glLineStipple");
Function<void, GLfloat> Binding::LineWidth("glLineWidth");
Function<void, GLfixed> Binding::LineWidthxOES("glLineWidthxOES");
Function<void, GLuint> Binding::LinkProgram("glLinkProgram");
Function<void, GLhandleARB> Binding::LinkProgramARB("glLinkProgramARB");
Function<void, GLuint> Binding::ListBase("glListBase");
Function<void, GLuint, GLuint, const void **, const GLsizei *, const GLuint *, const GLuint *, GLuint> Binding::ListDrawCommandsStatesClientNV("glListDrawCommandsStatesClientNV");
Function<void, GLuint, GLenum, GLfloat> Binding::ListParameterfSGIX("glListParameterfSGIX");
Function<void, GLuint, GLenum, const GLfloat *> Binding::ListParameterfvSGIX("glListParameterfvSGIX");
Function<void, GLuint, GLenum, GLint> Binding::ListParameteriSGIX("glListParameteriSGIX");
Function<void, GLuint, GLenum, const GLint *> Binding::ListParameterivSGIX("glListParameterivSGIX");
Function<void> Binding::LoadIdentity("glLoadIdentity");
Function<void, FfdMaskSGIX> Binding::LoadIdentityDeformationMapSGIX("glLoadIdentityDeformationMapSGIX");
Function<void, const GLdouble *> Binding::LoadMatrixd("glLoadMatrixd");
Function<void, const GLfloat *> Binding::LoadMatrixf("glLoadMatrixf");
Function<void, const GLfixed *> Binding::LoadMatrixxOES("glLoadMatrixxOES");
Function<void, GLuint> Binding::LoadName("glLoadName");
Function<void, GLenum, GLuint, GLsizei, const GLubyte *> Binding::LoadProgramNV("glLoadProgramNV");
Function<void, const GLdouble *> Binding::LoadTransposeMatrixd("glLoadTransposeMatrixd");
Function<void, const GLdouble *> Binding::LoadTransposeMatrixdARB("glLoadTransposeMatrixdARB");
Function<void, const GLfloat *> Binding::LoadTransposeMatrixf("glLoadTransposeMatrixf");
Function<void, const GLfloat *> Binding::LoadTransposeMatrixfARB("glLoadTransposeMatrixfARB");
Function<void, const GLfixed *> Binding::LoadTransposeMatrixxOES("glLoadTransposeMatrixxOES");
Function<void, GLint, GLsizei> Binding::LockArraysEXT("glLockArraysEXT");
Function<void, GLenum> Binding::LogicOp("glLogicOp");
Function<void, GLenum> Binding::MakeBufferNonResidentNV("glMakeBufferNonResidentNV");
Function<void, GLenum, GLenum> Binding::MakeBufferResidentNV("glMakeBufferResidentNV");
Function<void, GLuint64> Binding::MakeImageHandleNonResidentARB("glMakeImageHandleNonResidentARB");
Function<void, GLuint64> Binding::MakeImageHandleNonResidentNV("glMakeImageHandleNonResidentNV");
Function<void, GLuint64, GLenum> Binding::MakeImageHandleResidentARB("glMakeImageHandleResidentARB");
Function<void, GLuint64, GLenum> Binding::MakeImageHandleResidentNV("glMakeImageHandleResidentNV");
Function<void, GLuint> Binding::MakeNamedBufferNonResidentNV("glMakeNamedBufferNonResidentNV");
Function<void, GLuint, GLenum> Binding::MakeNamedBufferResidentNV("glMakeNamedBufferResidentNV");
Function<void, GLuint64> Binding::MakeTextureHandleNonResidentARB("glMakeTextureHandleNonResidentARB");
Function<void, GLuint64> Binding::MakeTextureHandleNonResidentNV("glMakeTextureHandleNonResidentNV");
Function<void, GLuint64> Binding::MakeTextureHandleResidentARB("glMakeTextureHandleResidentARB");
Function<void, GLuint64> Binding::MakeTextureHandleResidentNV("glMakeTextureHandleResidentNV");
Function<void, GLenum, GLdouble, GLdouble, GLint, GLint, const GLdouble *> Binding::Map1d("glMap1d");
Function<void, GLenum, GLfloat, GLfloat, GLint, GLint, const GLfloat *> Binding::Map1f("glMap1f");
Function<void, GLenum, GLfixed, GLfixed, GLint, GLint, GLfixed> Binding::Map1xOES("glMap1xOES");
Function<void, GLenum, GLdouble, GLdouble, GLint, GLint, GLdouble, GLdouble, GLint, GLint, const GLdouble *> Binding::Map2d("glMap2d");
Function<void, GLenum, GLfloat, GLfloat, GLint, GLint, GLfloat, GLfloat, GLint, GLint, const GLfloat *> Binding::Map2f("glMap2f");
Function<void, GLenum, GLfixed, GLfixed, GLint, GLint, GLfixed, GLfixed, GLint, GLint, GLfixed> Binding::Map2xOES("glMap2xOES");
Function<void *, GLenum, GLenum> Binding::MapBuffer("glMapBuffer");
Function<void *, GLenum, GLenum> Binding::MapBufferARB("glMapBufferARB");
Function<void *, GLenum, GLintptr, GLsizeiptr, BufferAccessMask> Binding::MapBufferRange("glMapBufferRange");
Function<void, GLenum, GLuint, GLenum, GLsizei, GLsizei, GLint, GLint, GLboolean, const void *> Binding::MapControlPointsNV("glMapControlPointsNV");
Function<void, GLint, GLdouble, GLdouble> Binding::MapGrid1d("glMapGrid1d");
Function<void, GLint, GLfloat, GLfloat> Binding::MapGrid1f("glMapGrid1f");
Function<void, GLint, GLfixed, GLfixed> Binding::MapGrid1xOES("glMapGrid1xOES");
Function<void, GLint, GLdouble, GLdouble, GLint, GLdouble, GLdouble> Binding::MapGrid2d("glMapGrid2d");
Function<void, GLint, GLfloat, GLfloat, GLint, GLfloat, GLfloat> Binding::MapGrid2f("glMapGrid2f");
Function<void, GLint, GLfixed, GLfixed, GLfixed, GLfixed> Binding::MapGrid2xOES("glMapGrid2xOES");
Function<void *, GLuint, GLenum> Binding::MapNamedBuffer("glMapNamedBuffer");
Function<void *, GLuint, GLenum> Binding::MapNamedBufferEXT("glMapNamedBufferEXT");
Function<void *, GLuint, GLintptr, GLsizeiptr, BufferAccessMask> Binding::MapNamedBufferRange("glMapNamedBufferRange");
Function<void *, GLuint, GLintptr, GLsizeiptr, BufferAccessMask> Binding::MapNamedBufferRangeEXT("glMapNamedBufferRangeEXT");
Function<void *, GLuint> Binding::MapObjectBufferATI("glMapObjectBufferATI");
Function<void, GLenum, GLenum, const GLfloat *> Binding::MapParameterfvNV("glMapParameterfvNV");
Function<void, GLenum, GLenum, const GLint *> Binding::MapParameterivNV("glMapParameterivNV");
Function<void *, GLuint, GLint, MapBufferUsageMask, GLint *, GLenum *> Binding::MapTexture2DINTEL("glMapTexture2DINTEL");
Function<void, GLuint, GLuint, GLdouble, GLdouble, GLint, GLint, const GLdouble *> Binding::MapVertexAttrib1dAPPLE("glMapVertexAttrib1dAPPLE");
Function<void, GLuint, GLuint, GLfloat, GLfloat, GLint, GLint, const GLfloat *> Binding::MapVertexAttrib1fAPPLE("glMapVertexAttrib1fAPPLE");
Function<void, GLuint, GLuint, GLdouble, GLdouble, GLint, GLint, GLdouble, GLdouble, GLint, GLint, const GLdouble *> Binding::MapVertexAttrib2dAPPLE("glMapVertexAttrib2dAPPLE");
Function<void, GLuint, GLuint, GLfloat, GLfloat, GLint, GLint, GLfloat, GLfloat, GLint, GLint, const GLfloat *> Binding::MapVertexAttrib2fAPPLE("glMapVertexAttrib2fAPPLE");
Function<void, GLenum, GLenum, GLfloat> Binding::Materialf("glMaterialf");
Function<void, GLenum, GLenum, const GLfloat *> Binding::Materialfv("glMaterialfv");
Function<void, GLenum, GLenum, GLint> Binding::Materiali("glMateriali");
Function<void, GLenum, GLenum, const GLint *> Binding::Materialiv("glMaterialiv");
Function<void, GLenum, GLenum, GLfixed> Binding::MaterialxOES("glMaterialxOES");
Function<void, GLenum, GLenum, const GLfixed *> Binding::MaterialxvOES("glMaterialxvOES");
Function<void, GLenum, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble> Binding::MatrixFrustumEXT("glMatrixFrustumEXT");
Function<void, GLint, GLenum, GLsizei, const void *> Binding::MatrixIndexPointerARB("glMatrixIndexPointerARB");
Function<void, GLint, const GLubyte *> Binding::MatrixIndexubvARB("glMatrixIndexubvARB");
Function<void, GLint, const GLuint *> Binding::MatrixIndexuivARB("glMatrixIndexuivARB");
Function<void, GLint, const GLushort *> Binding::MatrixIndexusvARB("glMatrixIndexusvARB");
Function<void, GLenum, const GLfloat *> Binding::MatrixLoad3x2fNV("glMatrixLoad3x2fNV");
Function<void, GLenum, const GLfloat *> Binding::MatrixLoad3x3fNV("glMatrixLoad3x3fNV");
Function<void, GLenum> Binding::MatrixLoadIdentityEXT("glMatrixLoadIdentityEXT");
Function<void, GLenum, const GLfloat *> Binding::MatrixLoadTranspose3x3fNV("glMatrixLoadTranspose3x3fNV");
Function<void, GLenum, const GLdouble *> Binding::MatrixLoadTransposedEXT("glMatrixLoadTransposedEXT");
Function<void, GLenum, const GLfloat *> Binding::MatrixLoadTransposefEXT("glMatrixLoadTransposefEXT");
Function<void, GLenum, const GLdouble *> Binding::MatrixLoaddEXT("glMatrixLoaddEXT");
Function<void, GLenum, const GLfloat *> Binding::MatrixLoadfEXT("glMatrixLoadfEXT");
Function<void, GLenum> Binding::MatrixMode("glMatrixMode");
Function<void, GLenum, const GLfloat *> Binding::MatrixMult3x2fNV("glMatrixMult3x2fNV");
Function<void, GLenum, const GLfloat *> Binding::MatrixMult3x3fNV("glMatrixMult3x3fNV");
Function<void, GLenum, const GLfloat *> Binding::MatrixMultTranspose3x3fNV("glMatrixMultTranspose3x3fNV");
Function<void, GLenum, const GLdouble *> Binding::MatrixMultTransposedEXT("glMatrixMultTransposedEXT");
Function<void, GLenum, const GLfloat *> Binding::MatrixMultTransposefEXT("glMatrixMultTransposefEXT");
Function<void, GLenum, const GLdouble *> Binding::MatrixMultdEXT("glMatrixMultdEXT");
Function<void, GLenum, const GLfloat *> Binding::MatrixMultfEXT("glMatrixMultfEXT");
Function<void, GLenum, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble> Binding::MatrixOrthoEXT("glMatrixOrthoEXT");
Function<void, GLenum> Binding::MatrixPopEXT("glMatrixPopEXT");
Function<void, GLenum> Binding::MatrixPushEXT("glMatrixPushEXT");
Function<void, GLenum, GLdouble, GLdouble, GLdouble, GLdouble> Binding::MatrixRotatedEXT("glMatrixRotatedEXT");
Function<void, GLenum, GLfloat, GLfloat, GLfloat, GLfloat> Binding::MatrixRotatefEXT("glMatrixRotatefEXT");
Function<void, GLenum, GLdouble, GLdouble, GLdouble> Binding::MatrixScaledEXT("glMatrixScaledEXT");
Function<void, GLenum, GLfloat, GLfloat, GLfloat> Binding::MatrixScalefEXT("glMatrixScalefEXT");
Function<void, GLenum, GLdouble, GLdouble, GLdouble> Binding::MatrixTranslatedEXT("glMatrixTranslatedEXT");
Function<void, GLenum, GLfloat, GLfloat, GLfloat> Binding::MatrixTranslatefEXT("glMatrixTranslatefEXT");
Function<void, GLuint> Binding::MaxShaderCompilerThreadsARB("glMaxShaderCompilerThreadsARB");
Function<void, MemoryBarrierMask> Binding::MemoryBarrier("glMemoryBarrier");
Function<void, MemoryBarrierMask> Binding::MemoryBarrierByRegion("glMemoryBarrierByRegion");
Function<void, MemoryBarrierMask> Binding::MemoryBarrierEXT("glMemoryBarrierEXT");
Function<void, GLfloat> Binding::MinSampleShading("glMinSampleShading");
Function<void, GLfloat> Binding::MinSampleShadingARB("glMinSampleShadingARB");
Function<void, GLenum, GLenum, GLboolean> Binding::Minmax("glMinmax");
Function<void, GLenum, GLenum, GLboolean> Binding::MinmaxEXT("glMinmaxEXT");
Function<void, const GLdouble *> Binding::MultMatrixd("glMultMatrixd");
Function<void, const GLfloat *> Binding::MultMatrixf("glMultMatrixf");
Function<void, const GLfixed *> Binding::MultMatrixxOES("glMultMatrixxOES");
Function<void, const GLdouble *> Binding::MultTransposeMatrixd("glMultTransposeMatrixd");
Function<void, const GLdouble *> Binding::MultTransposeMatrixdARB("glMultTransposeMatrixdARB");
Function<void, const GLfloat *> Binding::MultTransposeMatrixf("glMultTransposeMatrixf");
Function<void, const GLfloat *> Binding::MultTransposeMatrixfARB("glMultTransposeMatrixfARB");
Function<void, const GLfixed *> Binding::MultTransposeMatrixxOES("glMultTransposeMatrixxOES");
Function<void, GLenum, const GLint *, const GLsizei *, GLsizei> Binding::MultiDrawArrays("glMultiDrawArrays");
Function<void, GLenum, const GLint *, const GLsizei *, GLsizei> Binding::MultiDrawArraysEXT("glMultiDrawArraysEXT");
Function<void, GLenum, const void *, GLsizei, GLsizei> Binding::MultiDrawArraysIndirect("glMultiDrawArraysIndirect");
Function<void, GLenum, const void *, GLsizei, GLsizei> Binding::MultiDrawArraysIndirectAMD("glMultiDrawArraysIndirectAMD");
Function<void, GLenum, const void *, GLsizei, GLsizei, GLsizei, GLint> Binding::MultiDrawArraysIndirectBindlessCountNV("glMultiDrawArraysIndirectBindlessCountNV");
Function<void, GLenum, const void *, GLsizei, GLsizei, GLint> Binding::MultiDrawArraysIndirectBindlessNV("glMultiDrawArraysIndirectBindlessNV");
Function<void, GLenum, GLintptr, GLintptr, GLsizei, GLsizei> Binding::MultiDrawArraysIndirectCountARB("glMultiDrawArraysIndirectCountARB");
Function<void, GLenum, const GLint *, const GLsizei *, GLsizei> Binding::MultiDrawElementArrayAPPLE("glMultiDrawElementArrayAPPLE");
Function<void, GLenum, const GLsizei *, GLenum, const void *const*, GLsizei> Binding::MultiDrawElements("glMultiDrawElements");
Function<void, GLenum, const GLsizei *, GLenum, const void *const*, GLsizei, const GLint *> Binding::MultiDrawElementsBaseVertex("glMultiDrawElementsBaseVertex");
Function<void, GLenum, const GLsizei *, GLenum, const void *const*, GLsizei> Binding::MultiDrawElementsEXT("glMultiDrawElementsEXT");
Function<void, GLenum, GLenum, const void *, GLsizei, GLsizei> Binding::MultiDrawElementsIndirect("glMultiDrawElementsIndirect");
Function<void, GLenum, GLenum, const void *, GLsizei, GLsizei> Binding::MultiDrawElementsIndirectAMD("glMultiDrawElementsIndirectAMD");
Function<void, GLenum, GLenum, const void *, GLsizei, GLsizei, GLsizei, GLint> Binding::MultiDrawElementsIndirectBindlessCountNV("glMultiDrawElementsIndirectBindlessCountNV");
Function<void, GLenum, GLenum, const void *, GLsizei, GLsizei, GLint> Binding::MultiDrawElementsIndirectBindlessNV("glMultiDrawElementsIndirectBindlessNV");
Function<void, GLenum, GLenum, GLintptr, GLintptr, GLsizei, GLsizei> Binding::MultiDrawElementsIndirectCountARB("glMultiDrawElementsIndirectCountARB");
Function<void, GLenum, GLuint, GLuint, const GLint *, const GLsizei *, GLsizei> Binding::MultiDrawRangeElementArrayAPPLE("glMultiDrawRangeElementArrayAPPLE");
Function<void, const GLenum *, const GLint *, const GLsizei *, GLsizei, GLint> Binding::MultiModeDrawArraysIBM("glMultiModeDrawArraysIBM");
Function<void, const GLenum *, const GLsizei *, GLenum, const void *const*, GLsizei, GLint> Binding::MultiModeDrawElementsIBM("glMultiModeDrawElementsIBM");
Function<void, GLenum, GLenum, GLenum, GLuint> Binding::MultiTexBufferEXT("glMultiTexBufferEXT");
Function<void, GLenum, GLbyte> Binding::MultiTexCoord1bOES("glMultiTexCoord1bOES");
Function<void, GLenum, const GLbyte *> Binding::MultiTexCoord1bvOES("glMultiTexCoord1bvOES");
Function<void, GLenum, GLdouble> Binding::MultiTexCoord1d("glMultiTexCoord1d");
Function<void, GLenum, GLdouble> Binding::MultiTexCoord1dARB("glMultiTexCoord1dARB");
Function<void, GLenum, const GLdouble *> Binding::MultiTexCoord1dv("glMultiTexCoord1dv");
Function<void, GLenum, const GLdouble *> Binding::MultiTexCoord1dvARB("glMultiTexCoord1dvARB");
Function<void, GLenum, GLfloat> Binding::MultiTexCoord1f("glMultiTexCoord1f");
Function<void, GLenum, GLfloat> Binding::MultiTexCoord1fARB("glMultiTexCoord1fARB");
Function<void, GLenum, const GLfloat *> Binding::MultiTexCoord1fv("glMultiTexCoord1fv");
Function<void, GLenum, const GLfloat *> Binding::MultiTexCoord1fvARB("glMultiTexCoord1fvARB");
Function<void, GLenum, GLhalfNV> Binding::MultiTexCoord1hNV("glMultiTexCoord1hNV");
Function<void, GLenum, const GLhalfNV *> Binding::MultiTexCoord1hvNV("glMultiTexCoord1hvNV");
Function<void, GLenum, GLint> Binding::MultiTexCoord1i("glMultiTexCoord1i");
Function<void, GLenum, GLint> Binding::MultiTexCoord1iARB("glMultiTexCoord1iARB");
Function<void, GLenum, const GLint *> Binding::MultiTexCoord1iv("glMultiTexCoord1iv");
Function<void, GLenum, const GLint *> Binding::MultiTexCoord1ivARB("glMultiTexCoord1ivARB");
Function<void, GLenum, GLshort> Binding::MultiTexCoord1s("glMultiTexCoord1s");
Function<void, GLenum, GLshort> Binding::MultiTexCoord1sARB("glMultiTexCoord1sARB");
Function<void, GLenum, const GLshort *> Binding::MultiTexCoord1sv("glMultiTexCoord1sv");
Function<void, GLenum, const GLshort *> Binding::MultiTexCoord1svARB("glMultiTexCoord1svARB");
Function<void, GLenum, GLfixed> Binding::MultiTexCoord1xOES("glMultiTexCoord1xOES");
Function<void, GLenum, const GLfixed *> Binding::MultiTexCoord1xvOES("glMultiTexCoord1xvOES");
Function<void, GLenum, GLbyte, GLbyte> Binding::MultiTexCoord2bOES("glMultiTexCoord2bOES");
Function<void, GLenum, const GLbyte *> Binding::MultiTexCoord2bvOES("glMultiTexCoord2bvOES");
Function<void, GLenum, GLdouble, GLdouble> Binding::MultiTexCoord2d("glMultiTexCoord2d");
Function<void, GLenum, GLdouble, GLdouble> Binding::MultiTexCoord2dARB("glMultiTexCoord2dARB");
Function<void, GLenum, const GLdouble *> Binding::MultiTexCoord2dv("glMultiTexCoord2dv");
Function<void, GLenum, const GLdouble *> Binding::MultiTexCoord2dvARB("glMultiTexCoord2dvARB");
Function<void, GLenum, GLfloat, GLfloat> Binding::MultiTexCoord2f("glMultiTexCoord2f");
Function<void, GLenum, GLfloat, GLfloat> Binding::MultiTexCoord2fARB("glMultiTexCoord2fARB");
Function<void, GLenum, const GLfloat *> Binding::MultiTexCoord2fv("glMultiTexCoord2fv");
Function<void, GLenum, const GLfloat *> Binding::MultiTexCoord2fvARB("glMultiTexCoord2fvARB");
Function<void, GLenum, GLhalfNV, GLhalfNV> Binding::MultiTexCoord2hNV("glMultiTexCoord2hNV");
Function<void, GLenum, const GLhalfNV *> Binding::MultiTexCoord2hvNV("glMultiTexCoord2hvNV");
Function<void, GLenum, GLint, GLint> Binding::MultiTexCoord2i("glMultiTexCoord2i");
Function<void, GLenum, GLint, GLint> Binding::MultiTexCoord2iARB("glMultiTexCoord2iARB");
Function<void, GLenum, const GLint *> Binding::MultiTexCoord2iv("glMultiTexCoord2iv");
Function<void, GLenum, const GLint *> Binding::MultiTexCoord2ivARB("glMultiTexCoord2ivARB");
Function<void, GLenum, GLshort, GLshort> Binding::MultiTexCoord2s("glMultiTexCoord2s");
Function<void, GLenum, GLshort, GLshort> Binding::MultiTexCoord2sARB("glMultiTexCoord2sARB");
Function<void, GLenum, const GLshort *> Binding::MultiTexCoord2sv("glMultiTexCoord2sv");
Function<void, GLenum, const GLshort *> Binding::MultiTexCoord2svARB("glMultiTexCoord2svARB");
Function<void, GLenum, GLfixed, GLfixed> Binding::MultiTexCoord2xOES("glMultiTexCoord2xOES");
Function<void, GLenum, const GLfixed *> Binding::MultiTexCoord2xvOES("glMultiTexCoord2xvOES");
Function<void, GLenum, GLbyte, GLbyte, GLbyte> Binding::MultiTexCoord3bOES("glMultiTexCoord3bOES");
Function<void, GLenum, const GLbyte *> Binding::MultiTexCoord3bvOES("glMultiTexCoord3bvOES");
Function<void, GLenum, GLdouble, GLdouble, GLdouble> Binding::MultiTexCoord3d("glMultiTexCoord3d");
Function<void, GLenum, GLdouble, GLdouble, GLdouble> Binding::MultiTexCoord3dARB("glMultiTexCoord3dARB");
Function<void, GLenum, const GLdouble *> Binding::MultiTexCoord3dv("glMultiTexCoord3dv");
Function<void, GLenum, const GLdouble *> Binding::MultiTexCoord3dvARB("glMultiTexCoord3dvARB");
Function<void, GLenum, GLfloat, GLfloat, GLfloat> Binding::MultiTexCoord3f("glMultiTexCoord3f");
Function<void, GLenum, GLfloat, GLfloat, GLfloat> Binding::MultiTexCoord3fARB("glMultiTexCoord3fARB");
Function<void, GLenum, const GLfloat *> Binding::MultiTexCoord3fv("glMultiTexCoord3fv");
Function<void, GLenum, const GLfloat *> Binding::MultiTexCoord3fvARB("glMultiTexCoord3fvARB");
Function<void, GLenum, GLhalfNV, GLhalfNV, GLhalfNV> Binding::MultiTexCoord3hNV("glMultiTexCoord3hNV");
Function<void, GLenum, const GLhalfNV *> Binding::MultiTexCoord3hvNV("glMultiTexCoord3hvNV");
Function<void, GLenum, GLint, GLint, GLint> Binding::MultiTexCoord3i("glMultiTexCoord3i");
Function<void, GLenum, GLint, GLint, GLint> Binding::MultiTexCoord3iARB("glMultiTexCoord3iARB");
Function<void, GLenum, const GLint *> Binding::MultiTexCoord3iv("glMultiTexCoord3iv");
Function<void, GLenum, const GLint *> Binding::MultiTexCoord3ivARB("glMultiTexCoord3ivARB");
Function<void, GLenum, GLshort, GLshort, GLshort> Binding::MultiTexCoord3s("glMultiTexCoord3s");
Function<void, GLenum, GLshort, GLshort, GLshort> Binding::MultiTexCoord3sARB("glMultiTexCoord3sARB");
Function<void, GLenum, const GLshort *> Binding::MultiTexCoord3sv("glMultiTexCoord3sv");
Function<void, GLenum, const GLshort *> Binding::MultiTexCoord3svARB("glMultiTexCoord3svARB");
Function<void, GLenum, GLfixed, GLfixed, GLfixed> Binding::MultiTexCoord3xOES("glMultiTexCoord3xOES");
Function<void, GLenum, const GLfixed *> Binding::MultiTexCoord3xvOES("glMultiTexCoord3xvOES");
Function<void, GLenum, GLbyte, GLbyte, GLbyte, GLbyte> Binding::MultiTexCoord4bOES("glMultiTexCoord4bOES");
Function<void, GLenum, const GLbyte *> Binding::MultiTexCoord4bvOES("glMultiTexCoord4bvOES");
Function<void, GLenum, GLdouble, GLdouble, GLdouble, GLdouble> Binding::MultiTexCoord4d("glMultiTexCoord4d");
Function<void, GLenum, GLdouble, GLdouble, GLdouble, GLdouble> Binding::MultiTexCoord4dARB("glMultiTexCoord4dARB");
Function<void, GLenum, const GLdouble *> Binding::MultiTexCoord4dv("glMultiTexCoord4dv");
Function<void, GLenum, const GLdouble *> Binding::MultiTexCoord4dvARB("glMultiTexCoord4dvARB");
Function<void, GLenum, GLfloat, GLfloat, GLfloat, GLfloat> Binding::MultiTexCoord4f("glMultiTexCoord4f");
Function<void, GLenum, GLfloat, GLfloat, GLfloat, GLfloat> Binding::MultiTexCoord4fARB("glMultiTexCoord4fARB");
Function<void, GLenum, const GLfloat *> Binding::MultiTexCoord4fv("glMultiTexCoord4fv");
Function<void, GLenum, const GLfloat *> Binding::MultiTexCoord4fvARB("glMultiTexCoord4fvARB");
Function<void, GLenum, GLhalfNV, GLhalfNV, GLhalfNV, GLhalfNV> Binding::MultiTexCoord4hNV("glMultiTexCoord4hNV");
Function<void, GLenum, const GLhalfNV *> Binding::MultiTexCoord4hvNV("glMultiTexCoord4hvNV");
Function<void, GLenum, GLint, GLint, GLint, GLint> Binding::MultiTexCoord4i("glMultiTexCoord4i");
Function<void, GLenum, GLint, GLint, GLint, GLint> Binding::MultiTexCoord4iARB("glMultiTexCoord4iARB");
Function<void, GLenum, const GLint *> Binding::MultiTexCoord4iv("glMultiTexCoord4iv");
Function<void, GLenum, const GLint *> Binding::MultiTexCoord4ivARB("glMultiTexCoord4ivARB");
Function<void, GLenum, GLshort, GLshort, GLshort, GLshort> Binding::MultiTexCoord4s("glMultiTexCoord4s");
Function<void, GLenum, GLshort, GLshort, GLshort, GLshort> Binding::MultiTexCoord4sARB("glMultiTexCoord4sARB");
Function<void, GLenum, const GLshort *> Binding::MultiTexCoord4sv("glMultiTexCoord4sv");
Function<void, GLenum, const GLshort *> Binding::MultiTexCoord4svARB("glMultiTexCoord4svARB");
Function<void, GLenum, GLfixed, GLfixed, GLfixed, GLfixed> Binding::MultiTexCoord4xOES("glMultiTexCoord4xOES");
Function<void, GLenum, const GLfixed *> Binding::MultiTexCoord4xvOES("glMultiTexCoord4xvOES");
Function<void, GLenum, GLenum, GLuint> Binding::MultiTexCoordP1ui("glMultiTexCoordP1ui");
Function<void, GLenum, GLenum, const GLuint *> Binding::MultiTexCoordP1uiv("glMultiTexCoordP1uiv");
Function<void, GLenum, GLenum, GLuint> Binding::MultiTexCoordP2ui("glMultiTexCoordP2ui");
Function<void, GLenum, GLenum, const GLuint *> Binding::MultiTexCoordP2uiv("glMultiTexCoordP2uiv");
Function<void, GLenum, GLenum, GLuint> Binding::MultiTexCoordP3ui("glMultiTexCoordP3ui");
Function<void, GLenum, GLenum, const GLuint *> Binding::MultiTexCoordP3uiv("glMultiTexCoordP3uiv");
Function<void, GLenum, GLenum, GLuint> Binding::MultiTexCoordP4ui("glMultiTexCoordP4ui");
Function<void, GLenum, GLenum, const GLuint *> Binding::MultiTexCoordP4uiv("glMultiTexCoordP4uiv");
Function<void, GLenum, GLint, GLenum, GLsizei, const void *> Binding::MultiTexCoordPointerEXT("glMultiTexCoordPointerEXT");
Function<void, GLenum, GLenum, GLenum, GLfloat> Binding::MultiTexEnvfEXT("glMultiTexEnvfEXT");
Function<void, GLenum, GLenum, GLenum, const GLfloat *> Binding::MultiTexEnvfvEXT("glMultiTexEnvfvEXT");
Function<void, GLenum, GLenum, GLenum, GLint> Binding::MultiTexEnviEXT("glMultiTexEnviEXT");
Function<void, GLenum, GLenum, GLenum, const GLint *> Binding::MultiTexEnvivEXT("glMultiTexEnvivEXT");
Function<void, GLenum, GLenum, GLenum, GLdouble> Binding::MultiTexGendEXT("glMultiTexGendEXT");
Function<void, GLenum, GLenum, GLenum, const GLdouble *> Binding::MultiTexGendvEXT("glMultiTexGendvEXT");
Function<void, GLenum, GLenum, GLenum, GLfloat> Binding::MultiTexGenfEXT("glMultiTexGenfEXT");
Function<void, GLenum, GLenum, GLenum, const GLfloat *> Binding::MultiTexGenfvEXT("glMultiTexGenfvEXT");
Function<void, GLenum, GLenum, GLenum, GLint> Binding::MultiTexGeniEXT("glMultiTexGeniEXT");
Function<void, GLenum, GLenum, GLenum, const GLint *> Binding::MultiTexGenivEXT("glMultiTexGenivEXT");
Function<void, GLenum, GLenum, GLint, GLint, GLsizei, GLint, GLenum, GLenum, const void *> Binding::MultiTexImage1DEXT("glMultiTexImage1DEXT");
Function<void, GLenum, GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *> Binding::MultiTexImage2DEXT("glMultiTexImage2DEXT");
Function<void, GLenum, GLenum, GLint, GLint, GLsizei, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *> Binding::MultiTexImage3DEXT("glMultiTexImage3DEXT");
Function<void, GLenum, GLenum, GLenum, const GLint *> Binding::MultiTexParameterIivEXT("glMultiTexParameterIivEXT");
Function<void, GLenum, GLenum, GLenum, const GLuint *> Binding::MultiTexParameterIuivEXT("glMultiTexParameterIuivEXT");
Function<void, GLenum, GLenum, GLenum, GLfloat> Binding::MultiTexParameterfEXT("glMultiTexParameterfEXT");
Function<void, GLenum, GLenum, GLenum, const GLfloat *> Binding::MultiTexParameterfvEXT("glMultiTexParameterfvEXT");
Function<void, GLenum, GLenum, GLenum, GLint> Binding::MultiTexParameteriEXT("glMultiTexParameteriEXT");
Function<void, GLenum, GLenum, GLenum, const GLint *> Binding::MultiTexParameterivEXT("glMultiTexParameterivEXT");
Function<void, GLenum, GLenum, GLuint> Binding::MultiTexRenderbufferEXT("glMultiTexRenderbufferEXT");
Function<void, GLenum, GLenum, GLint, GLint, GLsizei, GLenum, GLenum, const void *> Binding::MultiTexSubImage1DEXT("glMultiTexSubImage1DEXT");
Function<void, GLenum, GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void *> Binding::MultiTexSubImage2DEXT("glMultiTexSubImage2DEXT");
Function<void, GLenum, GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLenum, const void *> Binding::MultiTexSubImage3DEXT("glMultiTexSubImage3DEXT");
Function<void, GLuint, GLsizeiptr, const void *, GLenum> Binding::NamedBufferData("glNamedBufferData");
Function<void, GLuint, GLsizeiptr, const void *, GLenum> Binding::NamedBufferDataEXT("glNamedBufferDataEXT");
Function<void, GLuint, GLintptr, GLsizeiptr, GLboolean> Binding::NamedBufferPageCommitmentARB("glNamedBufferPageCommitmentARB");
Function<void, GLuint, GLintptr, GLsizeiptr, GLboolean> Binding::NamedBufferPageCommitmentEXT("glNamedBufferPageCommitmentEXT");
Function<void, GLuint, GLsizeiptr, const void *, BufferStorageMask> Binding::NamedBufferStorage("glNamedBufferStorage");
Function<void, GLuint, GLsizeiptr, const void *, BufferStorageMask> Binding::NamedBufferStorageEXT("glNamedBufferStorageEXT");
Function<void, GLuint, GLintptr, GLsizeiptr, const void *> Binding::NamedBufferSubData("glNamedBufferSubData");
Function<void, GLuint, GLintptr, GLsizeiptr, const void *> Binding::NamedBufferSubDataEXT("glNamedBufferSubDataEXT");
Function<void, GLuint, GLuint, GLintptr, GLintptr, GLsizeiptr> Binding::NamedCopyBufferSubDataEXT("glNamedCopyBufferSubDataEXT");
Function<void, GLuint, GLenum> Binding::NamedFramebufferDrawBuffer("glNamedFramebufferDrawBuffer");
Function<void, GLuint, GLsizei, const GLenum *> Binding::NamedFramebufferDrawBuffers("glNamedFramebufferDrawBuffers");
Function<void, GLuint, GLenum, GLint> Binding::NamedFramebufferParameteri("glNamedFramebufferParameteri");
Function<void, GLuint, GLenum, GLint> Binding::NamedFramebufferParameteriEXT("glNamedFramebufferParameteriEXT");
Function<void, GLuint, GLenum> Binding::NamedFramebufferReadBuffer("glNamedFramebufferReadBuffer");
Function<void, GLuint, GLenum, GLenum, GLuint> Binding::NamedFramebufferRenderbuffer("glNamedFramebufferRenderbuffer");
Function<void, GLuint, GLenum, GLenum, GLuint> Binding::NamedFramebufferRenderbufferEXT("glNamedFramebufferRenderbufferEXT");
Function<void, GLuint, GLuint, GLsizei, const GLfloat *> Binding::NamedFramebufferSampleLocationsfvARB("glNamedFramebufferSampleLocationsfvARB");
Function<void, GLuint, GLuint, GLsizei, const GLfloat *> Binding::NamedFramebufferSampleLocationsfvNV("glNamedFramebufferSampleLocationsfvNV");
Function<void, GLuint, GLenum, GLuint, GLint> Binding::NamedFramebufferTexture("glNamedFramebufferTexture");
Function<void, GLuint, GLenum, GLenum, GLuint, GLint> Binding::NamedFramebufferTexture1DEXT("glNamedFramebufferTexture1DEXT");
Function<void, GLuint, GLenum, GLenum, GLuint, GLint> Binding::NamedFramebufferTexture2DEXT("glNamedFramebufferTexture2DEXT");
Function<void, GLuint, GLenum, GLenum, GLuint, GLint, GLint> Binding::NamedFramebufferTexture3DEXT("glNamedFramebufferTexture3DEXT");
Function<void, GLuint, GLenum, GLuint, GLint> Binding::NamedFramebufferTextureEXT("glNamedFramebufferTextureEXT");
Function<void, GLuint, GLenum, GLuint, GLint, GLenum> Binding::NamedFramebufferTextureFaceEXT("glNamedFramebufferTextureFaceEXT");
Function<void, GLuint, GLenum, GLuint, GLint, GLint> Binding::NamedFramebufferTextureLayer("glNamedFramebufferTextureLayer");
Function<void, GLuint, GLenum, GLuint, GLint, GLint> Binding::NamedFramebufferTextureLayerEXT("glNamedFramebufferTextureLayerEXT");
Function<void, GLuint, GLenum, GLuint, GLdouble, GLdouble, GLdouble, GLdouble> Binding::NamedProgramLocalParameter4dEXT("glNamedProgramLocalParameter4dEXT");
Function<void, GLuint, GLenum, GLuint, const GLdouble *> Binding::NamedProgramLocalParameter4dvEXT("glNamedProgramLocalParameter4dvEXT");
Function<void, GLuint, GLenum, GLuint, GLfloat, GLfloat, GLfloat, GLfloat> Binding::NamedProgramLocalParameter4fEXT("glNamedProgramLocalParameter4fEXT");
Function<void, GLuint, GLenum, GLuint, const GLfloat *> Binding::NamedProgramLocalParameter4fvEXT("glNamedProgramLocalParameter4fvEXT");
Function<void, GLuint, GLenum, GLuint, GLint, GLint, GLint, GLint> Binding::NamedProgramLocalParameterI4iEXT("glNamedProgramLocalParameterI4iEXT");
Function<void, GLuint, GLenum, GLuint, const GLint *> Binding::NamedProgramLocalParameterI4ivEXT("glNamedProgramLocalParameterI4ivEXT");
Function<void, GLuint, GLenum, GLuint, GLuint, GLuint, GLuint, GLuint> Binding::NamedProgramLocalParameterI4uiEXT("glNamedProgramLocalParameterI4uiEXT");
Function<void, GLuint, GLenum, GLuint, const GLuint *> Binding::NamedProgramLocalParameterI4uivEXT("glNamedProgramLocalParameterI4uivEXT");
Function<void, GLuint, GLenum, GLuint, GLsizei, const GLfloat *> Binding::NamedProgramLocalParameters4fvEXT("glNamedProgramLocalParameters4fvEXT");
Function<void, GLuint, GLenum, GLuint, GLsizei, const GLint *> Binding::NamedProgramLocalParametersI4ivEXT("glNamedProgramLocalParametersI4ivEXT");
Function<void, GLuint, GLenum, GLuint, GLsizei, const GLuint *> Binding::NamedProgramLocalParametersI4uivEXT("glNamedProgramLocalParametersI4uivEXT");
Function<void, GLuint, GLenum, GLenum, GLsizei, const void *> Binding::NamedProgramStringEXT("glNamedProgramStringEXT");
Function<void, GLuint, GLenum, GLsizei, GLsizei> Binding::NamedRenderbufferStorage("glNamedRenderbufferStorage");
Function<void, GLuint, GLenum, GLsizei, GLsizei> Binding::NamedRenderbufferStorageEXT("glNamedRenderbufferStorageEXT");
Function<void, GLuint, GLsizei, GLenum, GLsizei, GLsizei> Binding::NamedRenderbufferStorageMultisample("glNamedRenderbufferStorageMultisample");
Function<void, GLuint, GLsizei, GLsizei, GLenum, GLsizei, GLsizei> Binding::NamedRenderbufferStorageMultisampleCoverageEXT("glNamedRenderbufferStorageMultisampleCoverageEXT");
Function<void, GLuint, GLsizei, GLenum, GLsizei, GLsizei> Binding::NamedRenderbufferStorageMultisampleEXT("glNamedRenderbufferStorageMultisampleEXT");
Function<void, GLenum, GLint, const GLchar *, GLint, const GLchar *> Binding::NamedStringARB("glNamedStringARB");
Function<void, GLuint, GLenum> Binding::NewList("glNewList");
Function<GLuint, GLsizei, const void *, GLenum> Binding::NewObjectBufferATI("glNewObjectBufferATI");
Function<void, GLbyte, GLbyte, GLbyte> Binding::Normal3b("glNormal3b");
Function<void, const GLbyte *> Binding::Normal3bv("glNormal3bv");
Function<void, GLdouble, GLdouble, GLdouble> Binding::Normal3d("glNormal3d");
Function<void, const GLdouble *> Binding::Normal3dv("glNormal3dv");
Function<void, GLfloat, GLfloat, GLfloat> Binding::Normal3f("glNormal3f");
Function<void, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat> Binding::Normal3fVertex3fSUN("glNormal3fVertex3fSUN");
Function<void, const GLfloat *, const GLfloat *> Binding::Normal3fVertex3fvSUN("glNormal3fVertex3fvSUN");
Function<void, const GLfloat *> Binding::Normal3fv("glNormal3fv");
Function<void, GLhalfNV, GLhalfNV, GLhalfNV> Binding::Normal3hNV("glNormal3hNV");
Function<void, const GLhalfNV *> Binding::Normal3hvNV("glNormal3hvNV");
Function<void, GLint, GLint, GLint> Binding::Normal3i("glNormal3i");
Function<void, const GLint *> Binding::Normal3iv("glNormal3iv");
Function<void, GLshort, GLshort, GLshort> Binding::Normal3s("glNormal3s");
Function<void, const GLshort *> Binding::Normal3sv("glNormal3sv");
Function<void, GLfixed, GLfixed, GLfixed> Binding::Normal3xOES("glNormal3xOES");
Function<void, const GLfixed *> Binding::Normal3xvOES("glNormal3xvOES");
Function<void, GLenum, GLsizei> Binding::NormalFormatNV("glNormalFormatNV");
Function<void, GLenum, GLuint> Binding::NormalP3ui("glNormalP3ui");
Function<void, GLenum, const GLuint *> Binding::NormalP3uiv("glNormalP3uiv");
Function<void, GLenum, GLsizei, const void *> Binding::NormalPointer("glNormalPointer");
Function<void, GLenum, GLsizei, GLsizei, const void *> Binding::NormalPointerEXT("glNormalPointerEXT");
Function<void, GLenum, GLint, const void **, GLint> Binding::NormalPointerListIBM("glNormalPointerListIBM");
Function<void, GLenum, const void **> Binding::NormalPointervINTEL("glNormalPointervINTEL");
Function<void, GLenum, GLbyte, GLbyte, GLbyte> Binding::NormalStream3bATI("glNormalStream3bATI");
Function<void, GLenum, const GLbyte *> Binding::NormalStream3bvATI("glNormalStream3bvATI");
Function<void, GLenum, GLdouble, GLdouble, GLdouble> Binding::NormalStream3dATI("glNormalStream3dATI");
Function<void, GLenum, const GLdouble *> Binding::NormalStream3dvATI("glNormalStream3dvATI");
Function<void, GLenum, GLfloat, GLfloat, GLfloat> Binding::NormalStream3fATI("glNormalStream3fATI");
Function<void, GLenum, const GLfloat *> Binding::NormalStream3fvATI("glNormalStream3fvATI");
Function<void, GLenum, GLint, GLint, GLint> Binding::NormalStream3iATI("glNormalStream3iATI");
Function<void, GLenum, const GLint *> Binding::NormalStream3ivATI("glNormalStream3ivATI");
Function<void, GLenum, GLshort, GLshort, GLshort> Binding::NormalStream3sATI("glNormalStream3sATI");
Function<void, GLenum, const GLshort *> Binding::NormalStream3svATI("glNormalStream3svATI");
Function<void, GLenum, GLuint, GLsizei, const GLchar *> Binding::ObjectLabel("glObjectLabel");
Function<void, const void *, GLsizei, const GLchar *> Binding::ObjectPtrLabel("glObjectPtrLabel");
Function<GLenum, GLenum, GLuint, GLenum> Binding::ObjectPurgeableAPPLE("glObjectPurgeableAPPLE");
Function<GLenum, GLenum, GLuint, GLenum> Binding::ObjectUnpurgeableAPPLE("glObjectUnpurgeableAPPLE");
Function<void, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble> Binding::Ortho("glOrtho");
Function<void, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat> Binding::OrthofOES("glOrthofOES");
Function<void, GLfixed, GLfixed, GLfixed, GLfixed, GLfixed, GLfixed> Binding::OrthoxOES("glOrthoxOES");
Function<void, GLenum, GLfloat> Binding::PNTrianglesfATI("glPNTrianglesfATI");
Function<void, GLenum, GLint> Binding::PNTrianglesiATI("glPNTrianglesiATI");
Function<void, GLuint, GLuint, GLenum> Binding::PassTexCoordATI("glPassTexCoordATI");
Function<void, GLfloat> Binding::PassThrough("glPassThrough");
Function<void, GLfixed> Binding::PassThroughxOES("glPassThroughxOES");
Function<void, GLenum, const GLfloat *> Binding::PatchParameterfv("glPatchParameterfv");
Function<void, GLenum, GLint> Binding::PatchParameteri("glPatchParameteri");
Function<void, GLenum, GLenum, GLenum, const GLfloat *> Binding::PathColorGenNV("glPathColorGenNV");
Function<void, GLuint, GLsizei, const GLubyte *, GLsizei, GLenum, const void *> Binding::PathCommandsNV("glPathCommandsNV");
Function<void, GLuint, GLsizei, GLenum, const void *> Binding::PathCoordsNV("glPathCoordsNV");
Function<void, GLenum> Binding::PathCoverDepthFuncNV("glPathCoverDepthFuncNV");
Function<void, GLuint, GLsizei, const GLfloat *> Binding::PathDashArrayNV("glPathDashArrayNV");
Function<void, GLenum> Binding::PathFogGenNV("glPathFogGenNV");
Function<GLenum, GLuint, GLenum, const void *, PathFontStyle, GLuint, GLsizei, GLuint, GLfloat> Binding::PathGlyphIndexArrayNV("glPathGlyphIndexArrayNV");
Function<GLenum, GLenum, const void *, PathFontStyle, GLuint, GLfloat, GLuint_array_2> Binding::PathGlyphIndexRangeNV("glPathGlyphIndexRangeNV");
Function<void, GLuint, GLenum, const void *, PathFontStyle, GLuint, GLsizei, GLenum, GLuint, GLfloat> Binding::PathGlyphRangeNV("glPathGlyphRangeNV");
Function<void, GLuint, GLenum, const void *, PathFontStyle, GLsizei, GLenum, const void *, GLenum, GLuint, GLfloat> Binding::PathGlyphsNV("glPathGlyphsNV");
Function<GLenum, GLuint, GLenum, GLsizeiptr, const void *, GLsizei, GLuint, GLsizei, GLuint, GLfloat> Binding::PathMemoryGlyphIndexArrayNV("glPathMemoryGlyphIndexArrayNV");
Function<void, GLuint, GLenum, GLfloat> Binding::PathParameterfNV("glPathParameterfNV");
Function<void, GLuint, GLenum, const GLfloat *> Binding::PathParameterfvNV("glPathParameterfvNV");
Function<void, GLuint, GLenum, GLint> Binding::PathParameteriNV("glPathParameteriNV");
Function<void, GLuint, GLenum, const GLint *> Binding::PathParameterivNV("glPathParameterivNV");
Function<void, GLfloat, GLfloat> Binding::PathStencilDepthOffsetNV("glPathStencilDepthOffsetNV");
Function<void, GLenum, GLint, GLuint> Binding::PathStencilFuncNV("glPathStencilFuncNV");
Function<void, GLuint, GLenum, GLsizei, const void *> Binding::PathStringNV("glPathStringNV");
Function<void, GLuint, GLsizei, GLsizei, GLsizei, const GLubyte *, GLsizei, GLenum, const void *> Binding::PathSubCommandsNV("glPathSubCommandsNV");
Function<void, GLuint, GLsizei, GLsizei, GLenum, const void *> Binding::PathSubCoordsNV("glPathSubCoordsNV");
Function<void, GLenum, GLenum, GLint, const GLfloat *> Binding::PathTexGenNV("glPathTexGenNV");
Function<void> Binding::PauseTransformFeedback("glPauseTransformFeedback");
Function<void> Binding::PauseTransformFeedbackNV("glPauseTransformFeedbackNV");
Function<void, GLenum, GLsizei, const void *> Binding::PixelDataRangeNV("glPixelDataRangeNV");
Function<void, GLenum, GLsizei, const GLfloat *> Binding::PixelMapfv("glPixelMapfv");
Function<void, GLenum, GLsizei, const GLuint *> Binding::PixelMapuiv("glPixelMapuiv");
Function<void, GLenum, GLsizei, const GLushort *> Binding::PixelMapusv("glPixelMapusv");
Function<void, GLenum, GLint, const GLfixed *> Binding::PixelMapx("glPixelMapx");
Function<void, GLenum, GLfloat> Binding::PixelStoref("glPixelStoref");
Function<void, GLenum, GLint> Binding::PixelStorei("glPixelStorei");
Function<void, GLenum, GLfixed> Binding::PixelStorex("glPixelStorex");
Function<void, GLenum, GLfloat> Binding::PixelTexGenParameterfSGIS("glPixelTexGenParameterfSGIS");
Function<void, GLenum, const GLfloat *> Binding::PixelTexGenParameterfvSGIS("glPixelTexGenParameterfvSGIS");
Function<void, GLenum, GLint> Binding::PixelTexGenParameteriSGIS("glPixelTexGenParameteriSGIS");
Function<void, GLenum, const GLint *> Binding::PixelTexGenParameterivSGIS("glPixelTexGenParameterivSGIS");
Function<void, GLenum> Binding::PixelTexGenSGIX("glPixelTexGenSGIX");
Function<void, GLenum, GLfloat> Binding::PixelTransferf("glPixelTransferf");
Function<void, GLenum, GLint> Binding::PixelTransferi("glPixelTransferi");
Function<void, GLenum, GLfixed> Binding::PixelTransferxOES("glPixelTransferxOES");
Function<void, GLenum, GLenum, GLfloat> Binding::PixelTransformParameterfEXT("glPixelTransformParameterfEXT");
Function<void, GLenum, GLenum, const GLfloat *> Binding::PixelTransformParameterfvEXT("glPixelTransformParameterfvEXT");
Function<void, GLenum, GLenum, GLint> Binding::PixelTransformParameteriEXT("glPixelTransformParameteriEXT");
Function<void, GLenum, GLenum, const GLint *> Binding::PixelTransformParameterivEXT("glPixelTransformParameterivEXT");
Function<void, GLfloat, GLfloat> Binding::PixelZoom("glPixelZoom");
Function<void, GLfixed, GLfixed> Binding::PixelZoomxOES("glPixelZoomxOES");
Function<GLboolean, GLuint, GLsizei, GLsizei, GLfloat, GLfloat *, GLfloat *, GLfloat *, GLfloat *> Binding::PointAlongPathNV("glPointAlongPathNV");
Function<void, GLenum, GLfloat> Binding::PointParameterf("glPointParameterf");
Function<void, GLenum, GLfloat> Binding::PointParameterfARB("glPointParameterfARB");
Function<void, GLenum, GLfloat> Binding::PointParameterfEXT("glPointParameterfEXT");
Function<void, GLenum, GLfloat> Binding::PointParameterfSGIS("glPointParameterfSGIS");
Function<void, GLenum, const GLfloat *> Binding::PointParameterfv("glPointParameterfv");
Function<void, GLenum, const GLfloat *> Binding::PointParameterfvARB("glPointParameterfvARB");
Function<void, GLenum, const GLfloat *> Binding::PointParameterfvEXT("glPointParameterfvEXT");
Function<void, GLenum, const GLfloat *> Binding::PointParameterfvSGIS("glPointParameterfvSGIS");
Function<void, GLenum, GLint> Binding::PointParameteri("glPointParameteri");
Function<void, GLenum, GLint> Binding::PointParameteriNV("glPointParameteriNV");
Function<void, GLenum, const GLint *> Binding::PointParameteriv("glPointParameteriv");
Function<void, GLenum, const GLint *> Binding::PointParameterivNV("glPointParameterivNV");
Function<void, GLenum, const GLfixed *> Binding::PointParameterxvOES("glPointParameterxvOES");
Function<void, GLfloat> Binding::PointSize("glPointSize");
Function<void, GLfixed> Binding::PointSizexOES("glPointSizexOES");
Function<GLint, GLuint *> Binding::PollAsyncSGIX("glPollAsyncSGIX");
Function<GLint, GLint *> Binding::PollInstrumentsSGIX("glPollInstrumentsSGIX");
Function<void, GLenum, GLenum> Binding::PolygonMode("glPolygonMode");
Function<void, GLfloat, GLfloat> Binding::PolygonOffset("glPolygonOffset");
Function<void, GLfloat, GLfloat, GLfloat> Binding::PolygonOffsetClampEXT("glPolygonOffsetClampEXT");
Function<void, GLfloat, GLfloat> Binding::PolygonOffsetEXT("glPolygonOffsetEXT");
Function<void, GLfixed, GLfixed> Binding::PolygonOffsetxOES("glPolygonOffsetxOES");
Function<void, const GLubyte *> Binding::PolygonStipple("glPolygonStipple");
Function<void> Binding::PopAttrib("glPopAttrib");
Function<void> Binding::PopClientAttrib("glPopClientAttrib");
Function<void> Binding::PopDebugGroup("glPopDebugGroup");
Function<void> Binding::PopGroupMarkerEXT("glPopGroupMarkerEXT");
Function<void> Binding::PopMatrix("glPopMatrix");
Function<void> Binding::PopName("glPopName");
Function<void, GLuint, GLuint64EXT, GLuint, GLuint, GLenum, GLenum, GLuint, GLenum, GLuint, GLenum, GLuint, GLenum, GLuint> Binding::PresentFrameDualFillNV("glPresentFrameDualFillNV");
Function<void, GLuint, GLuint64EXT, GLuint, GLuint, GLenum, GLenum, GLuint, GLuint, GLenum, GLuint, GLuint> Binding::PresentFrameKeyedNV("glPresentFrameKeyedNV");
Function<void, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat> Binding::PrimitiveBoundingBoxARB("glPrimitiveBoundingBoxARB");
Function<void, GLuint> Binding::PrimitiveRestartIndex("glPrimitiveRestartIndex");
Function<void, GLuint> Binding::PrimitiveRestartIndexNV("glPrimitiveRestartIndexNV");
Function<void> Binding::PrimitiveRestartNV("glPrimitiveRestartNV");
Function<void, GLsizei, const GLuint *, const GLfloat *> Binding::PrioritizeTextures("glPrioritizeTextures");
Function<void, GLsizei, const GLuint *, const GLclampf *> Binding::PrioritizeTexturesEXT("glPrioritizeTexturesEXT");
Function<void, GLsizei, const GLuint *, const GLfixed *> Binding::PrioritizeTexturesxOES("glPrioritizeTexturesxOES");
Function<void, GLuint, GLenum, const void *, GLsizei> Binding::ProgramBinary("glProgramBinary");
Function<void, GLenum, GLuint, GLuint, GLsizei, const GLint *> Binding::ProgramBufferParametersIivNV("glProgramBufferParametersIivNV");
Function<void, GLenum, GLuint, GLuint, GLsizei, const GLuint *> Binding::ProgramBufferParametersIuivNV("glProgramBufferParametersIuivNV");
Function<void, GLenum, GLuint, GLuint, GLsizei, const GLfloat *> Binding::ProgramBufferParametersfvNV("glProgramBufferParametersfvNV");
Function<void, GLenum, GLuint, GLdouble, GLdouble, GLdouble, GLdouble> Binding::ProgramEnvParameter4dARB("glProgramEnvParameter4dARB");
Function<void, GLenum, GLuint, const GLdouble *> Binding::ProgramEnvParameter4dvARB("glProgramEnvParameter4dvARB");
Function<void, GLenum, GLuint, GLfloat, GLfloat, GLfloat, GLfloat> Binding::ProgramEnvParameter4fARB("glProgramEnvParameter4fARB");
Function<void, GLenum, GLuint, const GLfloat *> Binding::ProgramEnvParameter4fvARB("glProgramEnvParameter4fvARB");
Function<void, GLenum, GLuint, GLint, GLint, GLint, GLint> Binding::ProgramEnvParameterI4iNV("glProgramEnvParameterI4iNV");
Function<void, GLenum, GLuint, const GLint *> Binding::ProgramEnvParameterI4ivNV("glProgramEnvParameterI4ivNV");
Function<void, GLenum, GLuint, GLuint, GLuint, GLuint, GLuint> Binding::ProgramEnvParameterI4uiNV("glProgramEnvParameterI4uiNV");
Function<void, GLenum, GLuint, const GLuint *> Binding::ProgramEnvParameterI4uivNV("glProgramEnvParameterI4uivNV");
Function<void, GLenum, GLuint, GLsizei, const GLfloat *> Binding::ProgramEnvParameters4fvEXT("glProgramEnvParameters4fvEXT");
Function<void, GLenum, GLuint, GLsizei, const GLint *> Binding::ProgramEnvParametersI4ivNV("glProgramEnvParametersI4ivNV");
Function<void, GLenum, GLuint, GLsizei, const GLuint *> Binding::ProgramEnvParametersI4uivNV("glProgramEnvParametersI4uivNV");
Function<void, GLenum, GLuint, GLdouble, GLdouble, GLdouble, GLdouble> Binding::ProgramLocalParameter4dARB("glProgramLocalParameter4dARB");
Function<void, GLenum, GLuint, const GLdouble *> Binding::ProgramLocalParameter4dvARB("glProgramLocalParameter4dvARB");
Function<void, GLenum, GLuint, GLfloat, GLfloat, GLfloat, GLfloat> Binding::ProgramLocalParameter4fARB("glProgramLocalParameter4fARB");
Function<void, GLenum, GLuint, const GLfloat *> Binding::ProgramLocalParameter4fvARB("glProgramLocalParameter4fvARB");
Function<void, GLenum, GLuint, GLint, GLint, GLint, GLint> Binding::ProgramLocalParameterI4iNV("glProgramLocalParameterI4iNV");
Function<void, GLenum, GLuint, const GLint *> Binding::ProgramLocalParameterI4ivNV("glProgramLocalParameterI4ivNV");
Function<void, GLenum, GLuint, GLuint, GLuint, GLuint, GLuint> Binding::ProgramLocalParameterI4uiNV("glProgramLocalParameterI4uiNV");
Function<void, GLenum, GLuint, const GLuint *> Binding::ProgramLocalParameterI4uivNV("glProgramLocalParameterI4uivNV");
Function<void, GLenum, GLuint, GLsizei, const GLfloat *> Binding::ProgramLocalParameters4fvEXT("glProgramLocalParameters4fvEXT");
Function<void, GLenum, GLuint, GLsizei, const GLint *> Binding::ProgramLocalParametersI4ivNV("glProgramLocalParametersI4ivNV");
Function<void, GLenum, GLuint, GLsizei, const GLuint *> Binding::ProgramLocalParametersI4uivNV("glProgramLocalParametersI4uivNV");
Function<void, GLuint, GLsizei, const GLubyte *, GLdouble, GLdouble, GLdouble, GLdouble> Binding::ProgramNamedParameter4dNV("glProgramNamedParameter4dNV");
Function<void, GLuint, GLsizei, const GLubyte *, const GLdouble *> Binding::ProgramNamedParameter4dvNV("glProgramNamedParameter4dvNV");
Function<void, GLuint, GLsizei, const GLubyte *, GLfloat, GLfloat, GLfloat, GLfloat> Binding::ProgramNamedParameter4fNV("glProgramNamedParameter4fNV");
Function<void, GLuint, GLsizei, const GLubyte *, const GLfloat *> Binding::ProgramNamedParameter4fvNV("glProgramNamedParameter4fvNV");
Function<void, GLenum, GLuint, GLdouble, GLdouble, GLdouble, GLdouble> Binding::ProgramParameter4dNV("glProgramParameter4dNV");
Function<void, GLenum, GLuint, const GLdouble *> Binding::ProgramParameter4dvNV("glProgramParameter4dvNV");
Function<void, GLenum, GLuint, GLfloat, GLfloat, GLfloat, GLfloat> Binding::ProgramParameter4fNV("glProgramParameter4fNV");
Function<void, GLenum, GLuint, const GLfloat *> Binding::ProgramParameter4fvNV("glProgramParameter4fvNV");
Function<void, GLuint, GLenum, GLint> Binding::ProgramParameteri("glProgramParameteri");
Function<void, GLuint, GLenum, GLint> Binding::ProgramParameteriARB("glProgramParameteriARB");
Function<void, GLuint, GLenum, GLint> Binding::ProgramParameteriEXT("glProgramParameteriEXT");
Function<void, GLenum, GLuint, GLsizei, const GLdouble *> Binding::ProgramParameters4dvNV("glProgramParameters4dvNV");
Function<void, GLenum, GLuint, GLsizei, const GLfloat *> Binding::ProgramParameters4fvNV("glProgramParameters4fvNV");
Function<void, GLuint, GLint, GLenum, GLint, const GLfloat *> Binding::ProgramPathFragmentInputGenNV("glProgramPathFragmentInputGenNV");
Function<void, GLenum, GLenum, GLsizei, const void *> Binding::ProgramStringARB("glProgramStringARB");
Function<void, GLenum, GLsizei, const GLuint *> Binding::ProgramSubroutineParametersuivNV("glProgramSubroutineParametersuivNV");
Function<void, GLuint, GLint, GLdouble> Binding::ProgramUniform1d("glProgramUniform1d");
Function<void, GLuint, GLint, GLdouble> Binding::ProgramUniform1dEXT("glProgramUniform1dEXT");
Function<void, GLuint, GLint, GLsizei, const GLdouble *> Binding::ProgramUniform1dv("glProgramUniform1dv");
Function<void, GLuint, GLint, GLsizei, const GLdouble *> Binding::ProgramUniform1dvEXT("glProgramUniform1dvEXT");
Function<void, GLuint, GLint, GLfloat> Binding::ProgramUniform1f("glProgramUniform1f");
Function<void, GLuint, GLint, GLfloat> Binding::ProgramUniform1fEXT("glProgramUniform1fEXT");
Function<void, GLuint, GLint, GLsizei, const GLfloat *> Binding::ProgramUniform1fv("glProgramUniform1fv");
Function<void, GLuint, GLint, GLsizei, const GLfloat *> Binding::ProgramUniform1fvEXT("glProgramUniform1fvEXT");
Function<void, GLuint, GLint, GLint> Binding::ProgramUniform1i("glProgramUniform1i");
Function<void, GLuint, GLint, GLint64> Binding::ProgramUniform1i64ARB("glProgramUniform1i64ARB");
Function<void, GLuint, GLint, GLint64EXT> Binding::ProgramUniform1i64NV("glProgramUniform1i64NV");
Function<void, GLuint, GLint, GLsizei, const GLint64 *> Binding::ProgramUniform1i64vARB("glProgramUniform1i64vARB");
Function<void, GLuint, GLint, GLsizei, const GLint64EXT *> Binding::ProgramUniform1i64vNV("glProgramUniform1i64vNV");
Function<void, GLuint, GLint, GLint> Binding::ProgramUniform1iEXT("glProgramUniform1iEXT");
Function<void, GLuint, GLint, GLsizei, const GLint *> Binding::ProgramUniform1iv("glProgramUniform1iv");
Function<void, GLuint, GLint, GLsizei, const GLint *> Binding::ProgramUniform1ivEXT("glProgramUniform1ivEXT");
Function<void, GLuint, GLint, GLuint> Binding::ProgramUniform1ui("glProgramUniform1ui");
Function<void, GLuint, GLint, GLuint64> Binding::ProgramUniform1ui64ARB("glProgramUniform1ui64ARB");
Function<void, GLuint, GLint, GLuint64EXT> Binding::ProgramUniform1ui64NV("glProgramUniform1ui64NV");
Function<void, GLuint, GLint, GLsizei, const GLuint64 *> Binding::ProgramUniform1ui64vARB("glProgramUniform1ui64vARB");
Function<void, GLuint, GLint, GLsizei, const GLuint64EXT *> Binding::ProgramUniform1ui64vNV("glProgramUniform1ui64vNV");
Function<void, GLuint, GLint, GLuint> Binding::ProgramUniform1uiEXT("glProgramUniform1uiEXT");
Function<void, GLuint, GLint, GLsizei, const GLuint *> Binding::ProgramUniform1uiv("glProgramUniform1uiv");
Function<void, GLuint, GLint, GLsizei, const GLuint *> Binding::ProgramUniform1uivEXT("glProgramUniform1uivEXT");
Function<void, GLuint, GLint, GLdouble, GLdouble> Binding::ProgramUniform2d("glProgramUniform2d");
Function<void, GLuint, GLint, GLdouble, GLdouble> Binding::ProgramUniform2dEXT("glProgramUniform2dEXT");
Function<void, GLuint, GLint, GLsizei, const GLdouble *> Binding::ProgramUniform2dv("glProgramUniform2dv");
Function<void, GLuint, GLint, GLsizei, const GLdouble *> Binding::ProgramUniform2dvEXT("glProgramUniform2dvEXT");
Function<void, GLuint, GLint, GLfloat, GLfloat> Binding::ProgramUniform2f("glProgramUniform2f");
Function<void, GLuint, GLint, GLfloat, GLfloat> Binding::ProgramUniform2fEXT("glProgramUniform2fEXT");
Function<void, GLuint, GLint, GLsizei, const GLfloat *> Binding::ProgramUniform2fv("glProgramUniform2fv");
Function<void, GLuint, GLint, GLsizei, const GLfloat *> Binding::ProgramUniform2fvEXT("glProgramUniform2fvEXT");
Function<void, GLuint, GLint, GLint, GLint> Binding::ProgramUniform2i("glProgramUniform2i");
Function<void, GLuint, GLint, GLint64, GLint64> Binding::ProgramUniform2i64ARB("glProgramUniform2i64ARB");
Function<void, GLuint, GLint, GLint64EXT, GLint64EXT> Binding::ProgramUniform2i64NV("glProgramUniform2i64NV");
Function<void, GLuint, GLint, GLsizei, const GLint64 *> Binding::ProgramUniform2i64vARB("glProgramUniform2i64vARB");
Function<void, GLuint, GLint, GLsizei, const GLint64EXT *> Binding::ProgramUniform2i64vNV("glProgramUniform2i64vNV");
Function<void, GLuint, GLint, GLint, GLint> Binding::ProgramUniform2iEXT("glProgramUniform2iEXT");
Function<void, GLuint, GLint, GLsizei, const GLint *> Binding::ProgramUniform2iv("glProgramUniform2iv");
Function<void, GLuint, GLint, GLsizei, const GLint *> Binding::ProgramUniform2ivEXT("glProgramUniform2ivEXT");
Function<void, GLuint, GLint, GLuint, GLuint> Binding::ProgramUniform2ui("glProgramUniform2ui");
Function<void, GLuint, GLint, GLuint64, GLuint64> Binding::ProgramUniform2ui64ARB("glProgramUniform2ui64ARB");
Function<void, GLuint, GLint, GLuint64EXT, GLuint64EXT> Binding::ProgramUniform2ui64NV("glProgramUniform2ui64NV");
Function<void, GLuint, GLint, GLsizei, const GLuint64 *> Binding::ProgramUniform2ui64vARB("glProgramUniform2ui64vARB");
Function<void, GLuint, GLint, GLsizei, const GLuint64EXT *> Binding::ProgramUniform2ui64vNV("glProgramUniform2ui64vNV");
Function<void, GLuint, GLint, GLuint, GLuint> Binding::ProgramUniform2uiEXT("glProgramUniform2uiEXT");
Function<void, GLuint, GLint, GLsizei, const GLuint *> Binding::ProgramUniform2uiv("glProgramUniform2uiv");
Function<void, GLuint, GLint, GLsizei, const GLuint *> Binding::ProgramUniform2uivEXT("glProgramUniform2uivEXT");
Function<void, GLuint, GLint, GLdouble, GLdouble, GLdouble> Binding::ProgramUniform3d("glProgramUniform3d");
Function<void, GLuint, GLint, GLdouble, GLdouble, GLdouble> Binding::ProgramUniform3dEXT("glProgramUniform3dEXT");
Function<void, GLuint, GLint, GLsizei, const GLdouble *> Binding::ProgramUniform3dv("glProgramUniform3dv");
Function<void, GLuint, GLint, GLsizei, const GLdouble *> Binding::ProgramUniform3dvEXT("glProgramUniform3dvEXT");
Function<void, GLuint, GLint, GLfloat, GLfloat, GLfloat> Binding::ProgramUniform3f("glProgramUniform3f");
Function<void, GLuint, GLint, GLfloat, GLfloat, GLfloat> Binding::ProgramUniform3fEXT("glProgramUniform3fEXT");
Function<void, GLuint, GLint, GLsizei, const GLfloat *> Binding::ProgramUniform3fv("glProgramUniform3fv");
Function<void, GLuint, GLint, GLsizei, const GLfloat *> Binding::ProgramUniform3fvEXT("glProgramUniform3fvEXT");
Function<void, GLuint, GLint, GLint, GLint, GLint> Binding::ProgramUniform3i("glProgramUniform3i");
Function<void, GLuint, GLint, GLint64, GLint64, GLint64> Binding::ProgramUniform3i64ARB("glProgramUniform3i64ARB");
Function<void, GLuint, GLint, GLint64EXT, GLint64EXT, GLint64EXT> Binding::ProgramUniform3i64NV("glProgramUniform3i64NV");
Function<void, GLuint, GLint, GLsizei, const GLint64 *> Binding::ProgramUniform3i64vARB("glProgramUniform3i64vARB");
Function<void, GLuint, GLint, GLsizei, const GLint64EXT *> Binding::ProgramUniform3i64vNV("glProgramUniform3i64vNV");
Function<void, GLuint, GLint, GLint, GLint, GLint> Binding::ProgramUniform3iEXT("glProgramUniform3iEXT");
Function<void, GLuint, GLint, GLsizei, const GLint *> Binding::ProgramUniform3iv("glProgramUniform3iv");
Function<void, GLuint, GLint, GLsizei, const GLint *> Binding::ProgramUniform3ivEXT("glProgramUniform3ivEXT");
Function<void, GLuint, GLint, GLuint, GLuint, GLuint> Binding::ProgramUniform3ui("glProgramUniform3ui");
Function<void, GLuint, GLint, GLuint64, GLuint64, GLuint64> Binding::ProgramUniform3ui64ARB("glProgramUniform3ui64ARB");
Function<void, GLuint, GLint, GLuint64EXT, GLuint64EXT, GLuint64EXT> Binding::ProgramUniform3ui64NV("glProgramUniform3ui64NV");
Function<void, GLuint, GLint, GLsizei, const GLuint64 *> Binding::ProgramUniform3ui64vARB("glProgramUniform3ui64vARB");
Function<void, GLuint, GLint, GLsizei, const GLuint64EXT *> Binding::ProgramUniform3ui64vNV("glProgramUniform3ui64vNV");
Function<void, GLuint, GLint, GLuint, GLuint, GLuint> Binding::ProgramUniform3uiEXT("glProgramUniform3uiEXT");
Function<void, GLuint, GLint, GLsizei, const GLuint *> Binding::ProgramUniform3uiv("glProgramUniform3uiv");
Function<void, GLuint, GLint, GLsizei, const GLuint *> Binding::ProgramUniform3uivEXT("glProgramUniform3uivEXT");
Function<void, GLuint, GLint, GLdouble, GLdouble, GLdouble, GLdouble> Binding::ProgramUniform4d("glProgramUniform4d");
Function<void, GLuint, GLint, GLdouble, GLdouble, GLdouble, GLdouble> Binding::ProgramUniform4dEXT("glProgramUniform4dEXT");
Function<void, GLuint, GLint, GLsizei, const GLdouble *> Binding::ProgramUniform4dv("glProgramUniform4dv");
Function<void, GLuint, GLint, GLsizei, const GLdouble *> Binding::ProgramUniform4dvEXT("glProgramUniform4dvEXT");
Function<void, GLuint, GLint, GLfloat, GLfloat, GLfloat, GLfloat> Binding::ProgramUniform4f("glProgramUniform4f");
Function<void, GLuint, GLint, GLfloat, GLfloat, GLfloat, GLfloat> Binding::ProgramUniform4fEXT("glProgramUniform4fEXT");
Function<void, GLuint, GLint, GLsizei, const GLfloat *> Binding::ProgramUniform4fv("glProgramUniform4fv");
Function<void, GLuint, GLint, GLsizei, const GLfloat *> Binding::ProgramUniform4fvEXT("glProgramUniform4fvEXT");
Function<void, GLuint, GLint, GLint, GLint, GLint, GLint> Binding::ProgramUniform4i("glProgramUniform4i");
Function<void, GLuint, GLint, GLint64, GLint64, GLint64, GLint64> Binding::ProgramUniform4i64ARB("glProgramUniform4i64ARB");
Function<void, GLuint, GLint, GLint64EXT, GLint64EXT, GLint64EXT, GLint64EXT> Binding::ProgramUniform4i64NV("glProgramUniform4i64NV");
Function<void, GLuint, GLint, GLsizei, const GLint64 *> Binding::ProgramUniform4i64vARB("glProgramUniform4i64vARB");
Function<void, GLuint, GLint, GLsizei, const GLint64EXT *> Binding::ProgramUniform4i64vNV("glProgramUniform4i64vNV");
Function<void, GLuint, GLint, GLint, GLint, GLint, GLint> Binding::ProgramUniform4iEXT("glProgramUniform4iEXT");
Function<void, GLuint, GLint, GLsizei, const GLint *> Binding::ProgramUniform4iv("glProgramUniform4iv");
Function<void, GLuint, GLint, GLsizei, const GLint *> Binding::ProgramUniform4ivEXT("glProgramUniform4ivEXT");
Function<void, GLuint, GLint, GLuint, GLuint, GLuint, GLuint> Binding::ProgramUniform4ui("glProgramUniform4ui");
Function<void, GLuint, GLint, GLuint64, GLuint64, GLuint64, GLuint64> Binding::ProgramUniform4ui64ARB("glProgramUniform4ui64ARB");
Function<void, GLuint, GLint, GLuint64EXT, GLuint64EXT, GLuint64EXT, GLuint64EXT> Binding::ProgramUniform4ui64NV("glProgramUniform4ui64NV");
Function<void, GLuint, GLint, GLsizei, const GLuint64 *> Binding::ProgramUniform4ui64vARB("glProgramUniform4ui64vARB");
Function<void, GLuint, GLint, GLsizei, const GLuint64EXT *> Binding::ProgramUniform4ui64vNV("glProgramUniform4ui64vNV");
Function<void, GLuint, GLint, GLuint, GLuint, GLuint, GLuint> Binding::ProgramUniform4uiEXT("glProgramUniform4uiEXT");
Function<void, GLuint, GLint, GLsizei, const GLuint *> Binding::ProgramUniform4uiv("glProgramUniform4uiv");
Function<void, GLuint, GLint, GLsizei, const GLuint *> Binding::ProgramUniform4uivEXT("glProgramUniform4uivEXT");
Function<void, GLuint, GLint, GLuint64> Binding::ProgramUniformHandleui64ARB("glProgramUniformHandleui64ARB");
Function<void, GLuint, GLint, GLuint64> Binding::ProgramUniformHandleui64NV("glProgramUniformHandleui64NV");
Function<void, GLuint, GLint, GLsizei, const GLuint64 *> Binding::ProgramUniformHandleui64vARB("glProgramUniformHandleui64vARB");
Function<void, GLuint, GLint, GLsizei, const GLuint64 *> Binding::ProgramUniformHandleui64vNV("glProgramUniformHandleui64vNV");
Function<void, GLuint, GLint, GLsizei, GLboolean, const GLdouble *> Binding::ProgramUniformMatrix2dv("glProgramUniformMatrix2dv");
Function<void, GLuint, GLint, GLsizei, GLboolean, const GLdouble *> Binding::ProgramUniformMatrix2dvEXT("glProgramUniformMatrix2dvEXT");
Function<void, GLuint, GLint, GLsizei, GLboolean, const GLfloat *> Binding::ProgramUniformMatrix2fv("glProgramUniformMatrix2fv");
Function<void, GLuint, GLint, GLsizei, GLboolean, const GLfloat *> Binding::ProgramUniformMatrix2fvEXT("glProgramUniformMatrix2fvEXT");
Function<void, GLuint, GLint, GLsizei, GLboolean, const GLdouble *> Binding::ProgramUniformMatrix2x3dv("glProgramUniformMatrix2x3dv");
Function<void, GLuint, GLint, GLsizei, GLboolean, const GLdouble *> Binding::ProgramUniformMatrix2x3dvEXT("glProgramUniformMatrix2x3dvEXT");
Function<void, GLuint, GLint, GLsizei, GLboolean, const GLfloat *> Binding::ProgramUniformMatrix2x3fv("glProgramUniformMatrix2x3fv");
Function<void, GLuint, GLint, GLsizei, GLboolean, const GLfloat *> Binding::ProgramUniformMatrix2x3fvEXT("glProgramUniformMatrix2x3fvEXT");
Function<void, GLuint, GLint, GLsizei, GLboolean, const GLdouble *> Binding::ProgramUniformMatrix2x4dv("glProgramUniformMatrix2x4dv");
Function<void, GLuint, GLint, GLsizei, GLboolean, const GLdouble *> Binding::ProgramUniformMatrix2x4dvEXT("glProgramUniformMatrix2x4dvEXT");
Function<void, GLuint, GLint, GLsizei, GLboolean, const GLfloat *> Binding::ProgramUniformMatrix2x4fv("glProgramUniformMatrix2x4fv");
Function<void, GLuint, GLint, GLsizei, GLboolean, const GLfloat *> Binding::ProgramUniformMatrix2x4fvEXT("glProgramUniformMatrix2x4fvEXT");
Function<void, GLuint, GLint, GLsizei, GLboolean, const GLdouble *> Binding::ProgramUniformMatrix3dv("glProgramUniformMatrix3dv");
Function<void, GLuint, GLint, GLsizei, GLboolean, const GLdouble *> Binding::ProgramUniformMatrix3dvEXT("glProgramUniformMatrix3dvEXT");
Function<void, GLuint, GLint, GLsizei, GLboolean, const GLfloat *> Binding::ProgramUniformMatrix3fv("glProgramUniformMatrix3fv");
Function<void, GLuint, GLint, GLsizei, GLboolean, const GLfloat *> Binding::ProgramUniformMatrix3fvEXT("glProgramUniformMatrix3fvEXT");
Function<void, GLuint, GLint, GLsizei, GLboolean, const GLdouble *> Binding::ProgramUniformMatrix3x2dv("glProgramUniformMatrix3x2dv");
Function<void, GLuint, GLint, GLsizei, GLboolean, const GLdouble *> Binding::ProgramUniformMatrix3x2dvEXT("glProgramUniformMatrix3x2dvEXT");
Function<void, GLuint, GLint, GLsizei, GLboolean, const GLfloat *> Binding::ProgramUniformMatrix3x2fv("glProgramUniformMatrix3x2fv");
Function<void, GLuint, GLint, GLsizei, GLboolean, const GLfloat *> Binding::ProgramUniformMatrix3x2fvEXT("glProgramUniformMatrix3x2fvEXT");
Function<void, GLuint, GLint, GLsizei, GLboolean, const GLdouble *> Binding::ProgramUniformMatrix3x4dv("glProgramUniformMatrix3x4dv");
Function<void, GLuint, GLint, GLsizei, GLboolean, const GLdouble *> Binding::ProgramUniformMatrix3x4dvEXT("glProgramUniformMatrix3x4dvEXT");
Function<void, GLuint, GLint, GLsizei, GLboolean, const GLfloat *> Binding::ProgramUniformMatrix3x4fv("glProgramUniformMatrix3x4fv");
Function<void, GLuint, GLint, GLsizei, GLboolean, const GLfloat *> Binding::ProgramUniformMatrix3x4fvEXT("glProgramUniformMatrix3x4fvEXT");
Function<void, GLuint, GLint, GLsizei, GLboolean, const GLdouble *> Binding::ProgramUniformMatrix4dv("glProgramUniformMatrix4dv");
Function<void, GLuint, GLint, GLsizei, GLboolean, const GLdouble *> Binding::ProgramUniformMatrix4dvEXT("glProgramUniformMatrix4dvEXT");
Function<void, GLuint, GLint, GLsizei, GLboolean, const GLfloat *> Binding::ProgramUniformMatrix4fv("glProgramUniformMatrix4fv");
Function<void, GLuint, GLint, GLsizei, GLboolean, const GLfloat *> Binding::ProgramUniformMatrix4fvEXT("glProgramUniformMatrix4fvEXT");
Function<void, GLuint, GLint, GLsizei, GLboolean, const GLdouble *> Binding::ProgramUniformMatrix4x2dv("glProgramUniformMatrix4x2dv");
Function<void, GLuint, GLint, GLsizei, GLboolean, const GLdouble *> Binding::ProgramUniformMatrix4x2dvEXT("glProgramUniformMatrix4x2dvEXT");
Function<void, GLuint, GLint, GLsizei, GLboolean, const GLfloat *> Binding::ProgramUniformMatrix4x2fv("glProgramUniformMatrix4x2fv");
Function<void, GLuint, GLint, GLsizei, GLboolean, const GLfloat *> Binding::ProgramUniformMatrix4x2fvEXT("glProgramUniformMatrix4x2fvEXT");
Function<void, GLuint, GLint, GLsizei, GLboolean, const GLdouble *> Binding::ProgramUniformMatrix4x3dv("glProgramUniformMatrix4x3dv");
Function<void, GLuint, GLint, GLsizei, GLboolean, const GLdouble *> Binding::ProgramUniformMatrix4x3dvEXT("glProgramUniformMatrix4x3dvEXT");
Function<void, GLuint, GLint, GLsizei, GLboolean, const GLfloat *> Binding::ProgramUniformMatrix4x3fv("glProgramUniformMatrix4x3fv");
Function<void, GLuint, GLint, GLsizei, GLboolean, const GLfloat *> Binding::ProgramUniformMatrix4x3fvEXT("glProgramUniformMatrix4x3fvEXT");
Function<void, GLuint, GLint, GLuint64EXT> Binding::ProgramUniformui64NV("glProgramUniformui64NV");
Function<void, GLuint, GLint, GLsizei, const GLuint64EXT *> Binding::ProgramUniformui64vNV("glProgramUniformui64vNV");
Function<void, GLenum, GLint> Binding::ProgramVertexLimitNV("glProgramVertexLimitNV");
Function<void, GLenum> Binding::ProvokingVertex("glProvokingVertex");
Function<void, GLenum> Binding::ProvokingVertexEXT("glProvokingVertexEXT");
Function<void, AttribMask> Binding::PushAttrib("glPushAttrib");
Function<void, ClientAttribMask> Binding::PushClientAttrib("glPushClientAttrib");
Function<void, ClientAttribMask> Binding::PushClientAttribDefaultEXT("glPushClientAttribDefaultEXT");
Function<void, GLenum, GLuint, GLsizei, const GLchar *> Binding::PushDebugGroup("glPushDebugGroup");
Function<void, GLsizei, const GLchar *> Binding::PushGroupMarkerEXT("glPushGroupMarkerEXT");
Function<void> Binding::PushMatrix("glPushMatrix");
Function<void, GLuint> Binding::PushName("glPushName");
Function<void, GLuint, GLenum> Binding::QueryCounter("glQueryCounter");
Function<GLbitfield, GLfixed *, GLint *> Binding::QueryMatrixxOES("glQueryMatrixxOES");
Function<void, GLenum, GLuint, GLenum, GLuint> Binding::QueryObjectParameteruiAMD("glQueryObjectParameteruiAMD");
Function<void, GLdouble, GLdouble> Binding::RasterPos2d("glRasterPos2d");
Function<void, const GLdouble *> Binding::RasterPos2dv("glRasterPos2dv");
Function<void, GLfloat, GLfloat> Binding::RasterPos2f("glRasterPos2f");
Function<void, const GLfloat *> Binding::RasterPos2fv("glRasterPos2fv");
Function<void, GLint, GLint> Binding::RasterPos2i("glRasterPos2i");
Function<void, const GLint *> Binding::RasterPos2iv("glRasterPos2iv");
Function<void, GLshort, GLshort> Binding::RasterPos2s("glRasterPos2s");
Function<void, const GLshort *> Binding::RasterPos2sv("glRasterPos2sv");
Function<void, GLfixed, GLfixed> Binding::RasterPos2xOES("glRasterPos2xOES");
Function<void, const GLfixed *> Binding::RasterPos2xvOES("glRasterPos2xvOES");
Function<void, GLdouble, GLdouble, GLdouble> Binding::RasterPos3d("glRasterPos3d");
Function<void, const GLdouble *> Binding::RasterPos3dv("glRasterPos3dv");
Function<void, GLfloat, GLfloat, GLfloat> Binding::RasterPos3f("glRasterPos3f");
Function<void, const GLfloat *> Binding::RasterPos3fv("glRasterPos3fv");
Function<void, GLint, GLint, GLint> Binding::RasterPos3i("glRasterPos3i");
Function<void, const GLint *> Binding::RasterPos3iv("glRasterPos3iv");
Function<void, GLshort, GLshort, GLshort> Binding::RasterPos3s("glRasterPos3s");
Function<void, const GLshort *> Binding::RasterPos3sv("glRasterPos3sv");
Function<void, GLfixed, GLfixed, GLfixed> Binding::RasterPos3xOES("glRasterPos3xOES");
Function<void, const GLfixed *> Binding::RasterPos3xvOES("glRasterPos3xvOES");
Function<void, GLdouble, GLdouble, GLdouble, GLdouble> Binding::RasterPos4d("glRasterPos4d");
Function<void, const GLdouble *> Binding::RasterPos4dv("glRasterPos4dv");
Function<void, GLfloat, GLfloat, GLfloat, GLfloat> Binding::RasterPos4f("glRasterPos4f");
Function<void, const GLfloat *> Binding::RasterPos4fv("glRasterPos4fv");
Function<void, GLint, GLint, GLint, GLint> Binding::RasterPos4i("glRasterPos4i");
Function<void, const GLint *> Binding::RasterPos4iv("glRasterPos4iv");
Function<void, GLshort, GLshort, GLshort, GLshort> Binding::RasterPos4s("glRasterPos4s");
Function<void, const GLshort *> Binding::RasterPos4sv("glRasterPos4sv");
Function<void, GLfixed, GLfixed, GLfixed, GLfixed> Binding::RasterPos4xOES("glRasterPos4xOES");
Function<void, const GLfixed *> Binding::RasterPos4xvOES("glRasterPos4xvOES");
Function<void, GLuint, GLboolean> Binding::RasterSamplesEXT("glRasterSamplesEXT");
Function<void, GLenum> Binding::ReadBuffer("glReadBuffer");
Function<void, GLint> Binding::ReadInstrumentsSGIX("glReadInstrumentsSGIX");
Function<void, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *> Binding::ReadPixels("glReadPixels");
Function<void, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, GLsizei, void *> Binding::ReadnPixels("glReadnPixels");
Function<void, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, GLsizei, void *> Binding::ReadnPixelsARB("glReadnPixelsARB");
Function<void, GLdouble, GLdouble, GLdouble, GLdouble> Binding::Rectd("glRectd");
Function<void, const GLdouble *, const GLdouble *> Binding::Rectdv("glRectdv");
Function<void, GLfloat, GLfloat, GLfloat, GLfloat> Binding::Rectf("glRectf");
Function<void, const GLfloat *, const GLfloat *> Binding::Rectfv("glRectfv");
Function<void, GLint, GLint, GLint, GLint> Binding::Recti("glRecti");
Function<void, const GLint *, const GLint *> Binding::Rectiv("glRectiv");
Function<void, GLshort, GLshort, GLshort, GLshort> Binding::Rects("glRects");
Function<void, const GLshort *, const GLshort *> Binding::Rectsv("glRectsv");
Function<void, GLfixed, GLfixed, GLfixed, GLfixed> Binding::RectxOES("glRectxOES");
Function<void, const GLfixed *, const GLfixed *> Binding::RectxvOES("glRectxvOES");
Function<void, const GLdouble *> Binding::ReferencePlaneSGIX("glReferencePlaneSGIX");
Function<void> Binding::ReleaseShaderCompiler("glReleaseShaderCompiler");
Function<GLint, GLenum> Binding::RenderMode("glRenderMode");
Function<void, GLenum, GLenum, GLsizei, GLsizei> Binding::RenderbufferStorage("glRenderbufferStorage");
Function<void, GLenum, GLenum, GLsizei, GLsizei> Binding::RenderbufferStorageEXT("glRenderbufferStorageEXT");
Function<void, GLenum, GLsizei, GLenum, GLsizei, GLsizei> Binding::RenderbufferStorageMultisample("glRenderbufferStorageMultisample");
Function<void, GLenum, GLsizei, GLsizei, GLenum, GLsizei, GLsizei> Binding::RenderbufferStorageMultisampleCoverageNV("glRenderbufferStorageMultisampleCoverageNV");
Function<void, GLenum, GLsizei, GLenum, GLsizei, GLsizei> Binding::RenderbufferStorageMultisampleEXT("glRenderbufferStorageMultisampleEXT");
Function<void, GLenum, GLsizei, const void **> Binding::ReplacementCodePointerSUN("glReplacementCodePointerSUN");
Function<void, GLubyte> Binding::ReplacementCodeubSUN("glReplacementCodeubSUN");
Function<void, const GLubyte *> Binding::ReplacementCodeubvSUN("glReplacementCodeubvSUN");
Function<void, GLuint, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat> Binding::ReplacementCodeuiColor3fVertex3fSUN("glReplacementCodeuiColor3fVertex3fSUN");
Function<void, const GLuint *, const GLfloat *, const GLfloat *> Binding::ReplacementCodeuiColor3fVertex3fvSUN("glReplacementCodeuiColor3fVertex3fvSUN");
Function<void, GLuint, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat> Binding::ReplacementCodeuiColor4fNormal3fVertex3fSUN("glReplacementCodeuiColor4fNormal3fVertex3fSUN");
Function<void, const GLuint *, const GLfloat *, const GLfloat *, const GLfloat *> Binding::ReplacementCodeuiColor4fNormal3fVertex3fvSUN("glReplacementCodeuiColor4fNormal3fVertex3fvSUN");
Function<void, GLuint, GLubyte, GLubyte, GLubyte, GLubyte, GLfloat, GLfloat, GLfloat> Binding::ReplacementCodeuiColor4ubVertex3fSUN("glReplacementCodeuiColor4ubVertex3fSUN");
Function<void, const GLuint *, const GLubyte *, const GLfloat *> Binding::ReplacementCodeuiColor4ubVertex3fvSUN("glReplacementCodeuiColor4ubVertex3fvSUN");
Function<void, GLuint, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat> Binding::ReplacementCodeuiNormal3fVertex3fSUN("glReplacementCodeuiNormal3fVertex3fSUN");
Function<void, const GLuint *, const GLfloat *, const GLfloat *> Binding::ReplacementCodeuiNormal3fVertex3fvSUN("glReplacementCodeuiNormal3fVertex3fvSUN");
Function<void, GLuint> Binding::ReplacementCodeuiSUN("glReplacementCodeuiSUN");
Function<void, GLuint, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat> Binding::ReplacementCodeuiTexCoord2fColor4fNormal3fVertex3fSUN("glReplacementCodeuiTexCoord2fColor4fNormal3fVertex3fSUN");
Function<void, const GLuint *, const GLfloat *, const GLfloat *, const GLfloat *, const GLfloat *> Binding::ReplacementCodeuiTexCoord2fColor4fNormal3fVertex3fvSUN("glReplacementCodeuiTexCoord2fColor4fNormal3fVertex3fvSUN");
Function<void, GLuint, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat> Binding::ReplacementCodeuiTexCoord2fNormal3fVertex3fSUN("glReplacementCodeuiTexCoord2fNormal3fVertex3fSUN");
Function<void, const GLuint *, const GLfloat *, const GLfloat *, const GLfloat *> Binding::ReplacementCodeuiTexCoord2fNormal3fVertex3fvSUN("glReplacementCodeuiTexCoord2fNormal3fVertex3fvSUN");
Function<void, GLuint, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat> Binding::ReplacementCodeuiTexCoord2fVertex3fSUN("glReplacementCodeuiTexCoord2fVertex3fSUN");
Function<void, const GLuint *, const GLfloat *, const GLfloat *> Binding::ReplacementCodeuiTexCoord2fVertex3fvSUN("glReplacementCodeuiTexCoord2fVertex3fvSUN");
Function<void, GLuint, GLfloat, GLfloat, GLfloat> Binding::ReplacementCodeuiVertex3fSUN("glReplacementCodeuiVertex3fSUN");
Function<void, const GLuint *, const GLfloat *> Binding::ReplacementCodeuiVertex3fvSUN("glReplacementCodeuiVertex3fvSUN");
Function<void, const GLuint *> Binding::ReplacementCodeuivSUN("glReplacementCodeuivSUN");
Function<void, GLushort> Binding::ReplacementCodeusSUN("glReplacementCodeusSUN");
Function<void, const GLushort *> Binding::ReplacementCodeusvSUN("glReplacementCodeusvSUN");
Function<void, GLsizei, const GLuint *> Binding::RequestResidentProgramsNV("glRequestResidentProgramsNV");
Function<void, GLenum> Binding::ResetHistogram("glResetHistogram");
Function<void, GLenum> Binding::ResetHistogramEXT("glResetHistogramEXT");
Function<void, GLenum> Binding::ResetMinmax("glResetMinmax");
Function<void, GLenum> Binding::ResetMinmaxEXT("glResetMinmaxEXT");
Function<void> Binding::ResizeBuffersMESA("glResizeBuffersMESA");
Function<void> Binding::ResolveDepthValuesNV("glResolveDepthValuesNV");
Function<void> Binding::ResumeTransformFeedback("glResumeTransformFeedback");
Function<void> Binding::ResumeTransformFeedbackNV("glResumeTransformFeedbackNV");
Function<void, GLdouble, GLdouble, GLdouble, GLdouble> Binding::Rotated("glRotated");
Function<void, GLfloat, GLfloat, GLfloat, GLfloat> Binding::Rotatef("glRotatef");
Function<void, GLfixed, GLfixed, GLfixed, GLfixed> Binding::RotatexOES("glRotatexOES");
Function<void, GLfloat, GLboolean> Binding::SampleCoverage("glSampleCoverage");
Function<void, GLfloat, GLboolean> Binding::SampleCoverageARB("glSampleCoverageARB");
Function<void, GLuint, GLuint, GLenum> Binding::SampleMapATI("glSampleMapATI");
Function<void, GLclampf, GLboolean> Binding::SampleMaskEXT("glSampleMaskEXT");
Function<void, GLuint, GLbitfield> Binding::SampleMaskIndexedNV("glSampleMaskIndexedNV");
Function<void, GLclampf, GLboolean> Binding::SampleMaskSGIS("glSampleMaskSGIS");
Function<void, GLuint, GLbitfield> Binding::SampleMaski("glSampleMaski");
Function<void, GLenum> Binding::SamplePatternEXT("glSamplePatternEXT");
Function<void, GLenum> Binding::SamplePatternSGIS("glSamplePatternSGIS");
Function<void, GLuint, GLenum, const GLint *> Binding::SamplerParameterIiv("glSamplerParameterIiv");
Function<void, GLuint, GLenum, const GLuint *> Binding::SamplerParameterIuiv("glSamplerParameterIuiv");
Function<void, GLuint, GLenum, GLfloat> Binding::SamplerParameterf("glSamplerParameterf");
Function<void, GLuint, GLenum, const GLfloat *> Binding::SamplerParameterfv("glSamplerParameterfv");
Function<void, GLuint, GLenum, GLint> Binding::SamplerParameteri("glSamplerParameteri");
Function<void, GLuint, GLenum, const GLint *> Binding::SamplerParameteriv("glSamplerParameteriv");
Function<void, GLdouble, GLdouble, GLdouble> Binding::Scaled("glScaled");
Function<void, GLfloat, GLfloat, GLfloat> Binding::Scalef("glScalef");
Function<void, GLfixed, GLfixed, GLfixed> Binding::ScalexOES("glScalexOES");
Function<void, GLint, GLint, GLsizei, GLsizei> Binding::Scissor("glScissor");
Function<void, GLuint, GLsizei, const GLint *> Binding::ScissorArrayv("glScissorArrayv");
Function<void, GLuint, GLint, GLint, GLsizei, GLsizei> Binding::ScissorIndexed("glScissorIndexed");
Function<void, GLuint, const GLint *> Binding::ScissorIndexedv("glScissorIndexedv");
Function<void, GLbyte, GLbyte, GLbyte> Binding::SecondaryColor3b("glSecondaryColor3b");
Function<void, GLbyte, GLbyte, GLbyte> Binding::SecondaryColor3bEXT("glSecondaryColor3bEXT");
Function<void, const GLbyte *> Binding::SecondaryColor3bv("glSecondaryColor3bv");
Function<void, const GLbyte *> Binding::SecondaryColor3bvEXT("glSecondaryColor3bvEXT");
Function<void, GLdouble, GLdouble, GLdouble> Binding::SecondaryColor3d("glSecondaryColor3d");
Function<void, GLdouble, GLdouble, GLdouble> Binding::SecondaryColor3dEXT("glSecondaryColor3dEXT");
Function<void, const GLdouble *> Binding::SecondaryColor3dv("glSecondaryColor3dv");
Function<void, const GLdouble *> Binding::SecondaryColor3dvEXT("glSecondaryColor3dvEXT");
Function<void, GLfloat, GLfloat, GLfloat> Binding::SecondaryColor3f("glSecondaryColor3f");
Function<void, GLfloat, GLfloat, GLfloat> Binding::SecondaryColor3fEXT("glSecondaryColor3fEXT");
Function<void, const GLfloat *> Binding::SecondaryColor3fv("glSecondaryColor3fv");
Function<void, const GLfloat *> Binding::SecondaryColor3fvEXT("glSecondaryColor3fvEXT");
Function<void, GLhalfNV, GLhalfNV, GLhalfNV> Binding::SecondaryColor3hNV("glSecondaryColor3hNV");
Function<void, const GLhalfNV *> Binding::SecondaryColor3hvNV("glSecondaryColor3hvNV");
Function<void, GLint, GLint, GLint> Binding::SecondaryColor3i("glSecondaryColor3i");
Function<void, GLint, GLint, GLint> Binding::SecondaryColor3iEXT("glSecondaryColor3iEXT");
Function<void, const GLint *> Binding::SecondaryColor3iv("glSecondaryColor3iv");
Function<void, const GLint *> Binding::SecondaryColor3ivEXT("glSecondaryColor3ivEXT");
Function<void, GLshort, GLshort, GLshort> Binding::SecondaryColor3s("glSecondaryColor3s");
Function<void, GLshort, GLshort, GLshort> Binding::SecondaryColor3sEXT("glSecondaryColor3sEXT");
Function<void, const GLshort *> Binding::SecondaryColor3sv("glSecondaryColor3sv");
Function<void, const GLshort *> Binding::SecondaryColor3svEXT("glSecondaryColor3svEXT");
Function<void, GLubyte, GLubyte, GLubyte> Binding::SecondaryColor3ub("glSecondaryColor3ub");
Function<void, GLubyte, GLubyte, GLubyte> Binding::SecondaryColor3ubEXT("glSecondaryColor3ubEXT");
Function<void, const GLubyte *> Binding::SecondaryColor3ubv("glSecondaryColor3ubv");
Function<void, const GLubyte *> Binding::SecondaryColor3ubvEXT("glSecondaryColor3ubvEXT");
Function<void, GLuint, GLuint, GLuint> Binding::SecondaryColor3ui("glSecondaryColor3ui");
Function<void, GLuint, GLuint, GLuint> Binding::SecondaryColor3uiEXT("glSecondaryColor3uiEXT");
Function<void, const GLuint *> Binding::SecondaryColor3uiv("glSecondaryColor3uiv");
Function<void, const GLuint *> Binding::SecondaryColor3uivEXT("glSecondaryColor3uivEXT");
Function<void, GLushort, GLushort, GLushort> Binding::SecondaryColor3us("glSecondaryColor3us");
Function<void, GLushort, GLushort, GLushort> Binding::SecondaryColor3usEXT("glSecondaryColor3usEXT");
Function<void, const GLushort *> Binding::SecondaryColor3usv("glSecondaryColor3usv");
Function<void, const GLushort *> Binding::SecondaryColor3usvEXT("glSecondaryColor3usvEXT");
Function<void, GLint, GLenum, GLsizei> Binding::SecondaryColorFormatNV("glSecondaryColorFormatNV");
Function<void, GLenum, GLuint> Binding::SecondaryColorP3ui("glSecondaryColorP3ui");
Function<void, GLenum, const GLuint *> Binding::SecondaryColorP3uiv("glSecondaryColorP3uiv");
Function<void, GLint, GLenum, GLsizei, const void *> Binding::SecondaryColorPointer("glSecondaryColorPointer");
Function<void, GLint, GLenum, GLsizei, const void *> Binding::SecondaryColorPointerEXT("glSecondaryColorPointerEXT");
Function<void, GLint, GLenum, GLint, const void **, GLint> Binding::SecondaryColorPointerListIBM("glSecondaryColorPointerListIBM");
Function<void, GLsizei, GLuint *> Binding::SelectBuffer("glSelectBuffer");
Function<void, GLuint, GLboolean, GLuint, GLint, GLuint *> Binding::SelectPerfMonitorCountersAMD("glSelectPerfMonitorCountersAMD");
Function<void, GLenum, GLenum, GLsizei, GLsizei, GLenum, GLenum, const void *, const void *> Binding::SeparableFilter2D("glSeparableFilter2D");
Function<void, GLenum, GLenum, GLsizei, GLsizei, GLenum, GLenum, const void *, const void *> Binding::SeparableFilter2DEXT("glSeparableFilter2DEXT");
Function<void, GLuint> Binding::SetFenceAPPLE("glSetFenceAPPLE");
Function<void, GLuint, GLenum> Binding::SetFenceNV("glSetFenceNV");
Function<void, GLuint, const GLfloat *> Binding::SetFragmentShaderConstantATI("glSetFragmentShaderConstantATI");
Function<void, GLuint, GLenum, const void *> Binding::SetInvariantEXT("glSetInvariantEXT");
Function<void, GLuint, GLenum, const void *> Binding::SetLocalConstantEXT("glSetLocalConstantEXT");
Function<void, GLenum, GLuint, const GLfloat *> Binding::SetMultisamplefvAMD("glSetMultisamplefvAMD");
Function<void, GLenum> Binding::ShadeModel("glShadeModel");
Function<void, GLsizei, const GLuint *, GLenum, const void *, GLsizei> Binding::ShaderBinary("glShaderBinary");
Function<void, GLenum, GLuint, GLuint> Binding::ShaderOp1EXT("glShaderOp1EXT");
Function<void, GLenum, GLuint, GLuint, GLuint> Binding::ShaderOp2EXT("glShaderOp2EXT");
Function<void, GLenum, GLuint, GLuint, GLuint, GLuint> Binding::ShaderOp3EXT("glShaderOp3EXT");
Function<void, GLuint, GLsizei, const GLchar *const*, const GLint *> Binding::ShaderSource("glShaderSource");
Function<void, GLhandleARB, GLsizei, const GLcharARB **, const GLint *> Binding::ShaderSourceARB("glShaderSourceARB");
Function<void, GLuint, GLuint, GLuint> Binding::ShaderStorageBlockBinding("glShaderStorageBlockBinding");
Function<void, GLenum, GLsizei, const GLfloat *> Binding::SharpenTexFuncSGIS("glSharpenTexFuncSGIS");
Function<void, GLenum, GLfloat> Binding::SpriteParameterfSGIX("glSpriteParameterfSGIX");
Function<void, GLenum, const GLfloat *> Binding::SpriteParameterfvSGIX("glSpriteParameterfvSGIX");
Function<void, GLenum, GLint> Binding::SpriteParameteriSGIX("glSpriteParameteriSGIX");
Function<void, GLenum, const GLint *> Binding::SpriteParameterivSGIX("glSpriteParameterivSGIX");
Function<void> Binding::StartInstrumentsSGIX("glStartInstrumentsSGIX");
Function<void, GLuint, GLenum> Binding::StateCaptureNV("glStateCaptureNV");
Function<void, GLsizei, GLuint> Binding::StencilClearTagEXT("glStencilClearTagEXT");
Function<void, GLsizei, GLenum, const void *, GLuint, GLenum, GLuint, GLenum, const GLfloat *> Binding::StencilFillPathInstancedNV("glStencilFillPathInstancedNV");
Function<void, GLuint, GLenum, GLuint> Binding::StencilFillPathNV("glStencilFillPathNV");
Function<void, GLenum, GLint, GLuint> Binding::StencilFunc("glStencilFunc");
Function<void, GLenum, GLenum, GLint, GLuint> Binding::StencilFuncSeparate("glStencilFuncSeparate");
Function<void, GLenum, GLenum, GLint, GLuint> Binding::StencilFuncSeparateATI("glStencilFuncSeparateATI");
Function<void, GLuint> Binding::StencilMask("glStencilMask");
Function<void, GLenum, GLuint> Binding::StencilMaskSeparate("glStencilMaskSeparate");
Function<void, GLenum, GLenum, GLenum> Binding::StencilOp("glStencilOp");
Function<void, GLenum, GLenum, GLenum, GLenum> Binding::StencilOpSeparate("glStencilOpSeparate");
Function<void, GLenum, GLenum, GLenum, GLenum> Binding::StencilOpSeparateATI("glStencilOpSeparateATI");
Function<void, GLenum, GLuint> Binding::StencilOpValueAMD("glStencilOpValueAMD");
Function<void, GLsizei, GLenum, const void *, GLuint, GLint, GLuint, GLenum, const GLfloat *> Binding::StencilStrokePathInstancedNV("glStencilStrokePathInstancedNV");
Function<void, GLuint, GLint, GLuint> Binding::StencilStrokePathNV("glStencilStrokePathNV");
Function<void, GLsizei, GLenum, const void *, GLuint, GLenum, GLuint, GLenum, GLenum, const GLfloat *> Binding::StencilThenCoverFillPathInstancedNV("glStencilThenCoverFillPathInstancedNV");
Function<void, GLuint, GLenum, GLuint, GLenum> Binding::StencilThenCoverFillPathNV("glStencilThenCoverFillPathNV");
Function<void, GLsizei, GLenum, const void *, GLuint, GLint, GLuint, GLenum, GLenum, const GLfloat *> Binding::StencilThenCoverStrokePathInstancedNV("glStencilThenCoverStrokePathInstancedNV");
Function<void, GLuint, GLint, GLuint, GLenum> Binding::StencilThenCoverStrokePathNV("glStencilThenCoverStrokePathNV");
Function<void, GLint> Binding::StopInstrumentsSGIX("glStopInstrumentsSGIX");
Function<void, GLsizei, const void *> Binding::StringMarkerGREMEDY("glStringMarkerGREMEDY");
Function<void, GLuint, GLuint> Binding::SubpixelPrecisionBiasNV("glSubpixelPrecisionBiasNV");
Function<void, GLuint, GLuint, GLenum, GLenum, GLenum, GLenum> Binding::SwizzleEXT("glSwizzleEXT");
Function<void, GLuint> Binding::SyncTextureINTEL("glSyncTextureINTEL");
Function<void> Binding::TagSampleBufferSGIX("glTagSampleBufferSGIX");
Function<void, GLbyte, GLbyte, GLbyte> Binding::Tangent3bEXT("glTangent3bEXT");
Function<void, const GLbyte *> Binding::Tangent3bvEXT("glTangent3bvEXT");
Function<void, GLdouble, GLdouble, GLdouble> Binding::Tangent3dEXT("glTangent3dEXT");
Function<void, const GLdouble *> Binding::Tangent3dvEXT("glTangent3dvEXT");
Function<void, GLfloat, GLfloat, GLfloat> Binding::Tangent3fEXT("glTangent3fEXT");
Function<void, const GLfloat *> Binding::Tangent3fvEXT("glTangent3fvEXT");
Function<void, GLint, GLint, GLint> Binding::Tangent3iEXT("glTangent3iEXT");
Function<void, const GLint *> Binding::Tangent3ivEXT("glTangent3ivEXT");
Function<void, GLshort, GLshort, GLshort> Binding::Tangent3sEXT("glTangent3sEXT");
Function<void, const GLshort *> Binding::Tangent3svEXT("glTangent3svEXT");
Function<void, GLenum, GLsizei, const void *> Binding::TangentPointerEXT("glTangentPointerEXT");
Function<void, GLuint> Binding::TbufferMask3DFX("glTbufferMask3DFX");
Function<void, GLfloat> Binding::TessellationFactorAMD("glTessellationFactorAMD");
Function<void, GLenum> Binding::TessellationModeAMD("glTessellationModeAMD");
Function<GLboolean, GLuint> Binding::TestFenceAPPLE("glTestFenceAPPLE");
Function<GLboolean, GLuint> Binding::TestFenceNV("glTestFenceNV");
Function<GLboolean, GLenum, GLuint> Binding::TestObjectAPPLE("glTestObjectAPPLE");
Function<void, GLenum, GLenum, GLuint> Binding::TexBuffer("glTexBuffer");
Function<void, GLenum, GLenum, GLuint> Binding::TexBufferARB("glTexBufferARB");
Function<void, GLenum, GLenum, GLuint> Binding::TexBufferEXT("glTexBufferEXT");
Function<void, GLenum, GLenum, GLuint, GLintptr, GLsizeiptr> Binding::TexBufferRange("glTexBufferRange");
Function<void, GLenum, const GLfloat *> Binding::TexBumpParameterfvATI("glTexBumpParameterfvATI");
Function<void, GLenum, const GLint *> Binding::TexBumpParameterivATI("glTexBumpParameterivATI");
Function<void, GLbyte> Binding::TexCoord1bOES("glTexCoord1bOES");
Function<void, const GLbyte *> Binding::TexCoord1bvOES("glTexCoord1bvOES");
Function<void, GLdouble> Binding::TexCoord1d("glTexCoord1d");
Function<void, const GLdouble *> Binding::TexCoord1dv("glTexCoord1dv");
Function<void, GLfloat> Binding::TexCoord1f("glTexCoord1f");
Function<void, const GLfloat *> Binding::TexCoord1fv("glTexCoord1fv");
Function<void, GLhalfNV> Binding::TexCoord1hNV("glTexCoord1hNV");
Function<void, const GLhalfNV *> Binding::TexCoord1hvNV("glTexCoord1hvNV");
Function<void, GLint> Binding::TexCoord1i("glTexCoord1i");
Function<void, const GLint *> Binding::TexCoord1iv("glTexCoord1iv");
Function<void, GLshort> Binding::TexCoord1s("glTexCoord1s");
Function<void, const GLshort *> Binding::TexCoord1sv("glTexCoord1sv");
Function<void, GLfixed> Binding::TexCoord1xOES("glTexCoord1xOES");
Function<void, const GLfixed *> Binding::TexCoord1xvOES("glTexCoord1xvOES");
Function<void, GLbyte, GLbyte> Binding::TexCoord2bOES("glTexCoord2bOES");
Function<void, const GLbyte *> Binding::TexCoord2bvOES("glTexCoord2bvOES");
Function<void, GLdouble, GLdouble> Binding::TexCoord2d("glTexCoord2d");
Function<void, const GLdouble *> Binding::TexCoord2dv("glTexCoord2dv");
Function<void, GLfloat, GLfloat> Binding::TexCoord2f("glTexCoord2f");
Function<void, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat> Binding::TexCoord2fColor3fVertex3fSUN("glTexCoord2fColor3fVertex3fSUN");
Function<void, const GLfloat *, const GLfloat *, const GLfloat *> Binding::TexCoord2fColor3fVertex3fvSUN("glTexCoord2fColor3fVertex3fvSUN");
Function<void, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat> Binding::TexCoord2fColor4fNormal3fVertex3fSUN("glTexCoord2fColor4fNormal3fVertex3fSUN");
Function<void, const GLfloat *, const GLfloat *, const GLfloat *, const GLfloat *> Binding::TexCoord2fColor4fNormal3fVertex3fvSUN("glTexCoord2fColor4fNormal3fVertex3fvSUN");
Function<void, GLfloat, GLfloat, GLubyte, GLubyte, GLubyte, GLubyte, GLfloat, GLfloat, GLfloat> Binding::TexCoord2fColor4ubVertex3fSUN("glTexCoord2fColor4ubVertex3fSUN");
Function<void, const GLfloat *, const GLubyte *, const GLfloat *> Binding::TexCoord2fColor4ubVertex3fvSUN("glTexCoord2fColor4ubVertex3fvSUN");
Function<void, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat> Binding::TexCoord2fNormal3fVertex3fSUN("glTexCoord2fNormal3fVertex3fSUN");
Function<void, const GLfloat *, const GLfloat *, const GLfloat *> Binding::TexCoord2fNormal3fVertex3fvSUN("glTexCoord2fNormal3fVertex3fvSUN");
Function<void, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat> Binding::TexCoord2fVertex3fSUN("glTexCoord2fVertex3fSUN");
Function<void, const GLfloat *, const GLfloat *> Binding::TexCoord2fVertex3fvSUN("glTexCoord2fVertex3fvSUN");
Function<void, const GLfloat *> Binding::TexCoord2fv("glTexCoord2fv");
Function<void, GLhalfNV, GLhalfNV> Binding::TexCoord2hNV("glTexCoord2hNV");
Function<void, const GLhalfNV *> Binding::TexCoord2hvNV("glTexCoord2hvNV");
Function<void, GLint, GLint> Binding::TexCoord2i("glTexCoord2i");
Function<void, const GLint *> Binding::TexCoord2iv("glTexCoord2iv");
Function<void, GLshort, GLshort> Binding::TexCoord2s("glTexCoord2s");
Function<void, const GLshort *> Binding::TexCoord2sv("glTexCoord2sv");
Function<void, GLfixed, GLfixed> Binding::TexCoord2xOES("glTexCoord2xOES");
Function<void, const GLfixed *> Binding::TexCoord2xvOES("glTexCoord2xvOES");
Function<void, GLbyte, GLbyte, GLbyte> Binding::TexCoord3bOES("glTexCoord3bOES");
Function<void, const GLbyte *> Binding::TexCoord3bvOES("glTexCoord3bvOES");
Function<void, GLdouble, GLdouble, GLdouble> Binding::TexCoord3d("glTexCoord3d");
Function<void, const GLdouble *> Binding::TexCoord3dv("glTexCoord3dv");
Function<void, GLfloat, GLfloat, GLfloat> Binding::TexCoord3f("glTexCoord3f");
Function<void, const GLfloat *> Binding::TexCoord3fv("glTexCoord3fv");
Function<void, GLhalfNV, GLhalfNV, GLhalfNV> Binding::TexCoord3hNV("glTexCoord3hNV");
Function<void, const GLhalfNV *> Binding::TexCoord3hvNV("glTexCoord3hvNV");
Function<void, GLint, GLint, GLint> Binding::TexCoord3i("glTexCoord3i");
Function<void, const GLint *> Binding::TexCoord3iv("glTexCoord3iv");
Function<void, GLshort, GLshort, GLshort> Binding::TexCoord3s("glTexCoord3s");
Function<void, const GLshort *> Binding::TexCoord3sv("glTexCoord3sv");
Function<void, GLfixed, GLfixed, GLfixed> Binding::TexCoord3xOES("glTexCoord3xOES");
Function<void, const GLfixed *> Binding::TexCoord3xvOES("glTexCoord3xvOES");
Function<void, GLbyte, GLbyte, GLbyte, GLbyte> Binding::TexCoord4bOES("glTexCoord4bOES");
Function<void, const GLbyte *> Binding::TexCoord4bvOES("glTexCoord4bvOES");
Function<void, GLdouble, GLdouble, GLdouble, GLdouble> Binding::TexCoord4d("glTexCoord4d");
Function<void, const GLdouble *> Binding::TexCoord4dv("glTexCoord4dv");
Function<void, GLfloat, GLfloat, GLfloat, GLfloat> Binding::TexCoord4f("glTexCoord4f");
Function<void, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat> Binding::TexCoord4fColor4fNormal3fVertex4fSUN("glTexCoord4fColor4fNormal3fVertex4fSUN");
Function<void, const GLfloat *, const GLfloat *, const GLfloat *, const GLfloat *> Binding::TexCoord4fColor4fNormal3fVertex4fvSUN("glTexCoord4fColor4fNormal3fVertex4fvSUN");
Function<void, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat> Binding::TexCoord4fVertex4fSUN("glTexCoord4fVertex4fSUN");
Function<void, const GLfloat *, const GLfloat *> Binding::TexCoord4fVertex4fvSUN("glTexCoord4fVertex4fvSUN");
Function<void, const GLfloat *> Binding::TexCoord4fv("glTexCoord4fv");
Function<void, GLhalfNV, GLhalfNV, GLhalfNV, GLhalfNV> Binding::TexCoord4hNV("glTexCoord4hNV");
Function<void, const GLhalfNV *> Binding::TexCoord4hvNV("glTexCoord4hvNV");
Function<void, GLint, GLint, GLint, GLint> Binding::TexCoord4i("glTexCoord4i");
Function<void, const GLint *> Binding::TexCoord4iv("glTexCoord4iv");
Function<void, GLshort, GLshort, GLshort, GLshort> Binding::TexCoord4s("glTexCoord4s");
Function<void, const GLshort *> Binding::TexCoord4sv("glTexCoord4sv");
Function<void, GLfixed, GLfixed, GLfixed, GLfixed> Binding::TexCoord4xOES("glTexCoord4xOES");
Function<void, const GLfixed *> Binding::TexCoord4xvOES("glTexCoord4xvOES");
Function<void, GLint, GLenum, GLsizei> Binding::TexCoordFormatNV("glTexCoordFormatNV");
Function<void, GLenum, GLuint> Binding::TexCoordP1ui("glTexCoordP1ui");
Function<void, GLenum, const GLuint *> Binding::TexCoordP1uiv("glTexCoordP1uiv");
Function<void, GLenum, GLuint> Binding::TexCoordP2ui("glTexCoordP2ui");
Function<void, GLenum, const GLuint *> Binding::TexCoordP2uiv("glTexCoordP2uiv");
Function<void, GLenum, GLuint> Binding::TexCoordP3ui("glTexCoordP3ui");
Function<void, GLenum, const GLuint *> Binding::TexCoordP3uiv("glTexCoordP3uiv");
Function<void, GLenum, GLuint> Binding::TexCoordP4ui("glTexCoordP4ui");
Function<void, GLenum, const GLuint *> Binding::TexCoordP4uiv("glTexCoordP4uiv");
Function<void, GLint, GLenum, GLsizei, const void *> Binding::TexCoordPointer("glTexCoordPointer");
Function<void, GLint, GLenum, GLsizei, GLsizei, const void *> Binding::TexCoordPointerEXT("glTexCoordPointerEXT");
Function<void, GLint, GLenum, GLint, const void **, GLint> Binding::TexCoordPointerListIBM("glTexCoordPointerListIBM");
Function<void, GLint, GLenum, const void **> Binding::TexCoordPointervINTEL("glTexCoordPointervINTEL");
Function<void, GLenum, GLenum, GLfloat> Binding::TexEnvf("glTexEnvf");
Function<void, GLenum, GLenum, const GLfloat *> Binding::TexEnvfv("glTexEnvfv");
Function<void, GLenum, GLenum, GLint> Binding::TexEnvi("glTexEnvi");
Function<void, GLenum, GLenum, const GLint *> Binding::TexEnviv("glTexEnviv");
Function<void, GLenum, GLenum, GLfixed> Binding::TexEnvxOES("glTexEnvxOES");
Function<void, GLenum, GLenum, const GLfixed *> Binding::TexEnvxvOES("glTexEnvxvOES");
Function<void, GLenum, GLenum, GLsizei, const GLfloat *> Binding::TexFilterFuncSGIS("glTexFilterFuncSGIS");
Function<void, GLenum, GLenum, GLdouble> Binding::TexGend("glTexGend");
Function<void, GLenum, GLenum, const GLdouble *> Binding::TexGendv("glTexGendv");
Function<void, GLenum, GLenum, GLfloat> Binding::TexGenf("glTexGenf");
Function<void, GLenum, GLenum, const GLfloat *> Binding::TexGenfv("glTexGenfv");
Function<void, GLenum, GLenum, GLint> Binding::TexGeni("glTexGeni");
Function<void, GLenum, GLenum, const GLint *> Binding::TexGeniv("glTexGeniv");
Function<void, GLenum, GLenum, GLfixed> Binding::TexGenxOES("glTexGenxOES");
Function<void, GLenum, GLenum, const GLfixed *> Binding::TexGenxvOES("glTexGenxvOES");
Function<void, GLenum, GLint, GLint, GLsizei, GLint, GLenum, GLenum, const void *> Binding::TexImage1D("glTexImage1D");
Function<void, GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *> Binding::TexImage2D("glTexImage2D");
Function<void, GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLboolean> Binding::TexImage2DMultisample("glTexImage2DMultisample");
Function<void, GLenum, GLsizei, GLsizei, GLint, GLsizei, GLsizei, GLboolean> Binding::TexImage2DMultisampleCoverageNV("glTexImage2DMultisampleCoverageNV");
Function<void, GLenum, GLint, GLint, GLsizei, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *> Binding::TexImage3D("glTexImage3D");
Function<void, GLenum, GLint, GLenum, GLsizei, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *> Binding::TexImage3DEXT("glTexImage3DEXT");
Function<void, GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLsizei, GLboolean> Binding::TexImage3DMultisample("glTexImage3DMultisample");
Function<void, GLenum, GLsizei, GLsizei, GLint, GLsizei, GLsizei, GLsizei, GLboolean> Binding::TexImage3DMultisampleCoverageNV("glTexImage3DMultisampleCoverageNV");
Function<void, GLenum, GLint, GLenum, GLsizei, GLsizei, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *> Binding::TexImage4DSGIS("glTexImage4DSGIS");
Function<void, GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLboolean> Binding::TexPageCommitmentARB("glTexPageCommitmentARB");
Function<void, GLenum, GLenum, const GLint *> Binding::TexParameterIiv("glTexParameterIiv");
Function<void, GLenum, GLenum, const GLint *> Binding::TexParameterIivEXT("glTexParameterIivEXT");
Function<void, GLenum, GLenum, const GLuint *> Binding::TexParameterIuiv("glTexParameterIuiv");
Function<void, GLenum, GLenum, const GLuint *> Binding::TexParameterIuivEXT("glTexParameterIuivEXT");
Function<void, GLenum, GLenum, GLfloat> Binding::TexParameterf("glTexParameterf");
Function<void, GLenum, GLenum, const GLfloat *> Binding::TexParameterfv("glTexParameterfv");
Function<void, GLenum, GLenum, GLint> Binding::TexParameteri("glTexParameteri");
Function<void, GLenum, GLenum, const GLint *> Binding::TexParameteriv("glTexParameteriv");
Function<void, GLenum, GLenum, GLfixed> Binding::TexParameterxOES("glTexParameterxOES");
Function<void, GLenum, GLenum, const GLfixed *> Binding::TexParameterxvOES("glTexParameterxvOES");
Function<void, GLenum, GLuint> Binding::TexRenderbufferNV("glTexRenderbufferNV");
Function<void, GLenum, GLsizei, GLenum, GLsizei> Binding::TexStorage1D("glTexStorage1D");
Function<void, GLenum, GLsizei, GLenum, GLsizei, GLsizei> Binding::TexStorage2D("glTexStorage2D");
Function<void, GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLboolean> Binding::TexStorage2DMultisample("glTexStorage2DMultisample");
Function<void, GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLsizei> Binding::TexStorage3D("glTexStorage3D");
Function<void, GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLsizei, GLboolean> Binding::TexStorage3DMultisample("glTexStorage3DMultisample");
Function<void, GLenum, GLenum, GLsizei, GLsizei, GLsizei, GLsizei, TextureStorageMaskAMD> Binding::TexStorageSparseAMD("glTexStorageSparseAMD");
Function<void, GLenum, GLint, GLint, GLsizei, GLenum, GLenum, const void *> Binding::TexSubImage1D("glTexSubImage1D");
Function<void, GLenum, GLint, GLint, GLsizei, GLenum, GLenum, const void *> Binding::TexSubImage1DEXT("glTexSubImage1DEXT");
Function<void, GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void *> Binding::TexSubImage2D("glTexSubImage2D");
Function<void, GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void *> Binding::TexSubImage2DEXT("glTexSubImage2DEXT");
Function<void, GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLenum, const void *> Binding::TexSubImage3D("glTexSubImage3D");
Function<void, GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLenum, const void *> Binding::TexSubImage3DEXT("glTexSubImage3DEXT");
Function<void, GLenum, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLsizei, GLenum, GLenum, const void *> Binding::TexSubImage4DSGIS("glTexSubImage4DSGIS");
Function<void> Binding::TextureBarrier("glTextureBarrier");
Function<void> Binding::TextureBarrierNV("glTextureBarrierNV");
Function<void, GLuint, GLenum, GLuint> Binding::TextureBuffer("glTextureBuffer");
Function<void, GLuint, GLenum, GLenum, GLuint> Binding::TextureBufferEXT("glTextureBufferEXT");
Function<void, GLuint, GLenum, GLuint, GLintptr, GLsizeiptr> Binding::TextureBufferRange("glTextureBufferRange");
Function<void, GLuint, GLenum, GLenum, GLuint, GLintptr, GLsizeiptr> Binding::TextureBufferRangeEXT("glTextureBufferRangeEXT");
Function<void, GLboolean, GLboolean, GLboolean, GLboolean> Binding::TextureColorMaskSGIS("glTextureColorMaskSGIS");
Function<void, GLuint, GLenum, GLint, GLint, GLsizei, GLint, GLenum, GLenum, const void *> Binding::TextureImage1DEXT("glTextureImage1DEXT");
Function<void, GLuint, GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *> Binding::TextureImage2DEXT("glTextureImage2DEXT");
Function<void, GLuint, GLenum, GLsizei, GLsizei, GLint, GLsizei, GLsizei, GLboolean> Binding::TextureImage2DMultisampleCoverageNV("glTextureImage2DMultisampleCoverageNV");
Function<void, GLuint, GLenum, GLsizei, GLint, GLsizei, GLsizei, GLboolean> Binding::TextureImage2DMultisampleNV("glTextureImage2DMultisampleNV");
Function<void, GLuint, GLenum, GLint, GLint, GLsizei, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *> Binding::TextureImage3DEXT("glTextureImage3DEXT");
Function<void, GLuint, GLenum, GLsizei, GLsizei, GLint, GLsizei, GLsizei, GLsizei, GLboolean> Binding::TextureImage3DMultisampleCoverageNV("glTextureImage3DMultisampleCoverageNV");
Function<void, GLuint, GLenum, GLsizei, GLint, GLsizei, GLsizei, GLsizei, GLboolean> Binding::TextureImage3DMultisampleNV("glTextureImage3DMultisampleNV");
Function<void, GLenum> Binding::TextureLightEXT("glTextureLightEXT");
Function<void, GLenum, GLenum> Binding::TextureMaterialEXT("glTextureMaterialEXT");
Function<void, GLenum> Binding::TextureNormalEXT("glTextureNormalEXT");
Function<void, GLuint, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLboolean> Binding::TexturePageCommitmentEXT("glTexturePageCommitmentEXT");
Function<void, GLuint, GLenum, const GLint *> Binding::TextureParameterIiv("glTextureParameterIiv");
Function<void, GLuint, GLenum, GLenum, const GLint *> Binding::TextureParameterIivEXT("glTextureParameterIivEXT");
Function<void, GLuint, GLenum, const GLuint *> Binding::TextureParameterIuiv("glTextureParameterIuiv");
Function<void, GLuint, GLenum, GLenum, const GLuint *> Binding::TextureParameterIuivEXT("glTextureParameterIuivEXT");
Function<void, GLuint, GLenum, GLfloat> Binding::TextureParameterf("glTextureParameterf");
Function<void, GLuint, GLenum, GLenum, GLfloat> Binding::TextureParameterfEXT("glTextureParameterfEXT");
Function<void, GLuint, GLenum, const GLfloat *> Binding::TextureParameterfv("glTextureParameterfv");
Function<void, GLuint, GLenum, GLenum, const GLfloat *> Binding::TextureParameterfvEXT("glTextureParameterfvEXT");
Function<void, GLuint, GLenum, GLint> Binding::TextureParameteri("glTextureParameteri");
Function<void, GLuint, GLenum, GLenum, GLint> Binding::TextureParameteriEXT("glTextureParameteriEXT");
Function<void, GLuint, GLenum, const GLint *> Binding::TextureParameteriv("glTextureParameteriv");
Function<void, GLuint, GLenum, GLenum, const GLint *> Binding::TextureParameterivEXT("glTextureParameterivEXT");
Function<void, GLenum, GLsizei, const void *> Binding::TextureRangeAPPLE("glTextureRangeAPPLE");
Function<void, GLuint, GLenum, GLuint> Binding::TextureRenderbufferEXT("glTextureRenderbufferEXT");
Function<void, GLuint, GLsizei, GLenum, GLsizei> Binding::TextureStorage1D("glTextureStorage1D");
Function<void, GLuint, GLenum, GLsizei, GLenum, GLsizei> Binding::TextureStorage1DEXT("glTextureStorage1DEXT");
Function<void, GLuint, GLsizei, GLenum, GLsizei, GLsizei> Binding::TextureStorage2D("glTextureStorage2D");
Function<void, GLuint, GLenum, GLsizei, GLenum, GLsizei, GLsizei> Binding::TextureStorage2DEXT("glTextureStorage2DEXT");
Function<void, GLuint, GLsizei, GLenum, GLsizei, GLsizei, GLboolean> Binding::TextureStorage2DMultisample("glTextureStorage2DMultisample");
Function<void, GLuint, GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLboolean> Binding::TextureStorage2DMultisampleEXT("glTextureStorage2DMultisampleEXT");
Function<void, GLuint, GLsizei, GLenum, GLsizei, GLsizei, GLsizei> Binding::TextureStorage3D("glTextureStorage3D");
Function<void, GLuint, GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLsizei> Binding::TextureStorage3DEXT("glTextureStorage3DEXT");
Function<void, GLuint, GLsizei, GLenum, GLsizei, GLsizei, GLsizei, GLboolean> Binding::TextureStorage3DMultisample("glTextureStorage3DMultisample");
Function<void, GLuint, GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLsizei, GLboolean> Binding::TextureStorage3DMultisampleEXT("glTextureStorage3DMultisampleEXT");
Function<void, GLuint, GLenum, GLenum, GLsizei, GLsizei, GLsizei, GLsizei, TextureStorageMaskAMD> Binding::TextureStorageSparseAMD("glTextureStorageSparseAMD");
Function<void, GLuint, GLint, GLint, GLsizei, GLenum, GLenum, const void *> Binding::TextureSubImage1D("glTextureSubImage1D");
Function<void, GLuint, GLenum, GLint, GLint, GLsizei, GLenum, GLenum, const void *> Binding::TextureSubImage1DEXT("glTextureSubImage1DEXT");
Function<void, GLuint, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void *> Binding::TextureSubImage2D("glTextureSubImage2D");
Function<void, GLuint, GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void *> Binding::TextureSubImage2DEXT("glTextureSubImage2DEXT");
Function<void, GLuint, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLenum, const void *> Binding::TextureSubImage3D("glTextureSubImage3D");
Function<void, GLuint, GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLenum, const void *> Binding::TextureSubImage3DEXT("glTextureSubImage3DEXT");
Function<void, GLuint, GLenum, GLuint, GLenum, GLuint, GLuint, GLuint, GLuint> Binding::TextureView("glTextureView");
Function<void, GLenum, GLuint, GLenum, GLenum> Binding::TrackMatrixNV("glTrackMatrixNV");
Function<void, GLsizei, const GLint *, GLenum> Binding::TransformFeedbackAttribsNV("glTransformFeedbackAttribsNV");
Function<void, GLuint, GLuint, GLuint> Binding::TransformFeedbackBufferBase("glTransformFeedbackBufferBase");
Function<void, GLuint, GLuint, GLuint, GLintptr, GLsizeiptr> Binding::TransformFeedbackBufferRange("glTransformFeedbackBufferRange");
Function<void, GLsizei, const GLint *, GLsizei, const GLint *, GLenum> Binding::TransformFeedbackStreamAttribsNV("glTransformFeedbackStreamAttribsNV");
Function<void, GLuint, GLsizei, const GLchar *const*, GLenum> Binding::TransformFeedbackVaryings("glTransformFeedbackVaryings");
Function<void, GLuint, GLsizei, const GLchar *const*, GLenum> Binding::TransformFeedbackVaryingsEXT("glTransformFeedbackVaryingsEXT");
Function<void, GLuint, GLsizei, const GLint *, GLenum> Binding::TransformFeedbackVaryingsNV("glTransformFeedbackVaryingsNV");
Function<void, GLuint, GLuint, GLenum, const GLfloat *> Binding::TransformPathNV("glTransformPathNV");
Function<void, GLdouble, GLdouble, GLdouble> Binding::Translated("glTranslated");
Function<void, GLfloat, GLfloat, GLfloat> Binding::Translatef("glTranslatef");
Function<void, GLfixed, GLfixed, GLfixed> Binding::TranslatexOES("glTranslatexOES");
Function<void, GLint, GLdouble> Binding::Uniform1d("glUniform1d");
Function<void, GLint, GLsizei, const GLdouble *> Binding::Uniform1dv("glUniform1dv");
Function<void, GLint, GLfloat> Binding::Uniform1f("glUniform1f");
Function<void, GLint, GLfloat> Binding::Uniform1fARB("glUniform1fARB");
Function<void, GLint, GLsizei, const GLfloat *> Binding::Uniform1fv("glUniform1fv");
Function<void, GLint, GLsizei, const GLfloat *> Binding::Uniform1fvARB("glUniform1fvARB");
Function<void, GLint, GLint> Binding::Uniform1i("glUniform1i");
Function<void, GLint, GLint64> Binding::Uniform1i64ARB("glUniform1i64ARB");
Function<void, GLint, GLint64EXT> Binding::Uniform1i64NV("glUniform1i64NV");
Function<void, GLint, GLsizei, const GLint64 *> Binding::Uniform1i64vARB("glUniform1i64vARB");
Function<void, GLint, GLsizei, const GLint64EXT *> Binding::Uniform1i64vNV("glUniform1i64vNV");
Function<void, GLint, GLint> Binding::Uniform1iARB("glUniform1iARB");
Function<void, GLint, GLsizei, const GLint *> Binding::Uniform1iv("glUniform1iv");
Function<void, GLint, GLsizei, const GLint *> Binding::Uniform1ivARB("glUniform1ivARB");
Function<void, GLint, GLuint> Binding::Uniform1ui("glUniform1ui");
Function<void, GLint, GLuint64> Binding::Uniform1ui64ARB("glUniform1ui64ARB");
Function<void, GLint, GLuint64EXT> Binding::Uniform1ui64NV("glUniform1ui64NV");
Function<void, GLint, GLsizei, const GLuint64 *> Binding::Uniform1ui64vARB("glUniform1ui64vARB");
Function<void, GLint, GLsizei, const GLuint64EXT *> Binding::Uniform1ui64vNV("glUniform1ui64vNV");
Function<void, GLint, GLuint> Binding::Uniform1uiEXT("glUniform1uiEXT");
Function<void, GLint, GLsizei, const GLuint *> Binding::Uniform1uiv("glUniform1uiv");
Function<void, GLint, GLsizei, const GLuint *> Binding::Uniform1uivEXT("glUniform1uivEXT");
Function<void, GLint, GLdouble, GLdouble> Binding::Uniform2d("glUniform2d");
Function<void, GLint, GLsizei, const GLdouble *> Binding::Uniform2dv("glUniform2dv");
Function<void, GLint, GLfloat, GLfloat> Binding::Uniform2f("glUniform2f");
Function<void, GLint, GLfloat, GLfloat> Binding::Uniform2fARB("glUniform2fARB");
Function<void, GLint, GLsizei, const GLfloat *> Binding::Uniform2fv("glUniform2fv");
Function<void, GLint, GLsizei, const GLfloat *> Binding::Uniform2fvARB("glUniform2fvARB");
Function<void, GLint, GLint, GLint> Binding::Uniform2i("glUniform2i");
Function<void, GLint, GLint64, GLint64> Binding::Uniform2i64ARB("glUniform2i64ARB");
Function<void, GLint, GLint64EXT, GLint64EXT> Binding::Uniform2i64NV("glUniform2i64NV");
Function<void, GLint, GLsizei, const GLint64 *> Binding::Uniform2i64vARB("glUniform2i64vARB");
Function<void, GLint, GLsizei, const GLint64EXT *> Binding::Uniform2i64vNV("glUniform2i64vNV");
Function<void, GLint, GLint, GLint> Binding::Uniform2iARB("glUniform2iARB");
Function<void, GLint, GLsizei, const GLint *> Binding::Uniform2iv("glUniform2iv");
Function<void, GLint, GLsizei, const GLint *> Binding::Uniform2ivARB("glUniform2ivARB");
Function<void, GLint, GLuint, GLuint> Binding::Uniform2ui("glUniform2ui");
Function<void, GLint, GLuint64, GLuint64> Binding::Uniform2ui64ARB("glUniform2ui64ARB");
Function<void, GLint, GLuint64EXT, GLuint64EXT> Binding::Uniform2ui64NV("glUniform2ui64NV");
Function<void, GLint, GLsizei, const GLuint64 *> Binding::Uniform2ui64vARB("glUniform2ui64vARB");
Function<void, GLint, GLsizei, const GLuint64EXT *> Binding::Uniform2ui64vNV("glUniform2ui64vNV");
Function<void, GLint, GLuint, GLuint> Binding::Uniform2uiEXT("glUniform2uiEXT");
Function<void, GLint, GLsizei, const GLuint *> Binding::Uniform2uiv("glUniform2uiv");
Function<void, GLint, GLsizei, const GLuint *> Binding::Uniform2uivEXT("glUniform2uivEXT");
Function<void, GLint, GLdouble, GLdouble, GLdouble> Binding::Uniform3d("glUniform3d");
Function<void, GLint, GLsizei, const GLdouble *> Binding::Uniform3dv("glUniform3dv");
Function<void, GLint, GLfloat, GLfloat, GLfloat> Binding::Uniform3f("glUniform3f");
Function<void, GLint, GLfloat, GLfloat, GLfloat> Binding::Uniform3fARB("glUniform3fARB");
Function<void, GLint, GLsizei, const GLfloat *> Binding::Uniform3fv("glUniform3fv");
Function<void, GLint, GLsizei, const GLfloat *> Binding::Uniform3fvARB("glUniform3fvARB");
Function<void, GLint, GLint, GLint, GLint> Binding::Uniform3i("glUniform3i");
Function<void, GLint, GLint64, GLint64, GLint64> Binding::Uniform3i64ARB("glUniform3i64ARB");
Function<void, GLint, GLint64EXT, GLint64EXT, GLint64EXT> Binding::Uniform3i64NV("glUniform3i64NV");
Function<void, GLint, GLsizei, const GLint64 *> Binding::Uniform3i64vARB("glUniform3i64vARB");
Function<void, GLint, GLsizei, const GLint64EXT *> Binding::Uniform3i64vNV("glUniform3i64vNV");
Function<void, GLint, GLint, GLint, GLint> Binding::Uniform3iARB("glUniform3iARB");
Function<void, GLint, GLsizei, const GLint *> Binding::Uniform3iv("glUniform3iv");
Function<void, GLint, GLsizei, const GLint *> Binding::Uniform3ivARB("glUniform3ivARB");
Function<void, GLint, GLuint, GLuint, GLuint> Binding::Uniform3ui("glUniform3ui");
Function<void, GLint, GLuint64, GLuint64, GLuint64> Binding::Uniform3ui64ARB("glUniform3ui64ARB");
Function<void, GLint, GLuint64EXT, GLuint64EXT, GLuint64EXT> Binding::Uniform3ui64NV("glUniform3ui64NV");
Function<void, GLint, GLsizei, const GLuint64 *> Binding::Uniform3ui64vARB("glUniform3ui64vARB");
Function<void, GLint, GLsizei, const GLuint64EXT *> Binding::Uniform3ui64vNV("glUniform3ui64vNV");
Function<void, GLint, GLuint, GLuint, GLuint> Binding::Uniform3uiEXT("glUniform3uiEXT");
Function<void, GLint, GLsizei, const GLuint *> Binding::Uniform3uiv("glUniform3uiv");
Function<void, GLint, GLsizei, const GLuint *> Binding::Uniform3uivEXT("glUniform3uivEXT");
Function<void, GLint, GLdouble, GLdouble, GLdouble, GLdouble> Binding::Uniform4d("glUniform4d");
Function<void, GLint, GLsizei, const GLdouble *> Binding::Uniform4dv("glUniform4dv");
Function<void, GLint, GLfloat, GLfloat, GLfloat, GLfloat> Binding::Uniform4f("glUniform4f");
Function<void, GLint, GLfloat, GLfloat, GLfloat, GLfloat> Binding::Uniform4fARB("glUniform4fARB");
Function<void, GLint, GLsizei, const GLfloat *> Binding::Uniform4fv("glUniform4fv");
Function<void, GLint, GLsizei, const GLfloat *> Binding::Uniform4fvARB("glUniform4fvARB");
Function<void, GLint, GLint, GLint, GLint, GLint> Binding::Uniform4i("glUniform4i");
Function<void, GLint, GLint64, GLint64, GLint64, GLint64> Binding::Uniform4i64ARB("glUniform4i64ARB");
Function<void, GLint, GLint64EXT, GLint64EXT, GLint64EXT, GLint64EXT> Binding::Uniform4i64NV("glUniform4i64NV");
Function<void, GLint, GLsizei, const GLint64 *> Binding::Uniform4i64vARB("glUniform4i64vARB");
Function<void, GLint, GLsizei, const GLint64EXT *> Binding::Uniform4i64vNV("glUniform4i64vNV");
Function<void, GLint, GLint, GLint, GLint, GLint> Binding::Uniform4iARB("glUniform4iARB");
Function<void, GLint, GLsizei, const GLint *> Binding::Uniform4iv("glUniform4iv");
Function<void, GLint, GLsizei, const GLint *> Binding::Uniform4ivARB("glUniform4ivARB");
Function<void, GLint, GLuint, GLuint, GLuint, GLuint> Binding::Uniform4ui("glUniform4ui");
Function<void, GLint, GLuint64, GLuint64, GLuint64, GLuint64> Binding::Uniform4ui64ARB("glUniform4ui64ARB");
Function<void, GLint, GLuint64EXT, GLuint64EXT, GLuint64EXT, GLuint64EXT> Binding::Uniform4ui64NV("glUniform4ui64NV");
Function<void, GLint, GLsizei, const GLuint64 *> Binding::Uniform4ui64vARB("glUniform4ui64vARB");
Function<void, GLint, GLsizei, const GLuint64EXT *> Binding::Uniform4ui64vNV("glUniform4ui64vNV");
Function<void, GLint, GLuint, GLuint, GLuint, GLuint> Binding::Uniform4uiEXT("glUniform4uiEXT");
Function<void, GLint, GLsizei, const GLuint *> Binding::Uniform4uiv("glUniform4uiv");
Function<void, GLint, GLsizei, const GLuint *> Binding::Uniform4uivEXT("glUniform4uivEXT");
Function<void, GLuint, GLuint, GLuint> Binding::UniformBlockBinding("glUniformBlockBinding");
Function<void, GLuint, GLint, GLuint> Binding::UniformBufferEXT("glUniformBufferEXT");
Function<void, GLint, GLuint64> Binding::UniformHandleui64ARB("glUniformHandleui64ARB");
Function<void, GLint, GLuint64> Binding::UniformHandleui64NV("glUniformHandleui64NV");
Function<void, GLint, GLsizei, const GLuint64 *> Binding::UniformHandleui64vARB("glUniformHandleui64vARB");
Function<void, GLint, GLsizei, const GLuint64 *> Binding::UniformHandleui64vNV("glUniformHandleui64vNV");
Function<void, GLint, GLsizei, GLboolean, const GLdouble *> Binding::UniformMatrix2dv("glUniformMatrix2dv");
Function<void, GLint, GLsizei, GLboolean, const GLfloat *> Binding::UniformMatrix2fv("glUniformMatrix2fv");
Function<void, GLint, GLsizei, GLboolean, const GLfloat *> Binding::UniformMatrix2fvARB("glUniformMatrix2fvARB");
Function<void, GLint, GLsizei, GLboolean, const GLdouble *> Binding::UniformMatrix2x3dv("glUniformMatrix2x3dv");
Function<void, GLint, GLsizei, GLboolean, const GLfloat *> Binding::UniformMatrix2x3fv("glUniformMatrix2x3fv");
Function<void, GLint, GLsizei, GLboolean, const GLdouble *> Binding::UniformMatrix2x4dv("glUniformMatrix2x4dv");
Function<void, GLint, GLsizei, GLboolean, const GLfloat *> Binding::UniformMatrix2x4fv("glUniformMatrix2x4fv");
Function<void, GLint, GLsizei, GLboolean, const GLdouble *> Binding::UniformMatrix3dv("glUniformMatrix3dv");
Function<void, GLint, GLsizei, GLboolean, const GLfloat *> Binding::UniformMatrix3fv("glUniformMatrix3fv");
Function<void, GLint, GLsizei, GLboolean, const GLfloat *> Binding::UniformMatrix3fvARB("glUniformMatrix3fvARB");
Function<void, GLint, GLsizei, GLboolean, const GLdouble *> Binding::UniformMatrix3x2dv("glUniformMatrix3x2dv");
Function<void, GLint, GLsizei, GLboolean, const GLfloat *> Binding::UniformMatrix3x2fv("glUniformMatrix3x2fv");
Function<void, GLint, GLsizei, GLboolean, const GLdouble *> Binding::UniformMatrix3x4dv("glUniformMatrix3x4dv");
Function<void, GLint, GLsizei, GLboolean, const GLfloat *> Binding::UniformMatrix3x4fv("glUniformMatrix3x4fv");
Function<void, GLint, GLsizei, GLboolean, const GLdouble *> Binding::UniformMatrix4dv("glUniformMatrix4dv");
Function<void, GLint, GLsizei, GLboolean, const GLfloat *> Binding::UniformMatrix4fv("glUniformMatrix4fv");
Function<void, GLint, GLsizei, GLboolean, const GLfloat *> Binding::UniformMatrix4fvARB("glUniformMatrix4fvARB");
Function<void, GLint, GLsizei, GLboolean, const GLdouble *> Binding::UniformMatrix4x2dv("glUniformMatrix4x2dv");
Function<void, GLint, GLsizei, GLboolean, const GLfloat *> Binding::UniformMatrix4x2fv("glUniformMatrix4x2fv");
Function<void, GLint, GLsizei, GLboolean, const GLdouble *> Binding::UniformMatrix4x3dv("glUniformMatrix4x3dv");
Function<void, GLint, GLsizei, GLboolean, const GLfloat *> Binding::UniformMatrix4x3fv("glUniformMatrix4x3fv");
Function<void, GLenum, GLsizei, const GLuint *> Binding::UniformSubroutinesuiv("glUniformSubroutinesuiv");
Function<void, GLint, GLuint64EXT> Binding::Uniformui64NV("glUniformui64NV");
Function<void, GLint, GLsizei, const GLuint64EXT *> Binding::Uniformui64vNV("glUniformui64vNV");
Function<void> Binding::UnlockArraysEXT("glUnlockArraysEXT");
Function<GLboolean, GLenum> Binding::UnmapBuffer("glUnmapBuffer");
Function<GLboolean, GLenum> Binding::UnmapBufferARB("glUnmapBufferARB");
Function<GLboolean, GLuint> Binding::UnmapNamedBuffer("glUnmapNamedBuffer");
Function<GLboolean, GLuint> Binding::UnmapNamedBufferEXT("glUnmapNamedBufferEXT");
Function<void, GLuint> Binding::UnmapObjectBufferATI("glUnmapObjectBufferATI");
Function<void, GLuint, GLint> Binding::UnmapTexture2DINTEL("glUnmapTexture2DINTEL");
Function<void, GLuint, GLuint, GLsizei, const void *, GLenum> Binding::UpdateObjectBufferATI("glUpdateObjectBufferATI");
Function<void, GLuint> Binding::UseProgram("glUseProgram");
Function<void, GLhandleARB> Binding::UseProgramObjectARB("glUseProgramObjectARB");
Function<void, GLuint, UseProgramStageMask, GLuint> Binding::UseProgramStages("glUseProgramStages");
Function<void, GLenum, GLuint> Binding::UseShaderProgramEXT("glUseShaderProgramEXT");
Function<void> Binding::VDPAUFiniNV("glVDPAUFiniNV");
Function<void, GLvdpauSurfaceNV, GLenum, GLsizei, GLsizei *, GLint *> Binding::VDPAUGetSurfaceivNV("glVDPAUGetSurfaceivNV");
Function<void, const void *, const void *> Binding::VDPAUInitNV("glVDPAUInitNV");
Function<GLboolean, GLvdpauSurfaceNV> Binding::VDPAUIsSurfaceNV("glVDPAUIsSurfaceNV");
Function<void, GLsizei, const GLvdpauSurfaceNV *> Binding::VDPAUMapSurfacesNV("glVDPAUMapSurfacesNV");
Function<GLvdpauSurfaceNV, const void *, GLenum, GLsizei, const GLuint *> Binding::VDPAURegisterOutputSurfaceNV("glVDPAURegisterOutputSurfaceNV");
Function<GLvdpauSurfaceNV, const void *, GLenum, GLsizei, const GLuint *> Binding::VDPAURegisterVideoSurfaceNV("glVDPAURegisterVideoSurfaceNV");
Function<void, GLvdpauSurfaceNV, GLenum> Binding::VDPAUSurfaceAccessNV("glVDPAUSurfaceAccessNV");
Function<void, GLsizei, const GLvdpauSurfaceNV *> Binding::VDPAUUnmapSurfacesNV("glVDPAUUnmapSurfacesNV");
Function<void, GLvdpauSurfaceNV> Binding::VDPAUUnregisterSurfaceNV("glVDPAUUnregisterSurfaceNV");
Function<void, GLuint> Binding::ValidateProgram("glValidateProgram");
Function<void, GLhandleARB> Binding::ValidateProgramARB("glValidateProgramARB");
Function<void, GLuint> Binding::ValidateProgramPipeline("glValidateProgramPipeline");
Function<void, GLuint, GLenum, GLsizei, GLuint, GLuint> Binding::VariantArrayObjectATI("glVariantArrayObjectATI");
Function<void, GLuint, GLenum, GLuint, const void *> Binding::VariantPointerEXT("glVariantPointerEXT");
Function<void, GLuint, const GLbyte *> Binding::VariantbvEXT("glVariantbvEXT");
Function<void, GLuint, const GLdouble *> Binding::VariantdvEXT("glVariantdvEXT");
Function<void, GLuint, const GLfloat *> Binding::VariantfvEXT("glVariantfvEXT");
Function<void, GLuint, const GLint *> Binding::VariantivEXT("glVariantivEXT");
Function<void, GLuint, const GLshort *> Binding::VariantsvEXT("glVariantsvEXT");
Function<void, GLuint, const GLubyte *> Binding::VariantubvEXT("glVariantubvEXT");
Function<void, GLuint, const GLuint *> Binding::VariantuivEXT("glVariantuivEXT");
Function<void, GLuint, const GLushort *> Binding::VariantusvEXT("glVariantusvEXT");
Function<void, GLbyte, GLbyte> Binding::Vertex2bOES("glVertex2bOES");
Function<void, const GLbyte *> Binding::Vertex2bvOES("glVertex2bvOES");
Function<void, GLdouble, GLdouble> Binding::Vertex2d("glVertex2d");
Function<void, const GLdouble *> Binding::Vertex2dv("glVertex2dv");
Function<void, GLfloat, GLfloat> Binding::Vertex2f("glVertex2f");
Function<void, const GLfloat *> Binding::Vertex2fv("glVertex2fv");
Function<void, GLhalfNV, GLhalfNV> Binding::Vertex2hNV("glVertex2hNV");
Function<void, const GLhalfNV *> Binding::Vertex2hvNV("glVertex2hvNV");
Function<void, GLint, GLint> Binding::Vertex2i("glVertex2i");
Function<void, const GLint *> Binding::Vertex2iv("glVertex2iv");
Function<void, GLshort, GLshort> Binding::Vertex2s("glVertex2s");
Function<void, const GLshort *> Binding::Vertex2sv("glVertex2sv");
Function<void, GLfixed> Binding::Vertex2xOES("glVertex2xOES");
Function<void, const GLfixed *> Binding::Vertex2xvOES("glVertex2xvOES");
Function<void, GLbyte, GLbyte, GLbyte> Binding::Vertex3bOES("glVertex3bOES");
Function<void, const GLbyte *> Binding::Vertex3bvOES("glVertex3bvOES");
Function<void, GLdouble, GLdouble, GLdouble> Binding::Vertex3d("glVertex3d");
Function<void, const GLdouble *> Binding::Vertex3dv("glVertex3dv");
Function<void, GLfloat, GLfloat, GLfloat> Binding::Vertex3f("glVertex3f");
Function<void, const GLfloat *> Binding::Vertex3fv("glVertex3fv");
Function<void, GLhalfNV, GLhalfNV, GLhalfNV> Binding::Vertex3hNV("glVertex3hNV");
Function<void, const GLhalfNV *> Binding::Vertex3hvNV("glVertex3hvNV");
Function<void, GLint, GLint, GLint> Binding::Vertex3i("glVertex3i");
Function<void, const GLint *> Binding::Vertex3iv("glVertex3iv");
Function<void, GLshort, GLshort, GLshort> Binding::Vertex3s("glVertex3s");
Function<void, const GLshort *> Binding::Vertex3sv("glVertex3sv");
Function<void, GLfixed, GLfixed> Binding::Vertex3xOES("glVertex3xOES");
Function<void, const GLfixed *> Binding::Vertex3xvOES("glVertex3xvOES");
Function<void, GLbyte, GLbyte, GLbyte, GLbyte> Binding::Vertex4bOES("glVertex4bOES");
Function<void, const GLbyte *> Binding::Vertex4bvOES("glVertex4bvOES");
Function<void, GLdouble, GLdouble, GLdouble, GLdouble> Binding::Vertex4d("glVertex4d");
Function<void, const GLdouble *> Binding::Vertex4dv("glVertex4dv");
Function<void, GLfloat, GLfloat, GLfloat, GLfloat> Binding::Vertex4f("glVertex4f");
Function<void, const GLfloat *> Binding::Vertex4fv("glVertex4fv");
Function<void, GLhalfNV, GLhalfNV, GLhalfNV, GLhalfNV> Binding::Vertex4hNV("glVertex4hNV");
Function<void, const GLhalfNV *> Binding::Vertex4hvNV("glVertex4hvNV");
Function<void, GLint, GLint, GLint, GLint> Binding::Vertex4i("glVertex4i");
Function<void, const GLint *> Binding::Vertex4iv("glVertex4iv");
Function<void, GLshort, GLshort, GLshort, GLshort> Binding::Vertex4s("glVertex4s");
Function<void, const GLshort *> Binding::Vertex4sv("glVertex4sv");
Function<void, GLfixed, GLfixed, GLfixed> Binding::Vertex4xOES("glVertex4xOES");
Function<void, const GLfixed *> Binding::Vertex4xvOES("glVertex4xvOES");
Function<void, GLuint, GLuint, GLuint> Binding::VertexArrayAttribBinding("glVertexArrayAttribBinding");
Function<void, GLuint, GLuint, GLint, GLenum, GLboolean, GLuint> Binding::VertexArrayAttribFormat("glVertexArrayAttribFormat");
Function<void, GLuint, GLuint, GLint, GLenum, GLuint> Binding::VertexArrayAttribIFormat("glVertexArrayAttribIFormat");
Function<void, GLuint, GLuint, GLint, GLenum, GLuint> Binding::VertexArrayAttribLFormat("glVertexArrayAttribLFormat");
Function<void, GLuint, GLuint, GLuint, GLintptr, GLsizei> Binding::VertexArrayBindVertexBufferEXT("glVertexArrayBindVertexBufferEXT");
Function<void, GLuint, GLuint, GLuint> Binding::VertexArrayBindingDivisor("glVertexArrayBindingDivisor");
Function<void, GLuint, GLuint, GLint, GLenum, GLsizei, GLintptr> Binding::VertexArrayColorOffsetEXT("glVertexArrayColorOffsetEXT");
Function<void, GLuint, GLuint, GLsizei, GLintptr> Binding::VertexArrayEdgeFlagOffsetEXT("glVertexArrayEdgeFlagOffsetEXT");
Function<void, GLuint, GLuint> Binding::VertexArrayElementBuffer("glVertexArrayElementBuffer");
Function<void, GLuint, GLuint, GLenum, GLsizei, GLintptr> Binding::VertexArrayFogCoordOffsetEXT("glVertexArrayFogCoordOffsetEXT");
Function<void, GLuint, GLuint, GLenum, GLsizei, GLintptr> Binding::VertexArrayIndexOffsetEXT("glVertexArrayIndexOffsetEXT");
Function<void, GLuint, GLuint, GLenum, GLint, GLenum, GLsizei, GLintptr> Binding::VertexArrayMultiTexCoordOffsetEXT("glVertexArrayMultiTexCoordOffsetEXT");
Function<void, GLuint, GLuint, GLenum, GLsizei, GLintptr> Binding::VertexArrayNormalOffsetEXT("glVertexArrayNormalOffsetEXT");
Function<void, GLenum, GLint> Binding::VertexArrayParameteriAPPLE("glVertexArrayParameteriAPPLE");
Function<void, GLsizei, void *> Binding::VertexArrayRangeAPPLE("glVertexArrayRangeAPPLE");
Function<void, GLsizei, const void *> Binding::VertexArrayRangeNV("glVertexArrayRangeNV");
Function<void, GLuint, GLuint, GLint, GLenum, GLsizei, GLintptr> Binding::VertexArraySecondaryColorOffsetEXT("glVertexArraySecondaryColorOffsetEXT");
Function<void, GLuint, GLuint, GLint, GLenum, GLsizei, GLintptr> Binding::VertexArrayTexCoordOffsetEXT("glVertexArrayTexCoordOffsetEXT");
Function<void, GLuint, GLuint, GLuint> Binding::VertexArrayVertexAttribBindingEXT("glVertexArrayVertexAttribBindingEXT");
Function<void, GLuint, GLuint, GLuint> Binding::VertexArrayVertexAttribDivisorEXT("glVertexArrayVertexAttribDivisorEXT");
Function<void, GLuint, GLuint, GLint, GLenum, GLboolean, GLuint> Binding::VertexArrayVertexAttribFormatEXT("glVertexArrayVertexAttribFormatEXT");
Function<void, GLuint, GLuint, GLint, GLenum, GLuint> Binding::VertexArrayVertexAttribIFormatEXT("glVertexArrayVertexAttribIFormatEXT");
Function<void, GLuint, GLuint, GLuint, GLint, GLenum, GLsizei, GLintptr> Binding::VertexArrayVertexAttribIOffsetEXT("glVertexArrayVertexAttribIOffsetEXT");
Function<void, GLuint, GLuint, GLint, GLenum, GLuint> Binding::VertexArrayVertexAttribLFormatEXT("glVertexArrayVertexAttribLFormatEXT");
Function<void, GLuint, GLuint, GLuint, GLint, GLenum, GLsizei, GLintptr> Binding::VertexArrayVertexAttribLOffsetEXT("glVertexArrayVertexAttribLOffsetEXT");
Function<void, GLuint, GLuint, GLuint, GLint, GLenum, GLboolean, GLsizei, GLintptr> Binding::VertexArrayVertexAttribOffsetEXT("glVertexArrayVertexAttribOffsetEXT");
Function<void, GLuint, GLuint, GLuint> Binding::VertexArrayVertexBindingDivisorEXT("glVertexArrayVertexBindingDivisorEXT");
Function<void, GLuint, GLuint, GLuint, GLintptr, GLsizei> Binding::VertexArrayVertexBuffer("glVertexArrayVertexBuffer");
Function<void, GLuint, GLuint, GLsizei, const GLuint *, const GLintptr *, const GLsizei *> Binding::VertexArrayVertexBuffers("glVertexArrayVertexBuffers");
Function<void, GLuint, GLuint, GLint, GLenum, GLsizei, GLintptr> Binding::VertexArrayVertexOffsetEXT("glVertexArrayVertexOffsetEXT");
Function<void, GLuint, GLdouble> Binding::VertexAttrib1d("glVertexAttrib1d");
Function<void, GLuint, GLdouble> Binding::VertexAttrib1dARB("glVertexAttrib1dARB");
Function<void, GLuint, GLdouble> Binding::VertexAttrib1dNV("glVertexAttrib1dNV");
Function<void, GLuint, const GLdouble *> Binding::VertexAttrib1dv("glVertexAttrib1dv");
Function<void, GLuint, const GLdouble *> Binding::VertexAttrib1dvARB("glVertexAttrib1dvARB");
Function<void, GLuint, const GLdouble *> Binding::VertexAttrib1dvNV("glVertexAttrib1dvNV");
Function<void, GLuint, GLfloat> Binding::VertexAttrib1f("glVertexAttrib1f");
Function<void, GLuint, GLfloat> Binding::VertexAttrib1fARB("glVertexAttrib1fARB");
Function<void, GLuint, GLfloat> Binding::VertexAttrib1fNV("glVertexAttrib1fNV");
Function<void, GLuint, const GLfloat *> Binding::VertexAttrib1fv("glVertexAttrib1fv");
Function<void, GLuint, const GLfloat *> Binding::VertexAttrib1fvARB("glVertexAttrib1fvARB");
Function<void, GLuint, const GLfloat *> Binding::VertexAttrib1fvNV("glVertexAttrib1fvNV");
Function<void, GLuint, GLhalfNV> Binding::VertexAttrib1hNV("glVertexAttrib1hNV");
Function<void, GLuint, const GLhalfNV *> Binding::VertexAttrib1hvNV("glVertexAttrib1hvNV");
Function<void, GLuint, GLshort> Binding::VertexAttrib1s("glVertexAttrib1s");
Function<void, GLuint, GLshort> Binding::VertexAttrib1sARB("glVertexAttrib1sARB");
Function<void, GLuint, GLshort> Binding::VertexAttrib1sNV("glVertexAttrib1sNV");
Function<void, GLuint, const GLshort *> Binding::VertexAttrib1sv("glVertexAttrib1sv");
Function<void, GLuint, const GLshort *> Binding::VertexAttrib1svARB("glVertexAttrib1svARB");
Function<void, GLuint, const GLshort *> Binding::VertexAttrib1svNV("glVertexAttrib1svNV");
Function<void, GLuint, GLdouble, GLdouble> Binding::VertexAttrib2d("glVertexAttrib2d");
Function<void, GLuint, GLdouble, GLdouble> Binding::VertexAttrib2dARB("glVertexAttrib2dARB");
Function<void, GLuint, GLdouble, GLdouble> Binding::VertexAttrib2dNV("glVertexAttrib2dNV");
Function<void, GLuint, const GLdouble *> Binding::VertexAttrib2dv("glVertexAttrib2dv");
Function<void, GLuint, const GLdouble *> Binding::VertexAttrib2dvARB("glVertexAttrib2dvARB");
Function<void, GLuint, const GLdouble *> Binding::VertexAttrib2dvNV("glVertexAttrib2dvNV");
Function<void, GLuint, GLfloat, GLfloat> Binding::VertexAttrib2f("glVertexAttrib2f");
Function<void, GLuint, GLfloat, GLfloat> Binding::VertexAttrib2fARB("glVertexAttrib2fARB");
Function<void, GLuint, GLfloat, GLfloat> Binding::VertexAttrib2fNV("glVertexAttrib2fNV");
Function<void, GLuint, const GLfloat *> Binding::VertexAttrib2fv("glVertexAttrib2fv");
Function<void, GLuint, const GLfloat *> Binding::VertexAttrib2fvARB("glVertexAttrib2fvARB");
Function<void, GLuint, const GLfloat *> Binding::VertexAttrib2fvNV("glVertexAttrib2fvNV");
Function<void, GLuint, GLhalfNV, GLhalfNV> Binding::VertexAttrib2hNV("glVertexAttrib2hNV");
Function<void, GLuint, const GLhalfNV *> Binding::VertexAttrib2hvNV("glVertexAttrib2hvNV");
Function<void, GLuint, GLshort, GLshort> Binding::VertexAttrib2s("glVertexAttrib2s");
Function<void, GLuint, GLshort, GLshort> Binding::VertexAttrib2sARB("glVertexAttrib2sARB");
Function<void, GLuint, GLshort, GLshort> Binding::VertexAttrib2sNV("glVertexAttrib2sNV");
Function<void, GLuint, const GLshort *> Binding::VertexAttrib2sv("glVertexAttrib2sv");
Function<void, GLuint, const GLshort *> Binding::VertexAttrib2svARB("glVertexAttrib2svARB");
Function<void, GLuint, const GLshort *> Binding::VertexAttrib2svNV("glVertexAttrib2svNV");
Function<void, GLuint, GLdouble, GLdouble, GLdouble> Binding::VertexAttrib3d("glVertexAttrib3d");
Function<void, GLuint, GLdouble, GLdouble, GLdouble> Binding::VertexAttrib3dARB("glVertexAttrib3dARB");
Function<void, GLuint, GLdouble, GLdouble, GLdouble> Binding::VertexAttrib3dNV("glVertexAttrib3dNV");
Function<void, GLuint, const GLdouble *> Binding::VertexAttrib3dv("glVertexAttrib3dv");
Function<void, GLuint, const GLdouble *> Binding::VertexAttrib3dvARB("glVertexAttrib3dvARB");
Function<void, GLuint, const GLdouble *> Binding::VertexAttrib3dvNV("glVertexAttrib3dvNV");
Function<void, GLuint, GLfloat, GLfloat, GLfloat> Binding::VertexAttrib3f("glVertexAttrib3f");
Function<void, GLuint, GLfloat, GLfloat, GLfloat> Binding::VertexAttrib3fARB("glVertexAttrib3fARB");
Function<void, GLuint, GLfloat, GLfloat, GLfloat> Binding::VertexAttrib3fNV("glVertexAttrib3fNV");
Function<void, GLuint, const GLfloat *> Binding::VertexAttrib3fv("glVertexAttrib3fv");
Function<void, GLuint, const GLfloat *> Binding::VertexAttrib3fvARB("glVertexAttrib3fvARB");
Function<void, GLuint, const GLfloat *> Binding::VertexAttrib3fvNV("glVertexAttrib3fvNV");
Function<void, GLuint, GLhalfNV, GLhalfNV, GLhalfNV> Binding::VertexAttrib3hNV("glVertexAttrib3hNV");
Function<void, GLuint, const GLhalfNV *> Binding::VertexAttrib3hvNV("glVertexAttrib3hvNV");
Function<void, GLuint, GLshort, GLshort, GLshort> Binding::VertexAttrib3s("glVertexAttrib3s");
Function<void, GLuint, GLshort, GLshort, GLshort> Binding::VertexAttrib3sARB("glVertexAttrib3sARB");
Function<void, GLuint, GLshort, GLshort, GLshort> Binding::VertexAttrib3sNV("glVertexAttrib3sNV");
Function<void, GLuint, const GLshort *> Binding::VertexAttrib3sv("glVertexAttrib3sv");
Function<void, GLuint, const GLshort *> Binding::VertexAttrib3svARB("glVertexAttrib3svARB");
Function<void, GLuint, const GLshort *> Binding::VertexAttrib3svNV("glVertexAttrib3svNV");
Function<void, GLuint, const GLbyte *> Binding::VertexAttrib4Nbv("glVertexAttrib4Nbv");
Function<void, GLuint, const GLbyte *> Binding::VertexAttrib4NbvARB("glVertexAttrib4NbvARB");
Function<void, GLuint, const GLint *> Binding::VertexAttrib4Niv("glVertexAttrib4Niv");
Function<void, GLuint, const GLint *> Binding::VertexAttrib4NivARB("glVertexAttrib4NivARB");
Function<void, GLuint, const GLshort *> Binding::VertexAttrib4Nsv("glVertexAttrib4Nsv");
Function<void, GLuint, const GLshort *> Binding::VertexAttrib4NsvARB("glVertexAttrib4NsvARB");
Function<void, GLuint, GLubyte, GLubyte, GLubyte, GLubyte> Binding::VertexAttrib4Nub("glVertexAttrib4Nub");
Function<void, GLuint, GLubyte, GLubyte, GLubyte, GLubyte> Binding::VertexAttrib4NubARB("glVertexAttrib4NubARB");
Function<void, GLuint, const GLubyte *> Binding::VertexAttrib4Nubv("glVertexAttrib4Nubv");
Function<void, GLuint, const GLubyte *> Binding::VertexAttrib4NubvARB("glVertexAttrib4NubvARB");
Function<void, GLuint, const GLuint *> Binding::VertexAttrib4Nuiv("glVertexAttrib4Nuiv");
Function<void, GLuint, const GLuint *> Binding::VertexAttrib4NuivARB("glVertexAttrib4NuivARB");
Function<void, GLuint, const GLushort *> Binding::VertexAttrib4Nusv("glVertexAttrib4Nusv");
Function<void, GLuint, const GLushort *> Binding::VertexAttrib4NusvARB("glVertexAttrib4NusvARB");
Function<void, GLuint, const GLbyte *> Binding::VertexAttrib4bv("glVertexAttrib4bv");
Function<void, GLuint, const GLbyte *> Binding::VertexAttrib4bvARB("glVertexAttrib4bvARB");
Function<void, GLuint, GLdouble, GLdouble, GLdouble, GLdouble> Binding::VertexAttrib4d("glVertexAttrib4d");
Function<void, GLuint, GLdouble, GLdouble, GLdouble, GLdouble> Binding::VertexAttrib4dARB("glVertexAttrib4dARB");
Function<void, GLuint, GLdouble, GLdouble, GLdouble, GLdouble> Binding::VertexAttrib4dNV("glVertexAttrib4dNV");
Function<void, GLuint, const GLdouble *> Binding::VertexAttrib4dv("glVertexAttrib4dv");
Function<void, GLuint, const GLdouble *> Binding::VertexAttrib4dvARB("glVertexAttrib4dvARB");
Function<void, GLuint, const GLdouble *> Binding::VertexAttrib4dvNV("glVertexAttrib4dvNV");
Function<void, GLuint, GLfloat, GLfloat, GLfloat, GLfloat> Binding::VertexAttrib4f("glVertexAttrib4f");
Function<void, GLuint, GLfloat, GLfloat, GLfloat, GLfloat> Binding::VertexAttrib4fARB("glVertexAttrib4fARB");
Function<void, GLuint, GLfloat, GLfloat, GLfloat, GLfloat> Binding::VertexAttrib4fNV("glVertexAttrib4fNV");
Function<void, GLuint, const GLfloat *> Binding::VertexAttrib4fv("glVertexAttrib4fv");
Function<void, GLuint, const GLfloat *> Binding::VertexAttrib4fvARB("glVertexAttrib4fvARB");
Function<void, GLuint, const GLfloat *> Binding::VertexAttrib4fvNV("glVertexAttrib4fvNV");
Function<void, GLuint, GLhalfNV, GLhalfNV, GLhalfNV, GLhalfNV> Binding::VertexAttrib4hNV("glVertexAttrib4hNV");
Function<void, GLuint, const GLhalfNV *> Binding::VertexAttrib4hvNV("glVertexAttrib4hvNV");
Function<void, GLuint, const GLint *> Binding::VertexAttrib4iv("glVertexAttrib4iv");
Function<void, GLuint, const GLint *> Binding::VertexAttrib4ivARB("glVertexAttrib4ivARB");
Function<void, GLuint, GLshort, GLshort, GLshort, GLshort> Binding::VertexAttrib4s("glVertexAttrib4s");
Function<void, GLuint, GLshort, GLshort, GLshort, GLshort> Binding::VertexAttrib4sARB("glVertexAttrib4sARB");
Function<void, GLuint, GLshort, GLshort, GLshort, GLshort> Binding::VertexAttrib4sNV("glVertexAttrib4sNV");
Function<void, GLuint, const GLshort *> Binding::VertexAttrib4sv("glVertexAttrib4sv");
Function<void, GLuint, const GLshort *> Binding::VertexAttrib4svARB("glVertexAttrib4svARB");
Function<void, GLuint, const GLshort *> Binding::VertexAttrib4svNV("glVertexAttrib4svNV");
Function<void, GLuint, GLubyte, GLubyte, GLubyte, GLubyte> Binding::VertexAttrib4ubNV("glVertexAttrib4ubNV");
Function<void, GLuint, const GLubyte *> Binding::VertexAttrib4ubv("glVertexAttrib4ubv");
Function<void, GLuint, const GLubyte *> Binding::VertexAttrib4ubvARB("glVertexAttrib4ubvARB");
Function<void, GLuint, const GLubyte *> Binding::VertexAttrib4ubvNV("glVertexAttrib4ubvNV");
Function<void, GLuint, const GLuint *> Binding::VertexAttrib4uiv("glVertexAttrib4uiv");
Function<void, GLuint, const GLuint *> Binding::VertexAttrib4uivARB("glVertexAttrib4uivARB");
Function<void, GLuint, const GLushort *> Binding::VertexAttrib4usv("glVertexAttrib4usv");
Function<void, GLuint, const GLushort *> Binding::VertexAttrib4usvARB("glVertexAttrib4usvARB");
Function<void, GLuint, GLint, GLenum, GLboolean, GLsizei, GLuint, GLuint> Binding::VertexAttribArrayObjectATI("glVertexAttribArrayObjectATI");
Function<void, GLuint, GLuint> Binding::VertexAttribBinding("glVertexAttribBinding");
Function<void, GLuint, GLuint> Binding::VertexAttribDivisor("glVertexAttribDivisor");
Function<void, GLuint, GLuint> Binding::VertexAttribDivisorARB("glVertexAttribDivisorARB");
Function<void, GLuint, GLint, GLenum, GLboolean, GLuint> Binding::VertexAttribFormat("glVertexAttribFormat");
Function<void, GLuint, GLint, GLenum, GLboolean, GLsizei> Binding::VertexAttribFormatNV("glVertexAttribFormatNV");
Function<void, GLuint, GLint> Binding::VertexAttribI1i("glVertexAttribI1i");
Function<void, GLuint, GLint> Binding::VertexAttribI1iEXT("glVertexAttribI1iEXT");
Function<void, GLuint, const GLint *> Binding::VertexAttribI1iv("glVertexAttribI1iv");
Function<void, GLuint, const GLint *> Binding::VertexAttribI1ivEXT("glVertexAttribI1ivEXT");
Function<void, GLuint, GLuint> Binding::VertexAttribI1ui("glVertexAttribI1ui");
Function<void, GLuint, GLuint> Binding::VertexAttribI1uiEXT("glVertexAttribI1uiEXT");
Function<void, GLuint, const GLuint *> Binding::VertexAttribI1uiv("glVertexAttribI1uiv");
Function<void, GLuint, const GLuint *> Binding::VertexAttribI1uivEXT("glVertexAttribI1uivEXT");
Function<void, GLuint, GLint, GLint> Binding::VertexAttribI2i("glVertexAttribI2i");
Function<void, GLuint, GLint, GLint> Binding::VertexAttribI2iEXT("glVertexAttribI2iEXT");
Function<void, GLuint, const GLint *> Binding::VertexAttribI2iv("glVertexAttribI2iv");
Function<void, GLuint, const GLint *> Binding::VertexAttribI2ivEXT("glVertexAttribI2ivEXT");
Function<void, GLuint, GLuint, GLuint> Binding::VertexAttribI2ui("glVertexAttribI2ui");
Function<void, GLuint, GLuint, GLuint> Binding::VertexAttribI2uiEXT("glVertexAttribI2uiEXT");
Function<void, GLuint, const GLuint *> Binding::VertexAttribI2uiv("glVertexAttribI2uiv");
Function<void, GLuint, const GLuint *> Binding::VertexAttribI2uivEXT("glVertexAttribI2uivEXT");
Function<void, GLuint, GLint, GLint, GLint> Binding::VertexAttribI3i("glVertexAttribI3i");
Function<void, GLuint, GLint, GLint, GLint> Binding::VertexAttribI3iEXT("glVertexAttribI3iEXT");
Function<void, GLuint, const GLint *> Binding::VertexAttribI3iv("glVertexAttribI3iv");
Function<void, GLuint, const GLint *> Binding::VertexAttribI3ivEXT("glVertexAttribI3ivEXT");
Function<void, GLuint, GLuint, GLuint, GLuint> Binding::VertexAttribI3ui("glVertexAttribI3ui");
Function<void, GLuint, GLuint, GLuint, GLuint> Binding::VertexAttribI3uiEXT("glVertexAttribI3uiEXT");
Function<void, GLuint, const GLuint *> Binding::VertexAttribI3uiv("glVertexAttribI3uiv");
Function<void, GLuint, const GLuint *> Binding::VertexAttribI3uivEXT("glVertexAttribI3uivEXT");
Function<void, GLuint, const GLbyte *> Binding::VertexAttribI4bv("glVertexAttribI4bv");
Function<void, GLuint, const GLbyte *> Binding::VertexAttribI4bvEXT("glVertexAttribI4bvEXT");
Function<void, GLuint, GLint, GLint, GLint, GLint> Binding::VertexAttribI4i("glVertexAttribI4i");
Function<void, GLuint, GLint, GLint, GLint, GLint> Binding::VertexAttribI4iEXT("glVertexAttribI4iEXT");
Function<void, GLuint, const GLint *> Binding::VertexAttribI4iv("glVertexAttribI4iv");
Function<void, GLuint, const GLint *> Binding::VertexAttribI4ivEXT("glVertexAttribI4ivEXT");
Function<void, GLuint, const GLshort *> Binding::VertexAttribI4sv("glVertexAttribI4sv");
Function<void, GLuint, const GLshort *> Binding::VertexAttribI4svEXT("glVertexAttribI4svEXT");
Function<void, GLuint, const GLubyte *> Binding::VertexAttribI4ubv("glVertexAttribI4ubv");
Function<void, GLuint, const GLubyte *> Binding::VertexAttribI4ubvEXT("glVertexAttribI4ubvEXT");
Function<void, GLuint, GLuint, GLuint, GLuint, GLuint> Binding::VertexAttribI4ui("glVertexAttribI4ui");
Function<void, GLuint, GLuint, GLuint, GLuint, GLuint> Binding::VertexAttribI4uiEXT("glVertexAttribI4uiEXT");
Function<void, GLuint, const GLuint *> Binding::VertexAttribI4uiv("glVertexAttribI4uiv");
Function<void, GLuint, const GLuint *> Binding::VertexAttribI4uivEXT("glVertexAttribI4uivEXT");
Function<void, GLuint, const GLushort *> Binding::VertexAttribI4usv("glVertexAttribI4usv");
Function<void, GLuint, const GLushort *> Binding::VertexAttribI4usvEXT("glVertexAttribI4usvEXT");
Function<void, GLuint, GLint, GLenum, GLuint> Binding::VertexAttribIFormat("glVertexAttribIFormat");
Function<void, GLuint, GLint, GLenum, GLsizei> Binding::VertexAttribIFormatNV("glVertexAttribIFormatNV");
Function<void, GLuint, GLint, GLenum, GLsizei, const void *> Binding::VertexAttribIPointer("glVertexAttribIPointer");
Function<void, GLuint, GLint, GLenum, GLsizei, const void *> Binding::VertexAttribIPointerEXT("glVertexAttribIPointerEXT");
Function<void, GLuint, GLdouble> Binding::VertexAttribL1d("glVertexAttribL1d");
Function<void, GLuint, GLdouble> Binding::VertexAttribL1dEXT("glVertexAttribL1dEXT");
Function<void, GLuint, const GLdouble *> Binding::VertexAttribL1dv("glVertexAttribL1dv");
Function<void, GLuint, const GLdouble *> Binding::VertexAttribL1dvEXT("glVertexAttribL1dvEXT");
Function<void, GLuint, GLint64EXT> Binding::VertexAttribL1i64NV("glVertexAttribL1i64NV");
Function<void, GLuint, const GLint64EXT *> Binding::VertexAttribL1i64vNV("glVertexAttribL1i64vNV");
Function<void, GLuint, GLuint64EXT> Binding::VertexAttribL1ui64ARB("glVertexAttribL1ui64ARB");
Function<void, GLuint, GLuint64EXT> Binding::VertexAttribL1ui64NV("glVertexAttribL1ui64NV");
Function<void, GLuint, const GLuint64EXT *> Binding::VertexAttribL1ui64vARB("glVertexAttribL1ui64vARB");
Function<void, GLuint, const GLuint64EXT *> Binding::VertexAttribL1ui64vNV("glVertexAttribL1ui64vNV");
Function<void, GLuint, GLdouble, GLdouble> Binding::VertexAttribL2d("glVertexAttribL2d");
Function<void, GLuint, GLdouble, GLdouble> Binding::VertexAttribL2dEXT("glVertexAttribL2dEXT");
Function<void, GLuint, const GLdouble *> Binding::VertexAttribL2dv("glVertexAttribL2dv");
Function<void, GLuint, const GLdouble *> Binding::VertexAttribL2dvEXT("glVertexAttribL2dvEXT");
Function<void, GLuint, GLint64EXT, GLint64EXT> Binding::VertexAttribL2i64NV("glVertexAttribL2i64NV");
Function<void, GLuint, const GLint64EXT *> Binding::VertexAttribL2i64vNV("glVertexAttribL2i64vNV");
Function<void, GLuint, GLuint64EXT, GLuint64EXT> Binding::VertexAttribL2ui64NV("glVertexAttribL2ui64NV");
Function<void, GLuint, const GLuint64EXT *> Binding::VertexAttribL2ui64vNV("glVertexAttribL2ui64vNV");
Function<void, GLuint, GLdouble, GLdouble, GLdouble> Binding::VertexAttribL3d("glVertexAttribL3d");
Function<void, GLuint, GLdouble, GLdouble, GLdouble> Binding::VertexAttribL3dEXT("glVertexAttribL3dEXT");
Function<void, GLuint, const GLdouble *> Binding::VertexAttribL3dv("glVertexAttribL3dv");
Function<void, GLuint, const GLdouble *> Binding::VertexAttribL3dvEXT("glVertexAttribL3dvEXT");
Function<void, GLuint, GLint64EXT, GLint64EXT, GLint64EXT> Binding::VertexAttribL3i64NV("glVertexAttribL3i64NV");
Function<void, GLuint, const GLint64EXT *> Binding::VertexAttribL3i64vNV("glVertexAttribL3i64vNV");
Function<void, GLuint, GLuint64EXT, GLuint64EXT, GLuint64EXT> Binding::VertexAttribL3ui64NV("glVertexAttribL3ui64NV");
Function<void, GLuint, const GLuint64EXT *> Binding::VertexAttribL3ui64vNV("glVertexAttribL3ui64vNV");
Function<void, GLuint, GLdouble, GLdouble, GLdouble, GLdouble> Binding::VertexAttribL4d("glVertexAttribL4d");
Function<void, GLuint, GLdouble, GLdouble, GLdouble, GLdouble> Binding::VertexAttribL4dEXT("glVertexAttribL4dEXT");
Function<void, GLuint, const GLdouble *> Binding::VertexAttribL4dv("glVertexAttribL4dv");
Function<void, GLuint, const GLdouble *> Binding::VertexAttribL4dvEXT("glVertexAttribL4dvEXT");
Function<void, GLuint, GLint64EXT, GLint64EXT, GLint64EXT, GLint64EXT> Binding::VertexAttribL4i64NV("glVertexAttribL4i64NV");
Function<void, GLuint, const GLint64EXT *> Binding::VertexAttribL4i64vNV("glVertexAttribL4i64vNV");
Function<void, GLuint, GLuint64EXT, GLuint64EXT, GLuint64EXT, GLuint64EXT> Binding::VertexAttribL4ui64NV("glVertexAttribL4ui64NV");
Function<void, GLuint, const GLuint64EXT *> Binding::VertexAttribL4ui64vNV("glVertexAttribL4ui64vNV");
Function<void, GLuint, GLint, GLenum, GLuint> Binding::VertexAttribLFormat("glVertexAttribLFormat");
Function<void, GLuint, GLint, GLenum, GLsizei> Binding::VertexAttribLFormatNV("glVertexAttribLFormatNV");
Function<void, GLuint, GLint, GLenum, GLsizei, const void *> Binding::VertexAttribLPointer("glVertexAttribLPointer");
Function<void, GLuint, GLint, GLenum, GLsizei, const void *> Binding::VertexAttribLPointerEXT("glVertexAttribLPointerEXT");
Function<void, GLuint, GLenum, GLboolean, GLuint> Binding::VertexAttribP1ui("glVertexAttribP1ui");
Function<void, GLuint, GLenum, GLboolean, const GLuint *> Binding::VertexAttribP1uiv("glVertexAttribP1uiv");
Function<void, GLuint, GLenum, GLboolean, GLuint> Binding::VertexAttribP2ui("glVertexAttribP2ui");
Function<void, GLuint, GLenum, GLboolean, const GLuint *> Binding::VertexAttribP2uiv("glVertexAttribP2uiv");
Function<void, GLuint, GLenum, GLboolean, GLuint> Binding::VertexAttribP3ui("glVertexAttribP3ui");
Function<void, GLuint, GLenum, GLboolean, const GLuint *> Binding::VertexAttribP3uiv("glVertexAttribP3uiv");
Function<void, GLuint, GLenum, GLboolean, GLuint> Binding::VertexAttribP4ui("glVertexAttribP4ui");
Function<void, GLuint, GLenum, GLboolean, const GLuint *> Binding::VertexAttribP4uiv("glVertexAttribP4uiv");
Function<void, GLuint, GLenum, GLint> Binding::VertexAttribParameteriAMD("glVertexAttribParameteriAMD");
Function<void, GLuint, GLint, GLenum, GLboolean, GLsizei, const void *> Binding::VertexAttribPointer("glVertexAttribPointer");
Function<void, GLuint, GLint, GLenum, GLboolean, GLsizei, const void *> Binding::VertexAttribPointerARB("glVertexAttribPointerARB");
Function<void, GLuint, GLint, GLenum, GLsizei, const void *> Binding::VertexAttribPointerNV("glVertexAttribPointerNV");
Function<void, GLuint, GLsizei, const GLdouble *> Binding::VertexAttribs1dvNV("glVertexAttribs1dvNV");
Function<void, GLuint, GLsizei, const GLfloat *> Binding::VertexAttribs1fvNV("glVertexAttribs1fvNV");
Function<void, GLuint, GLsizei, const GLhalfNV *> Binding::VertexAttribs1hvNV("glVertexAttribs1hvNV");
Function<void, GLuint, GLsizei, const GLshort *> Binding::VertexAttribs1svNV("glVertexAttribs1svNV");
Function<void, GLuint, GLsizei, const GLdouble *> Binding::VertexAttribs2dvNV("glVertexAttribs2dvNV");
Function<void, GLuint, GLsizei, const GLfloat *> Binding::VertexAttribs2fvNV("glVertexAttribs2fvNV");
Function<void, GLuint, GLsizei, const GLhalfNV *> Binding::VertexAttribs2hvNV("glVertexAttribs2hvNV");
Function<void, GLuint, GLsizei, const GLshort *> Binding::VertexAttribs2svNV("glVertexAttribs2svNV");
Function<void, GLuint, GLsizei, const GLdouble *> Binding::VertexAttribs3dvNV("glVertexAttribs3dvNV");
Function<void, GLuint, GLsizei, const GLfloat *> Binding::VertexAttribs3fvNV("glVertexAttribs3fvNV");
Function<void, GLuint, GLsizei, const GLhalfNV *> Binding::VertexAttribs3hvNV("glVertexAttribs3hvNV");
Function<void, GLuint, GLsizei, const GLshort *> Binding::VertexAttribs3svNV("glVertexAttribs3svNV");
Function<void, GLuint, GLsizei, const GLdouble *> Binding::VertexAttribs4dvNV("glVertexAttribs4dvNV");
Function<void, GLuint, GLsizei, const GLfloat *> Binding::VertexAttribs4fvNV("glVertexAttribs4fvNV");
Function<void, GLuint, GLsizei, const GLhalfNV *> Binding::VertexAttribs4hvNV("glVertexAttribs4hvNV");
Function<void, GLuint, GLsizei, const GLshort *> Binding::VertexAttribs4svNV("glVertexAttribs4svNV");
Function<void, GLuint, GLsizei, const GLubyte *> Binding::VertexAttribs4ubvNV("glVertexAttribs4ubvNV");
Function<void, GLuint, GLuint> Binding::VertexBindingDivisor("glVertexBindingDivisor");
Function<void, GLint> Binding::VertexBlendARB("glVertexBlendARB");
Function<void, GLenum, GLfloat> Binding::VertexBlendEnvfATI("glVertexBlendEnvfATI");
Function<void, GLenum, GLint> Binding::VertexBlendEnviATI("glVertexBlendEnviATI");
Function<void, GLint, GLenum, GLsizei> Binding::VertexFormatNV("glVertexFormatNV");
Function<void, GLenum, GLuint> Binding::VertexP2ui("glVertexP2ui");
Function<void, GLenum, const GLuint *> Binding::VertexP2uiv("glVertexP2uiv");
Function<void, GLenum, GLuint> Binding::VertexP3ui("glVertexP3ui");
Function<void, GLenum, const GLuint *> Binding::VertexP3uiv("glVertexP3uiv");
Function<void, GLenum, GLuint> Binding::VertexP4ui("glVertexP4ui");
Function<void, GLenum, const GLuint *> Binding::VertexP4uiv("glVertexP4uiv");
Function<void, GLint, GLenum, GLsizei, const void *> Binding::VertexPointer("glVertexPointer");
Function<void, GLint, GLenum, GLsizei, GLsizei, const void *> Binding::VertexPointerEXT("glVertexPointerEXT");
Function<void, GLint, GLenum, GLint, const void **, GLint> Binding::VertexPointerListIBM("glVertexPointerListIBM");
Function<void, GLint, GLenum, const void **> Binding::VertexPointervINTEL("glVertexPointervINTEL");
Function<void, GLenum, GLdouble> Binding::VertexStream1dATI("glVertexStream1dATI");
Function<void, GLenum, const GLdouble *> Binding::VertexStream1dvATI("glVertexStream1dvATI");
Function<void, GLenum, GLfloat> Binding::VertexStream1fATI("glVertexStream1fATI");
Function<void, GLenum, const GLfloat *> Binding::VertexStream1fvATI("glVertexStream1fvATI");
Function<void, GLenum, GLint> Binding::VertexStream1iATI("glVertexStream1iATI");
Function<void, GLenum, const GLint *> Binding::VertexStream1ivATI("glVertexStream1ivATI");
Function<void, GLenum, GLshort> Binding::VertexStream1sATI("glVertexStream1sATI");
Function<void, GLenum, const GLshort *> Binding::VertexStream1svATI("glVertexStream1svATI");
Function<void, GLenum, GLdouble, GLdouble> Binding::VertexStream2dATI("glVertexStream2dATI");
Function<void, GLenum, const GLdouble *> Binding::VertexStream2dvATI("glVertexStream2dvATI");
Function<void, GLenum, GLfloat, GLfloat> Binding::VertexStream2fATI("glVertexStream2fATI");
Function<void, GLenum, const GLfloat *> Binding::VertexStream2fvATI("glVertexStream2fvATI");
Function<void, GLenum, GLint, GLint> Binding::VertexStream2iATI("glVertexStream2iATI");
Function<void, GLenum, const GLint *> Binding::VertexStream2ivATI("glVertexStream2ivATI");
Function<void, GLenum, GLshort, GLshort> Binding::VertexStream2sATI("glVertexStream2sATI");
Function<void, GLenum, const GLshort *> Binding::VertexStream2svATI("glVertexStream2svATI");
Function<void, GLenum, GLdouble, GLdouble, GLdouble> Binding::VertexStream3dATI("glVertexStream3dATI");
Function<void, GLenum, const GLdouble *> Binding::VertexStream3dvATI("glVertexStream3dvATI");
Function<void, GLenum, GLfloat, GLfloat, GLfloat> Binding::VertexStream3fATI("glVertexStream3fATI");
Function<void, GLenum, const GLfloat *> Binding::VertexStream3fvATI("glVertexStream3fvATI");
Function<void, GLenum, GLint, GLint, GLint> Binding::VertexStream3iATI("glVertexStream3iATI");
Function<void, GLenum, const GLint *> Binding::VertexStream3ivATI("glVertexStream3ivATI");
Function<void, GLenum, GLshort, GLshort, GLshort> Binding::VertexStream3sATI("glVertexStream3sATI");
Function<void, GLenum, const GLshort *> Binding::VertexStream3svATI("glVertexStream3svATI");
Function<void, GLenum, GLdouble, GLdouble, GLdouble, GLdouble> Binding::VertexStream4dATI("glVertexStream4dATI");
Function<void, GLenum, const GLdouble *> Binding::VertexStream4dvATI("glVertexStream4dvATI");
Function<void, GLenum, GLfloat, GLfloat, GLfloat, GLfloat> Binding::VertexStream4fATI("glVertexStream4fATI");
Function<void, GLenum, const GLfloat *> Binding::VertexStream4fvATI("glVertexStream4fvATI");
Function<void, GLenum, GLint, GLint, GLint, GLint> Binding::VertexStream4iATI("glVertexStream4iATI");
Function<void, GLenum, const GLint *> Binding::VertexStream4ivATI("glVertexStream4ivATI");
Function<void, GLenum, GLshort, GLshort, GLshort, GLshort> Binding::VertexStream4sATI("glVertexStream4sATI");
Function<void, GLenum, const GLshort *> Binding::VertexStream4svATI("glVertexStream4svATI");
Function<void, GLint, GLenum, GLsizei, const void *> Binding::VertexWeightPointerEXT("glVertexWeightPointerEXT");
Function<void, GLfloat> Binding::VertexWeightfEXT("glVertexWeightfEXT");
Function<void, const GLfloat *> Binding::VertexWeightfvEXT("glVertexWeightfvEXT");
Function<void, GLhalfNV> Binding::VertexWeighthNV("glVertexWeighthNV");
Function<void, const GLhalfNV *> Binding::VertexWeighthvNV("glVertexWeighthvNV");
Function<GLenum, GLuint, GLuint *, GLuint64EXT *> Binding::VideoCaptureNV("glVideoCaptureNV");
Function<void, GLuint, GLuint, GLenum, const GLdouble *> Binding::VideoCaptureStreamParameterdvNV("glVideoCaptureStreamParameterdvNV");
Function<void, GLuint, GLuint, GLenum, const GLfloat *> Binding::VideoCaptureStreamParameterfvNV("glVideoCaptureStreamParameterfvNV");
Function<void, GLuint, GLuint, GLenum, const GLint *> Binding::VideoCaptureStreamParameterivNV("glVideoCaptureStreamParameterivNV");
Function<void, GLint, GLint, GLsizei, GLsizei> Binding::Viewport("glViewport");
Function<void, GLuint, GLsizei, const GLfloat *> Binding::ViewportArrayv("glViewportArrayv");
Function<void, GLuint, GLfloat, GLfloat, GLfloat, GLfloat> Binding::ViewportIndexedf("glViewportIndexedf");
Function<void, GLuint, const GLfloat *> Binding::ViewportIndexedfv("glViewportIndexedfv");
Function<void, GLsync, UnusedMask, GLuint64> Binding::WaitSync("glWaitSync");
Function<void, GLuint, GLsizei, const GLuint *, const GLfloat *> Binding::WeightPathsNV("glWeightPathsNV");
Function<void, GLint, GLenum, GLsizei, const void *> Binding::WeightPointerARB("glWeightPointerARB");
Function<void, GLint, const GLbyte *> Binding::WeightbvARB("glWeightbvARB");
Function<void, GLint, const GLdouble *> Binding::WeightdvARB("glWeightdvARB");
Function<void, GLint, const GLfloat *> Binding::WeightfvARB("glWeightfvARB");
Function<void, GLint, const GLint *> Binding::WeightivARB("glWeightivARB");
Function<void, GLint, const GLshort *> Binding::WeightsvARB("glWeightsvARB");
Function<void, GLint, const GLubyte *> Binding::WeightubvARB("glWeightubvARB");
Function<void, GLint, const GLuint *> Binding::WeightuivARB("glWeightuivARB");
Function<void, GLint, const GLushort *> Binding::WeightusvARB("glWeightusvARB");
Function<void, GLdouble, GLdouble> Binding::WindowPos2d("glWindowPos2d");
Function<void, GLdouble, GLdouble> Binding::WindowPos2dARB("glWindowPos2dARB");
Function<void, GLdouble, GLdouble> Binding::WindowPos2dMESA("glWindowPos2dMESA");
Function<void, const GLdouble *> Binding::WindowPos2dv("glWindowPos2dv");
Function<void, const GLdouble *> Binding::WindowPos2dvARB("glWindowPos2dvARB");
Function<void, const GLdouble *> Binding::WindowPos2dvMESA("glWindowPos2dvMESA");
Function<void, GLfloat, GLfloat> Binding::WindowPos2f("glWindowPos2f");
Function<void, GLfloat, GLfloat> Binding::WindowPos2fARB("glWindowPos2fARB");
Function<void, GLfloat, GLfloat> Binding::WindowPos2fMESA("glWindowPos2fMESA");
Function<void, const GLfloat *> Binding::WindowPos2fv("glWindowPos2fv");
Function<void, const GLfloat *> Binding::WindowPos2fvARB("glWindowPos2fvARB");
Function<void, const GLfloat *> Binding::WindowPos2fvMESA("glWindowPos2fvMESA");
Function<void, GLint, GLint> Binding::WindowPos2i("glWindowPos2i");
Function<void, GLint, GLint> Binding::WindowPos2iARB("glWindowPos2iARB");
Function<void, GLint, GLint> Binding::WindowPos2iMESA("glWindowPos2iMESA");
Function<void, const GLint *> Binding::WindowPos2iv("glWindowPos2iv");
Function<void, const GLint *> Binding::WindowPos2ivARB("glWindowPos2ivARB");
Function<void, const GLint *> Binding::WindowPos2ivMESA("glWindowPos2ivMESA");
Function<void, GLshort, GLshort> Binding::WindowPos2s("glWindowPos2s");
Function<void, GLshort, GLshort> Binding::WindowPos2sARB("glWindowPos2sARB");
Function<void, GLshort, GLshort> Binding::WindowPos2sMESA("glWindowPos2sMESA");
Function<void, const GLshort *> Binding::WindowPos2sv("glWindowPos2sv");
Function<void, const GLshort *> Binding::WindowPos2svARB("glWindowPos2svARB");
Function<void, const GLshort *> Binding::WindowPos2svMESA("glWindowPos2svMESA");
Function<void, GLdouble, GLdouble, GLdouble> Binding::WindowPos3d("glWindowPos3d");
Function<void, GLdouble, GLdouble, GLdouble> Binding::WindowPos3dARB("glWindowPos3dARB");
Function<void, GLdouble, GLdouble, GLdouble> Binding::WindowPos3dMESA("glWindowPos3dMESA");
Function<void, const GLdouble *> Binding::WindowPos3dv("glWindowPos3dv");
Function<void, const GLdouble *> Binding::WindowPos3dvARB("glWindowPos3dvARB");
Function<void, const GLdouble *> Binding::WindowPos3dvMESA("glWindowPos3dvMESA");
Function<void, GLfloat, GLfloat, GLfloat> Binding::WindowPos3f("glWindowPos3f");
Function<void, GLfloat, GLfloat, GLfloat> Binding::WindowPos3fARB("glWindowPos3fARB");
Function<void, GLfloat, GLfloat, GLfloat> Binding::WindowPos3fMESA("glWindowPos3fMESA");
Function<void, const GLfloat *> Binding::WindowPos3fv("glWindowPos3fv");
Function<void, const GLfloat *> Binding::WindowPos3fvARB("glWindowPos3fvARB");
Function<void, const GLfloat *> Binding::WindowPos3fvMESA("glWindowPos3fvMESA");
Function<void, GLint, GLint, GLint> Binding::WindowPos3i("glWindowPos3i");
Function<void, GLint, GLint, GLint> Binding::WindowPos3iARB("glWindowPos3iARB");
Function<void, GLint, GLint, GLint> Binding::WindowPos3iMESA("glWindowPos3iMESA");
Function<void, const GLint *> Binding::WindowPos3iv("glWindowPos3iv");
Function<void, const GLint *> Binding::WindowPos3ivARB("glWindowPos3ivARB");
Function<void, const GLint *> Binding::WindowPos3ivMESA("glWindowPos3ivMESA");
Function<void, GLshort, GLshort, GLshort> Binding::WindowPos3s("glWindowPos3s");
Function<void, GLshort, GLshort, GLshort> Binding::WindowPos3sARB("glWindowPos3sARB");
Function<void, GLshort, GLshort, GLshort> Binding::WindowPos3sMESA("glWindowPos3sMESA");
Function<void, const GLshort *> Binding::WindowPos3sv("glWindowPos3sv");
Function<void, const GLshort *> Binding::WindowPos3svARB("glWindowPos3svARB");
Function<void, const GLshort *> Binding::WindowPos3svMESA("glWindowPos3svMESA");
Function<void, GLdouble, GLdouble, GLdouble, GLdouble> Binding::WindowPos4dMESA("glWindowPos4dMESA");
Function<void, const GLdouble *> Binding::WindowPos4dvMESA("glWindowPos4dvMESA");
Function<void, GLfloat, GLfloat, GLfloat, GLfloat> Binding::WindowPos4fMESA("glWindowPos4fMESA");
Function<void, const GLfloat *> Binding::WindowPos4fvMESA("glWindowPos4fvMESA");
Function<void, GLint, GLint, GLint, GLint> Binding::WindowPos4iMESA("glWindowPos4iMESA");
Function<void, const GLint *> Binding::WindowPos4ivMESA("glWindowPos4ivMESA");
Function<void, GLshort, GLshort, GLshort, GLshort> Binding::WindowPos4sMESA("glWindowPos4sMESA");
Function<void, const GLshort *> Binding::WindowPos4svMESA("glWindowPos4svMESA");
Function<void, GLuint, GLuint, GLenum, GLenum, GLenum, GLenum> Binding::WriteMaskEXT("glWriteMaskEXT");


const Binding::array_t Binding::s_functions = 
{{
	&Accum,
    &AccumxOES,
    &ActiveProgramEXT,
    &ActiveShaderProgram,
    &ActiveStencilFaceEXT,
    &ActiveTexture,
    &ActiveTextureARB,
    &ActiveVaryingNV,
    &AlphaFragmentOp1ATI,
    &AlphaFragmentOp2ATI,
    &AlphaFragmentOp3ATI,
    &AlphaFunc,
    &AlphaFuncxOES,
    &ApplyFramebufferAttachmentCMAAINTEL,
    &ApplyTextureEXT,
    &AreProgramsResidentNV,
    &AreTexturesResident,
    &AreTexturesResidentEXT,
    &ArrayElement,
    &ArrayElementEXT,
    &ArrayObjectATI,
    &AsyncMarkerSGIX,
    &AttachObjectARB,
    &AttachShader,
    &Begin,
    &BeginConditionalRender,
    &BeginConditionalRenderNV,
    &BeginConditionalRenderNVX,
    &BeginFragmentShaderATI,
    &BeginOcclusionQueryNV,
    &BeginPerfMonitorAMD,
    &BeginPerfQueryINTEL,
    &BeginQuery,
    &BeginQueryARB,
    &BeginQueryIndexed,
    &BeginTransformFeedback,
    &BeginTransformFeedbackEXT,
    &BeginTransformFeedbackNV,
    &BeginVertexShaderEXT,
    &BeginVideoCaptureNV,
    &BindAttribLocation,
    &BindAttribLocationARB,
    &BindBuffer,
    &BindBufferARB,
    &BindBufferBase,
    &BindBufferBaseEXT,
    &BindBufferBaseNV,
    &BindBufferOffsetEXT,
    &BindBufferOffsetNV,
    &BindBufferRange,
    &BindBufferRangeEXT,
    &BindBufferRangeNV,
    &BindBuffersBase,
    &BindBuffersRange,
    &BindFragDataLocation,
    &BindFragDataLocationEXT,
    &BindFragDataLocationIndexed,
    &BindFragmentShaderATI,
    &BindFramebuffer,
    &BindFramebufferEXT,
    &BindImageTexture,
    &BindImageTextureEXT,
    &BindImageTextures,
    &BindLightParameterEXT,
    &BindMaterialParameterEXT,
    &BindMultiTextureEXT,
    &BindParameterEXT,
    &BindProgramARB,
    &BindProgramNV,
    &BindProgramPipeline,
    &BindRenderbuffer,
    &BindRenderbufferEXT,
    &BindSampler,
    &BindSamplers,
    &BindTexGenParameterEXT,
    &BindTexture,
    &BindTextureEXT,
    &BindTextureUnit,
    &BindTextureUnitParameterEXT,
    &BindTextures,
    &BindTransformFeedback,
    &BindTransformFeedbackNV,
    &BindVertexArray,
    &BindVertexArrayAPPLE,
    &BindVertexBuffer,
    &BindVertexBuffers,
    &BindVertexShaderEXT,
    &BindVideoCaptureStreamBufferNV,
    &BindVideoCaptureStreamTextureNV,
    &Binormal3bEXT,
    &Binormal3bvEXT,
    &Binormal3dEXT,
    &Binormal3dvEXT,
    &Binormal3fEXT,
    &Binormal3fvEXT,
    &Binormal3iEXT,
    &Binormal3ivEXT,
    &Binormal3sEXT,
    &Binormal3svEXT,
    &BinormalPointerEXT,
    &Bitmap,
    &BitmapxOES,
    &BlendBarrierKHR,
    &BlendBarrierNV,
    &BlendColor,
    &BlendColorEXT,
    &BlendColorxOES,
    &BlendEquation,
    &BlendEquationEXT,
    &BlendEquationIndexedAMD,
    &BlendEquationSeparate,
    &BlendEquationSeparateEXT,
    &BlendEquationSeparateIndexedAMD,
    &BlendEquationSeparatei,
    &BlendEquationSeparateiARB,
    &BlendEquationi,
    &BlendEquationiARB,
    &BlendFunc,
    &BlendFuncIndexedAMD,
    &BlendFuncSeparate,
    &BlendFuncSeparateEXT,
    &BlendFuncSeparateINGR,
    &BlendFuncSeparateIndexedAMD,
    &BlendFuncSeparatei,
    &BlendFuncSeparateiARB,
    &BlendFunci,
    &BlendFunciARB,
    &BlendParameteriNV,
    &BlitFramebuffer,
    &BlitFramebufferEXT,
    &BlitNamedFramebuffer,
    &BufferAddressRangeNV,
    &BufferData,
    &BufferDataARB,
    &BufferPageCommitmentARB,
    &BufferParameteriAPPLE,
    &BufferStorage,
    &BufferSubData,
    &BufferSubDataARB,
    &CallCommandListNV,
    &CallList,
    &CallLists,
    &CheckFramebufferStatus,
    &CheckFramebufferStatusEXT,
    &CheckNamedFramebufferStatus,
    &CheckNamedFramebufferStatusEXT,
    &ClampColor,
    &ClampColorARB,
    &Clear,
    &ClearAccum,
    &ClearAccumxOES,
    &ClearBufferData,
    &ClearBufferSubData,
    &ClearBufferfi,
    &ClearBufferfv,
    &ClearBufferiv,
    &ClearBufferuiv,
    &ClearColor,
    &ClearColorIiEXT,
    &ClearColorIuiEXT,
    &ClearColorxOES,
    &ClearDepth,
    &ClearDepthdNV,
    &ClearDepthf,
    &ClearDepthfOES,
    &ClearDepthxOES,
    &ClearIndex,
    &ClearNamedBufferData,
    &ClearNamedBufferDataEXT,
    &ClearNamedBufferSubData,
    &ClearNamedBufferSubDataEXT,
    &ClearNamedFramebufferfi,
    &ClearNamedFramebufferfv,
    &ClearNamedFramebufferiv,
    &ClearNamedFramebufferuiv,
    &ClearStencil,
    &ClearTexImage,
    &ClearTexSubImage,
    &ClientActiveTexture,
    &ClientActiveTextureARB,
    &ClientActiveVertexStreamATI,
    &ClientAttribDefaultEXT,
    &ClientWaitSync,
    &ClipControl,
    &ClipPlane,
    &ClipPlanefOES,
    &ClipPlanexOES,
    &Color3b,
    &Color3bv,
    &Color3d,
    &Color3dv,
    &Color3f,
    &Color3fVertex3fSUN,
    &Color3fVertex3fvSUN,
    &Color3fv,
    &Color3hNV,
    &Color3hvNV,
    &Color3i,
    &Color3iv,
    &Color3s,
    &Color3sv,
    &Color3ub,
    &Color3ubv,
    &Color3ui,
    &Color3uiv,
    &Color3us,
    &Color3usv,
    &Color3xOES,
    &Color3xvOES,
    &Color4b,
    &Color4bv,
    &Color4d,
    &Color4dv,
    &Color4f,
    &Color4fNormal3fVertex3fSUN,
    &Color4fNormal3fVertex3fvSUN,
    &Color4fv,
    &Color4hNV,
    &Color4hvNV,
    &Color4i,
    &Color4iv,
    &Color4s,
    &Color4sv,
    &Color4ub,
    &Color4ubVertex2fSUN,
    &Color4ubVertex2fvSUN,
    &Color4ubVertex3fSUN,
    &Color4ubVertex3fvSUN,
    &Color4ubv,
    &Color4ui,
    &Color4uiv,
    &Color4us,
    &Color4usv,
    &Color4xOES,
    &Color4xvOES,
    &ColorFormatNV,
    &ColorFragmentOp1ATI,
    &ColorFragmentOp2ATI,
    &ColorFragmentOp3ATI,
    &ColorMask,
    &ColorMaskIndexedEXT,
    &ColorMaski,
    &ColorMaterial,
    &ColorP3ui,
    &ColorP3uiv,
    &ColorP4ui,
    &ColorP4uiv,
    &ColorPointer,
    &ColorPointerEXT,
    &ColorPointerListIBM,
    &ColorPointervINTEL,
    &ColorSubTable,
    &ColorSubTableEXT,
    &ColorTable,
    &ColorTableEXT,
    &ColorTableParameterfv,
    &ColorTableParameterfvSGI,
    &ColorTableParameteriv,
    &ColorTableParameterivSGI,
    &ColorTableSGI,
    &CombinerInputNV,
    &CombinerOutputNV,
    &CombinerParameterfNV,
    &CombinerParameterfvNV,
    &CombinerParameteriNV,
    &CombinerParameterivNV,
    &CombinerStageParameterfvNV,
    &CommandListSegmentsNV,
    &CompileCommandListNV,
    &CompileShader,
    &CompileShaderARB,
    &CompileShaderIncludeARB,
    &CompressedMultiTexImage1DEXT,
    &CompressedMultiTexImage2DEXT,
    &CompressedMultiTexImage3DEXT,
    &CompressedMultiTexSubImage1DEXT,
    &CompressedMultiTexSubImage2DEXT,
    &CompressedMultiTexSubImage3DEXT,
    &CompressedTexImage1D,
    &CompressedTexImage1DARB,
    &CompressedTexImage2D,
    &CompressedTexImage2DARB,
    &CompressedTexImage3D,
    &CompressedTexImage3DARB,
    &CompressedTexSubImage1D,
    &CompressedTexSubImage1DARB,
    &CompressedTexSubImage2D,
    &CompressedTexSubImage2DARB,
    &CompressedTexSubImage3D,
    &CompressedTexSubImage3DARB,
    &CompressedTextureImage1DEXT,
    &CompressedTextureImage2DEXT,
    &CompressedTextureImage3DEXT,
    &CompressedTextureSubImage1D,
    &CompressedTextureSubImage1DEXT,
    &CompressedTextureSubImage2D,
    &CompressedTextureSubImage2DEXT,
    &CompressedTextureSubImage3D,
    &CompressedTextureSubImage3DEXT,
    &ConservativeRasterParameterfNV,
    &ConvolutionFilter1D,
    &ConvolutionFilter1DEXT,
    &ConvolutionFilter2D,
    &ConvolutionFilter2DEXT,
    &ConvolutionParameterf,
    &ConvolutionParameterfEXT,
    &ConvolutionParameterfv,
    &ConvolutionParameterfvEXT,
    &ConvolutionParameteri,
    &ConvolutionParameteriEXT,
    &ConvolutionParameteriv,
    &ConvolutionParameterivEXT,
    &ConvolutionParameterxOES,
    &ConvolutionParameterxvOES,
    &CopyBufferSubData,
    &CopyColorSubTable,
    &CopyColorSubTableEXT,
    &CopyColorTable,
    &CopyColorTableSGI,
    &CopyConvolutionFilter1D,
    &CopyConvolutionFilter1DEXT,
    &CopyConvolutionFilter2D,
    &CopyConvolutionFilter2DEXT,
    &CopyImageSubData,
    &CopyImageSubDataNV,
    &CopyMultiTexImage1DEXT,
    &CopyMultiTexImage2DEXT,
    &CopyMultiTexSubImage1DEXT,
    &CopyMultiTexSubImage2DEXT,
    &CopyMultiTexSubImage3DEXT,
    &CopyNamedBufferSubData,
    &CopyPathNV,
    &CopyPixels,
    &CopyTexImage1D,
    &CopyTexImage1DEXT,
    &CopyTexImage2D,
    &CopyTexImage2DEXT,
    &CopyTexSubImage1D,
    &CopyTexSubImage1DEXT,
    &CopyTexSubImage2D,
    &CopyTexSubImage2DEXT,
    &CopyTexSubImage3D,
    &CopyTexSubImage3DEXT,
    &CopyTextureImage1DEXT,
    &CopyTextureImage2DEXT,
    &CopyTextureSubImage1D,
    &CopyTextureSubImage1DEXT,
    &CopyTextureSubImage2D,
    &CopyTextureSubImage2DEXT,
    &CopyTextureSubImage3D,
    &CopyTextureSubImage3DEXT,
    &CoverFillPathInstancedNV,
    &CoverFillPathNV,
    &CoverStrokePathInstancedNV,
    &CoverStrokePathNV,
    &CoverageModulationNV,
    &CoverageModulationTableNV,
    &CreateBuffers,
    &CreateCommandListsNV,
    &CreateFramebuffers,
    &CreatePerfQueryINTEL,
    &CreateProgram,
    &CreateProgramObjectARB,
    &CreateProgramPipelines,
    &CreateQueries,
    &CreateRenderbuffers,
    &CreateSamplers,
    &CreateShader,
    &CreateShaderObjectARB,
    &CreateShaderProgramEXT,
    &CreateShaderProgramv,
    &CreateStatesNV,
    &CreateSyncFromCLeventARB,
    &CreateTextures,
    &CreateTransformFeedbacks,
    &CreateVertexArrays,
    &CullFace,
    &CullParameterdvEXT,
    &CullParameterfvEXT,
    &CurrentPaletteMatrixARB,
    &DebugMessageCallback,
    &DebugMessageCallbackAMD,
    &DebugMessageCallbackARB,
    &DebugMessageControl,
    &DebugMessageControlARB,
    &DebugMessageEnableAMD,
    &DebugMessageInsert,
    &DebugMessageInsertAMD,
    &DebugMessageInsertARB,
    &DeformSGIX,
    &DeformationMap3dSGIX,
    &DeformationMap3fSGIX,
    &DeleteAsyncMarkersSGIX,
    &DeleteBuffers,
    &DeleteBuffersARB,
    &DeleteCommandListsNV,
    &DeleteFencesAPPLE,
    &DeleteFencesNV,
    &DeleteFragmentShaderATI,
    &DeleteFramebuffers,
    &DeleteFramebuffersEXT,
    &DeleteLists,
    &DeleteNamedStringARB,
    &DeleteNamesAMD,
    &DeleteObjectARB,
    &DeleteOcclusionQueriesNV,
    &DeletePathsNV,
    &DeletePerfMonitorsAMD,
    &DeletePerfQueryINTEL,
    &DeleteProgram,
    &DeleteProgramPipelines,
    &DeleteProgramsARB,
    &DeleteProgramsNV,
    &DeleteQueries,
    &DeleteQueriesARB,
    &DeleteRenderbuffers,
    &DeleteRenderbuffersEXT,
    &DeleteSamplers,
    &DeleteShader,
    &DeleteStatesNV,
    &DeleteSync,
    &DeleteTextures,
    &DeleteTexturesEXT,
    &DeleteTransformFeedbacks,
    &DeleteTransformFeedbacksNV,
    &DeleteVertexArrays,
    &DeleteVertexArraysAPPLE,
    &DeleteVertexShaderEXT,
    &DepthBoundsEXT,
    &DepthBoundsdNV,
    &DepthFunc,
    &DepthMask,
    &DepthRange,
    &DepthRangeArrayv,
    &DepthRangeIndexed,
    &DepthRangedNV,
    &DepthRangef,
    &DepthRangefOES,
    &DepthRangexOES,
    &DetachObjectARB,
    &DetachShader,
    &DetailTexFuncSGIS,
    &Disable,
    &DisableClientState,
    &DisableClientStateIndexedEXT,
    &DisableClientStateiEXT,
    &DisableIndexedEXT,
    &DisableVariantClientStateEXT,
    &DisableVertexArrayAttrib,
    &DisableVertexArrayAttribEXT,
    &DisableVertexArrayEXT,
    &DisableVertexAttribAPPLE,
    &DisableVertexAttribArray,
    &DisableVertexAttribArrayARB,
    &Disablei,
    &DispatchCompute,
    &DispatchComputeGroupSizeARB,
    &DispatchComputeIndirect,
    &DrawArrays,
    &DrawArraysEXT,
    &DrawArraysIndirect,
    &DrawArraysInstanced,
    &DrawArraysInstancedARB,
    &DrawArraysInstancedBaseInstance,
    &DrawArraysInstancedEXT,
    &DrawBuffer,
    &DrawBuffers,
    &DrawBuffersARB,
    &DrawBuffersATI,
    &DrawCommandsAddressNV,
    &DrawCommandsNV,
    &DrawCommandsStatesAddressNV,
    &DrawCommandsStatesNV,
    &DrawElementArrayAPPLE,
    &DrawElementArrayATI,
    &DrawElements,
    &DrawElementsBaseVertex,
    &DrawElementsIndirect,
    &DrawElementsInstanced,
    &DrawElementsInstancedARB,
    &DrawElementsInstancedBaseInstance,
    &DrawElementsInstancedBaseVertex,
    &DrawElementsInstancedBaseVertexBaseInstance,
    &DrawElementsInstancedEXT,
    &DrawMeshArraysSUN,
    &DrawPixels,
    &DrawRangeElementArrayAPPLE,
    &DrawRangeElementArrayATI,
    &DrawRangeElements,
    &DrawRangeElementsBaseVertex,
    &DrawRangeElementsEXT,
    &DrawTextureNV,
    &DrawTransformFeedback,
    &DrawTransformFeedbackInstanced,
    &DrawTransformFeedbackNV,
    &DrawTransformFeedbackStream,
    &DrawTransformFeedbackStreamInstanced,
    &EdgeFlag,
    &EdgeFlagFormatNV,
    &EdgeFlagPointer,
    &EdgeFlagPointerEXT,
    &EdgeFlagPointerListIBM,
    &EdgeFlagv,
    &ElementPointerAPPLE,
    &ElementPointerATI,
    &Enable,
    &EnableClientState,
    &EnableClientStateIndexedEXT,
    &EnableClientStateiEXT,
    &EnableIndexedEXT,
    &EnableVariantClientStateEXT,
    &EnableVertexArrayAttrib,
    &EnableVertexArrayAttribEXT,
    &EnableVertexArrayEXT,
    &EnableVertexAttribAPPLE,
    &EnableVertexAttribArray,
    &EnableVertexAttribArrayARB,
    &Enablei,
    &End,
    &EndConditionalRender,
    &EndConditionalRenderNV,
    &EndConditionalRenderNVX,
    &EndFragmentShaderATI,
    &EndList,
    &EndOcclusionQueryNV,
    &EndPerfMonitorAMD,
    &EndPerfQueryINTEL,
    &EndQuery,
    &EndQueryARB,
    &EndQueryIndexed,
    &EndTransformFeedback,
    &EndTransformFeedbackEXT,
    &EndTransformFeedbackNV,
    &EndVertexShaderEXT,
    &EndVideoCaptureNV,
    &EvalCoord1d,
    &EvalCoord1dv,
    &EvalCoord1f,
    &EvalCoord1fv,
    &EvalCoord1xOES,
    &EvalCoord1xvOES,
    &EvalCoord2d,
    &EvalCoord2dv,
    &EvalCoord2f,
    &EvalCoord2fv,
    &EvalCoord2xOES,
    &EvalCoord2xvOES,
    &EvalMapsNV,
    &EvalMesh1,
    &EvalMesh2,
    &EvalPoint1,
    &EvalPoint2,
    &EvaluateDepthValuesARB,
    &ExecuteProgramNV,
    &ExtractComponentEXT,
    &FeedbackBuffer,
    &FeedbackBufferxOES,
    &FenceSync,
    &FinalCombinerInputNV,
    &Finish,
    &FinishAsyncSGIX,
    &FinishFenceAPPLE,
    &FinishFenceNV,
    &FinishObjectAPPLE,
    &FinishTextureSUNX,
    &Flush,
    &FlushMappedBufferRange,
    &FlushMappedBufferRangeAPPLE,
    &FlushMappedNamedBufferRange,
    &FlushMappedNamedBufferRangeEXT,
    &FlushPixelDataRangeNV,
    &FlushRasterSGIX,
    &FlushStaticDataIBM,
    &FlushVertexArrayRangeAPPLE,
    &FlushVertexArrayRangeNV,
    &FogCoordFormatNV,
    &FogCoordPointer,
    &FogCoordPointerEXT,
    &FogCoordPointerListIBM,
    &FogCoordd,
    &FogCoorddEXT,
    &FogCoorddv,
    &FogCoorddvEXT,
    &FogCoordf,
    &FogCoordfEXT,
    &FogCoordfv,
    &FogCoordfvEXT,
    &FogCoordhNV,
    &FogCoordhvNV,
    &FogFuncSGIS,
    &Fogf,
    &Fogfv,
    &Fogi,
    &Fogiv,
    &FogxOES,
    &FogxvOES,
    &FragmentColorMaterialSGIX,
    &FragmentCoverageColorNV,
    &FragmentLightModelfSGIX,
    &FragmentLightModelfvSGIX,
    &FragmentLightModeliSGIX,
    &FragmentLightModelivSGIX,
    &FragmentLightfSGIX,
    &FragmentLightfvSGIX,
    &FragmentLightiSGIX,
    &FragmentLightivSGIX,
    &FragmentMaterialfSGIX,
    &FragmentMaterialfvSGIX,
    &FragmentMaterialiSGIX,
    &FragmentMaterialivSGIX,
    &FrameTerminatorGREMEDY,
    &FrameZoomSGIX,
    &FramebufferDrawBufferEXT,
    &FramebufferDrawBuffersEXT,
    &FramebufferParameteri,
    &FramebufferReadBufferEXT,
    &FramebufferRenderbuffer,
    &FramebufferRenderbufferEXT,
    &FramebufferSampleLocationsfvARB,
    &FramebufferSampleLocationsfvNV,
    &FramebufferTexture,
    &FramebufferTexture1D,
    &FramebufferTexture1DEXT,
    &FramebufferTexture2D,
    &FramebufferTexture2DEXT,
    &FramebufferTexture3D,
    &FramebufferTexture3DEXT,
    &FramebufferTextureARB,
    &FramebufferTextureEXT,
    &FramebufferTextureFaceARB,
    &FramebufferTextureFaceEXT,
    &FramebufferTextureLayer,
    &FramebufferTextureLayerARB,
    &FramebufferTextureLayerEXT,
    &FramebufferTextureMultiviewOVR,
    &FreeObjectBufferATI,
    &FrontFace,
    &Frustum,
    &FrustumfOES,
    &FrustumxOES,
    &GenAsyncMarkersSGIX,
    &GenBuffers,
    &GenBuffersARB,
    &GenFencesAPPLE,
    &GenFencesNV,
    &GenFragmentShadersATI,
    &GenFramebuffers,
    &GenFramebuffersEXT,
    &GenLists,
    &GenNamesAMD,
    &GenOcclusionQueriesNV,
    &GenPathsNV,
    &GenPerfMonitorsAMD,
    &GenProgramPipelines,
    &GenProgramsARB,
    &GenProgramsNV,
    &GenQueries,
    &GenQueriesARB,
    &GenRenderbuffers,
    &GenRenderbuffersEXT,
    &GenSamplers,
    &GenSymbolsEXT,
    &GenTextures,
    &GenTexturesEXT,
    &GenTransformFeedbacks,
    &GenTransformFeedbacksNV,
    &GenVertexArrays,
    &GenVertexArraysAPPLE,
    &GenVertexShadersEXT,
    &GenerateMipmap,
    &GenerateMipmapEXT,
    &GenerateMultiTexMipmapEXT,
    &GenerateTextureMipmap,
    &GenerateTextureMipmapEXT,
    &GetActiveAtomicCounterBufferiv,
    &GetActiveAttrib,
    &GetActiveAttribARB,
    &GetActiveSubroutineName,
    &GetActiveSubroutineUniformName,
    &GetActiveSubroutineUniformiv,
    &GetActiveUniform,
    &GetActiveUniformARB,
    &GetActiveUniformBlockName,
    &GetActiveUniformBlockiv,
    &GetActiveUniformName,
    &GetActiveUniformsiv,
    &GetActiveVaryingNV,
    &GetArrayObjectfvATI,
    &GetArrayObjectivATI,
    &GetAttachedObjectsARB,
    &GetAttachedShaders,
    &GetAttribLocation,
    &GetAttribLocationARB,
    &GetBooleanIndexedvEXT,
    &GetBooleani_v,
    &GetBooleanv,
    &GetBufferParameteri64v,
    &GetBufferParameteriv,
    &GetBufferParameterivARB,
    &GetBufferParameterui64vNV,
    &GetBufferPointerv,
    &GetBufferPointervARB,
    &GetBufferSubData,
    &GetBufferSubDataARB,
    &GetClipPlane,
    &GetClipPlanefOES,
    &GetClipPlanexOES,
    &GetColorTable,
    &GetColorTableEXT,
    &GetColorTableParameterfv,
    &GetColorTableParameterfvEXT,
    &GetColorTableParameterfvSGI,
    &GetColorTableParameteriv,
    &GetColorTableParameterivEXT,
    &GetColorTableParameterivSGI,
    &GetColorTableSGI,
    &GetCombinerInputParameterfvNV,
    &GetCombinerInputParameterivNV,
    &GetCombinerOutputParameterfvNV,
    &GetCombinerOutputParameterivNV,
    &GetCombinerStageParameterfvNV,
    &GetCommandHeaderNV,
    &GetCompressedMultiTexImageEXT,
    &GetCompressedTexImage,
    &GetCompressedTexImageARB,
    &GetCompressedTextureImage,
    &GetCompressedTextureImageEXT,
    &GetCompressedTextureSubImage,
    &GetConvolutionFilter,
    &GetConvolutionFilterEXT,
    &GetConvolutionParameterfv,
    &GetConvolutionParameterfvEXT,
    &GetConvolutionParameteriv,
    &GetConvolutionParameterivEXT,
    &GetConvolutionParameterxvOES,
    &GetCoverageModulationTableNV,
    &GetDebugMessageLog,
    &GetDebugMessageLogAMD,
    &GetDebugMessageLogARB,
    &GetDetailTexFuncSGIS,
    &GetDoubleIndexedvEXT,
    &GetDoublei_v,
    &GetDoublei_vEXT,
    &GetDoublev,
    &GetError,
    &GetFenceivNV,
    &GetFinalCombinerInputParameterfvNV,
    &GetFinalCombinerInputParameterivNV,
    &GetFirstPerfQueryIdINTEL,
    &GetFixedvOES,
    &GetFloatIndexedvEXT,
    &GetFloati_v,
    &GetFloati_vEXT,
    &GetFloatv,
    &GetFogFuncSGIS,
    &GetFragDataIndex,
    &GetFragDataLocation,
    &GetFragDataLocationEXT,
    &GetFragmentLightfvSGIX,
    &GetFragmentLightivSGIX,
    &GetFragmentMaterialfvSGIX,
    &GetFragmentMaterialivSGIX,
    &GetFramebufferAttachmentParameteriv,
    &GetFramebufferAttachmentParameterivEXT,
    &GetFramebufferParameteriv,
    &GetFramebufferParameterivEXT,
    &GetGraphicsResetStatus,
    &GetGraphicsResetStatusARB,
    &GetHandleARB,
    &GetHistogram,
    &GetHistogramEXT,
    &GetHistogramParameterfv,
    &GetHistogramParameterfvEXT,
    &GetHistogramParameteriv,
    &GetHistogramParameterivEXT,
    &GetHistogramParameterxvOES,
    &GetImageHandleARB,
    &GetImageHandleNV,
    &GetImageTransformParameterfvHP,
    &GetImageTransformParameterivHP,
    &GetInfoLogARB,
    &GetInstrumentsSGIX,
    &GetInteger64i_v,
    &GetInteger64v,
    &GetIntegerIndexedvEXT,
    &GetIntegeri_v,
    &GetIntegerui64i_vNV,
    &GetIntegerui64vNV,
    &GetIntegerv,
    &GetInternalformatSampleivNV,
    &GetInternalformati64v,
    &GetInternalformativ,
    &GetInvariantBooleanvEXT,
    &GetInvariantFloatvEXT,
    &GetInvariantIntegervEXT,
    &GetLightfv,
    &GetLightiv,
    &GetLightxOES,
    &GetListParameterfvSGIX,
    &GetListParameterivSGIX,
    &GetLocalConstantBooleanvEXT,
    &GetLocalConstantFloatvEXT,
    &GetLocalConstantIntegervEXT,
    &GetMapAttribParameterfvNV,
    &GetMapAttribParameterivNV,
    &GetMapControlPointsNV,
    &GetMapParameterfvNV,
    &GetMapParameterivNV,
    &GetMapdv,
    &GetMapfv,
    &GetMapiv,
    &GetMapxvOES,
    &GetMaterialfv,
    &GetMaterialiv,
    &GetMaterialxOES,
    &GetMinmax,
    &GetMinmaxEXT,
    &GetMinmaxParameterfv,
    &GetMinmaxParameterfvEXT,
    &GetMinmaxParameteriv,
    &GetMinmaxParameterivEXT,
    &GetMultiTexEnvfvEXT,
    &GetMultiTexEnvivEXT,
    &GetMultiTexGendvEXT,
    &GetMultiTexGenfvEXT,
    &GetMultiTexGenivEXT,
    &GetMultiTexImageEXT,
    &GetMultiTexLevelParameterfvEXT,
    &GetMultiTexLevelParameterivEXT,
    &GetMultiTexParameterIivEXT,
    &GetMultiTexParameterIuivEXT,
    &GetMultiTexParameterfvEXT,
    &GetMultiTexParameterivEXT,
    &GetMultisamplefv,
    &GetMultisamplefvNV,
    &GetNamedBufferParameteri64v,
    &GetNamedBufferParameteriv,
    &GetNamedBufferParameterivEXT,
    &GetNamedBufferParameterui64vNV,
    &GetNamedBufferPointerv,
    &GetNamedBufferPointervEXT,
    &GetNamedBufferSubData,
    &GetNamedBufferSubDataEXT,
    &GetNamedFramebufferAttachmentParameteriv,
    &GetNamedFramebufferAttachmentParameterivEXT,
    &GetNamedFramebufferParameteriv,
    &GetNamedFramebufferParameterivEXT,
    &GetNamedProgramLocalParameterIivEXT,
    &GetNamedProgramLocalParameterIuivEXT,
    &GetNamedProgramLocalParameterdvEXT,
    &GetNamedProgramLocalParameterfvEXT,
    &GetNamedProgramStringEXT,
    &GetNamedProgramivEXT,
    &GetNamedRenderbufferParameteriv,
    &GetNamedRenderbufferParameterivEXT,
    &GetNamedStringARB,
    &GetNamedStringivARB,
    &GetNextPerfQueryIdINTEL,
    &GetObjectBufferfvATI,
    &GetObjectBufferivATI,
    &GetObjectLabel,
    &GetObjectLabelEXT,
    &GetObjectParameterfvARB,
    &GetObjectParameterivAPPLE,
    &GetObjectParameterivARB,
    &GetObjectPtrLabel,
    &GetOcclusionQueryivNV,
    &GetOcclusionQueryuivNV,
    &GetPathColorGenfvNV,
    &GetPathColorGenivNV,
    &GetPathCommandsNV,
    &GetPathCoordsNV,
    &GetPathDashArrayNV,
    &GetPathLengthNV,
    &GetPathMetricRangeNV,
    &GetPathMetricsNV,
    &GetPathParameterfvNV,
    &GetPathParameterivNV,
    &GetPathSpacingNV,
    &GetPathTexGenfvNV,
    &GetPathTexGenivNV,
    &GetPerfCounterInfoINTEL,
    &GetPerfMonitorCounterDataAMD,
    &GetPerfMonitorCounterInfoAMD,
    &GetPerfMonitorCounterStringAMD,
    &GetPerfMonitorCountersAMD,
    &GetPerfMonitorGroupStringAMD,
    &GetPerfMonitorGroupsAMD,
    &GetPerfQueryDataINTEL,
    &GetPerfQueryIdByNameINTEL,
    &GetPerfQueryInfoINTEL,
    &GetPixelMapfv,
    &GetPixelMapuiv,
    &GetPixelMapusv,
    &GetPixelMapxv,
    &GetPixelTexGenParameterfvSGIS,
    &GetPixelTexGenParameterivSGIS,
    &GetPixelTransformParameterfvEXT,
    &GetPixelTransformParameterivEXT,
    &GetPointerIndexedvEXT,
    &GetPointeri_vEXT,
    &GetPointerv,
    &GetPointervEXT,
    &GetPolygonStipple,
    &GetProgramBinary,
    &GetProgramEnvParameterIivNV,
    &GetProgramEnvParameterIuivNV,
    &GetProgramEnvParameterdvARB,
    &GetProgramEnvParameterfvARB,
    &GetProgramInfoLog,
    &GetProgramInterfaceiv,
    &GetProgramLocalParameterIivNV,
    &GetProgramLocalParameterIuivNV,
    &GetProgramLocalParameterdvARB,
    &GetProgramLocalParameterfvARB,
    &GetProgramNamedParameterdvNV,
    &GetProgramNamedParameterfvNV,
    &GetProgramParameterdvNV,
    &GetProgramParameterfvNV,
    &GetProgramPipelineInfoLog,
    &GetProgramPipelineiv,
    &GetProgramResourceIndex,
    &GetProgramResourceLocation,
    &GetProgramResourceLocationIndex,
    &GetProgramResourceName,
    &GetProgramResourcefvNV,
    &GetProgramResourceiv,
    &GetProgramStageiv,
    &GetProgramStringARB,
    &GetProgramStringNV,
    &GetProgramSubroutineParameteruivNV,
    &GetProgramiv,
    &GetProgramivARB,
    &GetProgramivNV,
    &GetQueryBufferObjecti64v,
    &GetQueryBufferObjectiv,
    &GetQueryBufferObjectui64v,
    &GetQueryBufferObjectuiv,
    &GetQueryIndexediv,
    &GetQueryObjecti64v,
    &GetQueryObjecti64vEXT,
    &GetQueryObjectiv,
    &GetQueryObjectivARB,
    &GetQueryObjectui64v,
    &GetQueryObjectui64vEXT,
    &GetQueryObjectuiv,
    &GetQueryObjectuivARB,
    &GetQueryiv,
    &GetQueryivARB,
    &GetRenderbufferParameteriv,
    &GetRenderbufferParameterivEXT,
    &GetSamplerParameterIiv,
    &GetSamplerParameterIuiv,
    &GetSamplerParameterfv,
    &GetSamplerParameteriv,
    &GetSeparableFilter,
    &GetSeparableFilterEXT,
    &GetShaderInfoLog,
    &GetShaderPrecisionFormat,
    &GetShaderSource,
    &GetShaderSourceARB,
    &GetShaderiv,
    &GetSharpenTexFuncSGIS,
    &GetStageIndexNV,
    &GetString,
    &GetStringi,
    &GetSubroutineIndex,
    &GetSubroutineUniformLocation,
    &GetSynciv,
    &GetTexBumpParameterfvATI,
    &GetTexBumpParameterivATI,
    &GetTexEnvfv,
    &GetTexEnviv,
    &GetTexEnvxvOES,
    &GetTexFilterFuncSGIS,
    &GetTexGendv,
    &GetTexGenfv,
    &GetTexGeniv,
    &GetTexGenxvOES,
    &GetTexImage,
    &GetTexLevelParameterfv,
    &GetTexLevelParameteriv,
    &GetTexLevelParameterxvOES,
    &GetTexParameterIiv,
    &GetTexParameterIivEXT,
    &GetTexParameterIuiv,
    &GetTexParameterIuivEXT,
    &GetTexParameterPointervAPPLE,
    &GetTexParameterfv,
    &GetTexParameteriv,
    &GetTexParameterxvOES,
    &GetTextureHandleARB,
    &GetTextureHandleNV,
    &GetTextureImage,
    &GetTextureImageEXT,
    &GetTextureLevelParameterfv,
    &GetTextureLevelParameterfvEXT,
    &GetTextureLevelParameteriv,
    &GetTextureLevelParameterivEXT,
    &GetTextureParameterIiv,
    &GetTextureParameterIivEXT,
    &GetTextureParameterIuiv,
    &GetTextureParameterIuivEXT,
    &GetTextureParameterfv,
    &GetTextureParameterfvEXT,
    &GetTextureParameteriv,
    &GetTextureParameterivEXT,
    &GetTextureSamplerHandleARB,
    &GetTextureSamplerHandleNV,
    &GetTextureSubImage,
    &GetTrackMatrixivNV,
    &GetTransformFeedbackVarying,
    &GetTransformFeedbackVaryingEXT,
    &GetTransformFeedbackVaryingNV,
    &GetTransformFeedbacki64_v,
    &GetTransformFeedbacki_v,
    &GetTransformFeedbackiv,
    &GetUniformBlockIndex,
    &GetUniformBufferSizeEXT,
    &GetUniformIndices,
    &GetUniformLocation,
    &GetUniformLocationARB,
    &GetUniformOffsetEXT,
    &GetUniformSubroutineuiv,
    &GetUniformdv,
    &GetUniformfv,
    &GetUniformfvARB,
    &GetUniformi64vARB,
    &GetUniformi64vNV,
    &GetUniformiv,
    &GetUniformivARB,
    &GetUniformui64vARB,
    &GetUniformui64vNV,
    &GetUniformuiv,
    &GetUniformuivEXT,
    &GetVariantArrayObjectfvATI,
    &GetVariantArrayObjectivATI,
    &GetVariantBooleanvEXT,
    &GetVariantFloatvEXT,
    &GetVariantIntegervEXT,
    &GetVariantPointervEXT,
    &GetVaryingLocationNV,
    &GetVertexArrayIndexed64iv,
    &GetVertexArrayIndexediv,
    &GetVertexArrayIntegeri_vEXT,
    &GetVertexArrayIntegervEXT,
    &GetVertexArrayPointeri_vEXT,
    &GetVertexArrayPointervEXT,
    &GetVertexArrayiv,
    &GetVertexAttribArrayObjectfvATI,
    &GetVertexAttribArrayObjectivATI,
    &GetVertexAttribIiv,
    &GetVertexAttribIivEXT,
    &GetVertexAttribIuiv,
    &GetVertexAttribIuivEXT,
    &GetVertexAttribLdv,
    &GetVertexAttribLdvEXT,
    &GetVertexAttribLi64vNV,
    &GetVertexAttribLui64vARB,
    &GetVertexAttribLui64vNV,
    &GetVertexAttribPointerv,
    &GetVertexAttribPointervARB,
    &GetVertexAttribPointervNV,
    &GetVertexAttribdv,
    &GetVertexAttribdvARB,
    &GetVertexAttribdvNV,
    &GetVertexAttribfv,
    &GetVertexAttribfvARB,
    &GetVertexAttribfvNV,
    &GetVertexAttribiv,
    &GetVertexAttribivARB,
    &GetVertexAttribivNV,
    &GetVideoCaptureStreamdvNV,
    &GetVideoCaptureStreamfvNV,
    &GetVideoCaptureStreamivNV,
    &GetVideoCaptureivNV,
    &GetVideoi64vNV,
    &GetVideoivNV,
    &GetVideoui64vNV,
    &GetVideouivNV,
    &GetnColorTable,
    &GetnColorTableARB,
    &GetnCompressedTexImage,
    &GetnCompressedTexImageARB,
    &GetnConvolutionFilter,
    &GetnConvolutionFilterARB,
    &GetnHistogram,
    &GetnHistogramARB,
    &GetnMapdv,
    &GetnMapdvARB,
    &GetnMapfv,
    &GetnMapfvARB,
    &GetnMapiv,
    &GetnMapivARB,
    &GetnMinmax,
    &GetnMinmaxARB,
    &GetnPixelMapfv,
    &GetnPixelMapfvARB,
    &GetnPixelMapuiv,
    &GetnPixelMapuivARB,
    &GetnPixelMapusv,
    &GetnPixelMapusvARB,
    &GetnPolygonStipple,
    &GetnPolygonStippleARB,
    &GetnSeparableFilter,
    &GetnSeparableFilterARB,
    &GetnTexImage,
    &GetnTexImageARB,
    &GetnUniformdv,
    &GetnUniformdvARB,
    &GetnUniformfv,
    &GetnUniformfvARB,
    &GetnUniformi64vARB,
    &GetnUniformiv,
    &GetnUniformivARB,
    &GetnUniformui64vARB,
    &GetnUniformuiv,
    &GetnUniformuivARB,
    &GlobalAlphaFactorbSUN,
    &GlobalAlphaFactordSUN,
    &GlobalAlphaFactorfSUN,
    &GlobalAlphaFactoriSUN,
    &GlobalAlphaFactorsSUN,
    &GlobalAlphaFactorubSUN,
    &GlobalAlphaFactoruiSUN,
    &GlobalAlphaFactorusSUN,
    &Hint,
    &HintPGI,
    &Histogram,
    &HistogramEXT,
    &IglooInterfaceSGIX,
    &ImageTransformParameterfHP,
    &ImageTransformParameterfvHP,
    &ImageTransformParameteriHP,
    &ImageTransformParameterivHP,
    &ImportSyncEXT,
    &IndexFormatNV,
    &IndexFuncEXT,
    &IndexMask,
    &IndexMaterialEXT,
    &IndexPointer,
    &IndexPointerEXT,
    &IndexPointerListIBM,
    &Indexd,
    &Indexdv,
    &Indexf,
    &Indexfv,
    &Indexi,
    &Indexiv,
    &Indexs,
    &Indexsv,
    &Indexub,
    &Indexubv,
    &IndexxOES,
    &IndexxvOES,
    &InitNames,
    &InsertComponentEXT,
    &InsertEventMarkerEXT,
    &InstrumentsBufferSGIX,
    &InterleavedArrays,
    &InterpolatePathsNV,
    &InvalidateBufferData,
    &InvalidateBufferSubData,
    &InvalidateFramebuffer,
    &InvalidateNamedFramebufferData,
    &InvalidateNamedFramebufferSubData,
    &InvalidateSubFramebuffer,
    &InvalidateTexImage,
    &InvalidateTexSubImage,
    &IsAsyncMarkerSGIX,
    &IsBuffer,
    &IsBufferARB,
    &IsBufferResidentNV,
    &IsCommandListNV,
    &IsEnabled,
    &IsEnabledIndexedEXT,
    &IsEnabledi,
    &IsFenceAPPLE,
    &IsFenceNV,
    &IsFramebuffer,
    &IsFramebufferEXT,
    &IsImageHandleResidentARB,
    &IsImageHandleResidentNV,
    &IsList,
    &IsNameAMD,
    &IsNamedBufferResidentNV,
    &IsNamedStringARB,
    &IsObjectBufferATI,
    &IsOcclusionQueryNV,
    &IsPathNV,
    &IsPointInFillPathNV,
    &IsPointInStrokePathNV,
    &IsProgram,
    &IsProgramARB,
    &IsProgramNV,
    &IsProgramPipeline,
    &IsQuery,
    &IsQueryARB,
    &IsRenderbuffer,
    &IsRenderbufferEXT,
    &IsSampler,
    &IsShader,
    &IsStateNV,
    &IsSync,
    &IsTexture,
    &IsTextureEXT,
    &IsTextureHandleResidentARB,
    &IsTextureHandleResidentNV,
    &IsTransformFeedback,
    &IsTransformFeedbackNV,
    &IsVariantEnabledEXT,
    &IsVertexArray,
    &IsVertexArrayAPPLE,
    &IsVertexAttribEnabledAPPLE,
    &LabelObjectEXT,
    &LightEnviSGIX,
    &LightModelf,
    &LightModelfv,
    &LightModeli,
    &LightModeliv,
    &LightModelxOES,
    &LightModelxvOES,
    &Lightf,
    &Lightfv,
    &Lighti,
    &Lightiv,
    &LightxOES,
    &LightxvOES,
    &LineStipple,
    &LineWidth,
    &LineWidthxOES,
    &LinkProgram,
    &LinkProgramARB,
    &ListBase,
    &ListDrawCommandsStatesClientNV,
    &ListParameterfSGIX,
    &ListParameterfvSGIX,
    &ListParameteriSGIX,
    &ListParameterivSGIX,
    &LoadIdentity,
    &LoadIdentityDeformationMapSGIX,
    &LoadMatrixd,
    &LoadMatrixf,
    &LoadMatrixxOES,
    &LoadName,
    &LoadProgramNV,
    &LoadTransposeMatrixd,
    &LoadTransposeMatrixdARB,
    &LoadTransposeMatrixf,
    &LoadTransposeMatrixfARB,
    &LoadTransposeMatrixxOES,
    &LockArraysEXT,
    &LogicOp,
    &MakeBufferNonResidentNV,
    &MakeBufferResidentNV,
    &MakeImageHandleNonResidentARB,
    &MakeImageHandleNonResidentNV,
    &MakeImageHandleResidentARB,
    &MakeImageHandleResidentNV,
    &MakeNamedBufferNonResidentNV,
    &MakeNamedBufferResidentNV,
    &MakeTextureHandleNonResidentARB,
    &MakeTextureHandleNonResidentNV,
    &MakeTextureHandleResidentARB,
    &MakeTextureHandleResidentNV,
    &Map1d,
    &Map1f,
    &Map1xOES,
    &Map2d,
    &Map2f,
    &Map2xOES,
    &MapBuffer,
    &MapBufferARB,
    &MapBufferRange,
    &MapControlPointsNV,
    &MapGrid1d,
    &MapGrid1f,
    &MapGrid1xOES,
    &MapGrid2d,
    &MapGrid2f,
    &MapGrid2xOES,
    &MapNamedBuffer,
    &MapNamedBufferEXT,
    &MapNamedBufferRange,
    &MapNamedBufferRangeEXT,
    &MapObjectBufferATI,
    &MapParameterfvNV,
    &MapParameterivNV,
    &MapTexture2DINTEL,
    &MapVertexAttrib1dAPPLE,
    &MapVertexAttrib1fAPPLE,
    &MapVertexAttrib2dAPPLE,
    &MapVertexAttrib2fAPPLE,
    &Materialf,
    &Materialfv,
    &Materiali,
    &Materialiv,
    &MaterialxOES,
    &MaterialxvOES,
    &MatrixFrustumEXT,
    &MatrixIndexPointerARB,
    &MatrixIndexubvARB,
    &MatrixIndexuivARB,
    &MatrixIndexusvARB,
    &MatrixLoad3x2fNV,
    &MatrixLoad3x3fNV,
    &MatrixLoadIdentityEXT,
    &MatrixLoadTranspose3x3fNV,
    &MatrixLoadTransposedEXT,
    &MatrixLoadTransposefEXT,
    &MatrixLoaddEXT,
    &MatrixLoadfEXT,
    &MatrixMode,
    &MatrixMult3x2fNV,
    &MatrixMult3x3fNV,
    &MatrixMultTranspose3x3fNV,
    &MatrixMultTransposedEXT,
    &MatrixMultTransposefEXT,
    &MatrixMultdEXT,
    &MatrixMultfEXT,
    &MatrixOrthoEXT,
    &MatrixPopEXT,
    &MatrixPushEXT,
    &MatrixRotatedEXT,
    &MatrixRotatefEXT,
    &MatrixScaledEXT,
    &MatrixScalefEXT,
    &MatrixTranslatedEXT,
    &MatrixTranslatefEXT,
    &MaxShaderCompilerThreadsARB,
    &MemoryBarrier,
    &MemoryBarrierByRegion,
    &MemoryBarrierEXT,
    &MinSampleShading,
    &MinSampleShadingARB,
    &Minmax,
    &MinmaxEXT,
    &MultMatrixd,
    &MultMatrixf,
    &MultMatrixxOES,
    &MultTransposeMatrixd,
    &MultTransposeMatrixdARB,
    &MultTransposeMatrixf,
    &MultTransposeMatrixfARB,
    &MultTransposeMatrixxOES,
    &MultiDrawArrays,
    &MultiDrawArraysEXT,
    &MultiDrawArraysIndirect,
    &MultiDrawArraysIndirectAMD,
    &MultiDrawArraysIndirectBindlessCountNV,
    &MultiDrawArraysIndirectBindlessNV,
    &MultiDrawArraysIndirectCountARB,
    &MultiDrawElementArrayAPPLE,
    &MultiDrawElements,
    &MultiDrawElementsBaseVertex,
    &MultiDrawElementsEXT,
    &MultiDrawElementsIndirect,
    &MultiDrawElementsIndirectAMD,
    &MultiDrawElementsIndirectBindlessCountNV,
    &MultiDrawElementsIndirectBindlessNV,
    &MultiDrawElementsIndirectCountARB,
    &MultiDrawRangeElementArrayAPPLE,
    &MultiModeDrawArraysIBM,
    &MultiModeDrawElementsIBM,
    &MultiTexBufferEXT,
    &MultiTexCoord1bOES,
    &MultiTexCoord1bvOES,
    &MultiTexCoord1d,
    &MultiTexCoord1dARB,
    &MultiTexCoord1dv,
    &MultiTexCoord1dvARB,
    &MultiTexCoord1f,
    &MultiTexCoord1fARB,
    &MultiTexCoord1fv,
    &MultiTexCoord1fvARB,
    &MultiTexCoord1hNV,
    &MultiTexCoord1hvNV,
    &MultiTexCoord1i,
    &MultiTexCoord1iARB,
    &MultiTexCoord1iv,
    &MultiTexCoord1ivARB,
    &MultiTexCoord1s,
    &MultiTexCoord1sARB,
    &MultiTexCoord1sv,
    &MultiTexCoord1svARB,
    &MultiTexCoord1xOES,
    &MultiTexCoord1xvOES,
    &MultiTexCoord2bOES,
    &MultiTexCoord2bvOES,
    &MultiTexCoord2d,
    &MultiTexCoord2dARB,
    &MultiTexCoord2dv,
    &MultiTexCoord2dvARB,
    &MultiTexCoord2f,
    &MultiTexCoord2fARB,
    &MultiTexCoord2fv,
    &MultiTexCoord2fvARB,
    &MultiTexCoord2hNV,
    &MultiTexCoord2hvNV,
    &MultiTexCoord2i,
    &MultiTexCoord2iARB,
    &MultiTexCoord2iv,
    &MultiTexCoord2ivARB,
    &MultiTexCoord2s,
    &MultiTexCoord2sARB,
    &MultiTexCoord2sv,
    &MultiTexCoord2svARB,
    &MultiTexCoord2xOES,
    &MultiTexCoord2xvOES,
    &MultiTexCoord3bOES,
    &MultiTexCoord3bvOES,
    &MultiTexCoord3d,
    &MultiTexCoord3dARB,
    &MultiTexCoord3dv,
    &MultiTexCoord3dvARB,
    &MultiTexCoord3f,
    &MultiTexCoord3fARB,
    &MultiTexCoord3fv,
    &MultiTexCoord3fvARB,
    &MultiTexCoord3hNV,
    &MultiTexCoord3hvNV,
    &MultiTexCoord3i,
    &MultiTexCoord3iARB,
    &MultiTexCoord3iv,
    &MultiTexCoord3ivARB,
    &MultiTexCoord3s,
    &MultiTexCoord3sARB,
    &MultiTexCoord3sv,
    &MultiTexCoord3svARB,
    &MultiTexCoord3xOES,
    &MultiTexCoord3xvOES,
    &MultiTexCoord4bOES,
    &MultiTexCoord4bvOES,
    &MultiTexCoord4d,
    &MultiTexCoord4dARB,
    &MultiTexCoord4dv,
    &MultiTexCoord4dvARB,
    &MultiTexCoord4f,
    &MultiTexCoord4fARB,
    &MultiTexCoord4fv,
    &MultiTexCoord4fvARB,
    &MultiTexCoord4hNV,
    &MultiTexCoord4hvNV,
    &MultiTexCoord4i,
    &MultiTexCoord4iARB,
    &MultiTexCoord4iv,
    &MultiTexCoord4ivARB,
    &MultiTexCoord4s,
    &MultiTexCoord4sARB,
    &MultiTexCoord4sv,
    &MultiTexCoord4svARB,
    &MultiTexCoord4xOES,
    &MultiTexCoord4xvOES,
    &MultiTexCoordP1ui,
    &MultiTexCoordP1uiv,
    &MultiTexCoordP2ui,
    &MultiTexCoordP2uiv,
    &MultiTexCoordP3ui,
    &MultiTexCoordP3uiv,
    &MultiTexCoordP4ui,
    &MultiTexCoordP4uiv,
    &MultiTexCoordPointerEXT,
    &MultiTexEnvfEXT,
    &MultiTexEnvfvEXT,
    &MultiTexEnviEXT,
    &MultiTexEnvivEXT,
    &MultiTexGendEXT,
    &MultiTexGendvEXT,
    &MultiTexGenfEXT,
    &MultiTexGenfvEXT,
    &MultiTexGeniEXT,
    &MultiTexGenivEXT,
    &MultiTexImage1DEXT,
    &MultiTexImage2DEXT,
    &MultiTexImage3DEXT,
    &MultiTexParameterIivEXT,
    &MultiTexParameterIuivEXT,
    &MultiTexParameterfEXT,
    &MultiTexParameterfvEXT,
    &MultiTexParameteriEXT,
    &MultiTexParameterivEXT,
    &MultiTexRenderbufferEXT,
    &MultiTexSubImage1DEXT,
    &MultiTexSubImage2DEXT,
    &MultiTexSubImage3DEXT,
    &NamedBufferData,
    &NamedBufferDataEXT,
    &NamedBufferPageCommitmentARB,
    &NamedBufferPageCommitmentEXT,
    &NamedBufferStorage,
    &NamedBufferStorageEXT,
    &NamedBufferSubData,
    &NamedBufferSubDataEXT,
    &NamedCopyBufferSubDataEXT,
    &NamedFramebufferDrawBuffer,
    &NamedFramebufferDrawBuffers,
    &NamedFramebufferParameteri,
    &NamedFramebufferParameteriEXT,
    &NamedFramebufferReadBuffer,
    &NamedFramebufferRenderbuffer,
    &NamedFramebufferRenderbufferEXT,
    &NamedFramebufferSampleLocationsfvARB,
    &NamedFramebufferSampleLocationsfvNV,
    &NamedFramebufferTexture,
    &NamedFramebufferTexture1DEXT,
    &NamedFramebufferTexture2DEXT,
    &NamedFramebufferTexture3DEXT,
    &NamedFramebufferTextureEXT,
    &NamedFramebufferTextureFaceEXT,
    &NamedFramebufferTextureLayer,
    &NamedFramebufferTextureLayerEXT,
    &NamedProgramLocalParameter4dEXT,
    &NamedProgramLocalParameter4dvEXT,
    &NamedProgramLocalParameter4fEXT,
    &NamedProgramLocalParameter4fvEXT,
    &NamedProgramLocalParameterI4iEXT,
    &NamedProgramLocalParameterI4ivEXT,
    &NamedProgramLocalParameterI4uiEXT,
    &NamedProgramLocalParameterI4uivEXT,
    &NamedProgramLocalParameters4fvEXT,
    &NamedProgramLocalParametersI4ivEXT,
    &NamedProgramLocalParametersI4uivEXT,
    &NamedProgramStringEXT,
    &NamedRenderbufferStorage,
    &NamedRenderbufferStorageEXT,
    &NamedRenderbufferStorageMultisample,
    &NamedRenderbufferStorageMultisampleCoverageEXT,
    &NamedRenderbufferStorageMultisampleEXT,
    &NamedStringARB,
    &NewList,
    &NewObjectBufferATI,
    &Normal3b,
    &Normal3bv,
    &Normal3d,
    &Normal3dv,
    &Normal3f,
    &Normal3fVertex3fSUN,
    &Normal3fVertex3fvSUN,
    &Normal3fv,
    &Normal3hNV,
    &Normal3hvNV,
    &Normal3i,
    &Normal3iv,
    &Normal3s,
    &Normal3sv,
    &Normal3xOES,
    &Normal3xvOES,
    &NormalFormatNV,
    &NormalP3ui,
    &NormalP3uiv,
    &NormalPointer,
    &NormalPointerEXT,
    &NormalPointerListIBM,
    &NormalPointervINTEL,
    &NormalStream3bATI,
    &NormalStream3bvATI,
    &NormalStream3dATI,
    &NormalStream3dvATI,
    &NormalStream3fATI,
    &NormalStream3fvATI,
    &NormalStream3iATI,
    &NormalStream3ivATI,
    &NormalStream3sATI,
    &NormalStream3svATI,
    &ObjectLabel,
    &ObjectPtrLabel,
    &ObjectPurgeableAPPLE,
    &ObjectUnpurgeableAPPLE,
    &Ortho,
    &OrthofOES,
    &OrthoxOES,
    &PNTrianglesfATI,
    &PNTrianglesiATI,
    &PassTexCoordATI,
    &PassThrough,
    &PassThroughxOES,
    &PatchParameterfv,
    &PatchParameteri,
    &PathColorGenNV,
    &PathCommandsNV,
    &PathCoordsNV,
    &PathCoverDepthFuncNV,
    &PathDashArrayNV,
    &PathFogGenNV,
    &PathGlyphIndexArrayNV,
    &PathGlyphIndexRangeNV,
    &PathGlyphRangeNV,
    &PathGlyphsNV,
    &PathMemoryGlyphIndexArrayNV,
    &PathParameterfNV,
    &PathParameterfvNV,
    &PathParameteriNV,
    &PathParameterivNV,
    &PathStencilDepthOffsetNV,
    &PathStencilFuncNV,
    &PathStringNV,
    &PathSubCommandsNV,
    &PathSubCoordsNV,
    &PathTexGenNV,
    &PauseTransformFeedback,
    &PauseTransformFeedbackNV,
    &PixelDataRangeNV,
    &PixelMapfv,
    &PixelMapuiv,
    &PixelMapusv,
    &PixelMapx,
    &PixelStoref,
    &PixelStorei,
    &PixelStorex,
    &PixelTexGenParameterfSGIS,
    &PixelTexGenParameterfvSGIS,
    &PixelTexGenParameteriSGIS,
    &PixelTexGenParameterivSGIS,
    &PixelTexGenSGIX,
    &PixelTransferf,
    &PixelTransferi,
    &PixelTransferxOES,
    &PixelTransformParameterfEXT,
    &PixelTransformParameterfvEXT,
    &PixelTransformParameteriEXT,
    &PixelTransformParameterivEXT,
    &PixelZoom,
    &PixelZoomxOES,
    &PointAlongPathNV,
    &PointParameterf,
    &PointParameterfARB,
    &PointParameterfEXT,
    &PointParameterfSGIS,
    &PointParameterfv,
    &PointParameterfvARB,
    &PointParameterfvEXT,
    &PointParameterfvSGIS,
    &PointParameteri,
    &PointParameteriNV,
    &PointParameteriv,
    &PointParameterivNV,
    &PointParameterxvOES,
    &PointSize,
    &PointSizexOES,
    &PollAsyncSGIX,
    &PollInstrumentsSGIX,
    &PolygonMode,
    &PolygonOffset,
    &PolygonOffsetClampEXT,
    &PolygonOffsetEXT,
    &PolygonOffsetxOES,
    &PolygonStipple,
    &PopAttrib,
    &PopClientAttrib,
    &PopDebugGroup,
    &PopGroupMarkerEXT,
    &PopMatrix,
    &PopName,
    &PresentFrameDualFillNV,
    &PresentFrameKeyedNV,
    &PrimitiveBoundingBoxARB,
    &PrimitiveRestartIndex,
    &PrimitiveRestartIndexNV,
    &PrimitiveRestartNV,
    &PrioritizeTextures,
    &PrioritizeTexturesEXT,
    &PrioritizeTexturesxOES,
    &ProgramBinary,
    &ProgramBufferParametersIivNV,
    &ProgramBufferParametersIuivNV,
    &ProgramBufferParametersfvNV,
    &ProgramEnvParameter4dARB,
    &ProgramEnvParameter4dvARB,
    &ProgramEnvParameter4fARB,
    &ProgramEnvParameter4fvARB,
    &ProgramEnvParameterI4iNV,
    &ProgramEnvParameterI4ivNV,
    &ProgramEnvParameterI4uiNV,
    &ProgramEnvParameterI4uivNV,
    &ProgramEnvParameters4fvEXT,
    &ProgramEnvParametersI4ivNV,
    &ProgramEnvParametersI4uivNV,
    &ProgramLocalParameter4dARB,
    &ProgramLocalParameter4dvARB,
    &ProgramLocalParameter4fARB,
    &ProgramLocalParameter4fvARB,
    &ProgramLocalParameterI4iNV,
    &ProgramLocalParameterI4ivNV,
    &ProgramLocalParameterI4uiNV,
    &ProgramLocalParameterI4uivNV,
    &ProgramLocalParameters4fvEXT,
    &ProgramLocalParametersI4ivNV,
    &ProgramLocalParametersI4uivNV,
    &ProgramNamedParameter4dNV,
    &ProgramNamedParameter4dvNV,
    &ProgramNamedParameter4fNV,
    &ProgramNamedParameter4fvNV,
    &ProgramParameter4dNV,
    &ProgramParameter4dvNV,
    &ProgramParameter4fNV,
    &ProgramParameter4fvNV,
    &ProgramParameteri,
    &ProgramParameteriARB,
    &ProgramParameteriEXT,
    &ProgramParameters4dvNV,
    &ProgramParameters4fvNV,
    &ProgramPathFragmentInputGenNV,
    &ProgramStringARB,
    &ProgramSubroutineParametersuivNV,
    &ProgramUniform1d,
    &ProgramUniform1dEXT,
    &ProgramUniform1dv,
    &ProgramUniform1dvEXT,
    &ProgramUniform1f,
    &ProgramUniform1fEXT,
    &ProgramUniform1fv,
    &ProgramUniform1fvEXT,
    &ProgramUniform1i,
    &ProgramUniform1i64ARB,
    &ProgramUniform1i64NV,
    &ProgramUniform1i64vARB,
    &ProgramUniform1i64vNV,
    &ProgramUniform1iEXT,
    &ProgramUniform1iv,
    &ProgramUniform1ivEXT,
    &ProgramUniform1ui,
    &ProgramUniform1ui64ARB,
    &ProgramUniform1ui64NV,
    &ProgramUniform1ui64vARB,
    &ProgramUniform1ui64vNV,
    &ProgramUniform1uiEXT,
    &ProgramUniform1uiv,
    &ProgramUniform1uivEXT,
    &ProgramUniform2d,
    &ProgramUniform2dEXT,
    &ProgramUniform2dv,
    &ProgramUniform2dvEXT,
    &ProgramUniform2f,
    &ProgramUniform2fEXT,
    &ProgramUniform2fv,
    &ProgramUniform2fvEXT,
    &ProgramUniform2i,
    &ProgramUniform2i64ARB,
    &ProgramUniform2i64NV,
    &ProgramUniform2i64vARB,
    &ProgramUniform2i64vNV,
    &ProgramUniform2iEXT,
    &ProgramUniform2iv,
    &ProgramUniform2ivEXT,
    &ProgramUniform2ui,
    &ProgramUniform2ui64ARB,
    &ProgramUniform2ui64NV,
    &ProgramUniform2ui64vARB,
    &ProgramUniform2ui64vNV,
    &ProgramUniform2uiEXT,
    &ProgramUniform2uiv,
    &ProgramUniform2uivEXT,
    &ProgramUniform3d,
    &ProgramUniform3dEXT,
    &ProgramUniform3dv,
    &ProgramUniform3dvEXT,
    &ProgramUniform3f,
    &ProgramUniform3fEXT,
    &ProgramUniform3fv,
    &ProgramUniform3fvEXT,
    &ProgramUniform3i,
    &ProgramUniform3i64ARB,
    &ProgramUniform3i64NV,
    &ProgramUniform3i64vARB,
    &ProgramUniform3i64vNV,
    &ProgramUniform3iEXT,
    &ProgramUniform3iv,
    &ProgramUniform3ivEXT,
    &ProgramUniform3ui,
    &ProgramUniform3ui64ARB,
    &ProgramUniform3ui64NV,
    &ProgramUniform3ui64vARB,
    &ProgramUniform3ui64vNV,
    &ProgramUniform3uiEXT,
    &ProgramUniform3uiv,
    &ProgramUniform3uivEXT,
    &ProgramUniform4d,
    &ProgramUniform4dEXT,
    &ProgramUniform4dv,
    &ProgramUniform4dvEXT,
    &ProgramUniform4f,
    &ProgramUniform4fEXT,
    &ProgramUniform4fv,
    &ProgramUniform4fvEXT,
    &ProgramUniform4i,
    &ProgramUniform4i64ARB,
    &ProgramUniform4i64NV,
    &ProgramUniform4i64vARB,
    &ProgramUniform4i64vNV,
    &ProgramUniform4iEXT,
    &ProgramUniform4iv,
    &ProgramUniform4ivEXT,
    &ProgramUniform4ui,
    &ProgramUniform4ui64ARB,
    &ProgramUniform4ui64NV,
    &ProgramUniform4ui64vARB,
    &ProgramUniform4ui64vNV,
    &ProgramUniform4uiEXT,
    &ProgramUniform4uiv,
    &ProgramUniform4uivEXT,
    &ProgramUniformHandleui64ARB,
    &ProgramUniformHandleui64NV,
    &ProgramUniformHandleui64vARB,
    &ProgramUniformHandleui64vNV,
    &ProgramUniformMatrix2dv,
    &ProgramUniformMatrix2dvEXT,
    &ProgramUniformMatrix2fv,
    &ProgramUniformMatrix2fvEXT,
    &ProgramUniformMatrix2x3dv,
    &ProgramUniformMatrix2x3dvEXT,
    &ProgramUniformMatrix2x3fv,
    &ProgramUniformMatrix2x3fvEXT,
    &ProgramUniformMatrix2x4dv,
    &ProgramUniformMatrix2x4dvEXT,
    &ProgramUniformMatrix2x4fv,
    &ProgramUniformMatrix2x4fvEXT,
    &ProgramUniformMatrix3dv,
    &ProgramUniformMatrix3dvEXT,
    &ProgramUniformMatrix3fv,
    &ProgramUniformMatrix3fvEXT,
    &ProgramUniformMatrix3x2dv,
    &ProgramUniformMatrix3x2dvEXT,
    &ProgramUniformMatrix3x2fv,
    &ProgramUniformMatrix3x2fvEXT,
    &ProgramUniformMatrix3x4dv,
    &ProgramUniformMatrix3x4dvEXT,
    &ProgramUniformMatrix3x4fv,
    &ProgramUniformMatrix3x4fvEXT,
    &ProgramUniformMatrix4dv,
    &ProgramUniformMatrix4dvEXT,
    &ProgramUniformMatrix4fv,
    &ProgramUniformMatrix4fvEXT,
    &ProgramUniformMatrix4x2dv,
    &ProgramUniformMatrix4x2dvEXT,
    &ProgramUniformMatrix4x2fv,
    &ProgramUniformMatrix4x2fvEXT,
    &ProgramUniformMatrix4x3dv,
    &ProgramUniformMatrix4x3dvEXT,
    &ProgramUniformMatrix4x3fv,
    &ProgramUniformMatrix4x3fvEXT,
    &ProgramUniformui64NV,
    &ProgramUniformui64vNV,
    &ProgramVertexLimitNV,
    &ProvokingVertex,
    &ProvokingVertexEXT,
    &PushAttrib,
    &PushClientAttrib,
    &PushClientAttribDefaultEXT,
    &PushDebugGroup,
    &PushGroupMarkerEXT,
    &PushMatrix,
    &PushName,
    &QueryCounter,
    &QueryMatrixxOES,
    &QueryObjectParameteruiAMD,
    &RasterPos2d,
    &RasterPos2dv,
    &RasterPos2f,
    &RasterPos2fv,
    &RasterPos2i,
    &RasterPos2iv,
    &RasterPos2s,
    &RasterPos2sv,
    &RasterPos2xOES,
    &RasterPos2xvOES,
    &RasterPos3d,
    &RasterPos3dv,
    &RasterPos3f,
    &RasterPos3fv,
    &RasterPos3i,
    &RasterPos3iv,
    &RasterPos3s,
    &RasterPos3sv,
    &RasterPos3xOES,
    &RasterPos3xvOES,
    &RasterPos4d,
    &RasterPos4dv,
    &RasterPos4f,
    &RasterPos4fv,
    &RasterPos4i,
    &RasterPos4iv,
    &RasterPos4s,
    &RasterPos4sv,
    &RasterPos4xOES,
    &RasterPos4xvOES,
    &RasterSamplesEXT,
    &ReadBuffer,
    &ReadInstrumentsSGIX,
    &ReadPixels,
    &ReadnPixels,
    &ReadnPixelsARB,
    &Rectd,
    &Rectdv,
    &Rectf,
    &Rectfv,
    &Recti,
    &Rectiv,
    &Rects,
    &Rectsv,
    &RectxOES,
    &RectxvOES,
    &ReferencePlaneSGIX,
    &ReleaseShaderCompiler,
    &RenderMode,
    &RenderbufferStorage,
    &RenderbufferStorageEXT,
    &RenderbufferStorageMultisample,
    &RenderbufferStorageMultisampleCoverageNV,
    &RenderbufferStorageMultisampleEXT,
    &ReplacementCodePointerSUN,
    &ReplacementCodeubSUN,
    &ReplacementCodeubvSUN,
    &ReplacementCodeuiColor3fVertex3fSUN,
    &ReplacementCodeuiColor3fVertex3fvSUN,
    &ReplacementCodeuiColor4fNormal3fVertex3fSUN,
    &ReplacementCodeuiColor4fNormal3fVertex3fvSUN,
    &ReplacementCodeuiColor4ubVertex3fSUN,
    &ReplacementCodeuiColor4ubVertex3fvSUN,
    &ReplacementCodeuiNormal3fVertex3fSUN,
    &ReplacementCodeuiNormal3fVertex3fvSUN,
    &ReplacementCodeuiSUN,
    &ReplacementCodeuiTexCoord2fColor4fNormal3fVertex3fSUN,
    &ReplacementCodeuiTexCoord2fColor4fNormal3fVertex3fvSUN,
    &ReplacementCodeuiTexCoord2fNormal3fVertex3fSUN,
    &ReplacementCodeuiTexCoord2fNormal3fVertex3fvSUN,
    &ReplacementCodeuiTexCoord2fVertex3fSUN,
    &ReplacementCodeuiTexCoord2fVertex3fvSUN,
    &ReplacementCodeuiVertex3fSUN,
    &ReplacementCodeuiVertex3fvSUN,
    &ReplacementCodeuivSUN,
    &ReplacementCodeusSUN,
    &ReplacementCodeusvSUN,
    &RequestResidentProgramsNV,
    &ResetHistogram,
    &ResetHistogramEXT,
    &ResetMinmax,
    &ResetMinmaxEXT,
    &ResizeBuffersMESA,
    &ResolveDepthValuesNV,
    &ResumeTransformFeedback,
    &ResumeTransformFeedbackNV,
    &Rotated,
    &Rotatef,
    &RotatexOES,
    &SampleCoverage,
    &SampleCoverageARB,
    &SampleMapATI,
    &SampleMaskEXT,
    &SampleMaskIndexedNV,
    &SampleMaskSGIS,
    &SampleMaski,
    &SamplePatternEXT,
    &SamplePatternSGIS,
    &SamplerParameterIiv,
    &SamplerParameterIuiv,
    &SamplerParameterf,
    &SamplerParameterfv,
    &SamplerParameteri,
    &SamplerParameteriv,
    &Scaled,
    &Scalef,
    &ScalexOES,
    &Scissor,
    &ScissorArrayv,
    &ScissorIndexed,
    &ScissorIndexedv,
    &SecondaryColor3b,
    &SecondaryColor3bEXT,
    &SecondaryColor3bv,
    &SecondaryColor3bvEXT,
    &SecondaryColor3d,
    &SecondaryColor3dEXT,
    &SecondaryColor3dv,
    &SecondaryColor3dvEXT,
    &SecondaryColor3f,
    &SecondaryColor3fEXT,
    &SecondaryColor3fv,
    &SecondaryColor3fvEXT,
    &SecondaryColor3hNV,
    &SecondaryColor3hvNV,
    &SecondaryColor3i,
    &SecondaryColor3iEXT,
    &SecondaryColor3iv,
    &SecondaryColor3ivEXT,
    &SecondaryColor3s,
    &SecondaryColor3sEXT,
    &SecondaryColor3sv,
    &SecondaryColor3svEXT,
    &SecondaryColor3ub,
    &SecondaryColor3ubEXT,
    &SecondaryColor3ubv,
    &SecondaryColor3ubvEXT,
    &SecondaryColor3ui,
    &SecondaryColor3uiEXT,
    &SecondaryColor3uiv,
    &SecondaryColor3uivEXT,
    &SecondaryColor3us,
    &SecondaryColor3usEXT,
    &SecondaryColor3usv,
    &SecondaryColor3usvEXT,
    &SecondaryColorFormatNV,
    &SecondaryColorP3ui,
    &SecondaryColorP3uiv,
    &SecondaryColorPointer,
    &SecondaryColorPointerEXT,
    &SecondaryColorPointerListIBM,
    &SelectBuffer,
    &SelectPerfMonitorCountersAMD,
    &SeparableFilter2D,
    &SeparableFilter2DEXT,
    &SetFenceAPPLE,
    &SetFenceNV,
    &SetFragmentShaderConstantATI,
    &SetInvariantEXT,
    &SetLocalConstantEXT,
    &SetMultisamplefvAMD,
    &ShadeModel,
    &ShaderBinary,
    &ShaderOp1EXT,
    &ShaderOp2EXT,
    &ShaderOp3EXT,
    &ShaderSource,
    &ShaderSourceARB,
    &ShaderStorageBlockBinding,
    &SharpenTexFuncSGIS,
    &SpriteParameterfSGIX,
    &SpriteParameterfvSGIX,
    &SpriteParameteriSGIX,
    &SpriteParameterivSGIX,
    &StartInstrumentsSGIX,
    &StateCaptureNV,
    &StencilClearTagEXT,
    &StencilFillPathInstancedNV,
    &StencilFillPathNV,
    &StencilFunc,
    &StencilFuncSeparate,
    &StencilFuncSeparateATI,
    &StencilMask,
    &StencilMaskSeparate,
    &StencilOp,
    &StencilOpSeparate,
    &StencilOpSeparateATI,
    &StencilOpValueAMD,
    &StencilStrokePathInstancedNV,
    &StencilStrokePathNV,
    &StencilThenCoverFillPathInstancedNV,
    &StencilThenCoverFillPathNV,
    &StencilThenCoverStrokePathInstancedNV,
    &StencilThenCoverStrokePathNV,
    &StopInstrumentsSGIX,
    &StringMarkerGREMEDY,
    &SubpixelPrecisionBiasNV,
    &SwizzleEXT,
    &SyncTextureINTEL,
    &TagSampleBufferSGIX,
    &Tangent3bEXT,
    &Tangent3bvEXT,
    &Tangent3dEXT,
    &Tangent3dvEXT,
    &Tangent3fEXT,
    &Tangent3fvEXT,
    &Tangent3iEXT,
    &Tangent3ivEXT,
    &Tangent3sEXT,
    &Tangent3svEXT,
    &TangentPointerEXT,
    &TbufferMask3DFX,
    &TessellationFactorAMD,
    &TessellationModeAMD,
    &TestFenceAPPLE,
    &TestFenceNV,
    &TestObjectAPPLE,
    &TexBuffer,
    &TexBufferARB,
    &TexBufferEXT,
    &TexBufferRange,
    &TexBumpParameterfvATI,
    &TexBumpParameterivATI,
    &TexCoord1bOES,
    &TexCoord1bvOES,
    &TexCoord1d,
    &TexCoord1dv,
    &TexCoord1f,
    &TexCoord1fv,
    &TexCoord1hNV,
    &TexCoord1hvNV,
    &TexCoord1i,
    &TexCoord1iv,
    &TexCoord1s,
    &TexCoord1sv,
    &TexCoord1xOES,
    &TexCoord1xvOES,
    &TexCoord2bOES,
    &TexCoord2bvOES,
    &TexCoord2d,
    &TexCoord2dv,
    &TexCoord2f,
    &TexCoord2fColor3fVertex3fSUN,
    &TexCoord2fColor3fVertex3fvSUN,
    &TexCoord2fColor4fNormal3fVertex3fSUN,
    &TexCoord2fColor4fNormal3fVertex3fvSUN,
    &TexCoord2fColor4ubVertex3fSUN,
    &TexCoord2fColor4ubVertex3fvSUN,
    &TexCoord2fNormal3fVertex3fSUN,
    &TexCoord2fNormal3fVertex3fvSUN,
    &TexCoord2fVertex3fSUN,
    &TexCoord2fVertex3fvSUN,
    &TexCoord2fv,
    &TexCoord2hNV,
    &TexCoord2hvNV,
    &TexCoord2i,
    &TexCoord2iv,
    &TexCoord2s,
    &TexCoord2sv,
    &TexCoord2xOES,
    &TexCoord2xvOES,
    &TexCoord3bOES,
    &TexCoord3bvOES,
    &TexCoord3d,
    &TexCoord3dv,
    &TexCoord3f,
    &TexCoord3fv,
    &TexCoord3hNV,
    &TexCoord3hvNV,
    &TexCoord3i,
    &TexCoord3iv,
    &TexCoord3s,
    &TexCoord3sv,
    &TexCoord3xOES,
    &TexCoord3xvOES,
    &TexCoord4bOES,
    &TexCoord4bvOES,
    &TexCoord4d,
    &TexCoord4dv,
    &TexCoord4f,
    &TexCoord4fColor4fNormal3fVertex4fSUN,
    &TexCoord4fColor4fNormal3fVertex4fvSUN,
    &TexCoord4fVertex4fSUN,
    &TexCoord4fVertex4fvSUN,
    &TexCoord4fv,
    &TexCoord4hNV,
    &TexCoord4hvNV,
    &TexCoord4i,
    &TexCoord4iv,
    &TexCoord4s,
    &TexCoord4sv,
    &TexCoord4xOES,
    &TexCoord4xvOES,
    &TexCoordFormatNV,
    &TexCoordP1ui,
    &TexCoordP1uiv,
    &TexCoordP2ui,
    &TexCoordP2uiv,
    &TexCoordP3ui,
    &TexCoordP3uiv,
    &TexCoordP4ui,
    &TexCoordP4uiv,
    &TexCoordPointer,
    &TexCoordPointerEXT,
    &TexCoordPointerListIBM,
    &TexCoordPointervINTEL,
    &TexEnvf,
    &TexEnvfv,
    &TexEnvi,
    &TexEnviv,
    &TexEnvxOES,
    &TexEnvxvOES,
    &TexFilterFuncSGIS,
    &TexGend,
    &TexGendv,
    &TexGenf,
    &TexGenfv,
    &TexGeni,
    &TexGeniv,
    &TexGenxOES,
    &TexGenxvOES,
    &TexImage1D,
    &TexImage2D,
    &TexImage2DMultisample,
    &TexImage2DMultisampleCoverageNV,
    &TexImage3D,
    &TexImage3DEXT,
    &TexImage3DMultisample,
    &TexImage3DMultisampleCoverageNV,
    &TexImage4DSGIS,
    &TexPageCommitmentARB,
    &TexParameterIiv,
    &TexParameterIivEXT,
    &TexParameterIuiv,
    &TexParameterIuivEXT,
    &TexParameterf,
    &TexParameterfv,
    &TexParameteri,
    &TexParameteriv,
    &TexParameterxOES,
    &TexParameterxvOES,
    &TexRenderbufferNV,
    &TexStorage1D,
    &TexStorage2D,
    &TexStorage2DMultisample,
    &TexStorage3D,
    &TexStorage3DMultisample,
    &TexStorageSparseAMD,
    &TexSubImage1D,
    &TexSubImage1DEXT,
    &TexSubImage2D,
    &TexSubImage2DEXT,
    &TexSubImage3D,
    &TexSubImage3DEXT,
    &TexSubImage4DSGIS,
    &TextureBarrier,
    &TextureBarrierNV,
    &TextureBuffer,
    &TextureBufferEXT,
    &TextureBufferRange,
    &TextureBufferRangeEXT,
    &TextureColorMaskSGIS,
    &TextureImage1DEXT,
    &TextureImage2DEXT,
    &TextureImage2DMultisampleCoverageNV,
    &TextureImage2DMultisampleNV,
    &TextureImage3DEXT,
    &TextureImage3DMultisampleCoverageNV,
    &TextureImage3DMultisampleNV,
    &TextureLightEXT,
    &TextureMaterialEXT,
    &TextureNormalEXT,
    &TexturePageCommitmentEXT,
    &TextureParameterIiv,
    &TextureParameterIivEXT,
    &TextureParameterIuiv,
    &TextureParameterIuivEXT,
    &TextureParameterf,
    &TextureParameterfEXT,
    &TextureParameterfv,
    &TextureParameterfvEXT,
    &TextureParameteri,
    &TextureParameteriEXT,
    &TextureParameteriv,
    &TextureParameterivEXT,
    &TextureRangeAPPLE,
    &TextureRenderbufferEXT,
    &TextureStorage1D,
    &TextureStorage1DEXT,
    &TextureStorage2D,
    &TextureStorage2DEXT,
    &TextureStorage2DMultisample,
    &TextureStorage2DMultisampleEXT,
    &TextureStorage3D,
    &TextureStorage3DEXT,
    &TextureStorage3DMultisample,
    &TextureStorage3DMultisampleEXT,
    &TextureStorageSparseAMD,
    &TextureSubImage1D,
    &TextureSubImage1DEXT,
    &TextureSubImage2D,
    &TextureSubImage2DEXT,
    &TextureSubImage3D,
    &TextureSubImage3DEXT,
    &TextureView,
    &TrackMatrixNV,
    &TransformFeedbackAttribsNV,
    &TransformFeedbackBufferBase,
    &TransformFeedbackBufferRange,
    &TransformFeedbackStreamAttribsNV,
    &TransformFeedbackVaryings,
    &TransformFeedbackVaryingsEXT,
    &TransformFeedbackVaryingsNV,
    &TransformPathNV,
    &Translated,
    &Translatef,
    &TranslatexOES,
    &Uniform1d,
    &Uniform1dv,
    &Uniform1f,
    &Uniform1fARB,
    &Uniform1fv,
    &Uniform1fvARB,
    &Uniform1i,
    &Uniform1i64ARB,
    &Uniform1i64NV,
    &Uniform1i64vARB,
    &Uniform1i64vNV,
    &Uniform1iARB,
    &Uniform1iv,
    &Uniform1ivARB,
    &Uniform1ui,
    &Uniform1ui64ARB,
    &Uniform1ui64NV,
    &Uniform1ui64vARB,
    &Uniform1ui64vNV,
    &Uniform1uiEXT,
    &Uniform1uiv,
    &Uniform1uivEXT,
    &Uniform2d,
    &Uniform2dv,
    &Uniform2f,
    &Uniform2fARB,
    &Uniform2fv,
    &Uniform2fvARB,
    &Uniform2i,
    &Uniform2i64ARB,
    &Uniform2i64NV,
    &Uniform2i64vARB,
    &Uniform2i64vNV,
    &Uniform2iARB,
    &Uniform2iv,
    &Uniform2ivARB,
    &Uniform2ui,
    &Uniform2ui64ARB,
    &Uniform2ui64NV,
    &Uniform2ui64vARB,
    &Uniform2ui64vNV,
    &Uniform2uiEXT,
    &Uniform2uiv,
    &Uniform2uivEXT,
    &Uniform3d,
    &Uniform3dv,
    &Uniform3f,
    &Uniform3fARB,
    &Uniform3fv,
    &Uniform3fvARB,
    &Uniform3i,
    &Uniform3i64ARB,
    &Uniform3i64NV,
    &Uniform3i64vARB,
    &Uniform3i64vNV,
    &Uniform3iARB,
    &Uniform3iv,
    &Uniform3ivARB,
    &Uniform3ui,
    &Uniform3ui64ARB,
    &Uniform3ui64NV,
    &Uniform3ui64vARB,
    &Uniform3ui64vNV,
    &Uniform3uiEXT,
    &Uniform3uiv,
    &Uniform3uivEXT,
    &Uniform4d,
    &Uniform4dv,
    &Uniform4f,
    &Uniform4fARB,
    &Uniform4fv,
    &Uniform4fvARB,
    &Uniform4i,
    &Uniform4i64ARB,
    &Uniform4i64NV,
    &Uniform4i64vARB,
    &Uniform4i64vNV,
    &Uniform4iARB,
    &Uniform4iv,
    &Uniform4ivARB,
    &Uniform4ui,
    &Uniform4ui64ARB,
    &Uniform4ui64NV,
    &Uniform4ui64vARB,
    &Uniform4ui64vNV,
    &Uniform4uiEXT,
    &Uniform4uiv,
    &Uniform4uivEXT,
    &UniformBlockBinding,
    &UniformBufferEXT,
    &UniformHandleui64ARB,
    &UniformHandleui64NV,
    &UniformHandleui64vARB,
    &UniformHandleui64vNV,
    &UniformMatrix2dv,
    &UniformMatrix2fv,
    &UniformMatrix2fvARB,
    &UniformMatrix2x3dv,
    &UniformMatrix2x3fv,
    &UniformMatrix2x4dv,
    &UniformMatrix2x4fv,
    &UniformMatrix3dv,
    &UniformMatrix3fv,
    &UniformMatrix3fvARB,
    &UniformMatrix3x2dv,
    &UniformMatrix3x2fv,
    &UniformMatrix3x4dv,
    &UniformMatrix3x4fv,
    &UniformMatrix4dv,
    &UniformMatrix4fv,
    &UniformMatrix4fvARB,
    &UniformMatrix4x2dv,
    &UniformMatrix4x2fv,
    &UniformMatrix4x3dv,
    &UniformMatrix4x3fv,
    &UniformSubroutinesuiv,
    &Uniformui64NV,
    &Uniformui64vNV,
    &UnlockArraysEXT,
    &UnmapBuffer,
    &UnmapBufferARB,
    &UnmapNamedBuffer,
    &UnmapNamedBufferEXT,
    &UnmapObjectBufferATI,
    &UnmapTexture2DINTEL,
    &UpdateObjectBufferATI,
    &UseProgram,
    &UseProgramObjectARB,
    &UseProgramStages,
    &UseShaderProgramEXT,
    &VDPAUFiniNV,
    &VDPAUGetSurfaceivNV,
    &VDPAUInitNV,
    &VDPAUIsSurfaceNV,
    &VDPAUMapSurfacesNV,
    &VDPAURegisterOutputSurfaceNV,
    &VDPAURegisterVideoSurfaceNV,
    &VDPAUSurfaceAccessNV,
    &VDPAUUnmapSurfacesNV,
    &VDPAUUnregisterSurfaceNV,
    &ValidateProgram,
    &ValidateProgramARB,
    &ValidateProgramPipeline,
    &VariantArrayObjectATI,
    &VariantPointerEXT,
    &VariantbvEXT,
    &VariantdvEXT,
    &VariantfvEXT,
    &VariantivEXT,
    &VariantsvEXT,
    &VariantubvEXT,
    &VariantuivEXT,
    &VariantusvEXT,
    &Vertex2bOES,
    &Vertex2bvOES,
    &Vertex2d,
    &Vertex2dv,
    &Vertex2f,
    &Vertex2fv,
    &Vertex2hNV,
    &Vertex2hvNV,
    &Vertex2i,
    &Vertex2iv,
    &Vertex2s,
    &Vertex2sv,
    &Vertex2xOES,
    &Vertex2xvOES,
    &Vertex3bOES,
    &Vertex3bvOES,
    &Vertex3d,
    &Vertex3dv,
    &Vertex3f,
    &Vertex3fv,
    &Vertex3hNV,
    &Vertex3hvNV,
    &Vertex3i,
    &Vertex3iv,
    &Vertex3s,
    &Vertex3sv,
    &Vertex3xOES,
    &Vertex3xvOES,
    &Vertex4bOES,
    &Vertex4bvOES,
    &Vertex4d,
    &Vertex4dv,
    &Vertex4f,
    &Vertex4fv,
    &Vertex4hNV,
    &Vertex4hvNV,
    &Vertex4i,
    &Vertex4iv,
    &Vertex4s,
    &Vertex4sv,
    &Vertex4xOES,
    &Vertex4xvOES,
    &VertexArrayAttribBinding,
    &VertexArrayAttribFormat,
    &VertexArrayAttribIFormat,
    &VertexArrayAttribLFormat,
    &VertexArrayBindVertexBufferEXT,
    &VertexArrayBindingDivisor,
    &VertexArrayColorOffsetEXT,
    &VertexArrayEdgeFlagOffsetEXT,
    &VertexArrayElementBuffer,
    &VertexArrayFogCoordOffsetEXT,
    &VertexArrayIndexOffsetEXT,
    &VertexArrayMultiTexCoordOffsetEXT,
    &VertexArrayNormalOffsetEXT,
    &VertexArrayParameteriAPPLE,
    &VertexArrayRangeAPPLE,
    &VertexArrayRangeNV,
    &VertexArraySecondaryColorOffsetEXT,
    &VertexArrayTexCoordOffsetEXT,
    &VertexArrayVertexAttribBindingEXT,
    &VertexArrayVertexAttribDivisorEXT,
    &VertexArrayVertexAttribFormatEXT,
    &VertexArrayVertexAttribIFormatEXT,
    &VertexArrayVertexAttribIOffsetEXT,
    &VertexArrayVertexAttribLFormatEXT,
    &VertexArrayVertexAttribLOffsetEXT,
    &VertexArrayVertexAttribOffsetEXT,
    &VertexArrayVertexBindingDivisorEXT,
    &VertexArrayVertexBuffer,
    &VertexArrayVertexBuffers,
    &VertexArrayVertexOffsetEXT,
    &VertexAttrib1d,
    &VertexAttrib1dARB,
    &VertexAttrib1dNV,
    &VertexAttrib1dv,
    &VertexAttrib1dvARB,
    &VertexAttrib1dvNV,
    &VertexAttrib1f,
    &VertexAttrib1fARB,
    &VertexAttrib1fNV,
    &VertexAttrib1fv,
    &VertexAttrib1fvARB,
    &VertexAttrib1fvNV,
    &VertexAttrib1hNV,
    &VertexAttrib1hvNV,
    &VertexAttrib1s,
    &VertexAttrib1sARB,
    &VertexAttrib1sNV,
    &VertexAttrib1sv,
    &VertexAttrib1svARB,
    &VertexAttrib1svNV,
    &VertexAttrib2d,
    &VertexAttrib2dARB,
    &VertexAttrib2dNV,
    &VertexAttrib2dv,
    &VertexAttrib2dvARB,
    &VertexAttrib2dvNV,
    &VertexAttrib2f,
    &VertexAttrib2fARB,
    &VertexAttrib2fNV,
    &VertexAttrib2fv,
    &VertexAttrib2fvARB,
    &VertexAttrib2fvNV,
    &VertexAttrib2hNV,
    &VertexAttrib2hvNV,
    &VertexAttrib2s,
    &VertexAttrib2sARB,
    &VertexAttrib2sNV,
    &VertexAttrib2sv,
    &VertexAttrib2svARB,
    &VertexAttrib2svNV,
    &VertexAttrib3d,
    &VertexAttrib3dARB,
    &VertexAttrib3dNV,
    &VertexAttrib3dv,
    &VertexAttrib3dvARB,
    &VertexAttrib3dvNV,
    &VertexAttrib3f,
    &VertexAttrib3fARB,
    &VertexAttrib3fNV,
    &VertexAttrib3fv,
    &VertexAttrib3fvARB,
    &VertexAttrib3fvNV,
    &VertexAttrib3hNV,
    &VertexAttrib3hvNV,
    &VertexAttrib3s,
    &VertexAttrib3sARB,
    &VertexAttrib3sNV,
    &VertexAttrib3sv,
    &VertexAttrib3svARB,
    &VertexAttrib3svNV,
    &VertexAttrib4Nbv,
    &VertexAttrib4NbvARB,
    &VertexAttrib4Niv,
    &VertexAttrib4NivARB,
    &VertexAttrib4Nsv,
    &VertexAttrib4NsvARB,
    &VertexAttrib4Nub,
    &VertexAttrib4NubARB,
    &VertexAttrib4Nubv,
    &VertexAttrib4NubvARB,
    &VertexAttrib4Nuiv,
    &VertexAttrib4NuivARB,
    &VertexAttrib4Nusv,
    &VertexAttrib4NusvARB,
    &VertexAttrib4bv,
    &VertexAttrib4bvARB,
    &VertexAttrib4d,
    &VertexAttrib4dARB,
    &VertexAttrib4dNV,
    &VertexAttrib4dv,
    &VertexAttrib4dvARB,
    &VertexAttrib4dvNV,
    &VertexAttrib4f,
    &VertexAttrib4fARB,
    &VertexAttrib4fNV,
    &VertexAttrib4fv,
    &VertexAttrib4fvARB,
    &VertexAttrib4fvNV,
    &VertexAttrib4hNV,
    &VertexAttrib4hvNV,
    &VertexAttrib4iv,
    &VertexAttrib4ivARB,
    &VertexAttrib4s,
    &VertexAttrib4sARB,
    &VertexAttrib4sNV,
    &VertexAttrib4sv,
    &VertexAttrib4svARB,
    &VertexAttrib4svNV,
    &VertexAttrib4ubNV,
    &VertexAttrib4ubv,
    &VertexAttrib4ubvARB,
    &VertexAttrib4ubvNV,
    &VertexAttrib4uiv,
    &VertexAttrib4uivARB,
    &VertexAttrib4usv,
    &VertexAttrib4usvARB,
    &VertexAttribArrayObjectATI,
    &VertexAttribBinding,
    &VertexAttribDivisor,
    &VertexAttribDivisorARB,
    &VertexAttribFormat,
    &VertexAttribFormatNV,
    &VertexAttribI1i,
    &VertexAttribI1iEXT,
    &VertexAttribI1iv,
    &VertexAttribI1ivEXT,
    &VertexAttribI1ui,
    &VertexAttribI1uiEXT,
    &VertexAttribI1uiv,
    &VertexAttribI1uivEXT,
    &VertexAttribI2i,
    &VertexAttribI2iEXT,
    &VertexAttribI2iv,
    &VertexAttribI2ivEXT,
    &VertexAttribI2ui,
    &VertexAttribI2uiEXT,
    &VertexAttribI2uiv,
    &VertexAttribI2uivEXT,
    &VertexAttribI3i,
    &VertexAttribI3iEXT,
    &VertexAttribI3iv,
    &VertexAttribI3ivEXT,
    &VertexAttribI3ui,
    &VertexAttribI3uiEXT,
    &VertexAttribI3uiv,
    &VertexAttribI3uivEXT,
    &VertexAttribI4bv,
    &VertexAttribI4bvEXT,
    &VertexAttribI4i,
    &VertexAttribI4iEXT,
    &VertexAttribI4iv,
    &VertexAttribI4ivEXT,
    &VertexAttribI4sv,
    &VertexAttribI4svEXT,
    &VertexAttribI4ubv,
    &VertexAttribI4ubvEXT,
    &VertexAttribI4ui,
    &VertexAttribI4uiEXT,
    &VertexAttribI4uiv,
    &VertexAttribI4uivEXT,
    &VertexAttribI4usv,
    &VertexAttribI4usvEXT,
    &VertexAttribIFormat,
    &VertexAttribIFormatNV,
    &VertexAttribIPointer,
    &VertexAttribIPointerEXT,
    &VertexAttribL1d,
    &VertexAttribL1dEXT,
    &VertexAttribL1dv,
    &VertexAttribL1dvEXT,
    &VertexAttribL1i64NV,
    &VertexAttribL1i64vNV,
    &VertexAttribL1ui64ARB,
    &VertexAttribL1ui64NV,
    &VertexAttribL1ui64vARB,
    &VertexAttribL1ui64vNV,
    &VertexAttribL2d,
    &VertexAttribL2dEXT,
    &VertexAttribL2dv,
    &VertexAttribL2dvEXT,
    &VertexAttribL2i64NV,
    &VertexAttribL2i64vNV,
    &VertexAttribL2ui64NV,
    &VertexAttribL2ui64vNV,
    &VertexAttribL3d,
    &VertexAttribL3dEXT,
    &VertexAttribL3dv,
    &VertexAttribL3dvEXT,
    &VertexAttribL3i64NV,
    &VertexAttribL3i64vNV,
    &VertexAttribL3ui64NV,
    &VertexAttribL3ui64vNV,
    &VertexAttribL4d,
    &VertexAttribL4dEXT,
    &VertexAttribL4dv,
    &VertexAttribL4dvEXT,
    &VertexAttribL4i64NV,
    &VertexAttribL4i64vNV,
    &VertexAttribL4ui64NV,
    &VertexAttribL4ui64vNV,
    &VertexAttribLFormat,
    &VertexAttribLFormatNV,
    &VertexAttribLPointer,
    &VertexAttribLPointerEXT,
    &VertexAttribP1ui,
    &VertexAttribP1uiv,
    &VertexAttribP2ui,
    &VertexAttribP2uiv,
    &VertexAttribP3ui,
    &VertexAttribP3uiv,
    &VertexAttribP4ui,
    &VertexAttribP4uiv,
    &VertexAttribParameteriAMD,
    &VertexAttribPointer,
    &VertexAttribPointerARB,
    &VertexAttribPointerNV,
    &VertexAttribs1dvNV,
    &VertexAttribs1fvNV,
    &VertexAttribs1hvNV,
    &VertexAttribs1svNV,
    &VertexAttribs2dvNV,
    &VertexAttribs2fvNV,
    &VertexAttribs2hvNV,
    &VertexAttribs2svNV,
    &VertexAttribs3dvNV,
    &VertexAttribs3fvNV,
    &VertexAttribs3hvNV,
    &VertexAttribs3svNV,
    &VertexAttribs4dvNV,
    &VertexAttribs4fvNV,
    &VertexAttribs4hvNV,
    &VertexAttribs4svNV,
    &VertexAttribs4ubvNV,
    &VertexBindingDivisor,
    &VertexBlendARB,
    &VertexBlendEnvfATI,
    &VertexBlendEnviATI,
    &VertexFormatNV,
    &VertexP2ui,
    &VertexP2uiv,
    &VertexP3ui,
    &VertexP3uiv,
    &VertexP4ui,
    &VertexP4uiv,
    &VertexPointer,
    &VertexPointerEXT,
    &VertexPointerListIBM,
    &VertexPointervINTEL,
    &VertexStream1dATI,
    &VertexStream1dvATI,
    &VertexStream1fATI,
    &VertexStream1fvATI,
    &VertexStream1iATI,
    &VertexStream1ivATI,
    &VertexStream1sATI,
    &VertexStream1svATI,
    &VertexStream2dATI,
    &VertexStream2dvATI,
    &VertexStream2fATI,
    &VertexStream2fvATI,
    &VertexStream2iATI,
    &VertexStream2ivATI,
    &VertexStream2sATI,
    &VertexStream2svATI,
    &VertexStream3dATI,
    &VertexStream3dvATI,
    &VertexStream3fATI,
    &VertexStream3fvATI,
    &VertexStream3iATI,
    &VertexStream3ivATI,
    &VertexStream3sATI,
    &VertexStream3svATI,
    &VertexStream4dATI,
    &VertexStream4dvATI,
    &VertexStream4fATI,
    &VertexStream4fvATI,
    &VertexStream4iATI,
    &VertexStream4ivATI,
    &VertexStream4sATI,
    &VertexStream4svATI,
    &VertexWeightPointerEXT,
    &VertexWeightfEXT,
    &VertexWeightfvEXT,
    &VertexWeighthNV,
    &VertexWeighthvNV,
    &VideoCaptureNV,
    &VideoCaptureStreamParameterdvNV,
    &VideoCaptureStreamParameterfvNV,
    &VideoCaptureStreamParameterivNV,
    &Viewport,
    &ViewportArrayv,
    &ViewportIndexedf,
    &ViewportIndexedfv,
    &WaitSync,
    &WeightPathsNV,
    &WeightPointerARB,
    &WeightbvARB,
    &WeightdvARB,
    &WeightfvARB,
    &WeightivARB,
    &WeightsvARB,
    &WeightubvARB,
    &WeightuivARB,
    &WeightusvARB,
    &WindowPos2d,
    &WindowPos2dARB,
    &WindowPos2dMESA,
    &WindowPos2dv,
    &WindowPos2dvARB,
    &WindowPos2dvMESA,
    &WindowPos2f,
    &WindowPos2fARB,
    &WindowPos2fMESA,
    &WindowPos2fv,
    &WindowPos2fvARB,
    &WindowPos2fvMESA,
    &WindowPos2i,
    &WindowPos2iARB,
    &WindowPos2iMESA,
    &WindowPos2iv,
    &WindowPos2ivARB,
    &WindowPos2ivMESA,
    &WindowPos2s,
    &WindowPos2sARB,
    &WindowPos2sMESA,
    &WindowPos2sv,
    &WindowPos2svARB,
    &WindowPos2svMESA,
    &WindowPos3d,
    &WindowPos3dARB,
    &WindowPos3dMESA,
    &WindowPos3dv,
    &WindowPos3dvARB,
    &WindowPos3dvMESA,
    &WindowPos3f,
    &WindowPos3fARB,
    &WindowPos3fMESA,
    &WindowPos3fv,
    &WindowPos3fvARB,
    &WindowPos3fvMESA,
    &WindowPos3i,
    &WindowPos3iARB,
    &WindowPos3iMESA,
    &WindowPos3iv,
    &WindowPos3ivARB,
    &WindowPos3ivMESA,
    &WindowPos3s,
    &WindowPos3sARB,
    &WindowPos3sMESA,
    &WindowPos3sv,
    &WindowPos3svARB,
    &WindowPos3svMESA,
    &WindowPos4dMESA,
    &WindowPos4dvMESA,
    &WindowPos4fMESA,
    &WindowPos4fvMESA,
    &WindowPos4iMESA,
    &WindowPos4ivMESA,
    &WindowPos4sMESA,
    &WindowPos4svMESA,
    &WriteMaskEXT
}};

} // namespace glbinding
