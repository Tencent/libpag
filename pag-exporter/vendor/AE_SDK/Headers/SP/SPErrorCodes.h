/***********************************************************************/
/*                                                                     */
/* SPErrorCodes.h                                                      */
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

#ifndef __SPErrorCodes__
#define __SPErrorCodes__

#if PRAGMA_ONCE
#pragma once
#endif

/*******************************************************************************
 **
 ** General Errors
 **
 **/

// General errors
#define kASNoError				0
#define kASUnimplementedError	'!IMP'
#define kASUserCanceledError	'stop'


/*******************************************************************************
 **
 ** SP Errors
 **
 **/

// General errors
/** @ingroup Errors
	PICA no-error code is \c NULL. All other
	PICA errors are strings, except \c #kSPOutOfMemoryError. See \c SPTypes.h. */
#define kSPNoError				0
/** @ingroup Errors
	PICA error, applies to all PICA suites. See \c SPTypes.h. */
#define kSPUnimplementedError	'!IMP'
/** @ingroup Errors
	PICA error. */
#define kSPUserCanceledError	'stop'
#define kSPOperationInterrupted				'intr'
#define	kSPLogicError						'fbar' // general programming error

// SPAccessSuite errors
/** @ingroup Errors
	PICA access error. See \c #SPAccessSuite. */
#define kSPCantAcquirePluginError			'!Acq'
/** @ingroup Errors
	PICA access error. See \c #SPAccessSuite. */
#define kSPCantReleasePluginError			'!Rel'
/** @ingroup Errors
	PICA access error. See \c #SPAccessSuite. */
#define kSPPluginAlreadyReleasedError		'AlRl'

// SPAdaptsSuite errors
/** @ingroup Errors
	PICA adapter error. See \c #SPAdaptsSuite */
#define kSPAdapterAlreadyExistsError		'AdEx'
/** @ingroup Errors
	PICA adapter error.See \c #SPAdaptsSuite */
#define kSPBadAdapterListIteratorError		'BdAL'

// SPBasicSuite errors
/** @ingroup Errors
	Basic PICA error. See \c #SPBasicSuite */
#define kSPBadParameterError				'Parm'

// Block debugging errors
/** @ingroup Errors
	PICA debugging error. See \c #SPBlockDebugSuite */
#define kSPCantChangeBlockDebugNowError		'!Now'
/** @ingroup Errors
	PICA debugging error. See \c #SPBlockDebugSuite */
#define kSPBlockDebugNotEnabledError		'!Nbl'

// SPBlocks errors
/** @ingroup Errors
	PICA memory management error. See \c #SPBlocksSuite */
#define kSPOutOfMemoryError					(int32(0xFFFFFF6c))  /* -108, same as Mac memFullErr */
/** @ingroup Errors
	PICA memory management error. See \c #SPBlocksSuite */
#define kSPBlockSizeOutOfRangeError			'BkRg'

// SPCaches errors
/** @ingroup Errors
	PICA cache-flushing error. See \c #SPCachesSuite */
#define kSPPluginCachesFlushResponse		'pFls'
/** @ingroup Errors
	PICA cache-flushing error. See \c #SPCachesSuite */
#define kSPPluginCouldntFlushResponse		kSPNoError;

// SPFiles errors
/** @ingroup Errors
	PICA file-access error. See \c #SPFilesSuite */
#define kSPTroubleAddingFilesError			'TAdd'
/** @ingroup Errors
	PICA file-access error. See \c #SPFilesSuite */
#define kSPBadFileListIteratorError			'BFIt'

// SPHost errors
/** @ingroup Errors
	PICA plug-in atart-up error. See \c #SPHostSuite */
#define kSPTroubleInitializingError			'TIni'	// Some non-descript problem encountered while starting up.
/** @ingroup Errors PICA plug-in atart-up error. See \c #SPHostSuite */
#define kHostCanceledStartupPluginsError 	'H!St'

// SPInterface errors
/** @ingroup Errors
	PICA interface error. See \c #SPInterfaceSuite */
#define kSPNotASweetPeaPluginError			'NSPP'
/** @ingroup Errors
	PICA interface error. See \c #SPInterfaceSuite */
#define kSPAlreadyInSPCallerError			'AISC'

// SPPlugins errors
/** @ingroup Errors
	PICA plug-in error. See \c #SPPluginsSuite */
#define kSPUnknownAdapterError				'?Adp'
/** @ingroup Errors
	PICA plug-in error. See \c #SPPluginsSuite */
#define kSPBadPluginListIteratorError		'PiLI'
/** @ingroup Errors
	PICA plug-in error. See \c #SPPluginsSuite */
#define kSPBadPluginHost					'PiH0'
/** @ingroup Errors
	PICA plug-in error. See \c #SPPluginsSuite */
#define kSPCantAddHostPluginError			'AdHo'
/** @ingroup Errors
	PICA plug-in error. See \c #SPPluginsSuite */
#define kSPPluginNotFound					'P!Fd'

// SPProperties errors
/** @ingroup Errors
	PICA properties error. See \c #SPPropertiesSuite */
#define kSPCorruptPiPLError					'CPPL'
/** @ingroup Errors
	PICA properties error. See \c #SPPropertiesSuite */
#define kSPBadPropertyListIteratorError		'BPrI'

// SPSuites errors
/** @ingroup Errors
	PICA suite access error. See \c #SPSuitesSuite */
#define kSPSuiteNotFoundError				'S!Fd'
/** @ingroup Errors
	PICA suite access error. See \c #SPSuitesSuite */
#define kSPSuiteAlreadyExistsError			'SExi'
/** @ingroup Errors
	PICA suite access error. See \c #SPSuitesSuite */
#define kSPSuiteAlreadyReleasedError		'SRel'
/** @ingroup Errors
	PICA suite access error. See \c #SPSuitesSuite */
#define kSPBadSuiteListIteratorError		'SLIt'
/** @ingroup Errors
	PICA suite access error. See \c #SPSuitesSuite */
#define kSPBadSuiteInternalVersionError		'SIVs'

#endif
