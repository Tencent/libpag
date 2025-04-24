#pragma once

typedef enum {
	StrID_NONE, 
	StrID_Name,
	StrID_Description,

	StrID_ModeName,
	StrID_ModeChoices, 


	StrID_FlavorName,
	StrID_FlavorChoices,




	StrID_FlavorChoicesChanged,





	StrID_ColorName, 

	StrID_SliderName,  

	StrID_CheckboxName,
	StrID_CheckboxCaption, 

	StrID_Err_LoadSuite,
	StrID_Err_FreeSuite, 

	StrID_TopicName,
	StrID_TopicNameDisabled, 
	StrID_FlavorNameDisabled,

	StrID_GeneralError,
	
	StrID_NUMTYPES
} StrIDType;

#ifdef __cplusplus
extern "C" {
#endif
char	*GetStringPtr(int strNum);
#ifdef __cplusplus
}
#endif