/* Portable_Strings.cpp */

#include "Portable.h"

typedef struct {
	unsigned long	index;
	char			str[256];
} TableString;

TableString		g_strs[StrID_NUMTYPES] = {
	StrID_NONE,						"",
	StrID_Name,						"Portable",
	StrID_Description,				"This example shows how to detect and \t respond to different hosts.\rCopyright 2007-2023 Adobe Inc.",
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
	StrID_DependString1,			"All Dependencies requested.",
	StrID_DependString2,			"Missing Dependencies requested.",
	StrID_Err_LoadSuite,			"Error loading suite.",
	StrID_Err_FreeSuite,			"Error releasing suite.",
};


char *GetStringPtr(int strNum)
{
	return g_strs[strNum].str;
}
	