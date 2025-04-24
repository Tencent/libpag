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

#include	"ColorGrid.h"

PF_Err	
CreateDefaultArb(	
	PF_InData			*in_data,
	PF_OutData			*out_data,
	PF_ArbitraryH		*dephault)
{
	PF_Err			err		= PF_Err_NONE;
	PF_Handle		arbH	= NULL;
	
	AEGP_SuiteHandler	suites(in_data->pica_basicP);
	
	arbH = suites.HandleSuite1()->host_new_handle(sizeof(CG_ArbData));

	if (arbH) {
		CG_ArbData	*arbP = reinterpret_cast<CG_ArbData*>(PF_LOCK_HANDLE(arbH));
		if (!arbP) {
			err = PF_Err_OUT_OF_MEMORY;
		} else {
			AEFX_CLR_STRUCT(*arbP);
			#ifdef AE_OS_WIN
				#pragma warning (disable : 4305)
			#endif
			arbP->colorA[0].alpha	= 1.0;
			arbP->colorA[0].blue	= 0.0;
			arbP->colorA[0].green	= 0.5;
			arbP->colorA[0].red		= 1.0;
			
			arbP->colorA[1].alpha	= 1.0;
			arbP->colorA[1].blue	= 0.5;
			arbP->colorA[1].green	= 0.32;
			arbP->colorA[1].red		= 0.67;
			
			arbP->colorA[2].alpha	= 1.0;
			arbP->colorA[2].blue	= 0.6;
			arbP->colorA[2].green	= 0.27;
			arbP->colorA[2].red		= 0.34;
			
			arbP->colorA[3].alpha	= 1.0;
			arbP->colorA[3].blue	= 1.0;
			arbP->colorA[3].green	= 0.4039;
			arbP->colorA[3].red		= 0.47;
			
			arbP->colorA[4].alpha	= 1.0;
			arbP->colorA[4].blue	= 0.47;
			arbP->colorA[4].green	= 0.54;
			arbP->colorA[4].red		= 0.603;
			
			arbP->colorA[5].alpha	= 1.0;
			arbP->colorA[5].blue	= 0.0603;
			arbP->colorA[5].green	= 0.67;
			arbP->colorA[5].red		= 0.9;
			
			arbP->colorA[6].alpha	= 1.0;
			arbP->colorA[6].blue	= 0.737;
			arbP->colorA[6].green	= 0.81;
			arbP->colorA[6].red		= 0.89;
			
			arbP->colorA[7].alpha	= 1.0;
			arbP->colorA[7].blue	= 0.5;
			arbP->colorA[7].green	= 1.0;
			arbP->colorA[7].red		= 0.5;
			
			arbP->colorA[8].alpha	= 1.0;
			arbP->colorA[8].blue	= 0.5;
			arbP->colorA[8].green	= 0.5;
			arbP->colorA[8].red		= 0.9;
			#ifdef AE_OS_WIN
				#pragma warning (pop)
			#endif
			*dephault = arbH;
		}
		suites.HandleSuite1()->host_unlock_handle(arbH);
	}
	return err;
}

PF_Err
Arb_Copy(	
	PF_InData				*in_data,
	PF_OutData				*out_data,
	const PF_ArbitraryH		*srcP,
	PF_ArbitraryH			*dstP)
{
	PF_Err err = PF_Err_NONE;

	PF_Handle	sourceH = *srcP;
	
	AEGP_SuiteHandler suites(in_data->pica_basicP);

	if (sourceH) {
		CG_ArbData	*src_arbP = reinterpret_cast<CG_ArbData*>(suites.HandleSuite1()->host_lock_handle(sourceH));
		if (!src_arbP) {
			err = PF_Err_OUT_OF_MEMORY;
		} else {	
			PF_Handle	destH = *dstP;
			if (destH) {
				CG_ArbData	*dst_arbP = reinterpret_cast<CG_ArbData*>(suites.HandleSuite1()->host_lock_handle(destH));
				if (!dst_arbP) {
					err = PF_Err_OUT_OF_MEMORY;
				} else 	{
					memcpy(dst_arbP,src_arbP,sizeof(CG_ArbData));
					suites.HandleSuite1()->host_unlock_handle(destH);
				}
			}
			suites.HandleSuite1()->host_unlock_handle(sourceH);
		}	
	}
	return err;
}


