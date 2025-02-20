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

#include "Persisto.h"

static AEGP_PluginID	S_my_id				= 0;

static AEGP_Command		S_persisto_cmd 		= 0;

static SPBasicSuite		*sP					= NULL;

static A_Err
UpdateMenuHook(
	AEGP_GlobalRefcon		plugin_refconPV,		/* >> */
	AEGP_UpdateMenuRefcon	refconPV,				/* >> */
	AEGP_WindowType			active_window)			/* >> */
{
	A_Err 					err = A_Err_NONE;
	
	AEGP_SuiteHandler		suites(sP);
	
	if (S_persisto_cmd){
		err = suites.CommandSuite1()->AEGP_EnableCommand(S_persisto_cmd);
	} else {
		err = suites.CommandSuite1()->AEGP_DisableCommand(S_persisto_cmd);
	}
	return err;
}
			   
static A_Err
CommandHook(
	AEGP_GlobalRefcon	plugin_refconPV,		/* >> */
	AEGP_CommandRefcon	refconPV,				/* >> */
	AEGP_Command		command,				/* >> */
	AEGP_HookPriority	hook_priority,			/* >> */
	A_Boolean			already_handledB,		/* >> */
	A_Boolean			*handledPB)				/* << */
{
	A_Err 				err 	= A_Err_NONE, 
						err2 	= A_Err_NONE;

	if (S_persisto_cmd == command){
		try{
			AEGP_SuiteHandler		suites(sP);
			AEGP_PersistentBlobH	blobH			=	NULL;
			A_Boolean				found_my_stuffB	=	FALSE,
									askB			=	FALSE,
									defaultB		=	FALSE;
			A_FpLong				valueF			=	0.0;
			A_char					bufferAC[AEGP_MAX_ABOUT_STRING_SIZE] = {'\0'};
			A_u_long				actual_buf_size	=	0;

			ERR(suites.PersistentDataSuite3()->AEGP_GetApplicationBlob(&blobH));


			/*	Special bonus behavior: 

			Say you wanted to import a file, and not have AE ask the user
			about its alpha interpretation. Here's how to change the prefs
			to do that. Of course, being a nice developer, you restore any
			setting you've changed behind the user's back before continuing...

			*/

			ERR(suites.PersistentDataSuite3()->AEGP_DoesKeyExist(	blobH,
																	"Main Pref Section",
																	"Pref_DEFAULT_UNLABELED_ALPHA",
																	&askB));

			if (!askB){
				ERR(suites.PersistentDataSuite3()->AEGP_SetData(blobH,
																"Main Pref Section",
																"Pref_DEFAULT_UNLABELED_ALPHA",
																sizeof(A_char),
																&defaultB));
			}

			/*	Check to see if one value already exists, just to see if we've already 
			added our values to the blob. NOTE: Do a lot more error checking than 
			this in production code.
			*/

			ERR(suites.PersistentDataSuite3()->AEGP_DoesKeyExist(	blobH,
																	STR(StrID_Section_Key),
																	STR(StrID_Value_Key_1),
																	&found_my_stuffB));

			ERR(suites.PersistentDataSuite3()->AEGP_GetFpLong(	blobH,
																STR(StrID_Section_Key),
																STR(StrID_Value_Key_1),
																DEFAULT_FUZZINESS,
																&valueF));

			if (found_my_stuffB && DEFAULT_FUZZINESS == valueF){
				ERR2(suites.UtilitySuite3()->AEGP_ReportInfo(	S_my_id, STR(StrID_ValueExisted)));
			} else if (found_my_stuffB){
				ERR2(suites.UtilitySuite3()->AEGP_ReportInfo(	S_my_id, STR(StrID_DifferentValueSet)));
			} else	{
				ERR2(suites.UtilitySuite3()->AEGP_ReportInfo(	S_my_id, STR(StrID_NewValueAdded)));
			}

			/*	Somewhat counter-intuitively, AEGP_GetString() will actually
			SET as well, if the key isn't found. */

			ERR(suites.PersistentDataSuite3()->AEGP_GetString(	blobH,
																STR(StrID_Section_Key),
																STR(StrID_Value_Key_2),
																STR(StrID_DefaultString),
																AEGP_MAX_ABOUT_STRING_SIZE,
																bufferAC,
																&actual_buf_size));

			*handledPB = TRUE;
		} catch (A_Err &thrown_err) {
			err = thrown_err;
		}
	}
	return err;
}

A_Err
EntryPointFunc(
	struct SPBasicSuite		*pica_basicP,			/* >> */
	A_long				 	major_versionL,			/* >> */		
	A_long					minor_versionL,			/* >> */		
	AEGP_PluginID			aegp_plugin_id,			/* >> */
	AEGP_GlobalRefcon		*global_refconP)		/* << */
{
	A_Err 			err		=	A_Err_NONE, 
					err2	=	A_Err_NONE;
	sP		=	pica_basicP;
	S_my_id	=	aegp_plugin_id;	

	AEGP_SuiteHandler	suites(sP);

	ERR(suites.CommandSuite1()->AEGP_GetUniqueCommand(&S_persisto_cmd));

	if (S_persisto_cmd){
		ERR(suites.CommandSuite1()->AEGP_InsertMenuCommand(	S_persisto_cmd, 
															STR(StrID_Menu_Item), 
															AEGP_Menu_FILE, 
															AEGP_MENU_INSERT_SORTED));

		ERR(suites.RegisterSuite5()->AEGP_RegisterCommandHook(	S_my_id, 
																AEGP_HP_BeforeAE, 
																AEGP_Command_ALL, 
																CommandHook, 
																NULL));

		ERR(suites.RegisterSuite5()->AEGP_RegisterUpdateMenuHook(	S_my_id, 
																	UpdateMenuHook, 
																	NULL));
	}
	if (err){// not !err, err!
		err2 = suites.UtilitySuite3()->AEGP_ReportInfo(S_my_id, STR(StrID_Error));
	}
	return err;
}
