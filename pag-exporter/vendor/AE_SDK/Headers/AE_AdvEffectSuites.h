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
/*******************************************************************/

#ifndef _H_AE_AdvEffectSuites
#define _H_AE_AdvEffectSuites

#include <AE_Effect.h>
#include <AE_EffectUI.h>
#include <SPBasic.h>

#include <adobesdk/config/PreConfig.h>

#ifdef __cplusplus
	extern "C" {
#endif


#define kPFAdvAppSuite				"PF AE Adv App Suite"
#define kPFAdvAppSuiteVersion1		1	/* frozen in AE 5.0 */


typedef struct PF_AdvAppSuite1 {

		SPAPI	PF_Err	(*PF_SetProjectDirty)(void);

		SPAPI	PF_Err	(*PF_SaveProject)(void);

		SPAPI	PF_Err	(*PF_SaveBackgroundState)(void);

		SPAPI	PF_Err	(*PF_ForceForeground)(void);

		SPAPI	PF_Err	(*PF_RestoreBackgroundState)(void);

		SPAPI	PF_Err	(*PF_RefreshAllWindows)(void);

		// 2 lines of text, same as calling PF_InfoDrawText3( line1Z0, line2Z0, NULL)
		SPAPI	PF_Err	(*PF_InfoDrawText)(
								const A_char	*line1Z0,
								const A_char	*line2Z0);

		SPAPI	PF_Err	(*PF_InfoDrawColor)(
								PF_Pixel	color);

		// 3 lines of text
		SPAPI	PF_Err	(*PF_InfoDrawText3)(
								const A_char	*line1Z0,
								const A_char	*line2Z0,
								const A_char	*line3Z0);

		// 3 lines, with two lines including formatting for right and left justification
		SPAPI	PF_Err	(*PF_InfoDrawText3Plus)(
								const A_char	*line1Z0,
								const A_char	*line2_jrZ0,
								const A_char	*line2_jlZ0,
								const A_char	*line3_jrZ0,
								const A_char	*line3_jlZ0);
								
} PF_AdvAppSuite1;





#define kPFAdvAppSuiteVersion2		2/* to be frozen in  AE 6.0 */

typedef struct PF_AdvAppSuite2 {

		SPAPI	PF_Err	(*PF_SetProjectDirty)(void);

		SPAPI	PF_Err	(*PF_SaveProject)(void);

		SPAPI	PF_Err	(*PF_SaveBackgroundState)(void);

		SPAPI	PF_Err	(*PF_ForceForeground)(void);

		SPAPI	PF_Err	(*PF_RestoreBackgroundState)(void);

		SPAPI	PF_Err	(*PF_RefreshAllWindows)(void);

		// 2 lines of text, same as calling PF_InfoDrawText3( line1Z0, line2Z0, NULL)
		SPAPI	PF_Err	(*PF_InfoDrawText)(
								const A_char	*line1Z0,
								const A_char	*line2Z0);

		SPAPI	PF_Err	(*PF_InfoDrawColor)(
								PF_Pixel	color);

		// 3 lines of text
		SPAPI	PF_Err	(*PF_InfoDrawText3)(
								const A_char	*line1Z0,
								const A_char	*line2Z0,
								const A_char	*line3Z0);

		// 3 lines, with two lines including formatting for right and left justification
		SPAPI	PF_Err	(*PF_InfoDrawText3Plus)(
								const A_char	*line1Z0,
								const A_char	*line2_jrZ0,
								const A_char	*line2_jlZ0,
								const A_char	*line3_jrZ0,
								const A_char	*line3_jlZ0);

		// append a line of text to top line for so many ticks
		SPAPI	PF_Err	(*PF_AppendInfoText)(
								const A_char	*appendZ0);

								
} PF_AdvAppSuite2;



#define PF_MAX_TIME_LEN				31

enum { 
	PF_Step_FORWARD, 
	PF_Step_BACKWARD 
};
typedef A_LegacyEnumType PF_Step;

enum {
	PF_TimeDisplayFormatTimecode,
	PF_TimeDisplayFormatFrames,
	PF_TimeDisplayFormatFeetFrames // OBSOLETE: returned only by kPFAdvTimeSuiteVersion1
};
		
typedef struct {
	A_char		display_mode;
	A_long		framemax;
	A_long		frames_per_foot;
	A_char		frames_start;
	A_Boolean	nondrop30B;
	A_Boolean	honor_source_timecodeB;
	A_Boolean	use_feet_framesB;
} PF_TimeDisplayPrefVersion3;


		
#define kPFAdvTimeSuite				"PF AE Adv Time Suite"
#define kPFAdvTimeSuiteVersion4		4	//frozen for ae15.0
		
typedef struct PF_AdvTimeSuite4 {
	
	SPAPI PF_Err (*PF_FormatTimeActiveItem)(	A_long				time_valueUL,		// time is value/scale in seconds
											A_u_long			time_scaleL,
											PF_Boolean			durationB,			// is the time value a duration or time?
											A_char				*time_buf);			// allocate as PF_MAX_TIME_LEN + 1
	
	SPAPI PF_Err (*PF_FormatTime)(				PF_InData			*in_data,
										PF_EffectWorld		*world,
										A_long				time_valueUL,
										A_u_long			time_scaleL,
										PF_Boolean			durationB,
										A_char				*time_buf);
	
	SPAPI PF_Err (*PF_FormatTimePlus)(			PF_InData			*in_data,
									  PF_EffectWorld		*world,
									  A_long				time_valueUL,
									  A_u_long			time_scaleL,
									  PF_Boolean			comp_timeB,
									  PF_Boolean			durationB,
									  A_char				*time_buf);
	
	SPAPI PF_Err (*PF_GetTimeDisplayPref)(		PF_TimeDisplayPrefVersion3	*tdp,
										  A_long				*starting_frame_num);
	
	SPAPI PF_Err (*PF_TimeCountFrames)(	const A_Time*		start_timeTP,
										const A_Time*		time_stepTP,
										A_Boolean			include_partial_frameB,
										A_long*				frame_countL);
	
} PF_AdvTimeSuite4;
		
		
#define kPFAdvTimeSuite				"PF AE Adv Time Suite"
#define kPFAdvTimeSuiteVersion3		3	//frozen for ae14.2

typedef struct PF_AdvTimeSuite3 {

	SPAPI PF_Err (*PF_FormatTimeActiveItem)(	A_long				time_valueUL,		// time is value/scale in seconds
												A_u_long			time_scaleL,
												PF_Boolean			durationB,			// is the time value a duration or time?
												A_char				*time_buf);			// allocate as PF_MAX_TIME_LEN + 1
																	
	SPAPI PF_Err (*PF_FormatTime)(				PF_InData			*in_data,
												PF_EffectWorld		*world,
												A_long				time_valueUL,
												A_u_long			time_scaleL,
												PF_Boolean			durationB,
												A_char				*time_buf);
																	
	SPAPI PF_Err (*PF_FormatTimePlus)(			PF_InData			*in_data,
												PF_EffectWorld		*world,
												A_long				time_valueUL,
												A_u_long			time_scaleL,
												PF_Boolean			comp_timeB,
												PF_Boolean			durationB,
												A_char				*time_buf);
	
	SPAPI PF_Err (*PF_GetTimeDisplayPref)(		PF_TimeDisplayPrefVersion3	*tdp,
												A_long				*starting_frame_num);

} PF_AdvTimeSuite3;


#define kPFAdvTimeSuiteVersion2		2

typedef struct {
	A_char		display_mode;
	A_char		framemax;
	A_char		frames_per_foot;
	A_char		frames_start;
	A_Boolean	nondrop30B;
	A_Boolean	honor_source_timecodeB;
	A_Boolean	use_feet_framesB;
} PF_TimeDisplayPrefVersion2;

typedef struct PF_AdvTimeSuite2 {

	SPAPI PF_Err (*PF_FormatTimeActiveItem)(	A_long				time_valueUL,		// time is value/scale in seconds
												A_u_long		time_scaleL,
												PF_Boolean				durationB,			// is the time value a duration or time?
												A_char				*time_buf);			// allocate as PF_MAX_TIME_LEN + 1
																	
	SPAPI PF_Err (*PF_FormatTime)(				PF_InData			*in_data,
												PF_EffectWorld			*world,
												A_long				time_valueUL,
												A_u_long		time_scaleL,
												PF_Boolean				durationB,
												A_char				*time_buf);
																	
	SPAPI PF_Err (*PF_FormatTimePlus)(			PF_InData			*in_data,
												PF_EffectWorld			*world,
												A_long				time_valueUL,
												A_u_long		time_scaleL,
												PF_Boolean				comp_timeB,
												PF_Boolean				durationB,
												A_char				*time_buf);
																	
												
	SPAPI PF_Err (*PF_GetTimeDisplayPref)(		PF_TimeDisplayPrefVersion2	*tdp,
												A_long				*starting_frame_num);



} PF_AdvTimeSuite2;



#define kPFAdvTimeSuiteVersion1		1 /* frozen in AE 5.0 */

typedef struct {
	A_char	time_display_format;
	A_char	framemax;
	A_char	nondrop30;
	A_char	frames_per_foot;
} PF_TimeDisplayPref;

typedef struct PF_AdvTimeSuite1 {

	SPAPI PF_Err (*PF_FormatTimeActiveItem)(	A_long				time_valueUL,		// time is value/scale in seconds
												A_u_long		time_scaleL,
												PF_Boolean				durationB,			// is the time value a duration or time?
												A_char				*time_buf);			// allocate as PF_MAX_TIME_LEN + 1
																	
	SPAPI PF_Err (*PF_FormatTime)(				PF_InData			*in_data,
												PF_EffectWorld			*world,
												A_long				time_valueUL,
												A_u_long		time_scaleL,
												PF_Boolean				durationB,
												A_char				*time_buf);
																	
	SPAPI PF_Err (*PF_FormatTimePlus)(			PF_InData			*in_data,
												PF_EffectWorld			*world,
												A_long				time_valueUL,
												A_u_long		time_scaleL,
												PF_Boolean				comp_timeB,
												PF_Boolean				durationB,
												A_char				*time_buf);
																	
												
	SPAPI PF_Err (*PF_GetTimeDisplayPref)(		PF_TimeDisplayPref	*tdp,
												A_long				*starting_frame_num);

} PF_AdvTimeSuite1;



#define kPFAdvItemSuite				"PF AE Adv Item Suite"
#define kPFAdvItemSuiteVersion1		1	/* frozen in AE 5.0 */



typedef struct PF_AdvItemSuite1 {

	SPAPI PF_Err (*PF_MoveTimeStep)(			PF_InData		*in_data,
												PF_EffectWorld		*world,
												PF_Step			time_dir,
												A_long			num_stepsL);

	SPAPI PF_Err (*PF_MoveTimeStepActiveItem) (	PF_Step			time_dir,
												A_long			num_stepsL);

								
	SPAPI PF_Err (*PF_TouchActiveItem)			(void);


	SPAPI PF_Err (*PF_ForceRerender)(			PF_InData		*in_data,
												PF_EffectWorld		*world);
								

	SPAPI PF_Err (*PF_EffectIsActiveOrEnabled)(	PF_ContextH		contextH,
												PF_Boolean			*enabledPB);


} PF_AdvItemSuite1;



#ifdef __cplusplus
}
#endif

#include <adobesdk/config/PostConfig.h>


#endif
