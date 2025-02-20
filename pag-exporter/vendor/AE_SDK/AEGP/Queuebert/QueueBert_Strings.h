// QueueBert_Strings.h

#pragma once

typedef enum {
	StrID_NONE, 
	StrID_Name,
	StrID_Path,
	StrID_Pronounce,
	StrID_Troubles,
	StrID_NUMTYPES
} StrIDType;

char	*GetStringPtr(int strNum);