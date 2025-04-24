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

#include "Panelator.h"
#include "PanelatorUI.h"
#include <algorithm>

#undef min

template <>
const A_char* SuiteTraits<PFAppSuite4>::i_name = kPFAppSuite;
template <>
const int32_t SuiteTraits<PFAppSuite4>::i_version = kPFAppSuiteVersion4;


PanelatorUI::PanelatorUI(SPBasicSuite* spbP, AEGP_PanelH panelH,
								AEGP_PlatformViewRef platformWindowRef,
					   AEGP_PanelFunctions1* outFunctionTable)
					   : i_refH(platformWindowRef), i_panelH(panelH),
					   red(255), green(255), blue(255), i_use_bg(false),
					   i_appSuite(spbP), i_panelSuite(spbP), i_numClicks(0)

{
	outFunctionTable->DoFlyoutCommand = S_DoFlyoutCommand;
	outFunctionTable->GetSnapSizes = S_GetSnapSizes;
	outFunctionTable->PopulateFlyout = S_PopulateFlyout;
}


void PanelatorUI::GetSnapSizes(A_LPoint*	snapSizes, A_long * numSizesP)
{
	snapSizes[0].x = 100;
	snapSizes[0].y = 100;
	snapSizes[1].x = 200;
	snapSizes[1].y = 400;
	*numSizesP = 2;
}

void 	PanelatorUI::PopulateFlyout(AEGP_FlyoutMenuItem* itemsP, A_long * in_out_numItemsP)
{
	AEGP_FlyoutMenuItem	myMenu[] = { 
		{1, AEGP_FlyoutMenuMarkType_NORMAL,		FALSE,	AEGP_FlyoutMenuCmdID_NONE,	reinterpret_cast<const A_u_char*>("Hi!")},
		{1, AEGP_FlyoutMenuMarkType_SEPARATOR,	TRUE,	AEGP_FlyoutMenuCmdID_NONE,	NULL  },
		{1, AEGP_FlyoutMenuMarkType_NORMAL,		TRUE,	AEGP_FlyoutMenuCmdID_NONE,	reinterpret_cast<const A_u_char*>("Set BG Color")},
		{2, AEGP_FlyoutMenuMarkType_NORMAL,		TRUE,	PT_MenuCmd_RED,				reinterpret_cast<const A_u_char*>("Red")},
		{2, AEGP_FlyoutMenuMarkType_NORMAL,		TRUE,	PT_MenuCmd_GREEN,			reinterpret_cast<const A_u_char*>("Green")},
		{2, AEGP_FlyoutMenuMarkType_NORMAL,		TRUE,	PT_MenuCmd_BLUE,			reinterpret_cast<const A_u_char*>("Blue")},
		{1, AEGP_FlyoutMenuMarkType_NORMAL,		TRUE,	PT_MenuCmd_STANDARD,		reinterpret_cast<const A_u_char*>("Normal Fill Color")},
		{1, AEGP_FlyoutMenuMarkType_NORMAL,		TRUE,	AEGP_FlyoutMenuCmdID_NONE,	reinterpret_cast<const A_u_char*>("Set Title")},
		{2, AEGP_FlyoutMenuMarkType_NORMAL,		TRUE,	PT_MenuCmd_TITLE_LONGER,	reinterpret_cast<const A_u_char*>("Longer")},
		{2, AEGP_FlyoutMenuMarkType_NORMAL,		TRUE,	PT_MenuCmd_TITLE_SHORTER,	reinterpret_cast<const A_u_char*>("Shorter")},
	};
	A_long		menuTableSizeL = sizeof(myMenu) / sizeof(AEGP_FlyoutMenuItem);

#ifdef AE_OS_WIN // std::copy might be unsafe! Oh no! The sky is falling!
	#pragma warning(push)
	#pragma warning(disable:4996)
#endif
	if (menuTableSizeL <= *in_out_numItemsP ){
		std::copy(&myMenu[0], &myMenu[menuTableSizeL], itemsP);
	} else {
		std::copy(&myMenu[0], myMenu + *in_out_numItemsP, itemsP);
	}
#ifdef AE_OS_WIN
	#pragma warning(pop)
#endif

	*in_out_numItemsP = menuTableSizeL;
}

void	PanelatorUI::DoFlyoutCommand(AEGP_FlyoutMenuCmdID commandID)
{

	switch(commandID){
		case PT_MenuCmd_RED:
			i_use_bg = false;
			red = PF_MAX_CHAN8;
			green = blue = 0;
		break;
		case PT_MenuCmd_GREEN:
			i_use_bg = false;
			green = PF_MAX_CHAN8;
			red = blue = 0;
		break;
		case PT_MenuCmd_BLUE:
			i_use_bg = false;
			blue = PF_MAX_CHAN8;
			red = green = 0;
		break;
		case PT_MenuCmd_STANDARD:
			i_use_bg = true;
		break;

		case PT_MenuCmd_TITLE_LONGER:
			i_panelSuite->AEGP_SetTitle(i_panelH, reinterpret_cast<const A_u_char*>("This is a longer name"));
			break;

		case PT_MenuCmd_TITLE_SHORTER:
			i_panelSuite->AEGP_SetTitle(i_panelH, reinterpret_cast<const A_u_char*>("P!"));
			break;
	}
	InvalidateAll();
}

A_Err	PanelatorUI::S_GetSnapSizes(AEGP_PanelRefcon refcon, A_LPoint*	snapSizes, A_long * numSizesP)
{
	PT_XTE_START{
		reinterpret_cast<PanelatorUI*>(refcon)->GetSnapSizes(snapSizes, numSizesP);
	} PT_XTE_CATCH_RETURN_ERR;
}

A_Err	PanelatorUI::S_PopulateFlyout(AEGP_PanelRefcon refcon, AEGP_FlyoutMenuItem* itemsP, A_long * in_out_numItemsP)
{
	PT_XTE_START{
		reinterpret_cast<PanelatorUI*>(refcon)->PopulateFlyout(itemsP, in_out_numItemsP);
	} PT_XTE_CATCH_RETURN_ERR;
}

A_Err	PanelatorUI::S_DoFlyoutCommand(AEGP_PanelRefcon refcon, AEGP_FlyoutMenuCmdID commandID)
{
	PT_XTE_START{
		reinterpret_cast<PanelatorUI*>(refcon)->DoFlyoutCommand(commandID);
	} PT_XTE_CATCH_RETURN_ERR;
}
