/***********************************************************************/
/*                                                                     */
/* SPTypes.h                                                           */
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

	These are the basic declarations used by Sweet Pea.

 **/


#ifndef __SPTypes__
#define __SPTypes__


/*******************************************************************************
 **
 **	Imports
 **
 **/

#include "SPConfig.h"


/*
 *	You can replace SPTypes.h with your own. Define OTHER_SP_TYPES_H on the
 *	command line or in SPConfig.h to be the name of the replacement file.
 *
 *	Example:
 *
 *	#define OTHER_SP_TYPES_H "MySPTypes.h"
 *	#include "SPBasic.h"  // for example
 *
 *	Sweet Pea depends on TRUE, FALSE, SPErr, etc. Your replacement must
 *	define them.
 */

#ifdef OTHER_SP_TYPES_H
#include OTHER_SP_TYPES_H
#else


/*******************************************************************************
 **
 **	Constants
 **
 **/

#ifndef TRUE
#define TRUE	1
#endif

#ifndef FALSE
#define FALSE	0
#endif

#ifndef NULL

#ifdef MAC_ENV
#if !defined(__cplusplus) && (defined(__SC__) || defined(THINK_C))
#define NULL	((void *) 0)
#else
#define NULL	0
#endif
#endif

#ifdef WIN_ENV
#ifdef __cplusplus
#define NULL    0
#else
#define NULL    ((void *)0)
#endif
#endif

#endif


/*
 *	SPAPI is placed in front of procedure declarations in the API. On the Mac
 *	it used to be 'pascal', which forced consistent calling conventions across different
 *	compilers. No longer needed. On Windows it's nothing.
 *
 *	Example:
 *
 *	SPAPI void *SPAllocateBlock( int32 size, const char *debug, SPErr *error );
 *
 */

#if defined(MAC_ENV) || defined(__ANDROID__) || defined(__LINUX__) || defined (__EMSCRIPTEN__) || defined(SIMULATED_WASM)
#if defined(__GNUC__)
#define SPAPI
#else
#define SPAPI	pascal
#endif
#endif

#ifdef WIN_ENV
#define SPAPI
#endif

#include "PSIntTypes.h"

typedef uint8 SPUInt8;
typedef uint16 SPUInt16;
typedef uint32 SPUInt32;

typedef int32 SPInt32;

#if defined(MAC_ENV) || defined(__ANDROID__) || defined(__LINUX__) || defined (__EMSCRIPTEN__) || defined(SIMULATED_WASM)

/* SPBoolean is the same a Macintosh Boolean. */
typedef uint8 SPBoolean;

#endif

#ifdef WIN_ENV

/* SPBoolean is the same a Windows BOOL. */
typedef int32 SPBoolean;

#endif


/*******************************************************************************
 **
 **	Error Handling
 **
 **/

/*
 *	Error codes in Sweet Pea are C strings, with the exception of the code for
 *	no error, which is NULL. The error can first be compared with kSPNoError to
 *	test if the function succeeded. If it is not NULL then the error can be
 *	string-compared with predefined error strings.
 *
 *	Example:
 *
 *	SPErr error = kSPNoError;
 *
 *	block = SPAllocateBlock( size, debug, &error );
 *	if ( error != kSPNoError ) {
 *		if ( strcmp( error, kSPOutOfMemoryError ) == 0 )
 *			FailOutOfMemory();
 *		...
 *	}
 */

typedef int32 SPErr;

/*
 *	kSPNoError and kSPUnimplementedError are universal. Other error codes should
 *	be defined in the appropriate header files.
 */

#include "SPErrorCodes.h"

#endif /* OTHER_SP_TYPES_H */

#endif
