/*******************************************************************/
/*                                                                 */
/*                      ADOBE CONFIDENTIAL                         */
/*                   _ _ _ _ _ _ _ _ _ _ _ _ _                     */
/*                                                                 */
/* Copyright 2007-2023 Adobe Inc.                                  */
/* All Rights Reserved.                                            */
/*                                                                 */
/* NOTICE:  All information contained herein is, and remains the   */
/* property of Adobe Inc. and its suppliers, if                    */
/* any.  The intellectual and technical concepts contained         */
/* herein are proprietary to Adobe Inc. and its                    */
/* suppliers and may be covered by U.S. and Foreign Patents,       */
/* patents in process, and are protected by trade secret or        */
/* copyright law.  Dissemination of this information or            */
/* reproduction of this material is strictly forbidden unless      */
/* prior written permission is obtained from Adobe Inc.            */
/* Incorporated.                                                   */
/*                                                                 */
/*******************************************************************/

/*******************
Important Notice 06/02/2020
OpenGL was deprecated on macOS 10.14 with Apple asking all developers to move to using the Metal framework.
As such we are recommending cross-platform plugins are no longer developed directly against OpenGL.
For GPU-based plugin development, please refer to the SDK_Invert_ProcAmp SDK sample on how to use GPU rendering with the plugin SDK.
This plugin is kept only for reference.
*******************/

/*	GL_base.cpp

	This file invokes the basic OpenGL framework, keeping in mind that
	you would render to a hidden surface (FBO) and read-back into an AE frame
	
	Revision history: 

	1.0 Win and Mac versions use the same base files.	anindyar	7/4/2007
	1.1 Completely re-written for OGL 3.3 and threads	aparente	7/1/2015
*/

#include "GL_base.h"

#include <glbinding/callbacks.h>
#include <glbinding/Meta.h>
#include <glbinding/ContextInfo.h>
#include <glbinding/Version.h>
#include "glbinding/Binding.h"
#include <glbinding/AbstractFunction.h>

#include <stdlib.h>
#include <atomic>
#include <sstream>
#include <iostream>

using namespace gl33core;

namespace AESDK_OpenGL
{

	namespace {
		
