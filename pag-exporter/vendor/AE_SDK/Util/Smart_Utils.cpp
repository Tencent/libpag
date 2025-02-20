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

#include "Smart_Utils.h"


PF_Boolean IsEmptyRect(const PF_LRect *r){
	return (r->left >= r->right) || (r->top >= r->bottom);
}

void UnionLRect(const PF_LRect *src, PF_LRect *dst)
{
	if (IsEmptyRect(dst)) {
		*dst = *src;
	} else if (!IsEmptyRect(src)) {
		dst->left 	= mmin(dst->left, src->left);
		dst->top  	= mmin(dst->top, src->top);
		dst->right 	= mmax(dst->right, src->right);
		dst->bottom = mmax(dst->bottom, src->bottom);
	}
}

PF_Boolean
IsEdgePixel(
	PF_LRect	*rectP,
	A_long		x,
	A_long		y)
{
	PF_Boolean 	x_hitB = FALSE,
				y_hitB = FALSE;
				
	x_hitB = ((x == rectP->left)  || (x == rectP->right));
	
	y_hitB = ((y == rectP->top)  || (y == rectP->bottom));
	
	if (x_hitB){
		y_hitB = ((y >= rectP->top) && (y <= rectP->bottom));
	} else {
		if (y_hitB){
			x_hitB = ((x >= rectP->left) && (x <= rectP->right));
		}
	}
	return (x_hitB && y_hitB);
}