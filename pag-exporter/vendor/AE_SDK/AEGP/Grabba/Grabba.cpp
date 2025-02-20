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


/*	
	Grabba.cpp

	Answers the timeless question, "How do I get a rendered frame
	from After Effects?"
	

	version		notes							engineer		date
	
	1.0			First implementation			bbb				8/21/03
	
*/

#include "Grabba.h"

static AEGP_Command			S_Grabba_cmd		=	0L;
static AEGP_PluginID		S_my_id				=	0L;
static A_long				S_idle_count		=	0L;
static SPBasicSuite			*sP					=	NULL;

static A_Err
DeathHook(
	AEGP_GlobalRefcon	plugin_refconP,
	AEGP_DeathRefcon	refconP)
{
	A_Err	err			= A_Err_NONE;
	AEGP_SuiteHandler	suites(sP);

	A_char report[AEGP_MAX_ABOUT_STRING_SIZE] = {'\0'};
		
	suites.ANSICallbacksSuite1()->sprintf(report, STR(StrID_IdleCount), S_idle_count); 

	return err;
}

static	A_Err	
IdleHook(
	AEGP_GlobalRefcon	plugin_refconP,	
	AEGP_IdleRefcon		refconP,		
	A_long				*max_sleepPL)
{
	A_Err	err			= A_Err_NONE;
	S_idle_count++;
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
	
	ERR(suites.ItemSuite6()->AEGP_GetActiveItem(&active_itemH));

	if (active_itemH){
		ERR(suites.ItemSuite6()->AEGP_GetItemType(active_itemH, &item_type));
		
		if (AEGP_ItemType_COMP 		==	item_type	||
			AEGP_ItemType_FOOTAGE	==	item_type)	{
			ERR(suites.CommandSuite1()->AEGP_EnableCommand(S_Grabba_cmd));
		}
	} else {
		ERR2(suites.CommandSuite1()->AEGP_DisableCommand(S_Grabba_cmd));
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
	A_Err			err 		= 	A_Err_NONE,
					err2 		= 	A_Err_NONE; 
					
	AEGP_SuiteHandler	suites(sP);
	
	if (S_Grabba_cmd == command){

		AEGP_ItemH			active_itemH	=	NULL;
		AEGP_RenderOptionsH	roH				=	NULL;

		ERR(suites.ItemSuite6()->AEGP_GetActiveItem(&active_itemH));
		
		if (active_itemH){
			ERR(suites.RenderOptionsSuite1()->AEGP_NewFromItem(S_my_id, active_itemH, &roH));
			
			if (!err && roH){
				AEGP_FrameReceiptH	receiptH 	= NULL;
				AEGP_WorldH			frameH		= NULL;
				A_Time				timeT		= {0,1};
				AEGP_WorldType		type		= AEGP_WorldType_NONE;
				
				ERR(suites.RenderOptionsSuite1()->AEGP_SetTime(roH, timeT)); // avoid "div by 0"
				ERR(suites.RenderOptionsSuite1()->AEGP_GetWorldType(roH, &type));
				ERR(suites.RenderSuite2()->AEGP_RenderAndCheckoutFrame(roH, NULL, NULL, &receiptH));
				
				if (receiptH){
					ERR(suites.RenderSuite2()->AEGP_GetReceiptWorld(receiptH, &frameH));
					
					// 	WooHOO! Pixel Party!
					
					// 	We Crash, So You Don't Have To...
					//	
					//	A nice developer like yourself might instinctually
					//	AEGP_Dispose() of the frame you just got. Actually, 
					//	After Effects will manage that frame for you. Do 
					//	not dispose! Thanks for being so considerate. Now
					//	go wash your hands.

					ERR2(suites.RenderSuite2()->AEGP_CheckinFrame(receiptH));
				}
			}
			ERR(suites.RenderOptionsSuite1()->AEGP_Dispose(roH));
		}
		*handledPB = TRUE;	
	}
	return err;
}

A_Err
EntryPointFunc(
	struct SPBasicSuite		*pica_basicP,		/* >> */
	A_long				 	major_versionL,		/* >> */		
	A_long					minor_versionL,		/* >> */		
	AEGP_PluginID			aegp_plugin_id,		/* >> */
	AEGP_GlobalRefcon		*global_refconV)	/* << */
{
	S_my_id										= aegp_plugin_id;
	A_Err 					err 				= A_Err_NONE, 
							err2 				= A_Err_NONE;
	
	sP = pica_basicP;
	
	AEGP_SuiteHandler suites(pica_basicP);
	
	
	ERR(suites.CommandSuite1()->AEGP_GetUniqueCommand(&S_Grabba_cmd));
	
	
	ERR(suites.CommandSuite1()->AEGP_InsertMenuCommand(	S_Grabba_cmd, 
														"Grabba", 
														AEGP_Menu_EXPORT, 
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
		ERR2(suites.UtilitySuite3()->AEGP_ReportInfo(S_my_id, "Grabba : Could not register command hook."));
	}
	return err;
}
			  
			  
