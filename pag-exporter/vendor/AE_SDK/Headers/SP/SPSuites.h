/***********************************************************************/
/*                                                                     */
/* SPSuites.h                                                          */
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

#ifndef __SPSuites__
#define __SPSuites__


/*******************************************************************************
 **
 **	Imports
 **
 **/

#include "SPTypes.h"
#include "SPAccess.h"
#include "SPPlugs.h"
#include "SPStrngs.h"

#ifdef __cplusplus
extern "C" {
#endif


/*******************************************************************************
 **
 ** Constants
 **
 **/
/** PICA suite-management suite name */
#define kSPSuitesSuite				"SP Suites Suite"
/** PICA suite-management suite version */
#define kSPSuitesSuiteVersion		2

/** Internal */
#define kSPLatestInternalVersion	0

/** PICA global list of available suites.
	@see \c #SPRuntimeSuite::GetRuntimeSuiteList(). */
#define kSPRuntimeSuiteList			((SPSuiteListRef)NULL)


/*******************************************************************************
 **
 ** Types
 **
 **/

/** Opaque reference to a suite object. Access with the \c #SPSuitesSuite. */
typedef struct SPSuite *SPSuiteRef;
/** A list of suite objects. Create with
	\c #SPSuitesSuite::AllocateSuiteList(), or use
	the global list, \c #kSPRuntimeSuiteList. */
typedef struct SPSuiteList *SPSuiteListRef;
/** An iterator object for examining a suite list.
	See \c #SPSuitesSuite::NewSuiteListIterator(). */
typedef struct SPSuiteListIterator *SPSuiteListIteratorRef;


/*******************************************************************************
 **
 ** Suite
 **
 **/

/** @ingroup Suites
	This suite allows you to create, manage, and access PICA function suites.

	A suite associates a name and version number with a pointer to an array of
	function pointers. The functions generally haves some common purpose, such as
	accessing a data type, and are used by plug-ins to interact with PICA, with
	the application, and with each other.

	In order to use a function in a suite, you must first \e acquire it.
	This suite provides the low-level function \c #AcquireSuite(),
	but a plug-in more typically uses the \c #SPBasicSuite, which is provided
	with every message to a plug-in.

	PICA creates a global suite list at application startup, which contains
	references to every suite added by PICA, the application, or other plug-ins.
	You can use this suite to create and manage additional suite lists.

	\li Acquire this suite using \c #SPBasicSuite::AcquireSuite() with the constants
		\c #kSPSuitesSuite and \c #kSPSuitesSuiteVersion.
	*/
typedef struct SPSuitesSuite {

	/** Creates a new suite list. You can also access PICA's global suite list,
		using \c #SPRuntimeSuite::GetRuntimeSuiteList().
			@param stringPool The string pool in which to keep suite names.
			@param plugins <<what is this for? not in old doc>>
			@param suiteList [out] A buffer in which to return the new list object.
		*/
	SPAPI SPErr (*AllocateSuiteList)( SPStringPoolRef stringPool, SPPluginListRef plugins,
				SPSuiteListRef *suiteList );

	/** Frees a list of suites allocated with \c #AllocateSuiteList(), and
		also frees any entries in the list. Do not free the global list (\c #kSPRuntimeSuiteList).
			@param suiteList The suite list object.
		*/
	SPAPI SPErr (*FreeSuiteList)( SPSuiteListRef suiteList );

	/** Creates a new plug-in function suite and adds it to a suite list.
		Identifying constants for the suite name and version must be made
		available in a public header file.
			@param suiteList The suite list object, or \c NULL to use the
				global list.
			@param host The plug-in object providing the suite.
			@param name The unique name of the suite.
			@param apiVersion The public version number of the suite.
			@param internalVersion The internal version number of the suite.
			@param suiteProcs A pointer to a structure containing the function pointers
				for the suite.
			@param suite [out] A buffer in which to return the new suite object.
		*/
	SPAPI SPErr (*AddSuite)( SPSuiteListRef suiteList, SPPluginRef host, const char *name,
				int32 apiVersion, int32 internalVersion, const void *suiteProcs, SPSuiteRef *suite );

	/** Acquires a function suite from a suite list. Loads the suite if necessary,
		and increments its reference count. This function differs from
		\c #SPBasicSuite::AcquireSuite() in that you can specify a suite list
		and internal version number.
			@param suiteList The suite list object, or \c NULL to use the
				global list.
			@param name The suite name.
			@param version The public suite version number.
			@param internalVersion The internal suite version number.
			@param suiteProcs [out] A buffer in which to return a pointer to the
				suite function pointer array.
		*/
	SPAPI SPErr (*AcquireSuite)( SPSuiteListRef suiteList, const char *name, int32 apiVersion,
				int32 internalVersion, const void **suiteProcs );

	/** Decrements the reference count of a suite in a suite list and unloads it when the
		reference count reaches 0.
			@param suiteList The suite list object, or \c NULL to use the
				global list.
			@param name The suite name.
			@param version The public suite version number.
			@param internalVersion The internal suite version number.
		*/
	SPAPI SPErr (*ReleaseSuite)( SPSuiteListRef suiteList, const char *name, int32 apiVersion,
				int32 internalVersion );

	/** Retrieves a suite from a suite list.
			@param suiteList The suite list object, or \c NULL to use the
				global list.
			@param name The suite name.
			@param version The public suite version number.
			@param internalVersion The internal suite version number.
			@param suiteProcs [out] A buffer in which to return the suite object, or
				\c NULL if no matching suite is found in the list.
		*/
	SPAPI SPErr (*FindSuite)( SPSuiteListRef suiteList, const char *name, int32 apiVersion,
				int32 internalVersion, SPSuiteRef *suite );

	/** Creates an iterator object with which to traverse a suite list.
		The iterator is initially set to the first suite in the list.
			@param suiteList The suite list object, or \c NULL to use the
				global list.
			@param iter [out] A buffer in which to return the new iterator object.
			@see \c #NextSuite(), \c #DeleteSuiteListIterator()
		*/
	SPAPI SPErr (*NewSuiteListIterator)( SPSuiteListRef suiteList, SPSuiteListIteratorRef *iter );
	/** Retrieves the current suite and advances a suite-list iterator to the next suite in the list.
			@param iter The suite-list iterator object.
			@param suite [out] A buffer in which to return the current suite object, \c NULL
				if the end of the list has been reached.
		    @see \c #NewSuiteListIterator(),
		*/
	SPAPI SPErr (*NextSuite)( SPSuiteListIteratorRef iter, SPSuiteRef *suite );
	/** Frees a suite-list iterator that is no longer needed.
			@param iter The suite-list iterator object.
			@see \c #NewSuiteListIterator(),
		*/
	SPAPI SPErr (*DeleteSuiteListIterator)( SPSuiteListIteratorRef iter );

	/** Retrieves the plug-in that provides a suite.
			@param suite The suite object.
			@param plugin [out] A buffer in which to return the plug-in object.
		*/
	SPAPI SPErr (*GetSuiteHost)( SPSuiteRef suite, SPPluginRef *plugin );
	/** Retrieves the unique name of a suite.
			@param suite The suite object.
			@param name [out] A buffer in which to return the name string.
		*/
	SPAPI SPErr (*GetSuiteName)( SPSuiteRef suite, const char **name );
	/** Retrieves the public version number of a suite.
			@param suite The suite object.
			@param version [out] A buffer in which to return the public version number.
		*/
	SPAPI SPErr (*GetSuiteAPIVersion)( SPSuiteRef suite, int32 *version );
	/** Retrieves the internal version number of a suite.
			@param suite The suite object.
			@param version [out] A buffer in which to return the internal version number.
		*/
	SPAPI SPErr (*GetSuiteInternalVersion)( SPSuiteRef suite, int32 *version );
	/** Retrieves the function pointer array of a suite.
			@param suite The suite object.
			@param suiteProcs [out] A buffer in which to return a pointer
				to the function pointer array.
		*/
	SPAPI SPErr (*GetSuiteProcs)( SPSuiteRef suite, const void **suiteProcs );
	/** Retrieves the current reference count of a suite.
			@param suite The suite object.
			@param count [out] A buffer in which to return the reference count.
		*/
	SPAPI SPErr (*GetSuiteAcquireCount)( SPSuiteRef suite, int32 *count );

} SPSuitesSuite;


/** Internal */
SPAPI SPErr SPAllocateSuiteList( SPStringPoolRef stringPool, SPPluginListRef plugins,
			SPSuiteListRef *suiteList );

/** Internal */
SPAPI SPErr SPFreeSuiteList( SPSuiteListRef suiteList );

/** Internal */
SPAPI SPErr SPAddSuite( SPSuiteListRef suiteList, SPPluginRef host, const char *name,
			int32 apiVersion, int32 internalVersion, const void *suiteProcs, SPSuiteRef *suite );

/** Internal */
SPAPI SPErr SPAcquireSuite( SPSuiteListRef suiteList, const char *name, int32 apiVersion,
			int32 internalVersion, const void **suiteProcs );

/** Internal */
SPAPI SPErr SPReleaseSuite( SPSuiteListRef suiteList, const char *name, int32 apiVersion,
			int32 internalVersion );

/** Internal */
SPAPI SPErr SPFindSuite( SPSuiteListRef suiteList, const char *name, int32 apiVersion,
			int32 internalVersion, SPSuiteRef *suite );

/** Internal */
SPAPI SPErr SPNewSuiteListIterator( SPSuiteListRef suiteList, SPSuiteListIteratorRef *iter );
/** Internal */
SPAPI SPErr SPNextSuite( SPSuiteListIteratorRef iter, SPSuiteRef *suite );
/** Internal */
SPAPI SPErr SPDeleteSuiteListIterator( SPSuiteListIteratorRef iter );

/** Internal */
SPAPI SPErr SPGetSuiteHost( SPSuiteRef suite, SPPluginRef *plugin );
/** Internal */
SPAPI SPErr SPGetSuiteName( SPSuiteRef suite, const char **name );
/** Internal */
SPAPI SPErr SPGetSuiteAPIVersion( SPSuiteRef suite, int32 *version );
/** Internal */
SPAPI SPErr SPGetSuiteInternalVersion( SPSuiteRef suite, int32 *version );
/** Internal */
SPAPI SPErr SPGetSuiteProcs( SPSuiteRef suite, const void **suiteProcs );
/** Internal */
SPAPI SPErr SPGetSuiteAcquireCount( SPSuiteRef suite, int32 *count );


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
