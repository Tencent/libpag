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

#include "PanelatorUI.h"

#include <Carbon/Carbon.h>

class PanelatorUI_Plat : public PanelatorUI {

	public:
	explicit	PanelatorUI_Plat(
		SPBasicSuite			*spbP, 
		AEGP_PanelH				panelH, 
		AEGP_PlatformViewRef	platformWindowRef,
		AEGP_PanelFunctions1	*outFunctionTableP);
							
	virtual void	InvalidateAll();
	
	private:


	
};