		void InitializeOpenGLBindings()
		{
			glbinding::Binding::initialize(false);
			
			// tracing (optional, disabled)
#if 0
			glbinding::setCallbackMask(glbinding::CallbackMask::After | glbinding::CallbackMask::ParametersAndReturnValue);
			
			glbinding::setAfterCallback([](const glbinding::FunctionCall & call) {
				std::cout << call.function.name() << "(";
				
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
#endif
			
#ifdef _DEBUG
			// error checking (debug only)
			glbinding::setCallbackMaskExcept(glbinding::CallbackMask::After, { "glGetError" });
			glbinding::setAfterCallback([](const glbinding::FunctionCall &)
										{
											gl::GLenum error = glGetError();
											if (error != GL_NO_ERROR) {
												std::cout << "error: " << std::hex << error << std::endl; // breakpoint here!
											}
										});
#endif
			
			std::cout << std::endl
				<< "OpenGL Version:  " << glbinding::ContextInfo::version() << std::endl
				<< "OpenGL Vendor:   " << glbinding::ContextInfo::vendor() << std::endl
				<< "OpenGL Renderer: " << glbinding::ContextInfo::renderer() << std::endl
				<< "OpenGL Revision: " << glbinding::Meta::glRevision() << " (gl.xml)" << std::endl << std::endl;
		}
		
	#ifdef AE_OS_MAC
		class ScopedAutoreleasePool {
			
		public:
			
			ScopedAutoreleasePool() : pool_(nil)
			{
				pool_ = [[NSAutoreleasePool alloc] init];
			}
			
			~ScopedAutoreleasePool()
			{
				[pool_ drain];
				pool_ = nil;
			}
			
		private:
			NSAutoreleasePool* pool_;
		};
		
		void makeCurrentFlush(CGLContextObj newRC)
		{
			// workaround for WATSONBUG 1541765
			// Windows automagically inserts a flush call on the old context before actually
			// changing over to the new context
			// Here we're doing the same thing on the mac manually, it also fixes the memory leak in the above bug.
			if (newRC != CGLGetCurrentContext())
			{
				GLint nCurrentFBO;
				gl::GLenum status;
				glGetIntegerv(GL_FRAMEBUFFER_BINDING, reinterpret_cast<GLint *>(&nCurrentFBO));
				if (nCurrentFBO == 0
					|| (GL_FRAMEBUFFER_COMPLETE== (status = glCheckFramebufferStatus(GL_FRAMEBUFFER)))) {
					glFlush();
				}
				/*const CGLError cglError = */CGLSetCurrentContext(newRC);
			}
		}

		NSOpenGLContext* createNSContext(NSOpenGLContext* pNSShare, CGLContextObj& rc)
		{
			// Cocoa requires that a AutorelasePool be current for its Garbage Collection purposes
			// See NSAutoreleasePool API reference
			ScopedAutoreleasePool pool;
			
			NSOpenGLPixelFormatAttribute    aAttribs[64];
			int nIndex= -1;
			
			//aAttribs[++nIndex] = NSOpenGLPFAMPSafe;
			aAttribs[++nIndex] = NSOpenGLPFAClosestPolicy;
			aAttribs[++nIndex] = NSOpenGLPFANoRecovery;

			// color
			aAttribs[++nIndex] = NSOpenGLPFAAlphaSize;
			aAttribs[++nIndex] = 0;
			aAttribs[++nIndex] = NSOpenGLPFAColorSize;
			aAttribs[++nIndex] = 24;
						
			// stencil + depth
			aAttribs[++nIndex] = NSOpenGLPFAStencilSize;
			aAttribs[++nIndex] = 0;
			aAttribs[++nIndex] = NSOpenGLPFADepthSize;
			aAttribs[++nIndex] = 0;
			
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1070
			aAttribs[++nIndex] = NSOpenGLPFAOpenGLProfile;
			aAttribs[++nIndex] = NSOpenGLProfileVersion3_2Core;
#elif MAC_OS_X_VERSION_MAX_ALLOWED >= 101000
			aAttribs[++nIndex] = NSOpenGLPFAOpenGLProfile;
			aAttribs[++nIndex] = NSOpenGLProfileVersion4_1Core;
#endif /*MAC_OS_X_VERSION_MAX_ALLOWED*/
			
			aAttribs[++nIndex] = static_cast<NSOpenGLPixelFormatAttribute>(0);
			
			// create the context from the pixel format
			NSOpenGLContext* oContext = NULL;
			
			NSOpenGLPixelFormat* format = [[NSOpenGLPixelFormat alloc] initWithAttributes:aAttribs];

			oContext = [[NSOpenGLContext alloc] initWithFormat:format shareContext:pNSShare];
			rc = reinterpret_cast<CGLContextObj> ([oContext CGLContextObj]);
			
			[format release];
			return oContext;
		}

	#endif

	#ifdef AE_OS_WIN
		typedef const char* (APIENTRY * PFNWGLGETEXTENSIONSSTRINGARBPROC) (HDC hdc);
		typedef HGLRC(APIENTRY * PFNWGLCREATECONTEXTATTRIBSARBPROC) (HDC hDC, HGLRC hShareContext, const int* attribList);

		PFNWGLGETEXTENSIONSSTRINGARBPROC _wcgmGetExtensionsStringARB = nullptr;
		PFNWGLCREATECONTEXTATTRIBSARBPROC _wcgmCreateContextAttribsARB = nullptr;

		HWND CreateInternalWindow(std::string& className)
		{
			MSG        uMsg;

			::memset(&uMsg, 0, sizeof(uMsg));

			// very tricky, windows needs a unique class name for each window instance
			// to derive a unique name, a pointer to "this" is used
			static std::atomic_int S_cnt;
			std::stringstream ss;
			ss << "AESDK_OpenGL_Win_Class" << S_cnt.load();
			S_cnt++;
			className = ss.str();

			WNDCLASSEX winClass;
			::memset(&winClass, 0, sizeof(winClass));
			winClass.lpszClassName = className.c_str();
			winClass.cbSize = sizeof(WNDCLASSEX);
			winClass.style = CS_HREDRAW | CS_VREDRAW;
			winClass.lpfnWndProc = ::DefWindowProc;
			winClass.hInstance = GetModuleHandle(NULL);
			winClass.hCursor = ::LoadCursor(NULL, IDC_ARROW);
			winClass.hbrBackground = (HBRUSH)::GetStockObject(BLACK_BRUSH);

			if (!(::RegisterClassEx(&winClass))) {
				GL_CHECK(AESDK_OpenGL_OS_Load_Err);
			}

			HWND hwnd = ::CreateWindowEx(WS_EX_APPWINDOW | WS_EX_WINDOWEDGE,
				className.c_str(),
				"OpenGL-using FBOs in AE",
				0, 0,
				0, 8, 8,
				NULL,
				NULL,
				NULL,
				NULL);

			if (hwnd == NULL) {
				GL_CHECK(AESDK_OpenGL_OS_Load_Err);
			}

			return hwnd;
		}

		HGLRC CreatePlatformContext(HDC hdc, HGLRC sharedHRC=nullptr)
		{
			GLuint PixelFormat;
			PIXELFORMATDESCRIPTOR pfd;
			::ZeroMemory(&pfd, sizeof(pfd));

			pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
			pfd.nVersion = 1;
			pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL; // | PFD_DOUBLEBUFFER;
			pfd.iPixelType = PFD_TYPE_RGBA;
			pfd.cColorBits = 32;
			pfd.cDepthBits = 8;

			PixelFormat = ChoosePixelFormat(hdc, &pfd);
			if (PixelFormat == 0) {
				GL_CHECK(AESDK_OpenGL_OS_Load_Err);
			}
			if (!SetPixelFormat(hdc, PixelFormat, &pfd)) {
				GL_CHECK(AESDK_OpenGL_OS_Load_Err);
			}

			HGLRC hrc = nullptr;

			if (!sharedHRC) {
				hrc = wglCreateContext(hdc);

				// root OGL context
				if (!wglMakeCurrent(hdc, hrc)) {
					DWORD dwLastErr(GetLastError());
					GL_CHECK(AESDK_OpenGL_OS_Load_Err);
				}

				_wcgmGetExtensionsStringARB = (PFNWGLGETEXTENSIONSSTRINGARBPROC)wglGetProcAddress("wglGetExtensionsStringARB");
				if (!_wcgmGetExtensionsStringARB) {
					GL_CHECK(AESDK_OpenGL_OS_Load_Err);
				}

				std::string sExtensionsWGL = (const char *)_wcgmGetExtensionsStringARB(hdc);
				if (strstr(sExtensionsWGL.c_str(), "WGL_ARB_create_context") != 0) {
					_wcgmCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");
				}

				if (!_wcgmCreateContextAttribsARB) {
					GL_CHECK(AESDK_OpenGL_OS_Load_Err);
				}

			} else {
				// sub (shared) context
				#ifndef WGL_CONTEXT_MAJOR_VERSION_ARB
				#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
				#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
				#endif

				// create an OpenGL 3.3 context
				int attribList[] =
				{
					WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
					WGL_CONTEXT_MINOR_VERSION_ARB, 3,
					//WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB | WGL_CONTEXT_DEBUG_BIT_ARB,
					//WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
					//WGL_CONTEXT_DEBUG_BIT_ARB
					0, 0
				};

				hrc = _wcgmCreateContextAttribsARB(hdc, 0, &attribList[0]);
				if (!hrc) {
					GL_CHECK(AESDK_OpenGL_OS_Load_Err);
				}

				if (!wglMakeCurrent(hdc, hrc)) {
					DWORD dwLastErr(GetLastError());
					GL_CHECK(AESDK_OpenGL_OS_Load_Err);
				}

				if (!wglShareLists(sharedHRC, hrc)) {
					GL_CHECK(AESDK_OpenGL_OS_Load_Err);
				}
			}

			return hrc;
		}
	#endif

		// VBO quad
		GLuint CreateQuad(u_int16 widthL, u_int16 heightL)
		{
			// X, Y, X, U, V
			float positions[] = {
				0,				0,				0.0f, 0.0f, 0.0f, // A
				(float)widthL,	0,				0.0f, 1.0f, 0.0f, // B
				0,				(float)heightL, 0.0f, 0.0f, 1.0f, // C
				(float)widthL,	(float)heightL, 0.0f, 1.0f, 1.0f, // D
			};

			GLuint vbo;
			glGenBuffers(1, &vbo);
			glBindBuffer(GL_ARRAY_BUFFER, vbo);
			glBufferData(GL_ARRAY_BUFFER, sizeof(positions), positions, GL_STATIC_DRAW);

			return vbo;
		}

	} // namespace anonymous

/*
 * SaveRestoreOGLContext
 */

SaveRestoreOGLContext::SaveRestoreOGLContext()
{
#ifdef AE_OS_WIN
	h_RC = wglGetCurrentContext();
	h_DC = wglGetCurrentDC();
#endif
#ifdef AE_OS_MAC
	ScopedAutoreleasePool pool;
	pNSOpenGLContext_ = [NSOpenGLContext currentContext];
	o_RC = CGLGetCurrentContext();
#endif
}

SaveRestoreOGLContext::~SaveRestoreOGLContext()
{
#ifdef AE_OS_WIN
	if (h_RC != wglGetCurrentContext() || h_DC != wglGetCurrentDC())
	{
		if (!wglMakeCurrent(h_DC, h_RC))
		{
			DWORD dwLastErr(GetLastError());
			// complain
		}
	}
#endif
#ifdef AE_OS_MAC
	ScopedAutoreleasePool pool;
	if (pNSOpenGLContext_ != [NSOpenGLContext currentContext] && pNSOpenGLContext_)
	{
		[pNSOpenGLContext_ makeCurrentContext];
	}
	makeCurrentFlush(o_RC);
#endif
}

/*
 * AESDK_OpenGL_EffectCommonData
 */

AESDK_OpenGL_EffectCommonData::AESDK_OpenGL_EffectCommonData() :
	mInitialized(false)
#ifdef AE_OS_WIN
	, mHWnd(0), mHDC(0), mHRC(0)
#endif
#ifdef AE_OS_MAC
	, mRC(0), mNSOpenGLContext(0)
#endif
{
}

AESDK_OpenGL_EffectCommonData::~AESDK_OpenGL_EffectCommonData()
{
#ifdef AE_OS_WIN
	if (mHRC != NULL)
	{
		if (!wglMakeCurrent(NULL, NULL)) {
			DWORD dwLastErr(GetLastError());
		}
		if (!wglDeleteContext(mHRC)) {
			DWORD dwLastErr(GetLastError());
		}
		mHRC = NULL;
	}

	if (mHDC != NULL) {
		ReleaseDC(mHWnd, mHDC);
		mHDC = NULL;
	}
	::UnregisterClass(mClassName.c_str(), NULL);
#elif defined(AE_OS_MAC)
	[mNSOpenGLContext release];
#endif
}

void AESDK_OpenGL_EffectCommonData::SetPluginContext()
{
#ifdef AE_OS_MAC
	ScopedAutoreleasePool pool;
	if (mNSOpenGLContext) {
		[mNSOpenGLContext makeCurrentContext];
	}
	makeCurrentFlush(mRC);
#elif defined (AE_OS_WIN)
	wglMakeCurrent(mHDC, mHRC);
#endif

	glbinding::Binding::useCurrentContext();
}


/*
* AESDK_OpenGL_EffectRenderData
*/

AESDK_OpenGL_EffectRenderData::AESDK_OpenGL_EffectRenderData() :
	mFrameBufferSu(0),
	mColorRenderBufferSu(0),
	mRenderBufferWidthSu(0),
	mRenderBufferHeightSu(0),
	mProgramObjSu(0),
	mProgramObj2Su(0),
	mOutputFrameTexture(0),
	vao(0),
	quad(0)
{
}

AESDK_OpenGL_EffectRenderData::~AESDK_OpenGL_EffectRenderData()
{
	SetPluginContext();

	//local OpenGL resource un-loading
	if (mOutputFrameTexture) {
		glDeleteTextures(1, &mOutputFrameTexture);
	}

	//common OpenGL resource unloading
	if (mProgramObjSu) {
		glDeleteProgram(mProgramObjSu);
	}
	if (mProgramObj2Su) {
		glDeleteProgram(mProgramObj2Su);
	}

	//release framebuffer resources
	if (mFrameBufferSu) {
		glDeleteFramebuffers(1, &mFrameBufferSu);
	}
	if (mColorRenderBufferSu) {
		glDeleteRenderbuffers(1, &mColorRenderBufferSu);
	}

	if (vao) {
		glDeleteBuffers(1, &quad);
		glDeleteVertexArrays(1, &vao);
	}


}

/*
** OS Specific windowing context creation - essential for creating the OpenGL drawing context
*/
void AESDK_OpenGL_Startup(AESDK_OpenGL_EffectCommonData& inData, const AESDK_OpenGL_EffectCommonData* inRootContext)
{
#ifdef AE_OS_WIN
	if (!inRootContext) {
		inData.mHWnd = CreateInternalWindow(inData.mClassName);
		inData.mHDC = ::GetDC(inData.mHWnd);
		inData.mHRC = CreatePlatformContext(inData.mHDC);
	}
	else {
		inData.mHWnd = CreateInternalWindow(inData.mClassName);
		inData.mHDC = ::GetDC(inData.mHWnd);
		inData.mHRC = CreatePlatformContext(inData.mHDC, inRootContext->mHRC);
	}
	wglMakeCurrent(inData.mHDC, inData.mHRC);
#elif defined(AE_OS_MAC)
	if (!inRootContext) {
		inData.mNSOpenGLContext = createNSContext(nullptr, inData.mRC);
	}
	else {
		inData.mNSOpenGLContext = createNSContext(inRootContext->mNSOpenGLContext, inData.mRC);
	}
	[inData.mNSOpenGLContext makeCurrentContext];
#endif
	
	InitializeOpenGLBindings();
	
	inData.mExtensions = glbinding::ContextInfo::extensions();
}

/*
** OS Specific unloading
*/
void AESDK_OpenGL_Shutdown(AESDK_OpenGL_EffectCommonData& inData)
{
}

/*
** OpenGL resource loading
*/
void AESDK_OpenGL_InitResources(AESDK_OpenGL_EffectRenderData& inData, u_short inBufferWidth, u_short inBufferHeight, const std::string& resourcePath)
{
	bool sizeChangedB = inData.mRenderBufferWidthSu != inBufferWidth || inData.mRenderBufferHeightSu != inBufferHeight;
	
	inData.mRenderBufferWidthSu = inBufferWidth;
	inData.mRenderBufferHeightSu = inBufferHeight;

	if (sizeChangedB) {
		glBindTexture(GL_TEXTURE_2D, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		//release framebuffer resources
		if (inData.mFrameBufferSu) {
			glDeleteFramebuffers(1, &inData.mFrameBufferSu);
			inData.mFrameBufferSu = 0;
		}
		if (inData.mColorRenderBufferSu) {
			glDeleteRenderbuffers(1, &inData.mColorRenderBufferSu);
			inData.mColorRenderBufferSu = 0;
		}
		if (inData.mOutputFrameTexture) {
			glDeleteTextures(1, &inData.mOutputFrameTexture);
			inData.mOutputFrameTexture = 0;
		}

		if (inData.vao) {
			glDeleteBuffers(1, &inData.quad);
			glDeleteVertexArrays(1, &inData.vao);
			inData.quad = 0;
			inData.vao = 0;
		}

	}

	if (inData.vao == 0) {
		// create a quad
		glGenVertexArrays(1, &inData.vao);
		glBindVertexArray(inData.vao);
		inData.quad = CreateQuad(inData.mRenderBufferWidthSu, inData.mRenderBufferHeightSu);
		glBindVertexArray(0);
	}

	// Create a frame-buffer object and bind it...
	if (inData.mFrameBufferSu == 0) {
		//create the color buffer to render to
		glGenFramebuffers(1, &inData.mFrameBufferSu);
		glBindFramebuffer(GL_FRAMEBUFFER, inData.mFrameBufferSu);
	
		//now create the color render buffer
		glGenRenderbuffers(1, &inData.mColorRenderBufferSu);
		glBindRenderbuffer(GL_RENDERBUFFER, inData.mColorRenderBufferSu);
	
		// attach renderbuffer to framebuffer
		//glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA, inData.mRenderBufferWidthSu, inData.mRenderBufferHeightSu);
		//glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, inData.mColorRenderBufferSu);
	}

	//GLator effect specific OpenGL resource loading
	//create an empty texture for the input surface
	if (inData.mOutputFrameTexture == 0) {
		glGenTextures(1, &inData.mOutputFrameTexture);
		glBindTexture(GL_TEXTURE_2D, inData.mOutputFrameTexture);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLint)GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLint)GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (GLint)GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (GLint)GL_CLAMP_TO_EDGE);

		glTexImage2D(GL_TEXTURE_2D, 0, (GLint)GL_RGBA32F, inData.mRenderBufferWidthSu, inData.mRenderBufferHeightSu, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	}

	if (inData.mProgramObjSu == 0) {
		//initialize and compile the shader objects
		inData.mProgramObjSu = AESDK_OpenGL_InitShader(
			resourcePath + "vertex_shader.vert",
			resourcePath + "fragment_shader.frag");
	}
	if (inData.mProgramObj2Su == 0) {
		//initialize and compile the shader objects
		inData.mProgramObj2Su = AESDK_OpenGL_InitShader(
			resourcePath + "vertex_shader.vert",
			resourcePath + "fragment_shader2.frag");
	}
}

/*
** Making the FBO surface ready to render
*/
void AESDK_OpenGL_MakeReadyToRender(AESDK_OpenGL_EffectRenderData& inData, gl::GLuint textureHandle)
{
	// Bind the frame-buffer object and attach to it a render-buffer object set up as a depth-buffer.
	glBindFramebuffer(GL_FRAMEBUFFER, inData.mFrameBufferSu);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureHandle, 0);

