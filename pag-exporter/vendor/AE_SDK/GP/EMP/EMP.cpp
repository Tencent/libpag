/*******************************************************************/
/*                                                                 */
/*                      ADOBE CONFIDENTIAL                         */
/*                   _ _ _ _ _ _ _ _ _ _ _ _ _                     */
/*                                                                 */
/* Copyright 2007-2023 Adobe Inc.                                  */
/* All Rights Reserved.                                            */
/*                                                                 */
/* NOTICE:  All information contained herein is, and remains the   */
/* property of Adobe Inc. and its suppliers, if                    */
/* any.  The intellectual and technical concepts contained         */
/* herein are proprietary to Adobe Inc. and its                    */
/* suppliers and may be covered by U.S. and Foreign Patents,       */
/* patents in process, and are protected by trade secret or        */
/* copyright law.  Dissemination of this information or            */
/* reproduction of this material is strictly forbidden unless      */
/* prior written permission is obtained from Adobe Inc.            */
/* Incorporated.                                                   */
/*                                                                 */
/*******************************************************************/

#include "EMP.h"

#ifdef AE_OS_WIN 
	BOOL APIENTRY LibMain(HANDLE hInstance, DWORD fdwReason, LPVOID lpReserved)
	{
		printf("EMP.aex Initialization entry point.");
		return TRUE;
	}
#endif

static PF_Err	
MyBlit(
	void						*hook_refconPV,	
	const AE_PixBuffer			*pix_bufP0,		
	const AE_ViewCoordinates	*viewP,			
	AE_BlitReceipt				receipt,		
	AE_BlitCompleteFunc			complete_func0,
	AE_BlitInFlags				in_flags,	
	AE_BlitOutFlags				*out_flags)
{
	return PF_Err_NONE;
}

static void	
MyDeath(
	void 		*hook_refconPV)
{
	// free anything you allocated.
}

static void	
MyVersion(
	void 			*hook_refconPV,	
	A_u_long		*versionPV)		
{
	*versionPV = 1;
}	

DllExport PF_Err  
EntryPointFunc (
	A_long			major_version,
	A_long			minor_version,
	AE_FileSpecH	file_specH,			
	AE_FileSpecH	res_specH,	
	AE_Hooks		*hooksP)
{
	PF_Err	err 				= PF_Err_NONE;
	hooksP->blit_hook_func 		= MyBlit;
	hooksP->death_hook_func 	= MyDeath;
	hooksP->version_hook_func 	= MyVersion;
	return err;
}

