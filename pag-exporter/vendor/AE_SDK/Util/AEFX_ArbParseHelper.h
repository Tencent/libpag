// AEFX_ArbParseHelper.h
//

// This file has no header, and is designed to by #included as necessary. -jja 9/30/98


#ifndef _H_AEFX_ArbParseHelper
#define _H_AEFX_ArbParseHelper

#include <AE_Effect.h>

#define AEFX_Char_TAB			'\t'
#define AEFX_Char_EOL			'\r'
#define AEFX_Char_SPACE			' '

#ifndef AEFX_CELL_SIZE
#define		AEFX_CELL_SIZE			256
#endif

#ifdef __cplusplus
extern "C" {
#endif


enum {
	AEFX_ParseError_EXPECTING_MORE_DATA = 0x00FEBE00,
	AEFX_ParseError_APPEND_ERROR,
	AEFX_ParseError_EXPECTING_A_NUMBER,
	AEFX_ParseError_MATCH_ERROR
};
typedef A_long AEFX_ParseErrors;



#ifndef STR_EQUAL
	#define STR_EQUAL(A, B) (strcmp((A),(B)) == 0)
#endif


PF_Err			
AEFX_AppendText(	A_char					*srcAC,				/* >> */
					const A_u_long		dest_sizeL,			/* >> */
					A_char					*destAC,			/* <> */
					A_u_long			*current_indexPLu);	/* <> */


PF_Err
AEFX_ParseFpLong(	PF_InData			*in_data,				/* >> */
					PF_OutData			*out_data,				/* >> */
					const A_char			*startPC,				/* >> */
					A_u_long		*current_indexPL,		/* << */
					PF_FpLong			*dPF);					/* << */


PF_Err
AEFX_MatchCell(	PF_InData			*in_data,				/* >> */
				PF_OutData			*out_data,				/* >> */
				const A_char			*strPC,					/* >> */
				const A_char			*startPC,				/* >> */
				A_u_long		*current_indexPL,		/* << */
				PF_Boolean			*matchPB0);				/* << */

PF_Err
AEFX_ParseCell(	PF_InData			*in_data,				/* >> */
				PF_OutData			*out_data,				/* >> */
				const A_char			*startPC,				/* >> */
				A_u_long		*current_indexPL,		/* << */
				A_char				*bufAC);				/* << AEFX_CELL_SIZE */


#ifdef __cplusplus
} // extern "C"
#endif

#endif
