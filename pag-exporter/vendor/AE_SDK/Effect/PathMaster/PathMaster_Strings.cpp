#include "PathMaster.h"
	
typedef struct {
	unsigned long	index;
	char			str[1024];
} TableString;



TableString		g_strs[StrID_NUMTYPES] = 
{
	StrID_NONE,				"",
	StrID_Name,				"PathMaster!",
	StrID_Description,		"API Path usage, and sequence data flattening/unflattening.\rCopyright 2007-2023 Adobe Inc.",
	StrID_PathName,			"Path",
	StrID_ColorName,		"Color", 
	StrID_CheckName,		"Invert",		
	StrID_CheckDetail,		"",
	StrID_Err_LoadSuite,	"Error loading suite.",
	StrID_Err_FreeSuite,	"Error releasing suite.",
	StrID_OpacityName,		"Opacity of feather",
	StrID_X_Feather,		"X Feather",
	StrID_Y_Feather,		"Y Feather",
	StrID_API_Vers_Warning, "PathMaster requires After Effects 5.0 or later.",
	StrID_Really_Long_String, "This is a really long string, somewhat over 256 characters long. We'll use this to make sure we correctly clamp the flat version of our string to 256 characters. This is a really long string, somewhat over 256 characters long. We'll use this to make sure we correctly clamp the flat version of our string to 256 characters. This is a really long string, somewhat over 256 characters long. We'll use this to make sure we correctly clamp the flat version of our string to 256 characters."
};



char	*GetStringPtr(int strNum)
{
	return g_strs[strNum].str;
}