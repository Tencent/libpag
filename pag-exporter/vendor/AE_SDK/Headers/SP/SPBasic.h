/***********************************************************************/
/*                                                                     */
/* SPBasic.h                                                           */
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

#ifndef __SPBasic__
#define __SPBasic__


/*******************************************************************************
 **
 **	Imports
 **
 **/

#include "SPTypes.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif


/*******************************************************************************
 **
 **	Constants
 **
 **/
/** PICA basic suite name */
#define kSPBasicSuite				"SP Basic Suite"
/** PICA basic suite version */
#define kSPBasicSuiteVersion		4


/*******************************************************************************
 **
 **	Suite
 **
 **/

/** @ingroup Suites
	This suite provides basic memory management for PICA (the Adobe plug-in manager)
	and defines the basic functions for acquiring and releasing other suites.

	A suite consists of a list of function pointers. The application, or a
	plug-in that loads a suite, provides valid pointers when the suite is
	acquired. When a suite is not available, the pointers are set to the
	address of the \c #Undefined() function.

	Do not attempt to acquire a suite (other than the \c #SPBlocksSuite)
	in response to a PICA access (\c #kSPAccessCaller) or property
	(\c #kSPPropertiesCaller) message. Most suites are unavailable
	during these load and unload operations.

	You can acquire all the suites you will need when your plug-in is first
	loaded, as long as you release them before your plug-in is unloaded.
	At shutdown, however, it is most efficient to acquire only those
	suites explicitly needed to shut down; for example, to free memory
	and save preferences.

	The \c SPBasicSuite itself is a part of the message data passed
	to your plug-in with any call. To access it from the message data structure:
	@code
	SPBasicSuite sBasic = message->d.basic;
	sBasic->function( )
	@endcode
	*/
typedef struct SPBasicSuite {
	/** Acquires a function suite. Loads the suite if necessary,
		and increments its reference count. For example:
	@code
SPErr error;
SPBasicSuite *sBasic = message->d.basic;
AIRandomSuite *sRandom;
sBasic->AcquireSuite( kAIRandomSuite, kAIRandomVersion, &sRandom );
	@endcode
			@param name The suite name.
			@param version The suite version number.
			@param suite [out] A buffer in which to return the suite pointer.
			@see \c #SPSuitesSuite::AcquireSuite()
		*/
	SPAPI SPErr (*AcquireSuite)( const char *name, int32 version, const void **suite );
	/** Decrements the reference count of a suite and unloads it when the
		reference count reaches 0.
			@param name The suite name.
			@param version The suite version number.
		*/
	SPAPI SPErr (*ReleaseSuite)( const char *name, int32 version );
	/** Compares two strings for equality.
			@param token1 The first null-terminated string.
			@param token2 The second null-terminated string.
			@return True if the strings are the same, false otherwise.
		*/
	SPAPI SPBoolean (*IsEqual)( const char *token1, const char *token2 );
	/** Allocates a block of memory.
			@param size The number of bytes.
			@param block [out] A buffer in which to return the block pointer.
			@see \c #SPBlocksSuite::AllocateBlock()
		*/
	SPAPI SPErr (*AllocateBlock)( size_t size, void **block );
	/** Frees a block of memory allocated with \c #AllocateBlock().
			@param block The block pointer.
			@see \c #SPBlocksSuite::FreeBlock()
		*/
	SPAPI SPErr (*FreeBlock)( void *block );
	/** Reallocates a block previously allocated with \c #AllocateBlock().
		Increases the size without changing the location, if possible.
			@param block The block pointer.
			@param newSize The new number of bytes.
			@param newblock [out] A buffer in which to return the new block pointer.
			@see \c #SPBlocksSuite::ReallocateBlock()
		*/
	SPAPI SPErr (*ReallocateBlock)( void *block, size_t newSize, void **newblock );
 	/** A function pointer for unloaded suites. This is a protective measure
 		against other plug-ins that may mistakenly use the suite after they have
 		released it.

 		A plug-in that exports a suite should unload the suite's procedure pointers
 		when it is unloaded, and restore them when the plug-in is reloaded.
 		\li On unload, replace the suite's procedure pointers
 			with the address of this function.
		\li On reload, restore the suite's procedure
			pointers with the updated addresses of their functions.

		For example:
	@code
	 	SPErr UnloadSuite( MySuite *mySuite, SPAccessMessage *message ) {
	 		mySuite->functionA = (void *) message->d.basic->Undefined;
	 		mySuite->functionB = (void *) message->d.basic->Undefined;
	 	}

	 	SPErr ReloadSuite( MySuite *mySuite, SPAccessMessage *message ) {
	 		mySuite->functionA = functionA;
	 		mySuite->functionB = functionB;
	 	}
	@endcode
		*/
	SPAPI SPErr (*Undefined)( void );

} SPBasicSuite;


/** Internal */
SPAPI SPErr SPBasicAcquireSuite( const char *name, int32 version, const void **suite );
/** Internal */
SPAPI SPErr SPBasicReleaseSuite( const char *name, int32 version );
/** Internal */
SPAPI SPBoolean SPBasicIsEqual( const char *token1, const char *token2 );
/** Internal */
SPAPI SPErr SPBasicAllocateBlock( size_t size, void **block );
/** Internal */
SPAPI SPErr SPBasicFreeBlock( void *block );
/** Internal */
SPAPI SPErr SPBasicReallocateBlock( void *block, size_t newSize, void **newblock );
/** Internal */
SPAPI SPErr SPBasicUndefined( void );

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
