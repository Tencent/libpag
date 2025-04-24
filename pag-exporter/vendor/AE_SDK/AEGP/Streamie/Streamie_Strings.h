// Streamie_Strings.h

#pragma once

typedef enum {
	StrID_NONE, 
	StrID_Name,
	StrID_ChangedName,
	StrID_CommandName,
	StrID_Troubles,
	StrID_NUMTYPES
} StrIDType;

char	*GetStringPtr(int strNum);

