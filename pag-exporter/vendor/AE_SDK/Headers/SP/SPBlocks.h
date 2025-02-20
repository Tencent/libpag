/***********************************************************************/
/*                                                                     */
/* SPBlocks.h                                                          */
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

#ifndef __SPBlocks__
#define __SPBlocks__


/*******************************************************************************
 **
 **	Imports
 **
 **/

#ifdef SWEETPEA_STRICTER_HOST_COMPATIBILITY
#include <stddef.h>
#endif
#include "SPTypes.h"

#ifdef __cplusplus
extern "C" {
#endif


/*******************************************************************************
 **
 **	Constants
 **
 **/
/** SPBlocks suite name */
#define kSPBlocksSuite				"SP Blocks Suite"
/** SPBlocks suite version */
#define kSPBlocksSuiteVersion		2


/*******************************************************************************
 **
 **	Suite
 **
 **/
/** @ingroup Suites
	This suite provides basic memory management for PICA (the Adobe plug-in manager),
	with debugging capability for the development version of the application.

	Unlike other suites, this suite is available during plug-in load and unload operations.
	You can acquire it in response to a PICA access (\c #kSPAccessCaller) or property
	(\c #kSPPropertiesCaller) message, and use it to allocate and free memory
	for the plug-in you are loading or unloading.

	\li Acquire this suite using \c #SPBasicSuite::AcquireSuite() with the constants
		\c #kSPBlocksSuite and \c #kSPBlocksSuiteVersion.
	*/
typedef struct SPBlocksSuite {

	/** Allocates a block of memory.
			@param size The number of bytes.
			@param debug Optional. A tag for the block, available in the
				developement version of the application. Otherwise ignored.
				See \c #SPBlockDebugSuite::GetBlockDebug().
			@param block [out] A buffer in which to return the block pointer.
			@see \c #SPBasicSuite::AllocateBlock()
		*/
	SPAPI SPErr (*AllocateBlock)( size_t size, const char *debug, void **block );
	/** Frees a block of memory allocated with \c #AllocateBlock().
			@param block The block pointer.
			@see \c #SPBasicSuite::FreeBlock()
		*/
	SPAPI SPErr (*FreeBlock)( void *block );
	/** Reallocates a block previously allocated with \c #AllocateBlock().
		Increases the size without changing the location, if possible.
			@param block The block pointer.
			@param newSize The new number of bytes.
			@param debug Optional. A tag for the block, available in the
				developement version of the application. Otherwise ignored.
				See \c #SPBlockDebugSuite::GetBlockDebug().
			@param newblock [out] A buffer in which to return the new block pointer.
			@see \c #SPBasicSuite::ReallocateBlock()
		*/
	SPAPI SPErr (*ReallocateBlock)( void *block, size_t newSize, const char *debug, void **rblock );

} SPBlocksSuite;

/** Internal */
SPAPI SPErr SPAllocateBlock( size_t size, const char *debug, void **block );
/** Internal */
SPAPI SPErr SPFreeBlock( void *block );
/** Internal */
SPAPI SPErr SPReallocateBlock( void *block, size_t newSize, const char *debug, void **rblock );


/*******************************************************************************
 **
 **	Errors
 **
 **/

#include "SPErrorCodes.h"

#ifdef __cplusplus
}
#endif

#endif
