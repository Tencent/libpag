/**************************************************************************
*
* ADOBE CONFIDENTIAL
* ___________________
*
*  Copyright 2009 Adobe Systems Incorporated
*  All Rights Reserved.
*
* NOTICE:  All information contained herein is, and remains the property of
* Adobe Systems Incorporated  and its suppliers,  if any.  The intellectual
* and technical concepts contained herein are proprietary to  Adobe Systems
* Incorporated  and its suppliers  and may be  covered by U.S.  and Foreign
* Patents,patents in process,and are protected by trade secret or copyright
* law.  Dissemination of this  information or reproduction of this material
* is strictly  forbidden  unless prior written permission is  obtained from
* Adobe Systems Incorporated.
**************************************************************************/

/**
	Definition of common types used by adobesdk.
**/

#ifndef ADOBESDK_CONFIG_TYPES_H
#define ADOBESDK_CONFIG_TYPES_H

#include <adobesdk/config/PreConfig.h>

#include "stdint.h"


typedef uint16_t			ADOBESDK_UTF16Char;
typedef uint8_t				ADOBESDK_UTF8Char;
typedef uint8_t				ADOBESDK_Boolean;

enum
{
	kAdobesdk_False = 0,
	kAdobesdk_True = 1
};


typedef ADOBESDK_UTF16Char	DRAWBOT_UTF16Char;
typedef ADOBESDK_Boolean	DRAWBOT_Boolean;


typedef struct 
{
	int64_t opaque[2];
} ADOBESDK_String; 


#include <adobesdk/config/PostConfig.h>

#endif //ADOBESDK_CONFIG_TYPES_H
