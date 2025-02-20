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

/*
	GL_base.h
*/

#pragma once

#ifndef GL_BASE_H
#define GL_BASE_H

#include "AEConfig.h"

//OS specific includes
#ifdef AE_OS_WIN
	// mambo jambo with stdint
	#define _STDINT
	#define WIN32_LEAN_AND_MEAN 
	#define DVACORE_MATCH_MAC_STDINT_ON_WINDOWS
	#include <stdint.h>
	#ifndef INT64_MAX
		#define INT64_MAX       LLONG_MAX
	#endif
	#ifndef INTMAX_MAX
		#define INTMAX_MAX       INT64_MAX
	#endif

	#include <windows.h>
	#include <stdlib.h>

	#if (_MSC_VER >= 1400)
		#define THREAD_LOCAL __declspec(thread)
	#endif
#else
	#define THREAD_LOCAL __thread
#endif

// OpenGL 3.3 bindings (see https://github.com/hpicgs/glbinding)
#include "glbinding/gl33core/gl.h"

#ifdef AE_OS_MAC
	#import <Cocoa/Cocoa.h>
#endif

//general includes
#include <string>
#include <fstream>
#include <memory>
#include <set>

//typedefs
typedef unsigned char		u_char;
typedef unsigned short		u_short;
typedef unsigned short		u_int16;
typedef unsigned long		u_long;
typedef short int			int16;

namespace AESDK_OpenGL
{

/*
// Global (to the effect) supporting OpenGL contexts
*/

struct AESDK_OpenGL_EffectCommonData
{
	AESDK_OpenGL_EffectCommonData();
	virtual ~AESDK_OpenGL_EffectCommonData();

	// must surround plug-in OpenGL calls with these functions so that AE
	// doesn't know we're borrowing the OpenGL renderer
	void SetPluginContext();

	bool mInitialized;
	std::set<gl::GLextension> mExtensions;

	//OS specific handles
#ifdef AE_OS_WIN
	HWND	mHWnd;
	HDC		mHDC;
	HGLRC	mHRC;
	std::string mClassName;
#endif
#ifdef AE_OS_MAC
	CGLContextObj		mRC;
	NSOpenGLContext*    mNSOpenGLContext;
#endif
};

typedef std::shared_ptr<AESDK_OpenGL_EffectCommonData> AESDK_OpenGL_EffectCommonDataPtr;

/*
// Per render/thread supporting OpenGL variables
*/

struct AESDK_OpenGL_EffectRenderData : public AESDK_OpenGL_EffectCommonData
{
	AESDK_OpenGL_EffectRenderData();
	virtual ~AESDK_OpenGL_EffectRenderData();

	gl::GLuint mFrameBufferSu;
	gl::GLuint mColorRenderBufferSu;

	u_int16 mRenderBufferWidthSu;
	u_int16 mRenderBufferHeightSu;

	gl::GLuint mProgramObjSu;
	gl::GLuint mProgramObj2Su;

	gl::GLuint mOutputFrameTexture; //pbo texture

	gl::GLuint vao;
	gl::GLuint quad;
};

typedef std::shared_ptr<AESDK_OpenGL_EffectRenderData> AESDK_OpenGL_EffectRenderDataPtr;

enum AESDK_OpenGL_Err
{
	AESDK_OpenGL_OK = 0,
	AESDK_OpenGL_OS_Load_Err,
	AESDK_OpenGL_OS_Unload_Err,
	AESDK_OpenGL_Res_Load_Err,
	AESDK_OpenGL_Res_Unload_Err,
	AESDK_OpenGL_Extensions_Err,
	AESDK_OpenGL_ShaderInit_Err,
	AESDK_OpenGL_Unknown_Err
};

enum { PositionSlot, UVSlot };

/*
// Core functions
*/
void AESDK_OpenGL_Startup(AESDK_OpenGL_EffectCommonData& inData, const AESDK_OpenGL_EffectCommonData* inRootContext = nullptr);
void AESDK_OpenGL_Shutdown(AESDK_OpenGL_EffectCommonData& inData);

void AESDK_OpenGL_InitResources(AESDK_OpenGL_EffectRenderData& inData, u_short inBufferWidth, u_short inBufferHeight, const std::string& resourcePath);
void AESDK_OpenGL_MakeReadyToRender(AESDK_OpenGL_EffectRenderData& inData, gl::GLuint textureHandle);
gl::GLuint AESDK_OpenGL_InitShader(std::string inVertexShaderFile, std::string inFragmentShaderFile);
void AESDK_OpenGL_BindTextureToTarget(gl::GLuint program, gl::GLint inTexture, std::string inTargetName);


/*
// Independent macros and helper functions
*/

//GetProcAddress
#ifdef AE_OS_WIN
	#define GetProcAddress(N) wglGetProcAddress((LPCSTR)N)
#elif defined(AE_OS_MAC)
	#define GetProcAddress(N) NSGLGetProcAddress(N)
#endif

//helper function - error reporting util
std::string ReportError(AESDK_OpenGL_Err inError);
//helper function - check frame buffer status before final render call
std::string CheckFramebufferStatus();
//helper function - read shader file into the compiler
unsigned char* ReadShaderFile(std::string inFile);

/*
//	Error class and macros used to trap errors
*/
class AESDK_OpenGL_Fault 
{
public:
	AESDK_OpenGL_Fault(AESDK_OpenGL_Err inError)
	{
		mError = inError;
	}
	operator AESDK_OpenGL_Err()
	{
		return mError;
	}

protected:
	AESDK_OpenGL_Err mError;
};

#define GL_CHECK(err) {AESDK_OpenGL_Err err1 = err; if (err1 != AESDK_OpenGL_OK ){ throw AESDK_OpenGL_Fault(err1);}}

class SaveRestoreOGLContext
{
public:
	SaveRestoreOGLContext();
	~SaveRestoreOGLContext();

private:
#ifdef AE_OS_MAC
	CGLContextObj    o_RC;
	NSOpenGLContext* pNSOpenGLContext_;
#endif
#ifdef AE_OS_WIN
	HDC   h_DC; /// Device context handle
	HGLRC h_RC; /// Handle to an OpenGL rendering context
#endif

	SaveRestoreOGLContext(const SaveRestoreOGLContext &);
	SaveRestoreOGLContext &operator=(const SaveRestoreOGLContext &);
};
};
#endif // GL_BASE_H
