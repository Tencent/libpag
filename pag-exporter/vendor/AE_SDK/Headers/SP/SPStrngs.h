/***********************************************************************/
/*                                                                     */
/* SPStrngs.h                                                          */
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

#ifndef __SPStrings__
#define __SPStrings__


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
 ** Constants
 **
 **/
/** PICA strings suite name */
#define kSPStringsSuite				"SP Strings Suite"
/** PICA strings suite version */
#define kSPStringsSuiteVersion		2

/** Globally available PICA strings resources.
	@see \c #SPRuntimeSuite::GetRuntimeStringPool(). */
#define kSPRuntimeStringPool		((SPStringPoolRef)NULL)


/*******************************************************************************
 **
 ** Types
 **
 **/

/*  If you override the default string pool handler by defining host proc routines,
 *	how the string pool memory allocation and searching is done is up to you.  As an example,
 *	the structure below is similar to what Sweet Pea uses for its default string pool
 *	routines. The pool is a sorted list of strings of number count, kept in memory referenced
 *	by the heap field.
 *
 *			typedef struct SPStringPool {
 *
 *				SPPoolHeapRef heap;
 *				int32 count;
 *
 *			} SPStringPool;
 */

/** Opaque reference to a string pool. Access with the \c #SPStringsSuite. */
typedef struct SPStringPool *SPStringPoolRef;


/*******************************************************************************
 **
 ** Suite
 **
 **/

/** @ingroup Suites
	This suite allows you to work with the PICA string pool.

	PICA manages a string pool, which provides an efficient central
	storage space for C strings. When a string is placed in the pool, PICA
    checks whether it already exists in the pool, and if so, returns a
    pointer to the existing string. If not, it copies the string into the pool,
    and returns a pointer to the copy.

	This mechanisms atomizes the strings. Because each string exists in
	only one place, strings can be compared by address, rather than character
	by character, and string searches are made much more efficient.

	\li Acquire this suite using \c #SPBasicSuite::AcquireSuite() with the constants
		\c #kSPStringsSuite and \c #kSPStringsSuiteVersion.
	*/
typedef struct SPStringsSuite {

	/** Creates a new string pool and allocates an initial block of memory for
		its strings. You can also access PICA's global string pool,
		using \c #SPRuntimeSuite::GetRuntimeStringPool().
			@param stringPool [out] A buffer in which to return the new string pool reference.
		*/
	SPAPI SPErr (*AllocateStringPool)( SPStringPoolRef *stringPool );
	/** Frees the memory used for a string pool created with \c #AllocateStringPool().
		Do not free the global string pool (\c #kSPRuntimeStringPool).
			@param stringPool The string pool reference.
		*/
	SPAPI SPErr (*FreeStringPool)( SPStringPoolRef stringPool );
	/** Adds a string to a string pool, or, if the string has already been added
		to the pool, retrieves a reference to the pooled string.
			@param stringPool The string pool reference.
			@param string The string.
			@param wString [out] A buffer in which to return the address of
				the atomized string in the pool.
		*/
	SPAPI SPErr (*MakeWString)( SPStringPoolRef stringPool, const char *string, const char **wString );

} SPStringsSuite;


SPAPI SPErr SPAllocateStringPool( SPStringPoolRef *stringPool );
SPAPI SPErr SPFreeStringPool( SPStringPoolRef stringPool );
SPAPI SPErr SPMakeWString( SPStringPoolRef stringPool, const char *string, const char **wString );


#ifdef __cplusplus
}
#endif

#endif