PF_Err
Arb_Interpolate(
	PF_InData				*in_data,
	PF_OutData				*out_data,
	double					intrp_amtF,
	const PF_ArbitraryH		*l_arbPH,
	const PF_ArbitraryH		*r_arbPH,
	PF_ArbitraryH			*intrp_arbP)
{
	PF_Err			err			= PF_Err_NONE;

	CG_ArbData		*int_arbP	= NULL,
					*l_arbP		= NULL,
					*r_arbP		= NULL;

	PF_PixelFloat	*headP		= NULL,
					*lpixP		= NULL,
					*rpixP		= NULL;

	A_short 		iS 			= 0;

	AEGP_SuiteHandler suites(in_data->pica_basicP);
	
	int_arbP	= reinterpret_cast<CG_ArbData*>(suites.HandleSuite1()->host_lock_handle(*intrp_arbP));
	l_arbP		= reinterpret_cast<CG_ArbData*>(suites.HandleSuite1()->host_lock_handle(*l_arbPH));
	r_arbP		= reinterpret_cast<CG_ArbData*>(suites.HandleSuite1()->host_lock_handle(*r_arbPH));

	headP	= reinterpret_cast<PF_PixelFloat*>(int_arbP);
	lpixP	= reinterpret_cast<PF_PixelFloat*>(l_arbP);
	rpixP	= reinterpret_cast<PF_PixelFloat*>(r_arbP);

	for(iS = 0; iS < CG_ARBDATA_ELEMENTS; ++iS) {
		ColorGrid_InterpPixel(	intrp_amtF,
								lpixP,
								rpixP,
								headP);
		lpixP++;
		rpixP++;
		headP++;
	}
	suites.HandleSuite1()->host_unlock_handle(*intrp_arbP);
	suites.HandleSuite1()->host_unlock_handle(*l_arbPH);
	suites.HandleSuite1()->host_unlock_handle(*r_arbPH);
	return err;
}

PF_Err
Arb_Compare(
	PF_InData				*in_data,
	PF_OutData				*out_data,
	const PF_ArbitraryH		*a_arbP,
	const PF_ArbitraryH		*b_arbP,
	PF_ArbCompareResult		*resultP)
{
	PF_Err err = PF_Err_NONE;
	
	PF_Handle	a_handle = *a_arbP,
				b_handle = *b_arbP;

	PF_FpShort		total_a_rL	= 0,
					total_a_gL	= 0,
					total_a_bL	= 0,
					total_aL	= 0,
				
					total_b_rL	= 0,
					total_b_gL	= 0,
					total_b_bL	= 0,
					total_bL	= 0;

	*resultP = PF_ArbCompare_EQUAL;

	if (a_handle && b_handle) {
		CG_ArbData	*first_arbP;
		CG_ArbData  *second_arbP;
				
		first_arbP = (CG_ArbData*)PF_LOCK_HANDLE(a_handle);
		second_arbP = (CG_ArbData*)PF_LOCK_HANDLE(b_handle);

		if (!first_arbP || !second_arbP) {
			err = PF_Err_UNRECOGNIZED_PARAM_TYPE;
		} else {	
			A_long iL = 0;
			
			while (iL++ < CG_ARBDATA_ELEMENTS){	
				total_a_rL	+= first_arbP->colorA->red;
				total_a_gL	+= first_arbP->colorA->green;
				total_a_bL	+= first_arbP->colorA->blue;

				total_b_rL	+= second_arbP->colorA->red;
				total_b_gL	+= second_arbP->colorA->green;
				total_b_bL	+= second_arbP->colorA->blue;
				
				first_arbP++;
				second_arbP++;
			}

			total_aL = total_a_rL + total_a_gL + total_a_bL;
			total_bL = total_b_rL + total_b_gL + total_b_bL; 

			if(total_aL > total_bL)	{
				*resultP = PF_ArbCompare_MORE;
			} else if(total_aL < total_bL){
				*resultP = PF_ArbCompare_LESS;
			} else {
				*resultP = PF_ArbCompare_EQUAL;
			}
			PF_UNLOCK_HANDLE(a_handle);
			PF_UNLOCK_HANDLE(b_handle);
		}
	}
	return err;
}

PF_Err
Arb_Print_Size()
{
	// This size is actually provided directly in ColorGrid.cpp,
	// in response to PF_Arbitrary_PRINT_FUNC
	PF_Err err = PF_Err_NONE;
	return err;
}


PF_Err
Arb_Print(
	PF_InData			*in_data,
	PF_OutData			*out_data,
	PF_ArbPrintFlags	print_flags,
	PF_ArbitraryH		arbH,
	A_u_long			print_sizeLu,
	A_char				*print_bufferPC)
{
	PF_Err		err			= PF_Err_NONE;
	PF_Handle	a_handle	= arbH;
	A_long 		countL 		= 0;
	if (a_handle) {
		CG_ArbData	*a_arbP;
				
		a_arbP = (CG_ArbData*)PF_LOCK_HANDLE(a_handle);

		if (!a_arbP) {
			err = PF_Err_UNRECOGNIZED_PARAM_TYPE;
		} else {	
			if (!print_flags && print_sizeLu >= COLORGRID_ARB_MAX_PRINT_SIZE) {
				A_u_long		indexLu = 0;
				A_char			bufAC[64];

				// All data for this one parameter must be on the same line
				for (countL = 0; countL < CG_ARBDATA_ELEMENTS; ++countL) {
					PF_SPRINTF(bufAC, "ColorGrid %d\t", countL + 1);

					ERR(AEFX_AppendText(bufAC, (const A_u_long)print_sizeLu, print_bufferPC, &indexLu));	

					if (!err) {
						PF_SPRINTF(	bufAC, 
									"R=%f\tG=%f\tB=%f\t",
									a_arbP->colorA[countL].red,
									a_arbP->colorA[countL].green,
									a_arbP->colorA[countL].blue);

						ERR(AEFX_AppendText(bufAC, (const A_u_long)print_sizeLu, print_bufferPC, &indexLu));	
					}
				}
			} else if ((print_sizeLu) && print_bufferPC) {
				print_bufferPC[0] = 0x00;
			}
			PF_UNLOCK_HANDLE(a_handle);
		}
	}
	if (err == AEFX_ParseError_APPEND_ERROR) {
		err = PF_Err_OUT_OF_MEMORY; // increase COLORGRID_ARB_MAX_PRINT_SIZE
	}
	return err;
}

