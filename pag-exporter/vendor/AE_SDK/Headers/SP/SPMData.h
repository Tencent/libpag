/***********************************************************************/
/*                                                                     */
/* SPMData.h                                                           */
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

#ifndef __SPMessageData__
#define __SPMessageData__


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
 ** Types
 **
 **/

/** The value of \c #SPMessageData::SPCheck, if the message data associated
	with a call to a plug-in has come from \c #SPInterfaceSuite::SendMessage(),
	or is prepared using \c #SPInterfaceSuite::SetupMessageData(). */
#define kSPValidSPMessageData 'SPCk'

/** Basic suite-access information provided with every call. */
typedef struct SPMessageData {
	/** \c #kSPValidSPMessageData if this is a valid PICA message. */
	int32 SPCheck;
	/** This plug-in, an \c #SPPluginRef. */
	struct SPPlugin *self;
	/** An array of application-wide global variables. */
	void *globals;
	/** A pointer to the basic PICA suite, which you use to obtain all other suites. */
	struct SPBasicSuite *basic;

} SPMessageData;


#ifdef __cplusplus
}
#endif

#endif
