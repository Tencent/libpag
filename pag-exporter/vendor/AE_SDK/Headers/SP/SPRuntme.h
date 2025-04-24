/***********************************************************************/
/*                                                                     */
/* SPRuntme.h                                                          */
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

#ifndef __SPRuntime__
#define __SPRuntime__


/*******************************************************************************
 **
 **	Imports
 **
 **/

#include "SPTypes.h"
#include "SPAdapts.h"
#include "SPFiles.h"
#include "SPPlugs.h"
#include "SPStrngs.h"
#include "SPSuites.h"
#include "SPStrngs.h"

#ifdef __cplusplus
extern "C" {
#endif


/*******************************************************************************
 **
 **	Constants
 **
 **/

#define kSPRuntimeSuite				"SP Runtime Suite"
#define kSPRuntimeSuiteVersion		5
#define kSPRuntimeSuiteXPlatVersion	6


/*******************************************************************************
 **
 **	Types
 **
 **/

/*	INTERNAL DOCS
 *	PICA makes callbacks into the host through the host procs. The host
 *	procs are filled in by the host and passed to Sweet Pea at SPInit().
 *
 *	hostData - data that is given back to each host proc when Sweet Pea
 *		calls it. Sweet Pea does nothing with it itself.
 *
 *	extAllocate - implementation of the Block Suite's AllocateBlock() routine.
 *		It is identical to ANSI C malloc(). It returns a pointer to the
 *		beginning of the allocated block or NULL.
 *
 *	extFree - implementation of the Block Suite's FreeBlock() routine. It is
 *		identical to ANSI C free(). Note that you can pass it NULL.
 *
 *	extReallocate - implementation of the Block Suite's ReallocateBlock()
 *		routine. It is identical to ANSI C realloc(). It returns a pointer
 *		to the resized block or NULL. Note that you can pass it NULL or a
 *		newSize of 0.
 *
 *	intAllocate, intFree, intReallocate - routines used by Sweet Pea for
 *		its own memory needs. You may want to allocate blocks differently
 *		with plug-ins and Sweet Pea. Plug-ins are unbounded in their memory
 *		needs, while Sweet Pea's memory usage can be approximated.
 *
 *	startupNotify - called as each plug-in is started up. This is intended
 *		as a way to tell the user what's happening during start up.
 *		Note that plug-ins may start up at any time, not just during
 *		SPStartupPlugins().
 *
 *	shutdownNotify - called as each plug-in is shut down. Also intended as
 *		a way to let users know what's going on.
 *
 *	assertTrap - called when a fatal assert is triggered. Sweet Pea does
 *		not expect execution to continue after an assert.
 *
 *	throwTrap - called when an internal error is thrown. This can be used
 *		during debugging to catch errors as they happen. It should return
 *		to allow Sweet Pea to handle the error.
 *
 *
 *	To aid in getting Sweet Pea up and running quickly, you can set any of
 *	these to NULL and Sweet Pea will use a default implementation. However:
 *	you cannot mix your implementations of the memory routines with
 *	Sweet Pea's defaults.
 *
 *
 *	The string pool functions replace the default routines used internally
 *	and exported by the Strings suite.  Because they are exported, the behaviors
 *	listed below should be followed.
 *
 *	allocateStringPool - creates a new string pool instance. The host app and
 *		Sweet Pea have a string pool which can be used by a plug-in, or a plug-in
 *		can create its own.  See the notes in SPStrngs.h on how the pool is
 *		implemented.
 *		The function should return kSPNoError if the pool is allocated successfully
 *		or kSPOutOfMemoryError if allocation fails.
 *
 *	freeStringPool - disposes of the string pool and any associated memory.  The
 *		funtion should return kSPNoError
 *
 *	makeWString - the string pool keeps a list of added strings. When a new string is
 *		added with MakeWString(), the routine checks to see if it is already in the
 *		pool.  If so, the address of the string instance in the pool is returned.  If
 *		not, it will add it to the pool and return the address of the newly
 *		created string instance.  The behavior is:
 *
 *			if ( string == NULL )
 *					*wString = NULL;
 *					returns kSPNoError;
 *			else if ( string in string pool )
 *					*wString = found string;
 *					returns kSPNoError;
 *			else add string
 *				if successful
 *					*wString = new string;
 *					returns kSPNoError;
 *				else
 *					*wString = nil
 *					returns kSPOutOfMemoryError
 *
 *	appStringPool - if the host application has already allocated a string pool to use,
 *		it's reference should be passed here.  If this value is NULL, Sweet Pea will
 *		allocate the pool when initialized and dispose of it at termination.
 *
 *	filterEvent - a function called for each event allowing the host to cancel it.
 *		The event type is indicative of what the filter is to do.  A  file validation
 *		is called before a directory entry is added to the file list (kAddFile).
 *		A plug-in validation before a file is checked for PiPL information (kAddPlugin);
 *		the host might examine the file name/type to determine whether it should be added.
 *		For these 'add' events the return value is TRUE if the item should be skipped
 *		or FALSE if should be should be added. The default filter proc, used (if NULL)
 *		is passed, will skip files/folders in ( ).
 *		The other event is kSuitesAvailable.  It is called when the last suite adding
 *		plug-in	(as determined by available PiPL information) has been added.  This is
 *		a point at which the host can cancel the startup process; for instance, if the host
 *		requires a suite from a plug-in, this is the time to check for it.  If the
 *		host returns TRUE, the startup process continues.  If it returns FALSE, the
 *		plug-in startup is canceled and the host would likely terminate or startup in
 *		an alternate manner.
 *
 *	overrideAddPlugins - if supplied, SP will call the host to create the runtime
 *		plug-in list.  This occurs at SPStartupPlugins().  The function takes no parameters
 *		as it is up to the host to determine how to do this.  For instance, the host can do
 *		this from cached data or, as SP would, from the file list.  A returned error will
 *		stop the plug-in startup process.
 *
 *	overrideStartup - a function called for each SP2 plug-in before it is sent the
 *		startup message.  If the host returns FALSE, SP will startup the plug-in normal.
 *		If the host returns true, it is assumed that the host has handled the startup
 *		for the plug-in, so SP will not do anything for the plug-in.  This is intended
 *		to be used with a plug-in caching scheme.
 *		The host would be responsible, for instance, for defining the cacheable
 * 		information in the PiPL, adding it when the callback is made, and later issuing
 *		a startup message when the plug-in is actually needed (e.g. when a menu item
 *		is selected.)  Two notes: don't forget to SetPluginStarted(), and make sure
 *		to use a string pooled char* to kSPInterfaceCaller and kSPInterfaceStartupSelector.
 *
 *	resolveLink - Windows only.  If the search for plug-ins is to recurse sub-folders,
 *		the host needs to suply this routine.  When a .lnk file is encountered, the
 *		resolveLink host callback function will be called and should return a resolved path.
 *		This is a host callback due to OLE issues such as initialization, which the SP
 *		libary does not currently handle.  If it returns an error code, the result will
 *		be ignored.
 *
 *	getPluginAccess - Allows the host to set the plug-in access information.  This would
 *		be used if, for instance, the host kept its own plug-in list (ala, Photoshop), but
 *		still needed these to be compatible with SPPlugins (e.g. whose accesses are used by ADM)
 *
 *	memoryIsCritical - Mac only.  Allows the host to indicate that memory is in a critical state
 *		(really low, but can't be purged because you are, say, shutdown.)
 * 		If so and the plug-in load target heap is the app heap, when a plug-in fails to load
 *		SP will then try to load the plug-in into the system heap
 */

/* These are passed in startup and shutdown host notify procs and the filter file proc. */
/** A notification event type, that an adapter passes to the \c #SPStartupNotifyProc
	and \c #SPShutdownNotifyProc when the associated plug-in is loaded or unloaded.
	*/
typedef enum {
	/** Sent to the \c #SPStartupNotifyProc after a file has been added as a plug-in.
		The \c notifyData value is the plug-in object, an \c #SPPluginRef. */
	kAddFile,				/* 	Internal: for filter file, received before a file is
								added to a file list, notifyData is a pointer to the
								SPPlatformFileSpecification */
	/** Sent to the \c #SPStartupNotifyProc after a plug-in has been added.
		The \c notifyData value is the plug-in object, an \c #SPPluginRef. */
	kAddPlugin,				/* 	Internal: for filter file, received before a file is
								checked to see if it is a plugin, notifyData is the
								files SPFileRef */
	/** Sent to the \c #SPStartupNotifyProc to specify a general message for the application splash screen.
		The \c notifyData value is a pointer to a C string, <code>char**</code>. */
	kSetMessage,
	/** Internal */
	kSuitesAvailable,		/*  Internal: used only by event filter to allow host to
								check for suites it requires, notifyDatais NULL */
	/** Internal */
	kError,					/* 	Internal: notifyData is SPErrorDataPtr*/
	/** Sent to the \c #SPStartupNotifyProc after the plug-in is started.
		The \c notifyData value is the plug-in object, an \c #SPPluginRef. */
	kStartingupPlugin,		/*  Internal: for filter file, received before a file is
								checked to see if it is a plugin, notifyData is the
								files SPFileRef */
	kXPlatError,			/* 	Internal: notifyData is SPXPlatErrorData */
							/** Sent to the \c #SPStartupNotifyProc after the plug-in is started.
								The \c notifyData value is the plug-in object, an \c #SPPluginRef. */
	/** Internal */
	kNoEvent = 0xffffffff
 } NotifyEvent;

/** Internal */
typedef void *(*SPAllocateProc)( size_t size, void *hostData );
/** Internal */
typedef void (*SPFreeProc)( void *block, void *hostData );
/** Internal */
typedef void *(*SPReallocateProc)( void *block, size_t newSize, void *hostData );
/** Called by an adapter to inform the application that a plug-in is being started up.
	The application uses this information to track the start-up process; for example,
	to display a list of plug-ins being loaded.
		@param event The notification event constant that identifies which event occurred.
		@param notifyData A pointer to plug-in-defined initialization data.
		@param hostData	A pointer to application-defined initialization data.
		@return Nothing.
	*/
typedef void (*SPStartupNotifyProc)( NotifyEvent event, void *notifyData, void *hostData );
/** Called by an adapter to inform the application that a plug-in is being shut down.
	The application uses this information to track the shut-down process.
		@param event The notificatin event.
		@param notifyData A pointer to plug-in-defined termination data.
		@param hostData	A pointer to application-defined termination data.
		@return Nothing.
	*/
typedef void (*SPShutdownNotifyProc)( NotifyEvent event, void *notifyData, void *hostData );
/** Internal */
typedef void (*SPAssertTrapProc)( const char *failMessage, void *hostData );
/** Internal */
typedef void (*SPThrowTrapProc)( SPErr error, void *hostData );
/** Internal */
typedef void (*SPDebugTrapProc)( const char *debugMessage, void *hostData );

/** Internal */
typedef SPAPI SPErr (*SPAllocateStringPoolProc)( SPStringPoolRef *pool );
/** Internal */
typedef SPAPI SPErr (*SPFreeStringPoolProc)( SPStringPoolRef stringPool );
/** Internal */
typedef SPAPI SPErr (*SPMakeWStringProc)( SPStringPoolRef stringPool, const char *string,
			const char **wString );

/** Internal */
typedef SPAPI SPErr (*SPGetHostAccessInfoProc)( SPPlatformAccessInfo *spHostAccessInfo );

/** Internal */
typedef SPAPI SPBoolean (*SPFilterEventProc)( NotifyEvent event, const void *eventData );
/** Internal */
typedef SPAPI SPErr (*SPAddPluginsProc)( void );
/** Internal */
typedef SPAPI SPBoolean (*SPOverrideStartupProc)( SPPluginRef currentPlugin );

#if defined(WIN_ENV) || defined(ANDROID_ENV)
/** Internal */
typedef SPAPI SPErr (*SPResolveLinkProc)(const char *shortcutFile, char *resolvedPath);
#endif

/** Internal */
typedef SPAPI SPErr (*GetNativePluginAccessProc)(SPPluginRef plugin, SPAccessRef *access);

/** Internal */
typedef SPAPI SPBoolean (*MemoryIsCriticalProc)( void );

/** Callback procedures provided to PICA by the application.
	Plug-ins do not use these, except for adapters, which
	call the initialization and termination procedures.
	@see \c #SPRuntimeSuite::GetRuntimeHostFileSpec() */
typedef struct SPHostProcs {

	void *hostData;

	SPAllocateProc extAllocate;
	SPFreeProc extFree;
	SPReallocateProc extReallocate;

	SPAllocateProc intAllocate;
	SPFreeProc intFree;
	SPReallocateProc intReallocate;
	/** Plug-in initialization procedure */
	SPStartupNotifyProc startupNotify;
	/** Plug-in termination procedure */
	SPShutdownNotifyProc shutdownNotify;

	SPAssertTrapProc assertTrap;
	SPThrowTrapProc throwTrap;
	SPDebugTrapProc debugTrap;

	SPAllocateStringPoolProc allocateStringPool;
	SPFreeStringPoolProc freeStringPool;
	SPMakeWStringProc makeWString;
	SPStringPoolRef appStringPool;

	SPFilterEventProc filterEvent;
	SPAddPluginsProc overrideAddPlugins;
	SPOverrideStartupProc overridePluginStartup;

#if defined(WIN_ENV) || defined(ANDROID_ENV)
	SPResolveLinkProc resolveLink;
#endif

	GetNativePluginAccessProc getPluginAccess;

#ifdef MAC_ENV
	// enable second-chance plugin loading for success-critical situations
	MemoryIsCriticalProc memoryIsCritical;
#endif

} SPHostProcs;


/*******************************************************************************
 **
 **	Suite
 **
 **/
/** @ingroup Suites
	This suite allows you to obtain specific references to the
	PICA global lists and string pool.

	\li Acquire this suite using \c #SPBasicSuite::AcquireSuite() with the constants
		\c #kSPRuntimeSuite and \c #kSPRuntimeSuiteVersion.
	*/
typedef struct SPRuntimeSuite {
	/** Retrieves the PICA global string pool.
			@param stringPool [out] A buffer in which to return the string-pool object.
			@see \c #SPStringsSuite
		*/
	SPAPI SPErr (*GetRuntimeStringPool)( SPStringPoolRef *stringPool );
	/** Retrieves the PICA global suite list.
			@param suiteList [out] A buffer in which to return the list object.
			@see \c #SPSuitesSuite
		*/
	SPAPI SPErr (*GetRuntimeSuiteList)( SPSuiteListRef *suiteList );
	/** Retrieves the PICA global file list.
			@param fileList [out] A buffer in which to return the list object.
			@see \c #SPFilesSuite
		*/
	SPAPI SPErr (*GetRuntimeFileList)( SPFileListRef *fileList );
	/** Retrieves the PICA global plug-in list.
			@param pluginList [out] A buffer in which to return the list object.
			@see \c #SPPluginsSuite
		*/
	SPAPI SPErr (*GetRuntimePluginList)( SPPluginListRef *pluginList );
	/** Retrieves the PICA global adapter list.
			@param adapterList [out] A buffer in which to return the list object.
			@see \c #SPAdaptersSuite
		*/
	SPAPI SPErr (*GetRuntimeAdapterList)( SPAdapterListRef *adapterList );
	/** Retrieves the block of function pointers supplied to PICA by the
		application, which contains memory management routines, notification routines,
		exception handling, and string pool routines.

		A plug-in does not normally call the host functions directly; you
		can use the PICA suite functions for most operations. An adapter, however,
		uses the host functions for start-up and shut-down notification.
			@param hostProcs [out] A buffer in which to return a pointer to the
				block of function pointers.
		*/
	SPAPI SPErr (*GetRuntimeHostProcs)( SPHostProcs **hostProcs );
	/** Retrieves the location of the application's plug-in folder.
			@param pluginFolder [out] A buffer in which to return the
				file specification for the directory that contains plug-ins.
		*/
	SPAPI SPErr (*GetRuntimePluginsFolder)( SPPlatformFileSpecification *pluginFolder );
	/** Retrieves the location of the application's executable file.
			@param hostFileSpec [out] A buffer in which to return the
				file specification for the application's executable file.
		*/
	SPAPI SPErr (*GetRuntimeHostFileSpec)( SPPlatformFileSpecification *hostFileSpec );
} SPRuntimeSuite;



/*******************************************************************************
 **
 **	Suite
 **
 **/
/** @ingroup Suites
	This suite allows you to obtain specific references to the
	PICA global lists and string pool using the new XPlatFileSpec
	rather than the old SPPlatformFileSpecification.

	\li Acquire this suite using \c #SPBasicSuite::AcquireSuite() with the constants
		\c #kSPRuntimeSuite and \c #kSPRuntimeSuiteXPlatVersion.
	*/
typedef struct SPXPlatRuntimeSuite {
	/** Retrieves the PICA global string pool.
			@param stringPool [out] A buffer in which to return the string-pool object.
			@see \c #SPStringsSuite
		*/
	SPAPI SPErr (*GetRuntimeStringPool)( SPStringPoolRef *stringPool );
	/** Retrieves the PICA global suite list.
			@param suiteList [out] A buffer in which to return the list object.
			@see \c #SPSuitesSuite
		*/
	SPAPI SPErr (*GetRuntimeSuiteList)( SPSuiteListRef *suiteList );
	/** Retrieves the PICA global file list.
			@param fileList [out] A buffer in which to return the list object.
			@see \c #SPFilesSuite
		*/
	SPAPI SPErr (*GetRuntimeFileList)( SPFileListRef *fileList );
	/** Retrieves the PICA global plug-in list.
			@param pluginList [out] A buffer in which to return the list object.
			@see \c #SPPluginsSuite
		*/
	SPAPI SPErr (*GetRuntimePluginList)( SPPluginListRef *pluginList );
	/** Retrieves the PICA global adapter list.
			@param adapterList [out] A buffer in which to return the list object.
			@see \c #SPAdaptersSuite
		*/
	SPAPI SPErr (*GetRuntimeAdapterList)( SPAdapterListRef *adapterList );
	/** Retrieves the block of function pointers supplied to PICA by the
		application, which contains memory management routines, notification routines,
		exception handling, and string pool routines.

		A plug-in does not normally call the host functions directly; you
		can use the PICA suite functions for most operations. An adapter, however,
		uses the host functions for start-up and shut-down notification.
			@param hostProcs [out] A buffer in which to return a pointer to the
				block of function pointers.
		*/
	SPAPI SPErr (*GetRuntimeHostProcs)( SPHostProcs **hostProcs );
	/** Retrieves the location of the application's plug-in folder.
			@param pluginFolder [out] A buffer in which to return the
				file specification for the directory that contains plug-ins.
		*/
	SPAPI SPErr (*XPlatGetRuntimePluginsFolder)( XPlatFileSpec *pluginFolder );
	/** Retrieves the location of the application's executable file.
			@param hostFileSpec [out] A buffer in which to return the
				file specification for the application's executable file.
		*/
	SPAPI SPErr (*XPlatGetRuntimeHostFileSpec)( XPlatFileSpec *hostFileSpec );
} SPXPlatRuntimeSuite;


/** Internal */
SPAPI SPErr SPGetRuntimeStringPool( SPStringPoolRef *stringPool );
/** Internal */
SPAPI SPErr SPGetRuntimeSuiteList( SPSuiteListRef *suiteList );
/** Internal */
SPAPI SPErr SPGetRuntimeFileList( SPFileListRef *fileList );
/** Internal */
SPAPI SPErr SPGetRuntimePluginList( SPPluginListRef *pluginList );
/** Internal */
SPAPI SPErr SPGetRuntimeAdapterList( SPAdapterListRef *adapterList );
/** Internal */
SPAPI SPErr SPGetRuntimeHostProcs( SPHostProcs **hostProcs );
/** Internal */
SPAPI SPErr SPGetRuntimePluginsFolder( SPPlatformFileSpecification *pluginFolder );
/** Internal */
SPAPI SPErr SPXPlatGetRuntimePluginsFolder( XPlatFileSpec *pluginFolder );
/** Internal */
SPAPI SPErr SPGetRuntimeHostFileSpec( SPPlatformFileSpecification *hostFileSpec );
/** Internal */
SPAPI SPErr SPXPlatGetRuntimeHostFileSpec( XPlatFileSpec *hostFileSpec );

/** Internal */
typedef struct
{
	SPAPI SPErr (*SPAcquireSuiteFunc)( SPSuiteListRef suiteList, const char *name, int32 apiVersion, int32 internalVersion, const void **suiteProcs );
	SPAPI SPErr (*SPReleaseSuiteFunc)( SPSuiteListRef suiteList, const char *name, int32 apiVersion, int32 internalVersion );
	SPErr (*spAllocateBlockFunc)( SPAllocateProc allocateProc, size_t size, const char *debug, void **block );
	SPErr (*spFreeBlockFunc)( SPFreeProc freeProc, void *block );
	SPErr (*spReallocateBlockFunc)( SPReallocateProc reallocateProc, void *block, size_t newSize, const char *debug, void **newblock );
	SPHostProcs *gProcs;
} SPBasicFuncStruct;

/** Internal */
void SetUpBasicFuncs(SPBasicFuncStruct *inStruct);

#ifdef __cplusplus
}
#endif

#endif
