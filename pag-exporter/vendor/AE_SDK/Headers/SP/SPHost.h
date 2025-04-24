/***********************************************************************/
/*                                                                     */
/* SPHost.h                                                            */
/*                                                                     */
/* Copyright 1995-1999 Adobe Systems Incorporated.                     */
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

#ifndef __SPHost__
#define __SPHost__


/*******************************************************************************
 **
 **	Imports
 **
 **/

#include "SPTypes.h"
#include "SPAccess.h"
#include "SPAdapts.h"
#include "SPBasic.h"
#include "SPBckDbg.h"
#include "SPBlocks.h"
#include "SPCaches.h"
#include "SPFiles.h"
#include "SPInterf.h"
#include "SPPlugs.h"
#include "SPProps.h"
#include "SPRuntme.h"
#include "SPStrngs.h"
#include "SPSuites.h"

#ifdef __cplusplus
extern "C" {
#endif


/*******************************************************************************
 **
 **	Constants
 **
 **/


/*
 *	The version number of the Sweet Pea host interface. You can compare it to the
 *	version number that Sweet Pea was compiled with to determine that you are
 *	running the version you think you are. See SPVersion() below.
 *
 *	This number consists of a hi word which is the major version number reflecting
 *	changes to the SP inteface, and a low word which is the minor revision number,
 *	for instance indicating bug fixes.
 */

#define kSPVersion		0x000B0002			// version 11.2


/*
 *	Options available to the host at init time. They are:
 *
 *	kSPHostBlockDebugOption - block allocation debugging is enabled. See
 *		SPBckDbg.h for details.
 *
 *	Examples:
 *
 *		SPInit( hostProcs, pluginsFolder, kSPHostNoOptions, error );
 *		// No debugging.
 *
 *		SPInit( hostProcs, pluginsFolder, kSPHostBlockDebugOption, error );
 *		// Enable block debugging.
 */

#define kSPHostNoOptions			0
#define kSPHostBlockDebugOption		(1<<0L)


/*******************************************************************************
 **
 **	Functions
 **
 **/

/*
 *	SPInit() initializes Sweet Pea. It is the first call made to Sweet Pea. It
 *	initializes its suites and builds the list of application files.
 *
 *	HostProcs points to a filled-in structure of host callbacks, as described
 *	in SPRuntme.h. It may be NULL, in which case Sweet Pea uses its default
 *	versions for all of the host callbacks. PluginFolder is the file spec of the
 *	plug-ins folder. The contents of this folder are collected into the application
 *	files list (see SPFiles.h). Options may be any of the host options described
 *	above.
 */
SPErr SPInit( SPHostProcs *hostProcs, const SPPlatformFileSpecification *pluginFolder, int32 options );

/*
 *	SPXPlatInit() uses the XPlatFileSpec rather than the SPPlatformFileSpecification.
 *
 *	Otherwise it's identical to SPInit (and actually calls through to it for Photoshop 17.0)
 */
SPErr SPXPlatInit( SPHostProcs *hostProcs, const XPlatFileSpec *pluginFolder, int32 options );

SPBoolean SPInited( void );

/*
 *	SPTerm() terminates Sweet Pea. Call this when your application quits.
 */
SPErr SPTerm( void );

/*
 *	SPStartupPlugins() scans the list of application files and builds the list
 *	of plug-ins. It then starts them up, calling each in turn with a start up
 *	message (see SPPlugs.h). Call this sometime after SPInit().
 */
SPErr SPStartupPlugins( void );

/*
 *	SPShutdownPlugins() shuts down the plug-ins. It calls each in turn with a
 *	shut down message (see SPPlugs.h). Call this when your application quits
 *	before calling SPTerm().
 */
SPErr SPShutdownPlugins( void );


#ifdef MAC_ENV
/*
 *	SPSetPPCLoadHeap() sets the destination heap of plug-in accesses on PowerPPC
 *	Macintoshes.  Plug-ins can load into the system heap or the application heap.
 */
typedef enum {
	kAppHeap = 0,
	kSysHeap
} SPTargetHeap;

SPErr SPSetPPCLoadHeap( SPTargetHeap target );

/*
 *	SPSetHostBundleRef() lets sweet pea know the bundle ref of the host.
 */
SPErr SPSetHostBundleRef(CFBundleRef hostBundle);
#endif


/*
 *	SPVersion() returns the version number of the Sweet Pea host interface (this
 *	file) for which Sweet Pea was built. You can compare this to the constant
 *	kSPVersion to make sure you're using the version you think you are.
 */
uint32 SPVersion( void );


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
