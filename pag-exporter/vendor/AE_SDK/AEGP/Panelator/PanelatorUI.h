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

#ifndef PanelatorUI_H
#define PanelatorUI_H

#include "AE_GeneralPlugPanels.h"

#ifndef DEBUG
	#define DEBUG 1
#endif

#include "SuiteHelper.h"

enum {
	PT_MenuCmd_RED = 30,
	PT_MenuCmd_GREEN,
	PT_MenuCmd_BLUE,
	PT_MenuCmd_STANDARD,
	PT_MenuCmd_TITLE_LONGER,
	PT_MenuCmd_TITLE_SHORTER
};


class PanelatorUI
{
public:
	explicit PanelatorUI(SPBasicSuite* spbP, AEGP_PanelH panelH, 
							AEGP_PlatformViewRef platformWindowRef,
							AEGP_PanelFunctions1* outFunctionTable);

protected:
	void operator=(const PanelatorUI&);
	PanelatorUI(const PanelatorUI&); // private, unimplemented

	AEGP_PlatformViewRef i_refH;
	AEGP_PanelH			i_panelH;

	A_u_char red;
	A_u_char green;
	A_u_char blue;
	A_Boolean		i_use_bg;
	A_long			i_numClicks;
	virtual void 	GetSnapSizes(A_LPoint*	snapSizes, A_long * numSizesP);
	virtual void	PopulateFlyout(AEGP_FlyoutMenuItem* itemsP, A_long * in_out_numItemsP);
	virtual void	DoFlyoutCommand(AEGP_FlyoutMenuCmdID commandID);


	virtual void InvalidateAll() = 0;

	SuiteHelper<PFAppSuite4>	i_appSuite;
	SuiteHelper<AEGP_PanelSuite1>	i_panelSuite;

private:
	static A_Err	S_GetSnapSizes(AEGP_PanelRefcon refcon, A_LPoint*	snapSizes, A_long * numSizesP);
	static A_Err	S_PopulateFlyout(AEGP_PanelRefcon refcon, AEGP_FlyoutMenuItem* itemsP, A_long * in_out_numItemsP);
	static A_Err	S_DoFlyoutCommand(AEGP_PanelRefcon refcon, AEGP_FlyoutMenuCmdID commandID);

};

//////////////////////////////////////////////////////////////////////////////
#endif // PanelatorUI
