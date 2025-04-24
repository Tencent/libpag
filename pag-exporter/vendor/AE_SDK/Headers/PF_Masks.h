/* PF_Masks.h */


#ifndef H_PF_MASKS
#define H_PF_MASKS

#include <A.h>
#include <AE_Effect.h>
#include <SPBasic.h>
#include <AE_EffectSuites.h>


#pragma pack( push, 2 )

#define PF_MASKS_MAJOR_VERSION			1
#define PF_MASKS_MINOR_VERSION			0

#ifdef __cplusplus
	extern "C" {
#endif


#define kPF_MaskSuite				"AEGP Mask Suite"
#define kPF_MaskSuiteVersion1		1

typedef struct PF_MaskSuite1 {

	SPAPI A_Err (*PF_MaskWorldWithPath)(
			PF_ProgPtr			effect_ref,
			PF_PathOutlinePtr	*mask,			/* >> */
			PF_FpLong			feather_x,		/* >> */
			PF_FpLong			feather_y,		/* >> */	
			PF_Boolean			invert,			/* >> */
			PF_FpLong			opacity,		/* >> 0...1 */
			PF_Quality			quality,		/* >> */
			PF_EffectWorld			*worldP,		/* <> */
			PF_Rect				*bboxPR0);		/* >> */
				
} PF_MaskSuite1;

#ifdef __cplusplus
}		// end extern "C"
#endif


#pragma pack( pop )


#endif /* H_PH_MASKS */