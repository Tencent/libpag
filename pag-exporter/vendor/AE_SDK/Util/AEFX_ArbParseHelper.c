// AEFX_ArbParseHelper.c  
//



#include "AEFX_ArbParseHelper.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

PF_Err			
AEFX_AppendText(	A_char					*srcAC,				/* >> */
					const A_u_long		dest_sizeL,			/* >> */
					A_char					*destAC,			/* <> */
					A_u_long			*current_indexPLu)	/* <> */
{
	PF_Err			err = PF_Err_NONE;

	A_u_long	new_strlenLu = (A_u_long) strlen(srcAC) + *current_indexPLu;


	if (new_strlenLu <= dest_sizeL) {
		destAC[*current_indexPLu] = 0x00;

		strcat(destAC, srcAC);
		*current_indexPLu = new_strlenLu;
	} else {
		err = AEFX_ParseError_APPEND_ERROR;
	}

	return err;
}




PF_Err
AEFX_ParseCell(	PF_InData			*in_data,				/* >> */
				PF_OutData			*out_data,				/* >> */
				const A_char			*startPC,				/* >> */
				A_u_long		*current_indexPL,		/* << */
				A_char				*bufAC)					/* << AEFX_CELL_SIZE */
{
	PF_Err	err = PF_Err_NONE;
	A_char	c;
	A_char	buf[AEFX_CELL_SIZE];
	A_char	*scan;
	A_short	count;
	
	if (startPC[*current_indexPL] == AEFX_Char_EOL) {
		err = AEFX_ParseError_EXPECTING_MORE_DATA;
	} else if (startPC[*current_indexPL] == 0) {
		err = AEFX_ParseError_EXPECTING_MORE_DATA;
	} else {
		count = 0;
		scan = buf;
		while ((c = startPC[(*current_indexPL)++]) != 0) {
			if (c == AEFX_Char_TAB)
				break;
			if (c == AEFX_Char_EOL) {
				(*current_indexPL)--;
				break;
			}
			
			*scan++ = c;
			
			if (++count >= AEFX_CELL_SIZE)
				break;
		}
		*scan = 0;
		
		// trim spaces from head
		scan = buf;
		while (isspace(*scan))
			scan++;
		
		strcpy(bufAC, scan);
		
		// trim spaces from end (guaranteed not to scan past start of string, because
		//		if the string were all spaces, it would be trimmed down to nothing in the
		//		head-trim step above
		if (*bufAC) {
			scan = bufAC + strlen(bufAC) - 1;
			while (*scan == AEFX_Char_SPACE) {
				*scan-- = 0;
			}
		}
	}
	
	return err;

}


/** AEFX_ParseFpLong

	Reads in the next cell in the input stream and converts it to a double.
	The first non-space character must be numeric.

**/
PF_Err
AEFX_ParseFpLong(	PF_InData			*in_data,				/* >> */
					PF_OutData			*out_data,				/* >> */
					const A_char			*startPC,				/* >> */
					A_u_long		*current_indexPL,		/* << */
					PF_FpLong			*dPF)					/* << */
{
	PF_Err	err = PF_Err_NONE;
	A_char	c;
	A_char	*end_ptr;
	A_char	buf[AEFX_CELL_SIZE];
	
	err = AEFX_ParseCell(in_data, out_data, startPC, current_indexPL, buf);

	if (!err) {
		c = buf[0];
		
		if (!isdigit(c) && c != '.' && c != '-') {
			err = AEFX_ParseError_EXPECTING_A_NUMBER;
		} else {
			errno = 0;
			*dPF = strtod(buf, &end_ptr);
		}
	}
	
	return err;
}



PF_Err
AEFX_MatchCell(	PF_InData			*in_data,				/* >> */
				PF_OutData			*out_data,				/* >> */
				const A_char			*strPC,					/* >> */
				const A_char			*startPC,				/* >> */
				A_u_long		*current_indexPL,		/* << */
				PF_Boolean			*matchPB0)				/* << */
{
	PF_Err			err = PF_Err_NONE;
	A_char			buf[AEFX_CELL_SIZE];
	A_u_long	origLu = *current_indexPL;
	PF_Boolean		found = 0;

	if (startPC[*current_indexPL] == AEFX_Char_EOL) {
		found = 0;
		err = AEFX_ParseError_EXPECTING_MORE_DATA;
	} else {
		err = AEFX_ParseCell(in_data, out_data, startPC, current_indexPL, buf);

		if (!err) {
			found = STR_EQUAL(buf, strPC);
		
			if (!found)
				*current_indexPL = origLu;
		}
	}
	
	if (!err && !found && matchPB0 == NULL) {
		err = AEFX_ParseError_MATCH_ERROR;
	}

	if (matchPB0)
		*matchPB0 = found;

	return err;
}


