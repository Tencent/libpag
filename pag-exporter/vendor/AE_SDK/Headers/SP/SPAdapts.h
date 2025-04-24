/***********************************************************************/
/*                                                                     */
/* SPAdapts.h														   */
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

#ifndef __SPAdapters__
#define __SPAdapters__


/*******************************************************************************
 **
 **	Imports
 **
 **/

#include "SPTypes.h"
#include "SPCaches.h"
#include "SPMData.h"
#include "SPProps.h"

#ifdef __cplusplus
extern "C" {
#endif



/*******************************************************************************
 **
 ** Constants
 **
 **/

/** SPAdapters suite name */
#define kSPAdaptersSuite					"SP Adapters Suite"
/** SPAdapters suite version */
#define kSPAdaptersSuiteVersion				3

/** @ingroup Callers
	Caller for a plug-in adapter. Sent to plug-ins
	with adapters to allow the adapter to load or unload
	files of managed types, or to  make translations for
	compatability with legacy versions of PICA, the application,
	or earlier versions of the plug-in.
	See \c #SPAdaptersSuite.  */
#define kSPAdaptersCaller					"SP Adapters"

/** @ingroup Selectors
	Received by a plug-in with an adapter on application startup, after
	PICA completes its plug-in startup process. Use for initialization
	related to the adapter.
	See \c #kSPAdaptersCaller, \c #SPAdaptersMessage, \c #SPAdaptersSuite.  */
#define kSPAdaptersStartupSelector			"Start up"
/** @ingroup Selectors
	Received by a plug-in with an adapter on application shutdown, after
	PICA completes its plug-in shutdown process. Use for termination cleanup
	related to the adapter.
	See \c #kSPAdaptersCaller, \c #SPAdaptersMessage, \c #SPAdaptersSuite.  */
#define kSPAdaptersShutdownSelector			"Shut down"
/** @ingroup Selectors
	Received by a plug-in with an adapter after a call to
	\c #SPCachesSuite::SPFlushCaches(), for a final flush of
	any memory used for private data, including PICA lists and
	string pools.
	See \c #kSPAdaptersCaller, \c #SPAdaptersMessage, \c #SPAdaptersSuite.  */
#define kSPAdaptersDisposeInfoSelector		"Dispose info"
/** @ingroup Selectors
	Received by a plug-in with an adapter when	the application
	frees memory, to allow garbage collection.
	See \c #kSPAdaptersCaller, \c #SPAdaptersMessage, \c #SPAdaptersSuite.  */
#define kSPAdaptersFlushSelector			"Flush"

// Second generation adapters (has property 'adpt'/2)
//---------------------------------------------------
/** @ingroup Selectors
	Received by a plug-in after its initialization, to allow it to
	register its own adapter. Get the adapter list from PICA and
	use \c #SPAdaptersSuite::AddAdapter( ) to register your adapter.
	For example:
	@code
SPAdapterRef oldAPI;
SPAdapterListRef adapterList;
SPErr error;
error = sSPRuntime->GetRuntimeAdapterList( &adapterList);
error = sSPAdapters->AddAdapter( adapterList, message->d.self, "old API adapter", &oldAPI );
	@endcode

	See \c #kSPAdaptersCaller, \c #SPAdaptersMessage, \c #SPAdaptersSuite.  */
#define kSPAdaptersRegisterPluginsSelector		"Register plugins"

/** @ingroup Selectors
	Received by a plug-in with an adapter before it is loaded, to allow the
	adapter to perform needed translations before the load occurs. The
	handler does not need to call the load operation.
	See \c #kSPAdaptersCaller, \c #SPAdaptersMessage, \c #SPAdaptersSuite.  */
#define kSPAdaptersLoadPluginSelector			"Load plugin"
/** @ingroup Selectors
	Received by a plug-in with an adapter before it is unloaded, to allow the
	adapter to perform needed translations before the unload occurs. The
	handler does not need to call the unload operation.
	See \c #kSPAdaptersCaller, \c #SPAdaptersMessage, \c #SPAdaptersSuite.  */
#define kSPAdaptersReleasePluginSelector		"Release plugin"

/** @ingroup Selectors
	Received by a plug-in with an adapter when the application needs to
	communicate with it. The adapter should relay the message, performing
	any required translation.
	See \c #kSPAdaptersCaller, \c #SPAdaptersMessage, \c #SPAdaptersSuite.  */
#define kSPAdaptersSendMessageSelector			"Send message"

// First generation adapters (no 'adpt' property, or 'adpt'/1 )
//-------------------------------------------------------------
// These messages are only for the "SP2 Adapter" that is built into
// Sweet Pea and maybe legacy adapters (i.e., AI6Adapter for AI7).
// For newer adapters, all of these methods are now deprecated.
// New adapters should do the vast majority of their processing via
// the message kSPAdaptersSendMessageSelector.
/** @deprecated Used internally. */
#define kSPAdaptersFindPropertySelector			"Find property"
/** @deprecated Used internally.  */
#define kSPAdaptersAboutSelector				"About"
/** @deprecated Used internally.   */
#define kSPAdaptersAcquireSuiteHostSelector		"Acquire Suite"
/** @deprecated Used internally.   */
#define kSPAdaptersReleaseSuiteHostSelector		"Release Suite"

/** PICA global list of registered plug-in adapters.
	@see \c #SPAdaptersSuite::AddAdapter(), \c #SPRuntimeSuite::GetRuntimeAdapterList(). */
#define kSPRuntimeAdapterList				((SPAdapterListRef)NULL)


/*******************************************************************************
 **
 ** Types
 **
 **/

/** An opaque reference to an adapter object. Access using the \c #SPAdaptersSuite. */
typedef struct SPAdapter *SPAdapterRef;
/** A list of adapter objects. Create with
	\c #SPAdaptersSuite::AllocateAdapterList(), or use
	the global list, \c #kSPRuntimeAdapterList. */
typedef struct SPAdapterList *SPAdapterListRef;
/** An iterator object for examining an adapter list.
	See \c #SPAdaptersSuite::NewAdapterListIterator(). */
typedef struct SPAdapterListIterator *SPAdapterListIteratorRef;

/** The message passed with all \c #kSPAdaptersCaller calls.
	Fields are used by specific selectors as indicated. */
typedef struct SPAdaptersMessage {
	/** The message data. All selectors. */
	SPMessageData d;
	/** The adapter object. All selectors. If you add more than
		one adapter for a plug-in, use this to determine which handler to use. */
	SPAdapterRef adapter;

	/** For \c #kSPAdaptersAboutSelector, the plug-in object for which to display information.
		No longer used. <<right?>> */
	struct SPPlugin *targetPlugin;
	/** For \c #kSPAdaptersAboutSelector, the target result of the handler, if any.
		No longer used. <<right?>>  */
	SPErr targetResult;

	/** For \c #kSPAdaptersFindPropertySelector.
		No longer used. */
	PIType vendorID;
	/** For \c #kSPAdaptersFindPropertySelector.
		No longer used. */
	PIType propertyKey;
	/** For \c #kSPAdaptersFindPropertySelector.
		No longer used.  */
	int32 propertyID;
	/** For \c #kSPAdaptersFindPropertySelector.
		No longer used.  */
	void *property;

	/** For \c #kSPAdaptersFlushSelector. The procedure with which to flush caches,
		passed from the call to \c #SPCachesSuite::SPFlushCaches().
		The adapter should call this to determine which plug-ins are being removed from
		memory, and unload them.  */
	SPFlushCachesProc flushProc;
	/** For \c #kSPAdaptersFlushSelector. Return the result of the flush procedure,
		the number of plug-ins removed.*/
	int32 flushed;

	/** For \c #kSPAdaptersAcquireSuiteHostSelector and \c #kSPAdaptersReleaseSuiteHostSelector.
	 	No longer used.*/
	struct SPSuiteList *suiteList;	/* use these if you need name, apiVersion, internalVersion */
	/** For \c #kSPAdaptersAcquireSuiteHostSelector and \c #kSPAdaptersReleaseSuiteHostSelector.
	 	No longer used.*/
	struct SPSuite *suite;
	/** For \c #kSPAdaptersAcquireSuiteHostSelector and \c #kSPAdaptersReleaseSuiteHostSelector.
	 	No longer used.*/
	struct SPPlugin *host;			/* plug-in hosting the suite, to be aquired/released by adapter */
	/** For \c #kSPAdaptersAcquireSuiteHostSelector and \c #kSPAdaptersReleaseSuiteHostSelector.
	 	No longer used.*/
	const void *suiteProcs;			/* returned here if reallocated */
	/** For \c #kSPAdaptersAcquireSuiteHostSelector and \c #kSPAdaptersReleaseSuiteHostSelector.
	 	No longer used.*/
	int32 acquired;					/* returned here */

	/** For \c #kSPAdaptersSendMessageSelector.
		The caller to pass to the adapted plug-in. */
	const char *plugin_caller;
	/** For \c #kSPAdaptersSendMessageSelector.
		The selector to pass to the adapted plug-in. */
	const char *plugin_selector;
	/** For \c #kSPAdaptersSendMessageSelector.
		The message to pass to the adapted plug-in. */
	void *plugin_message;
} SPAdaptersMessage;

/** A string pool structure. See \c #SPStringsSuite. */
struct SPStringPool;

/*******************************************************************************
 **
 ** Suite
 **
 **/
/** @ingroup Suites
	An adapter is an interface between the PICA plug-in manager and
	an individual plug-in. PICA and application plug-ins are hosted by
	internal PICA adapters. Plug-ins can add other adapters to PICA's
	\e adapter \e list, allowing non-PICA plug-ins to run under the PICA API.

	\li Acquire this suite using \c #SPBasicSuite::AcquireSuite() with the constants
		\c #kSPAdaptersSuite and \c #kSPAdaptersSuiteVersion.

	An adapter searches the PICA \e file \e list (\c #kSPRuntimeFileList)
	for plug-ins types that it supports, and adds them to the \e plug-in \e list
	(\c #kSPRuntimePluginList). When notified by PICA to do so, the adapter is
	responsible for loading and calling the plug-ins it adds, and must
	do any conversion of messages, data structures or other API elements.
	An adapter can be used to update

	There are always at least two adapters, the internal adapters for PICA
	and for the application, in the  global adapter list (\c #kSPRuntimeAdapterList).
	The internal adapters translate any legacy function calls into those
	currently supported by PICA and the application.

	You can provide an adapter for backward compatability with older
	versions of your own plug-ins, or to interpret your own file types.
	To make an adapter available for your plug-in, add it in response to the
	\c #kSPAdaptersRegisterPluginsSelector when your plug-in is loaded.

	You can also use this suite to create and maintain your own adapter list,
	in addition to the global list.

	Use the adapter to load and call the adapted plug-in. For example, to
	verify that a message could be sent to a plug-in:
@code
SPErr error;
SPPluginRef pluginToCall;
SPAdapterRef pluginsAdapter;
char *adapterName;
int32 adapterVersion;

error = sSPPlugins->GetPluginAdapter( pluginToCall, &pluginsAdapter );
error = sSPAdapters->GetAdapterName( pluginsAdapter, &adapterName );
if ( strcmp( adapterName, kSPSweetPea2Adapter ) == 0 ) {
	// it is a PICA plug-in, call it as such with sSPInterface.
	} else if ( strcmp( adapterName, "MYAPP Legacy Plug-in Adapter" ) == 0 ) {
		// it is an adapted plug-in, call it with the adapter's
		// interface suite
		error = sSPAdapters->GetAdapterVersion( pluginsAdapter, &adapterVersion );
		if ( adapterVersion == 1 ) {
			// use one hypothetical interface suite
			} else if ( adapterVersion == 2) {
			// use another hypothetical interface suite
		}
	}
@endcode
*/
typedef struct SPAdaptersSuite {

	/** Allocates a new list of adapters. You can keep your own list,
		or obtain the global list with \c #SPRuntimeSuite::GetRuntimeAdapterList().
			@param stringPool The string pool in which to keep adapter names.
			@param adapterList [out] A buffer in which to return the new list object.
		*/
	SPAPI SPErr (*AllocateAdapterList)( struct SPStringPool *stringPool, SPAdapterListRef *adapterList );
	/** Frees a list of adapters allocated with \c #AllocateAdapterList(), and
		also frees any entries in the list. Do not free the global list (\c #kSPRuntimeAdapterList).
			@param adapterList The adapter list object.
		*/
	SPAPI SPErr (*FreeAdapterList)( SPAdapterListRef adapterList );
	/** Creates a new adapter object and adds it to an adapter list. Do this in
		response to the \c #kSPAdaptersRegisterPluginsSelector message.
			@param adapterList The adapter list object, or \c NULL to use the
				global list.
			@param host	This plug-in object, for which the adapter is responsible.
			@param name The unique, identifying name of the adapter.
			@param version The version number of the adapter. Only the latest version
				of any adapter is used to start up plug-ins.
			@param adapter [out] A buffer in which to return the new adapter object, or
				\c NULL if you add only one adapter. If you add more than one adapter,
				compare this to \c #SPAdaptersMessage::adapter to determine which handler to use.
			@see \c #AllocateAdapterList()
		*/
	SPAPI SPErr (*AddAdapter)( SPAdapterListRef adapterList, struct SPPlugin *host, const char *name,
				int32 version, SPAdapterRef *adapter );

	/** Retrieves an adapter by name.
			@param adapterList The adapter list object, or \c NULL to use the
				global list.
			@param name The unique, identifying name of the adapter.
			@param adapter [out] A buffer in which to return the adapter object.
		*/
	SPAPI SPErr (*SPFindAdapter)( SPAdapterListRef adapterList, const char *name, SPAdapterRef *adapter );

	/** Creates an iterator object with which to traverse an adapter list.
		The iterator is initially set to the first adapter in the list.
			@param adapterList The adapter list object, or \c NULL to use the
				global list.
			@param iter [out] A buffer in which to return the new iterator object.
			@see \c #NextAdapter(), \c #DeleteAdapterListIterator()
		*/
	SPAPI SPErr (*NewAdapterListIterator)( SPAdapterListRef adapterList, SPAdapterListIteratorRef *iter );
	/** Retrieves the current adapter and advances an adapter-list iterator to the
		next adapter in the list.
			@param iter The adapter-list iterator object.
			@param adapter [out] A buffer in which to return the current adapter object, \c NULL
				if the end of the list has been reached.
		    @see \c #NewAdapterListIterator(),
		*/
	SPAPI SPErr (*NextAdapter)( SPAdapterListIteratorRef iter, SPAdapterRef *adapter );
	/** Frees an adapter-list iterator that is no longer needed.
			@param iter The adapter-list iterator object.
			@see \c #NewAdapterListIterator(),
		*/
	SPAPI SPErr (*DeleteAdapterListIterator)( SPAdapterListIteratorRef iter );

	/** Retrieves the plug-in that an adapter manages.
			@param adapter The adapter object.
			@param plug-in [out] A buffer in which to return the plug-in object.
		*/
	SPAPI SPErr (*GetAdapterHost)( SPAdapterRef adapter, struct SPPlugin **plugin );
	/** Retrieves the unique, identifying name of an adapter.
			@param adapter The adapter object.
			@param name [out] A buffer in which to return the name string.
		*/
	SPAPI SPErr (*GetAdapterName)( SPAdapterRef adapter, const char **name );
	/** Retrieves the version of an adapter.
			@param adapter The adapter object.
			@param version [out] A buffer in which to return the version number.
		*/
	SPAPI SPErr (*GetAdapterVersion)( SPAdapterRef adapter, int32 *version );

} SPAdaptersSuite;


/** Internal */
SPAPI SPErr SPAllocateAdapterList( struct SPStringPool *stringPool, SPAdapterListRef *adapterList );
/** Internal */
SPAPI SPErr SPFreeAdapterList( SPAdapterListRef adapterList );

/** Internal */
SPAPI SPErr SPAddAdapter( SPAdapterListRef adapterList, struct SPPlugin *host, const char *name,
			int32 version, SPAdapterRef *adapter );

/** Internal */
SPAPI SPErr SPFindAdapter( SPAdapterListRef adapterList, const char *name, SPAdapterRef *adapter );

/** Internal */
SPAPI SPErr SPNewAdapterListIterator( SPAdapterListRef adapterList, SPAdapterListIteratorRef *iter );
/** Internal */
SPAPI SPErr SPNextAdapter( SPAdapterListIteratorRef iter, SPAdapterRef *adapter );
/** Internal */
SPAPI SPErr SPDeleteAdapterListIterator( SPAdapterListIteratorRef iter );

/** Internal */
SPAPI SPErr SPGetAdapterHost( SPAdapterRef adapter, struct SPPlugin **plugin );
/** Internal */
SPAPI SPErr SPGetAdapterName( SPAdapterRef adapter, const char **name );
/** Internal */
SPAPI SPErr SPGetAdapterVersion( SPAdapterRef adapter, int32 *version );


/*******************************************************************************
 **
 ** Errors
 **
 **/

#include "SPErrorCodes.h"


#ifdef __cplusplus
}
#endif

#endif
