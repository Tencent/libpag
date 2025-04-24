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

#include "AEConfig.h"
#ifdef AE_OS_WIN
	#include <windows.h>
#endif

#include "entry.h"
#include "AE_GeneralPlug.h"
#include "AE_Macros.h"
#include "AEGP_SuiteHandler.h"
#include "String_Utils.h"
#include "Commando_Strings.h"

#define AEGP_MAX_STREAM_DIM 4

typedef enum {
	StrID_NONE,							
	StrID_Name,							
	StrID_Description,
	StrID_GenericError,
	StrID_NUMTYPES
} StrIDType;

extern "C" {
DllExport A_Err
EntryPointFunc(
	struct SPBasicSuite		*pica_basicP,		
	A_long				 	major_versionL,		
	A_long					minor_versionL,
	const A_char		 	*file_pathZ,	
	const A_char		 	*res_pathZ,		
	AEGP_PluginID			aegp_plugin_id,	
	void					*global_refconPV);
}
