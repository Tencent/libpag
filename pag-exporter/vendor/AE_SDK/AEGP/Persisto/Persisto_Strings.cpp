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

#include "Persisto.h"

TableString		g_strs[StrID_NUMTYPES] = {
	StrID_NONE,						"",
	StrID_Menu_Item,				"Persisto!",
	StrID_Section_Key,				"Persisto",
	StrID_Value_Key_1,				"Fuzziness",
	StrID_Value_Key_2,				"Cliche Du Jour",
	StrID_DefaultString,			"Default",
	StrID_NewValueAdded,			"New value added!",
	StrID_ValueExisted,				"Value already existed",
	StrID_DifferentValueSet,		"Different value found than expected!",
	StrID_Error,					"Problems encountered while registering menu command."
};

char	*GetStringPtr(int strNum)
{
	return g_strs[strNum].str;
}
	
