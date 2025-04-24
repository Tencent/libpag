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


/*	
	Commando.cpp

	This is a husk of a command hooking plug-in. If you're adding 
	a command to After Effects using an AEGP, and you're not directly
	modeling your plug-in on one of the other samples, this is the 
	place to be.
	

	version		notes							engineer		date
	
	1.0			First implementation			bbb				8/4/06
	
*/

#include "Commando.h"

//	Yes, yes, statics are morally wrong, not to mention their causing
//	weight gain and hair loss. Still, AEGPs just plain don't get 
//	unloaded, and the following three items are about as likely
//	to change as movie popcorn being a rip-off, Apple having 
//	good marketing, and hockey not appealing to a broader American
//	audience.

static AEGP_Command			S_Commando_cmd		=	0L;
static AEGP_PluginID		S_my_id				=	0L;
static SPBasicSuite			*sP					=	NULL;

static A_Err
DeathHook(
	AEGP_GlobalRefcon	plugin_refconP,
	AEGP_DeathRefcon	refconP)
{
	A_Err	err			= A_Err_NONE;

	//	Clean up anything you allocated during your 
	//	EntryPointFunc.
	
	return err;
}

static	A_Err	
IdleHook(
	AEGP_GlobalRefcon	plugin_refconP,	
	AEGP_IdleRefcon		refconP,		
	A_long				*max_sleepPL)
{
	A_Err	err			= A_Err_NONE;

	//	User moving along too fast? Getting too much done?
	//	Throw some crunchy processing in here, and they'll 
	//	slow right down!
	
	return err;
}

static A_Err
UpdateMenuHook(
	AEGP_GlobalRefcon		plugin_refconPV,	/* >> */
	AEGP_UpdateMenuRefcon	refconPV,			/* >> */
	AEGP_WindowType			active_window)		/* >> */
{
	A_Err 				err 			=	A_Err_NONE,
						err2			=	A_Err_NONE;
	AEGP_ItemH			active_itemH	=	NULL;
	
	AEGP_ItemType		item_type		=	AEGP_ItemType_NONE;
	
	AEGP_SuiteHandler	suites(sP);
	
	//	Here's where you to put logic around when your plug-in
	//	should and should not be available. We're going to be 
	//	available whenever the user has footage or a comp 
	//	selected; you can use whatever logic you like to decide
	//	when to en/disable your plug-in.
	
	ERR(suites.ItemSuite6()->AEGP_GetActiveItem(&active_itemH));

	if (active_itemH){
		ERR(suites.ItemSuite6()->AEGP_GetItemType(active_itemH, &item_type));
		
		if (AEGP_ItemType_COMP 		==	item_type	||	AEGP_ItemType_FOOTAGE	==	item_type)	{
			ERR(suites.CommandSuite1()->AEGP_EnableCommand(S_Commando_cmd));
		}
	} else {
		ERR2(suites.CommandSuite1()->AEGP_DisableCommand(S_Commando_cmd));
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
	A_Err			err 		= 	A_Err_NONE; 
					
	AEGP_SuiteHandler	suites(sP);
	
	if (S_Commando_cmd == command){
		
		// Put interesting code here.

		*handledPB = TRUE;	
	}

	//	It's legal, but underhanded, to do something in response to
	//	AE's commands but not tell AE about it. Rest assured, users
	//	will let your support department know about it.
	
	return err;
}

DllExport A_Err
EntryPointFunc(
	struct SPBasicSuite		*pica_basicP,		/* >> */
	A_long				 	major_versionL,		/* >> */		
	A_long					minor_versionL,		/* >> */		
	const A_char		 	*file_pathZ,		/* >> platform-specific delimiters */
	const A_char		 	*res_pathZ,			/* >> platform-specific delimiters */
	AEGP_PluginID			aegp_plugin_id,		/* >> */
	void					*global_refconPV)	/* << */
{
	A_Err 					err 				= A_Err_NONE, 
							err2 				= A_Err_NONE;
	
	sP = pica_basicP;
	
	AEGP_SuiteHandler suites(pica_basicP);
	
	
	ERR(suites.CommandSuite1()->AEGP_GetUniqueCommand(&S_Commando_cmd));
	
	
	ERR(suites.CommandSuite1()->AEGP_InsertMenuCommand(	S_Commando_cmd, 
														STR(StrID_Name), 
														AEGP_Menu_WINDOW, 
														AEGP_MENU_INSERT_SORTED));
	
	ERR(suites.RegisterSuite5()->AEGP_RegisterCommandHook(	S_my_id, 
															AEGP_HP_BeforeAE, 
															AEGP_Command_ALL, 
															CommandHook, 
															0));
	ERR(suites.RegisterSuite5()->AEGP_RegisterDeathHook(	S_my_id, DeathHook,	NULL));
	ERR(suites.RegisterSuite5()->AEGP_RegisterUpdateMenuHook(S_my_id, UpdateMenuHook, NULL));
	ERR(suites.RegisterSuite5()->AEGP_RegisterIdleHook(S_my_id, IdleHook, NULL));
	
	if (err){ // not !err, err!
		ERR2(suites.UtilitySuite3()->AEGP_ReportInfo(S_my_id, STR(StrID_GenericError)));
	}
	return err;
}
			  
			  
