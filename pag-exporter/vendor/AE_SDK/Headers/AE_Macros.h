/*******************************************************************/
/*                                                                 */
/*                      ADOBE CONFIDENTIAL                         */
/*                   _ _ _ _ _ _ _ _ _ _ _ _ _                     */
/*                                                                 */
/* Copyright 1999 Adobe Systems Incorporated                       */
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
/*******************************************************************/
/**	AE_Macros.h
	
	Part of the Adobe After Effects 4.0 SDK.
	Copyright 1998 Adobe Systems Incorporated.
	All Rights Reserved.
	
	REVISION HISTORY	
		06/12/96	bsa		Updated for After Effects 3.1
		04/06/97	bsa		Updated for After Effects 3.1 Windows version
		03/01/99	bbb		Added DH.

**/

#ifndef _H_AE_MACROS
#define _H_AE_MACROS

#include "A.h"

#include <string.h>

#ifndef ERR
	#define ERR(FUNC)	do { if (!err) { err = (FUNC); } } while (0)
#endif

#ifndef ERR2
	#define ERR2(FUNC)	do { if (((err2 = (FUNC)) != A_Err_NONE) && !err) err = err2; } while (0)
#endif

#ifndef AEFX_CLR_STRUCT
	#define AEFX_CLR_STRUCT(STRUCT) memset(&(STRUCT), 0, sizeof(STRUCT));
#endif

#ifndef DH
#define DH(h)				(*(h))
#endif

#define FIX2INT(X)				((A_long)(X) >> 16)
#define INT2FIX(X)				((A_long)(X) << 16)
#define FIX2INT_ROUND(X)		(FIX2INT((X) + 32768))
#define	FIX_2_FLOAT(X)			((A_FpLong)(X) / 65536.0)
#define	FLOAT2FIX(F)			((PF_Fixed)((F) * 65536 + (((F) < 0) ? -0.5 : 0.5)))

// These are already defined if using Objective-C
#ifndef ABS
	#define ABS(N)					((N) < 0 ? -(N) : (N))
#endif

#ifndef MIN
	#define MIN(A,B)				((A) < (B) ? (A) : (B))
#endif

#ifndef MAX
	#define MAX(A, B)				((A) > (B) ? (A) : (B))
#endif

#define A_Fixed_ONE					((A_Fixed)0x00010000L)
#define A_Fixed_HALF				((A_Fixed)0x00008000L)

#define	PF_RECT_2_FIXEDRECT(R,FR)	do {	\
	(FR).left = INT2FIX((R).left);			\
	(FR).top = INT2FIX((R).top);			\
	(FR).right = INT2FIX((R).right);		\
	(FR).bottom = INT2FIX((R).bottom);		\
	} while (0)

#define	PF_FIXEDRECT_2_RECT(FR,R)	do {			\
	(R).left = (A_short)FIX2INT_ROUND((FR).left);		\
	(R).top  = (A_short)FIX2INT_ROUND((FR).top);		\
	(R).right = (A_short)FIX2INT_ROUND((FR).right);	\
	(R).bottom = (A_short)FIX2INT_ROUND((FR).bottom);	\
	} while (0)

#define CONVERT8TO16(A)		( (((long)(A) * PF_MAX_CHAN16) + PF_HALF_CHAN8) / PF_MAX_CHAN8 )

#define	RATIO2FLOAT(R)		(A_FpLong)((A_FpLong)(R).num / ((A_FpLong)(R).den))

#endif		// _H_AX_MACROS
