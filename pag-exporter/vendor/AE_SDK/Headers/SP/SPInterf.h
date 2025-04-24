/***********************************************************************/
/*                                                                     */
/* SPInterf.h                                                          */
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

#ifndef __SPInterface__
#define __SPInterface__


/*******************************************************************************
 **
 **	Imports
 **
 **/

#include "SPTypes.h"
#include "SPBasic.h"
#include "SPFiles.h"
#include "SPMData.h"
#include "SPPlugs.h"
#include "SPProps.h"

#ifdef __cplusplus
extern "C" {
#endif


/*******************************************************************************
 **
 ** Constants
 **
 **/
/** PICA Interface suite name. */
#define kSPInterfaceSuite				"SP Interface Suite"
/** PICA Interface suite version. */
#define kSPInterfaceSuiteVersion		2

/** PICA messaging system caller; see \c #SPInterfaceSuite. */
#define kSPInterfaceCaller				"SP Interface"
/** PICA messaging system startup; see \c #SPInterfaceSuite.  */
#define kSPInterfaceStartupSelector		"Startup"
/** PICA messaging system shutdown; see \c #SPInterfaceSuite. */
#define kSPInterfaceShutdownSelector	"Shutdown"
/**  PICA messaging system request for information; see \c #SPInterfaceSuite.
	Illustrator sends this call to all plug-ins to implement
	the "About Plug-ins" feature.*/
#define kSPInterfaceAboutSelector		"About"

/** Adapter name for PICA version 2. */
#define kSPSweetPea2Adapter				"Sweet Pea 2 Adapter"
/** Adapter version for PICA version 2.*/
#define kSPSweetPea2AdapterVersion		1

/*******************************************************************************
 **
 ** Types
 **
 **/

/** A basic message, sent with \c #kSPInterfaceCaller. */
typedef struct SPInterfaceMessage {

	/** The message data. */
	SPMessageData d;

} SPInterfaceMessage;


/*******************************************************************************
 **
 ** Suite
 **
 **/
/** @ingroup Suites
	This suite provides is the ability for a plug-in to call
	other plug-ins, by sending a message to the main entry point.
	This is how the application communicates with plug-ins.

	Use \c #SetupMessageData() to prepare the message for a call,
	\c #SendMessage() to send the call with the message, and
	\c #EmptyMessageData() to terminate the operation, allowing
	PICA to release the basic suite and store global variables.

	These calls work only with PICA plug-ins. Before making the calls,
	use  \c #SPAdaptersSuite::GetAdapterName() to determine that
	the target is a PICA plug-in.  For non-PICA plug-ins, use the
	interface suite provided by the adapter. See \c #SPAdaptersSuite.

 	\li Acquire this suite using \c #SPBasicSuite::AcquireSuite() with the constants
		\c #kSPInterfaceSuite and \c #kSPInterfaceSuiteVersion.
*/
typedef struct SPInterfaceSuite {

	/** Sends a message to a PICA plug-in, loading it if needed and
		passing the caller, selector, and message to the main entry point.
			@param plug-in The target plug-in object.
			@param caller The caller constant. See @ref Callers.
			@param selector The selector constant. See @ref Selectors.
			@param message The message, initialized by \c #SetupMessageData().
			@param result [out] A buffer in which to return the result of the call,
				as returned by the target plug-in.
		*/
	SPAPI SPErr (*SendMessage)( SPPluginRef plugin, const char *caller, const char *selector,
				void *message, SPErr *result );

	/** Initializes a message to be sent with \c #SendMessage().
		The function fills in the basic suite, the plug-in reference,
		and the globals pointer that PICA keeps for that plug-in.
		You must provide any additional data needed.
			@param plugin The target plug-in object.
			@param data The message structure, initialized with data required
				for the intended call.
		*/
	SPAPI SPErr (*SetupMessageData)( SPPluginRef plugin, SPMessageData *data );
	/** Terminates a call to another plug-in, releasing the basic suite and
		updating the target plug-in's globals pointer, in case it has changed.
		Use after a call to \c #SendMessage().
			@param plugin The target plug-in object.
			@param data The message structure, updated during the call.
		*/
	SPAPI SPErr (*EmptyMessageData)( SPPluginRef plugin, SPMessageData *data );

	/** Starts up the plug-in in a plug-in list that exports a given suite.
		Searches in the given plug-in list for the plug-in that exports the named
		suite, and, if found, sends it the startup message.
			@param pluginList The plug-in list object. Access PICA's global plug-in
				list using \c #SPRuntimeSuite::GetRuntimePluginList(),
				or create your own lists with \c #SPPluginsSuite::AllocatePluginList().
			@param name The suite name constant.
			@param version The suite version number constant.
			@param started [out] A buffer in which to return true (non-zero) if a
				plug-in that exports the suite was found, false (0) if not.
		*/
	SPAPI SPErr (*StartupExport)( SPPluginListRef pluginList, const char *name, int32 version,
				int32 *started );

} SPInterfaceSuite;


/** Internal */
SPAPI SPErr SPSendMessage( SPPluginRef plugin, const char *caller, const char *selector,
			void *message, SPErr *result );

/** Internal */
SPAPI SPErr SPSetupMessageData( SPPluginRef plugin, SPMessageData *data );
/** Internal */
SPAPI SPErr SPEmptyMessageData( SPPluginRef plugin, SPMessageData *data );

/** Internal */
SPAPI SPErr SPStartupExport( SPPluginListRef pluginList, const char *name, int32 version,
			int32 *started );


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
