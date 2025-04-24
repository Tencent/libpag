/***********************************************************************/
/*                                                                     */
/* SPConfig.h                                                          */
/*                                                                     */
/* Copyright 1995-2006 Adobe Systems Incorporated.                     */
/* All Rights Reserved.                                                */
/*                                                                     */
/* Patents Pending                                                     */
/*                                                                     */
/* NOTICE: All information contained herein is the property of Adobe   */
/* Systems Incorporated. Many of the intellectual and technical        */
/* concepts contained herein are proprietary to Adobe, are protected   */
/* as trade secrets, and are made available only to Adobe licensees    */
/* for their internal use. Any reproduction or dissemination of this   */
/* software is strictly forbidden unless prior written permission is   */
/* obtained from Adobe.                                                */
/*                                                                     */
/***********************************************************************/

/**

	SPConfig.h is the environment configuration file for Sweet Pea. It
	defines MAC_ENV or WIN_ENV. These are used to control platform-specific
	sections of code.

 **/

#ifndef __SPCnfig__
#define __SPCnfig__

#if defined(__APPLE_CC__)
#if !defined (MAC_ENV) && !defined(SIMULATED_WASM)
#define MAC_ENV 1
#endif
#endif

/*
 *	Windows
 */
#if defined(_WINDOWS) || defined(_MSC_VER) || defined(WINDOWS)		// PSMod, better compiler check
#ifndef WIN_ENV
#define WIN_ENV 1
#endif
#endif

#ifndef qiOS
#if defined(MAC_ENV) && defined(__arm__)
#define qiOS 1
#endif
#endif

/*
 *	Make certain that one and only one of the platform constants is defined.
 */

#ifdef __ANDROID__
	#define ANDROID_ENV 1
#endif

#ifdef __LINUX__
    #define UNIX_ENV 1
#endif

#if defined (__EMSCRIPTEN__)
	#define WEB_ENV 1
#endif

#if !defined(WIN_ENV) && !defined(MAC_ENV) && !defined(ANDROID_ENV) && !defined(UNIX_ENV) && !defined(WEB_ENV)
	#error
#endif

#if defined(WIN_ENV) && defined(MAC_ENV) && defined(ANDROID_ENV) && defined(UNIX_ENV) && defined (WEB_ENV)
	#error
#endif

#endif