	// Set the render target - primary surface
	glDrawBuffer(GL_COLOR_ATTACHMENT0);
		
	if( CheckFramebufferStatus() != std::string("OK"))
		GL_CHECK(AESDK_OpenGL_Res_Load_Err);
}

/*
** Initializing the Shader objects
*/
gl::GLuint AESDK_OpenGL_InitShader(std::string inVertexShaderFile, std::string inFragmentShaderFile)
{
	const char *vertexShaderStringsP[1];
	const char *fragmentShaderStringsP[1];
	GLint vertCompiledB;
	GLint fragCompiledB;
	GLint linkedB;
	    
	// Create the vertex shader...
	GLuint vertexShaderSu = glCreateShader(GL_VERTEX_SHADER);

	unsigned char* vertexShaderAssemblyP = NULL;
	if ((vertexShaderAssemblyP = ReadShaderFile(inVertexShaderFile)) == NULL) {
		GL_CHECK(AESDK_OpenGL_ShaderInit_Err);
	}

	vertexShaderStringsP[0] = (char*)vertexShaderAssemblyP;
	glShaderSource(vertexShaderSu, 1, vertexShaderStringsP, NULL);
	glCompileShader(vertexShaderSu);
	delete vertexShaderAssemblyP;

	glGetShaderiv(vertexShaderSu, GL_COMPILE_STATUS, &vertCompiledB);
	char str[4096];
	if(!vertCompiledB) {
		glGetShaderInfoLog(vertexShaderSu, sizeof(str), NULL, str);
		GL_CHECK(AESDK_OpenGL_ShaderInit_Err);
	}

	// Create the fragment shader...
	GLuint fragmentShaderSu = glCreateShader(GL_FRAGMENT_SHADER);

	unsigned char* fragmentShaderAssemblyP = NULL;
	if(	(fragmentShaderAssemblyP = ReadShaderFile( inFragmentShaderFile )) == NULL)
		GL_CHECK(AESDK_OpenGL_ShaderInit_Err);

	fragmentShaderStringsP[0] = (char*)fragmentShaderAssemblyP;
	glShaderSource(fragmentShaderSu, 1, fragmentShaderStringsP, NULL);
	glCompileShader(fragmentShaderSu);
	delete fragmentShaderAssemblyP;

	glGetShaderiv(fragmentShaderSu, GL_COMPILE_STATUS, &fragCompiledB);
	if(!fragCompiledB) {
		glGetShaderInfoLog(fragmentShaderSu, sizeof(str), NULL, str);
		GL_CHECK(AESDK_OpenGL_ShaderInit_Err);
	}

	// Create a program object and attach the two compiled shaders...
	GLuint programObjSu = glCreateProgram();
	glAttachShader(programObjSu, vertexShaderSu);
	glAttachShader(programObjSu, fragmentShaderSu);

	glBindAttribLocation(programObjSu, PositionSlot, "Position");
	glBindAttribLocation(programObjSu, UVSlot, "UVs");

	// Link the program object
	glLinkProgram(programObjSu);
	glGetProgramiv(programObjSu, GL_LINK_STATUS, &linkedB);

	if( !linkedB ) {
		glGetProgramInfoLog(programObjSu, sizeof(str), NULL, str);
		GL_CHECK(AESDK_OpenGL_ShaderInit_Err);
	}

	glDetachShader(programObjSu, vertexShaderSu);
	glDetachShader(programObjSu, fragmentShaderSu);
	glDeleteShader(vertexShaderSu);
	glDeleteShader(fragmentShaderSu);

	return programObjSu;
}

/*
** Bind Texture
*/
void AESDK_OpenGL_BindTextureToTarget(gl::GLuint program, GLint inTexture, std::string inTargetName)
{
	if (inTexture != -1) {
		glActiveTexture(GL_TEXTURE0);
		glBindTexture( GL_TEXTURE_2D, inTexture );
		glUniform1i(glGetUniformLocation(program, inTargetName.c_str()), 0);
	}
	else {
		GL_CHECK(AESDK_OpenGL_ShaderInit_Err);
	}
}

/*
** AESDK error reporting
*/
std::string ReportError(AESDK_OpenGL_Err inError)
{
	//[TODO]: Localize
	switch(inError)
	{
	case 0: return std::string("OK"); break;
	case 1: return std::string("OS specific calls failed!"); break;
	case 2: return std::string("OS specific unloading error!"); break;
	case 3: return std::string("OpenGL resources failed to load!"); break;
	case 4: return std::string("OpenGL resources failed to un-load!"); break;
	case 5: return std::string("Appropriate OpenGL extensions not found!"); break;
	case 6: return std::string("Shader Error!"); break;
	case 7: return std::string("Unknown AESDK_OpenGL error!"); break;
	default: return std::string("Unknown Error Code!");
	}
}

/*
** OpenGL helper function
*/
std::string CheckFramebufferStatus()
{
	gl::GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	switch(status)
	{
    case GL_FRAMEBUFFER_COMPLETE:
		return std::string("OK");
    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
		return std::string("Framebuffer incomplete, incomplete attachment!");
    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
		return std::string("Framebuffer incomplete, missing attachment!");
    case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
		return std::string("Framebuffer incomplete, missing draw buffer!");
    case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
		return std::string("Framebuffer incomplete, missing read buffer!");
    case GL_FRAMEBUFFER_UNSUPPORTED:
		return std::string("Unsupported framebuffer format!");
	default:
		break;
    }
	return std::string("Unknown error!");
}

/*
** ReadShaderFile
*/
unsigned char *ReadShaderFile(std::string inFilename)
{
    FILE *fileP;
	unsigned char *bufferP = NULL;
#ifdef AE_OS_WIN
	fopen_s(&fileP, inFilename.c_str() , "r" );
#elif defined(AE_OS_MAC)
	fileP = fopen( inFilename.c_str() , "r" );
#endif	
	if(NULL != fileP)
	{
		fseek(fileP, 0L, SEEK_END);
		int32_t fileLength = ftell( fileP);
		rewind(fileP);
		bufferP = new unsigned char[fileLength];
		int32_t bytes = static_cast<int32_t>(fread( bufferP, 1, fileLength, fileP ));
		bufferP[bytes] = 0;

#if defined(AE_OS_WIN) && defined(_DEBUG)
		OutputDebugStringA((const char*)bufferP);
		OutputDebugStringA("\n");
#endif
#if defined(AE_OS_MAC) && defined(_DEBUG)
		std::cout << bufferP << std::endl;
#endif
	}
		
	return bufferP;
}

} //namespace ends
