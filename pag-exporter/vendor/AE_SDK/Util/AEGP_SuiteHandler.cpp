/*******************************************************************/
/*                                                                 */
/*                      ADOBE CONFIDENTIAL                         */
/*                   _ _ _ _ _ _ _ _ _ _ _ _ _                     */
/*                                                                 */
/* Copyright 2007 Adobe Systems Incorporated                       */
/* All Rights Reserved.                                            */
/*                                                                 */
/* NOTICE:  All information contained herein is, and remains the   */
/* property of Adobe Systems Incorporated and its suppliers, if    */
/* any.  The intellectual and technical concepts contained         */
/* herein are proprietary to Adobe Systems Incorporated and its    */
/* suppliers and may be covered by U.S. and Foreign Patents,       */
/* patents in process, and are protected by trade secret or        */
/* copyright law.  Dissemination of this information or            */
/* reproduction of this material is strictly forbidden unless      */
/* prior written permission is obtained from Adobe Systems         */
/* Incorporated.                                                   */
/*                                                                 */
/*******************************************************************/

/** AEGP_SuiteHandler.cpp

	Implementation of AEGP_SuiteHandler non-inlines. See AEGP_SuiteHandler.h for usage.

	created 9/11/2000 jms
**/


#include <AEGP_SuiteHandler.h>
#include <AE_Macros.h>

AEGP_SuiteHandler::AEGP_SuiteHandler(const SPBasicSuite *pica_basicP) :
	i_pica_basicP(pica_basicP)
{
	AEFX_CLR_STRUCT(i_suites);

	if (!i_pica_basicP) {						//can't construct w/out basic suite, everything else is demand loaded
		MissingSuiteError();
	}
}

AEGP_SuiteHandler::~AEGP_SuiteHandler()
{
	ReleaseAllSuites();
}

// Had to go to the header file to be inlined to please CW mach-o target
/*void *AEGP_SuiteHandler::pLoadSuite(A_char *nameZ, A_long versionL) const
{
	const void *suiteP;
	A_long err = i_pica_basicP->AcquireSuite(nameZ, versionL, &suiteP);

	if (err || !suiteP) {
		MissingSuiteError();
	}

	return (void*)suiteP;
}*/

// Free a particular suite. Ignore errors, because, what is there to be done if release fails?
void AEGP_SuiteHandler::ReleaseSuite(const A_char *nameZ, A_long versionL)
{
	i_pica_basicP->ReleaseSuite(nameZ, versionL);
}

