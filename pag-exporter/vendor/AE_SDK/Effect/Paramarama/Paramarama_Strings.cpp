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

/* Paramarama_Strings.cpp */

#include "Paramarama.h"

typedef struct {
	unsigned long	index;
	char			str[256];
} TableString;

TableString		g_strs[StrID_NUMTYPES] = {
	StrID_NONE,						"",
	StrID_Name,						"Paramarama",
	StrID_Description,				"Parameter Party!\rExercising all parameter types.\rCopyright 2007-2023 Adobe Inc.",
	StrID_Slider_Param_Name,		"An obsolete slider",
	StrID_Color_Param_Name,			"Color to mix",
	StrID_Float_Param_Name,			"Some float value",
	StrID_Angle_Param_Name,			"An angle control",
	StrID_Null_Param_Name,			"Null param (no TLW UI)",
	StrID_Popup_Param_Name,			"Pop-up param",
	StrID_Popup_Choices,			"Make Slower|"
									"Make Jaggy|"
									"(-|"
									"Plan A|"
									"Plan B",

	StrID_Checkbox_Param_Name,		"Some checkbox",
	StrID_Checkbox_Description,		"(with comment!)",
	StrID_3D_Point_Param_Name,		"3D Point",
	StrID_Button_Param_Name,		"Button",
	StrID_Button_Label_Name,		"Button Label",
	StrID_Button_Message,			"Paramarama button hit!",
	StrID_DependString1,			"All Dependencies requested.",
	StrID_DependString2,			"Missing Dependencies requested.",
	StrID_Err_LoadSuite,			"Error loading suite.",
	StrID_Err_FreeSuite,			"Error releasing suite.",
};


char *GetStringPtr(int strNum)
{
	return g_strs[strNum].str;
}
	