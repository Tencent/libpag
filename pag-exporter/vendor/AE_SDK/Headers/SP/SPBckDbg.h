/***********************************************************************/
/*                                                                     */
/* SPBckDbg.h                                                          */
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

#ifndef __SPBlockDebug__
#define __SPBlockDebug__


/*******************************************************************************
 **
 **	Imports
 **
 **/

#include "SPTypes.h"

#ifdef __cplusplus
extern "C" {
#endif


/*******************************************************************************
 **
 **	Constants
 **
 **/
/** SPBlockDebug suite name */
#define kSPBlockDebugSuite			"SP Block Debug Suite"
/** SPBlockDebug suite version */
#define kSPBlockDebugSuiteVersion	2


/*******************************************************************************
 **
 **	Suite
 **
 **/

/** @ingroup Suites
	This suite provides basic debugging capability for blocks of memory
	allocated with the \c #SPBlocksSuite.  Debugging can only be enabled
	in the developement environment.

	\li Acquire this suite using \c #SPBasicSuite::AcquireSuite() with the constants
		\c #kSPBlockDebugSuite and \c #kSPBlockDebugSuiteVersion.
	*/
typedef struct SPBlockDebugSuite {
	/** Reports whether block debugging is enabled.
			@param enabled [out] A buffer in which to return 1 if debugging
				is enabled, 0 otherwise.
		*/
	SPAPI SPErr (*GetBlockDebugEnabled)( int32 *enabled );
	/** Turns debugging on or off.
			@param debug 1 to turn debugging on, 0 to turn it off.
		*/
	SPAPI SPErr (*SetBlockDebugEnabled)( int32 debug );
	/** Retrieves the first block of memory allocated. Use with \c #GetNextBlock()
		to iterate through all allocated blocks.
			@param block [out] A buffer in which to return the block pointer.
		*/
	SPAPI SPErr (*GetFirstBlock)( void **block );
	/** Retrieves the block of memory allocated immediately after a given block.
		Use with \c #GetFirstBlock() to iterate through all allocated blocks.
			@param block The current block pointer
			@param nextblock [out] A buffer in which to return the next block pointer.
		*/
	SPAPI SPErr (*GetNextBlock)( void *block, void **nextblock );
	/** Retrieves the debugging tag assigned to a block of memory when it
		was allocated or reallocated.
			@param block The block pointer.
			@param debug [out] A buffer in which to return the tag string.
			@see \c #SPBlocksSuite::AllocateBlock(), \c #SPBlocksSuite::ReallocateBlock()
		*/
	SPAPI SPErr (*GetBlockDebug)( void *block, const char **debug );

} SPBlockDebugSuite;


/** Internal */
SPAPI SPErr SPGetBlockDebugEnabled( int32 *enabled );
/** Internal */
SPAPI SPErr SPSetBlockDebugEnabled( int32 debug );
/** Internal */
SPAPI SPErr SPGetFirstBlock( void **block );
/** Internal */
SPAPI SPErr SPGetNextBlock( void *block, void **nextblock );
/** Internal */
SPAPI SPErr SPGetBlockDebug( void *block, const char **debug );


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
