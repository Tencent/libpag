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

#include "SmartyPants.h"

typedef struct {
	A_u_long		index;
	A_char			str[256];
} TableString;

TableString		g_strs[StrID_NUMTYPES] = {
	StrID_NONE,						"",
	StrID_Name,						"SmartyPants",
	StrID_Description,				"Demonstrate new SmartFX render messaging.\nCopyright 2007-2023 Adobe Inc.",
	StrID_ChannelParam_Name,		"Channel",
	StrID_Channels,					"RGB|Red|Green|Blue|(-|HLS|Hue|Lightness|Saturation|(-|YIQ|Luminance|In Phase Chrominance|Quadrature Chrominance|(-|Alpha",
	StrID_Topic_Name,				"Layer Controls",
	StrID_LayerBlendSlider_Name,	"Layer Opacity",
	StrID_Layer_Param_Name,			"Layer Blend Ratio",
	StrID_Err_LoadSuite,			"Error loading suite.",
	StrID_Err_FreeSuite,			"Error releasing suite.",
};


A_char *
GetStringPtr(int strNum)
{
	return g_strs[strNum].str;
}
	