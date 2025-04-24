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

#ifndef GAMMA_TABLE_H
#define GAMMA_TABLE_H

#include "AEConfig.h"

#ifdef AE_OS_WIN
#include <Windows.h>
#endif

#include "entry.h"
#include "AE_Effect.h"
#include "A.h"
#include "AE_EffectCB.h"
#include "AE_Macros.h"
#include "Param_Utils.h"


#define	MAJOR_VERSION		2
#define	MINOR_VERSION		1
#define	BUG_VERSION			0
#define	STAGE_VERSION		PF_Stage_DEVELOP
#define	BUILD_VERSION		1

#define	NAME "Gamma_Table"
#define DESCRIPTION	"Perform simple image gamma correction.   Copyright 1994-2023 Adobe Inc."

enum {
	GAMMA_INPUT = 0,	// default input layer 
	GAMMA_GAMMA,		// gamma correction factor
	GAMMA_NUM_PARAMS
};

enum {
	GAMMA_DISK_ID = 1
};

typedef struct {
	PF_Fixed	gamma_val;	
	A_u_char	lut[256];
} Gamma_Table;

typedef struct {				
	unsigned char		*lut;
} GammaInfo;

#define	GAMMA_MIN		(0)	
#define	GAMMA_MAX		(2)
#define	GAMMA_BIG_MAX	(2)
#define	GAMMA_DFLT		(1)

extern "C" {

	DllExport
	PF_Err 
	EffectMain(
		PF_Cmd			cmd,
		PF_InData		*in_data,
		PF_OutData		*out_data,
		PF_ParamDef		*params[],
		PF_LayerDef		*output);

}
	
#endif // GAMMA_TABLE_H