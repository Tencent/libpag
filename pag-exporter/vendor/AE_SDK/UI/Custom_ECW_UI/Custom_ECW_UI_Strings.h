/* Custom_ECW_UI_Strings.h */

#pragma once

#ifdef AE_OS_MAC
#include <wchar.h>
#endif

#define CUSTOM_UI_STRING	L"Whoopee! A Custom UI!!!\nClick and drag here to\nchange the background color.\nHold down shift or cmd/ctrl\nfor different cursors!"

typedef enum {
	StrID_NONE, 
	StrID_Name,
	StrID_Description,
	StrID_Color_Param_Name,
	StrID_Slider_Param_Name,	
	StrID_Err_LoadSuite,
	StrID_Err_FreeSuite, 
	StrID_Frank,
	StrID_NUMTYPES
} StrIDType;