PF_Err
Arb_Scan(
		PF_InData			*in_data,
		PF_OutData			*out_data,
		void 				*refconPV,
		const char			*bufPC,		
		unsigned long		bytes_to_scanLu,
		PF_ArbitraryH		*arbPH)
{
	PF_Err err = PF_Err_NONE;
/*
	err = CreateDefaultArb(	in_data,
							out_data,
							arbPH);
	if (!err) 
	{
		PF_Handle	a_handle = *arbPH;

		if (a_handle) {
			CG_ArbData		*a_arbP;
			unsigned long	indexLu = 0;
					
			a_arbP = (CG_ArbData*)PF_LOCK_HANDLE(a_handle);


		if (!a_arbP) 
		{
			err = PF_Err_UNRECOGNIZED_PARAM_TYPE;
		} 
		else 
		{	
			long			countL = 0;
			char			tabAC[2];

			//tabAC[0] = AEFX_Char_TAB;
			tabAC[0] = '\t'; 
			tabAC[1] = 0x00;

			for (countL = 0; !err && countL < CG_ARBDATA_ELEMENTS; countL++) 
			{

				err = AEFX_MatchCell(	in_data,	
										out_data,	
										g_dim_namesAC[countL],
										bufPC,
										&indexLu,	
										NULL);
			}

			if (!err) err = AEFX_AppendText(tabAC, print_sizeLu, print_bufferPC, &indexLu);	
			{
				a_arbP++;
			}
			else if ((print_sizeLu) && print_bufferPC) 
			{
				print_bufferPC[0] = 0x00;
			}

			PF_UNLOCK_HANDLE(a_handle);
		}
	}

	if (err == AEFX_ParseError_APPEND_ERROR) {
		err = PF_Err_OUT_OF_MEMORY; // increase COLORGRID_ARB_MAX_PRINT_SIZE
	}
*/
	return err;
}

void
ColorGrid_InterpPixel(
		PF_FpLong		intrp_amtF,
		PF_PixelFloat	*lpixP,
		PF_PixelFloat	*rpixP,
		PF_PixelFloat	*headP)
{
	headP->alpha = 1.0;

	if(lpixP->blue > rpixP->blue){
		headP->blue = (lpixP->blue - rpixP->blue) * intrp_amtF;
		headP->blue = lpixP->blue - headP->blue;
	} else if (lpixP->blue < rpixP->blue){
		headP->blue = (rpixP->blue - lpixP->blue) * intrp_amtF;
		headP->blue += lpixP->blue;
	} else{
		headP->blue = lpixP->blue;
	}
	
	if(lpixP->green > rpixP->green){
		headP->green = (lpixP->green - rpixP->green) * intrp_amtF;
		headP->green = lpixP->green - headP->green;
	} else if (lpixP->green < rpixP->green){
		headP->green = (rpixP->green - lpixP->green) * intrp_amtF;
		headP->green += lpixP->green;
	} else {
		headP->green = lpixP->green;
	}

	if(lpixP->red > rpixP->red){
		headP->red = (lpixP->red - rpixP->red) * intrp_amtF;
		headP->red = lpixP->red - headP->red;
	} else if (lpixP->red < rpixP->red){
		headP->red = (rpixP->red - lpixP->red) * intrp_amtF;
		headP->red += lpixP->red;
	} else {
		headP->red = lpixP->red;
	}
}

PF_Err			
AEFX_AppendText(	
	A_char					*srcAC,				/* >> */
	const A_u_long			dest_sizeLu,		/* >> */
	A_char					*destAC,			/* <> */
	A_u_long				*current_indexPLu)	/* <> */
{
	PF_Err			err = PF_Err_NONE;

	A_u_long		new_strlenLu = strlen(srcAC) + *current_indexPLu;


	if (new_strlenLu <= dest_sizeLu) {
		destAC[*current_indexPLu] = 0x00;

#ifdef AE_OS_WIN
		strncat_s(destAC, dest_sizeLu, srcAC, strlen(srcAC));
#else
		strncat(destAC, srcAC, strlen(srcAC));
#endif
		*current_indexPLu = new_strlenLu;
	} else {
		err = AEFX_ParseError_APPEND_ERROR;
	}

	return err;
}
