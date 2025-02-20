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
/* Adobe Premiere Device Control plug-in definitions			   */
/*																   */
/* AE_EffectSuitesHelper.h										   */
/*																   */ 
/* After Effects 5.0 PICA Suites (extended 3/8/00)				   */
/*																   */
/*																   */
/*******************************************************************/

#ifndef _H_AE_EffectSuitesHelper
#define _H_AE_EffectSuitesHelper

#include <AE_Effect.h>
#include <SPBasic.h>

#include <adobesdk/config/PreConfig.h>

#ifdef __cplusplus
	extern "C" {
#endif


/** PF_HelperSuite1

	PF_GetCurrentTool()

		retrieves the type of the current tool palette tool selected					

**/

#define kPFHelperSuite				"AE Plugin Helper Suite"
#define kPFHelperSuiteVersion1		1
#define kPFHelperSuiteVersion		kPFHelperSuiteVersion1

enum {
	PF_SuiteTool_NONE = 0, 				
	PF_SuiteTool_ARROW,
	PF_SuiteTool_ROTATE,
	PF_SuiteTool_SHAPE,
	PF_SuiteTool_OBSOLETE,
	PF_SuiteTool_PEN,
	PF_SuiteTool_PAN,
	PF_SuiteTool_HAND,
	PF_SuiteTool_MAGNIFY,
	PF_SuiteTool_ROUNDED_RECT,
	PF_SuiteTool_POLYGON,
	PF_SuiteTool_STAR,
	PF_SuiteTool_PIN,
	PF_SuiteTool_PIN_STARCH,
	PF_SuiteTool_PIN_DEPTH
};
typedef A_LegacyEnumType PF_SuiteTool;



typedef struct PF_HelperSuite1 {
	// obsolete, use PF_HelperSuite2
	SPAPI PF_Err 	(*PF_GetCurrentTool)( PF_SuiteTool 		*toolP );		/* << */
} PF_HelperSuite1;


/** PF_HelperSuite2

	PF_ParseClipboard()

		causes After Effects to parse the clipboard immediately
					
**/
enum {
	PF_ExtendedSuiteTool_NONE = 0,
	PF_ExtendedSuiteTool_ARROW,
	PF_ExtendedSuiteTool_ROTATE,
	PF_ExtendedSuiteTool_PEN_NORMAL,
	PF_ExtendedSuiteTool_PEN_ADD_POINT,
	PF_ExtendedSuiteTool_PEN_DELETE_POINT,
	PF_ExtendedSuiteTool_PEN_CONVERT_POINT,
	PF_ExtendedSuiteTool_RECT,
	PF_ExtendedSuiteTool_OVAL,
	PF_ExtendedSuiteTool_CAMERA_ORBIT_CAMERA,
	PF_ExtendedSuiteTool_CAMERA_PAN_CAMERA,				//changed from PF_ExtendedSuiteTool_CAMERA_TRACK_XY
	PF_ExtendedSuiteTool_CAMERA_DOLLY_CAMERA,			//changed from PF_ExtendedSuiteTool_CAMERA_TRACK_Z
	PF_ExtendedSuiteTool_PAN_BEHIND,
	PF_ExtendedSuiteTool_HAND,
	PF_ExtendedSuiteTool_MAGNIFY,
	PF_ExtendedSuiteTool_PAINTBRUSH,	// All below added in 6.0
	PF_ExtendedSuiteTool_PENCIL,
	PF_ExtendedSuiteTool_CLONE_STAMP,
	PF_ExtendedSuiteTool_ERASER,
	PF_ExtendedSuiteTool_TEXT,
	PF_ExtendedSuiteTool_TEXT_VERTICAL,
	PF_ExtendedSuiteTool_PIN,
	PF_ExtendedSuiteTool_PIN_STARCH,
	PF_ExtendedSuiteTool_PIN_DEPTH,
	PF_ExtendedSuiteTool_ROUNDED_RECT,
	PF_ExtendedSuiteTool_POLYGON,
	PF_ExtendedSuiteTool_STAR,
	PF_ExtendedSuiteTool_QUICKSELECT,
	PF_ExtendedSuiteTool_CAMERA_MAYA,
	PF_ExtendedSuiteTool_HAIRBRUSH,
	PF_ExtendedSuiteTool_FEATHER,
	PF_ExtendedSuiteTool_PIN_BEND,
	PF_ExtendedSuiteTool_PIN_ADVANCED,
	PF_ExtendedSuiteTool_CAMERA_ORBIT_CURSOR,			//new cursors should go at the end to avoid messing with sdk order
	PF_ExtendedSuiteTool_CAMERA_ORBIT_SCENE,
	PF_ExtendedSuiteTool_CAMERA_PAN_CURSOR,
	PF_ExtendedSuiteTool_CAMERA_DOLLY_TOWARDS_CURSOR,
	PF_ExtendedSuiteTool_CAMERA_DOLLY_TO_CURSOR,
};
typedef A_LegacyEnumType PF_ExtendedSuiteTool;


#define kPFHelperSuite2				"AE Plugin Helper Suite2"
#define kPFHelperSuite2Version1		1
#define kPFHelperSuite2Version2		2
#define kPFHelperSuite2Version		kPFHelperSuite2Version2

typedef struct PF_HelperSuite2 {
	SPAPI PF_Err 	(*PF_ParseClipboard)( void );
	// Do not call PF_SetCurrentExtendedTool until the UI is built. i.e. Do not call it from
	// your plugin init function.
	SPAPI PF_Err	(*PF_SetCurrentExtendedTool)(PF_ExtendedSuiteTool tool);
	SPAPI PF_Err	(*PF_GetCurrentExtendedTool)(PF_ExtendedSuiteTool *tool);
} PF_HelperSuite2;

#ifdef __cplusplus
	}
#endif

#include <adobesdk/config/PostConfig.h>



#endif
