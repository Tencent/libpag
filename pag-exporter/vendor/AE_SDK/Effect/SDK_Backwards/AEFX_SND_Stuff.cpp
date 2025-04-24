/*	AEFX_SND_Stuff.c

	Part of the Adobe After Effects 8.0 SDK.
	Copyright 2007-2023 Adobe Inc.
	All Rights Reserved.

*/

#ifndef	AEFX_SND_Stuff_DECL
#define AEFX_SND_Stuff_DECL

#include "AE_Effect.h"

#ifdef DEBUG
	#define AEFX_INLINE				static
#else
	
	#ifdef AE_OS_WIN
		#define AEFX_INLINE			static _inline
	#else
		#define AEFX_INLINE			static inline
	#endif

#endif	


AEFX_INLINE long
AEFX_RoundDouble(PF_FpLong	x)
{
	long	ret;
	
	if (x > 0) {
		ret = (long)(x + 0.5);
	} else {
		if ((long)(x + 0.5) == (x + 0.5)) {
			ret = (long)x;
		} else {
			ret = (long)(x - 0.5);
		}
	}
	
	return ret;
}

#define AEFX_ROUNDDBL(X)	AEFX_RoundDouble(X);


#define	AEFX_TypeFto8(TYPE)		(char)AEFX_ROUNDDBL((TYPE) * 127.0)
#define	AEFX_TypeFto16(TYPE)	(short)AEFX_ROUNDDBL((TYPE) * 32767.0)
#define	AEFX_Type8toF(TYPE)		((TYPE) / 128.0)
#define	AEFX_Type16toF(TYPE)	((TYPE) / 32768.0)

#define	AEFX_8_SIGNED_MAX		((char) 0x7F)
#define	AEFX_8_SIGNED_MIN		((char) 0x80)
#define	AEFX_16_SIGNED_MAX		((short)0x7FFF)
#define	AEFX_16_SIGNED_MIN		((short)0x8000)
#define	AEFX_FLOAT_SIGNED_MAX	((float)  1.0)
#define	AEFX_FLOAT_SIGNED_MIN	((float) -1.0)



#define AEFX_CLIP_8_SIGNED( VALUE )									\
 			if ((VALUE) > AEFX_8_SIGNED_MAX)						\
				(VALUE) = AEFX_8_SIGNED_MAX;						\
			else if ((VALUE) < AEFX_8_SIGNED_MIN)					\
				(VALUE) = AEFX_8_SIGNED_MIN

#define AEFX_CLIP_16_SIGNED( VALUE )								\
 			if ((VALUE) > AEFX_16_SIGNED_MAX)						\
				(VALUE) = AEFX_16_SIGNED_MAX;						\
			else if ((VALUE) < AEFX_16_SIGNED_MIN)					\
				(VALUE) = AEFX_16_SIGNED_MIN

#define AEFX_CLIP_FLOAT_SIGNED( VALUE )								\
 			if ((VALUE) > AEFX_FLOAT_SIGNED_MAX)					\
				(VALUE) = AEFX_FLOAT_SIGNED_MAX;					\
			else if ((VALUE) < AEFX_FLOAT_SIGNED_MIN)				\
				(VALUE) = AEFX_FLOAT_SIGNED_MIN



#endif //AEFX_SND_Stuff_DECL