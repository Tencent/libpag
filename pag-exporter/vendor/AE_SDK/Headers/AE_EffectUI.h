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

/** AE_EffectUI.h

	Part of the After Effects SDK.

	Describes structures and callbacks used by plug-ins with Custom
	composition window and effect palette UIs.
 
**/

#ifndef _H_AE_EffectUI
#define _H_AE_EffectUI


#include <AE_Effect.h>

#ifdef ADOBE_SDK_INTERNAL		
	#include <adobesdk/private/drawbotsuite/DrawbotSuiteTypes.h>
#else
	#include <adobesdk/drawbotsuite/DrawbotSuiteTypes.h>
#endif


#include <adobesdk/config/PreConfig.h>





#ifdef __cplusplus
	extern "C" {
#endif


/** PF_CustomFlags
 **
 ** kinds of events and actions the custom parameter type might require
 **
 **/

enum {
	PF_CustomEFlag_NONE 			= 0,

	PF_CustomEFlag_COMP 			= 1L << 0,
	PF_CustomEFlag_LAYER			= 1L << 1,
	PF_CustomEFlag_EFFECT			= 1L << 2,
	PF_CustomEFlag_PREVIEW			= 1L << 3
};
typedef A_long PF_CustomEventFlags;


enum {
	PF_Window_NONE = -1,
	PF_Window_COMP,
	PF_Window_LAYER,
	PF_Window_EFFECT,
	PF_Window_PREVIEW
};
typedef A_long PF_WindowType;


/* $$$ document!

	new context -> evt extra has context and type, but everything else should be ignored
		fill in plugin_state[4]
	close context -> 
	all other evts -> params set up, but no layers 

*/
enum {
	PF_Event_NONE = -1,
	PF_Event_NEW_CONTEXT,
	PF_Event_ACTIVATE,
	PF_Event_DO_CLICK,
	PF_Event_DRAG,
	PF_Event_DRAW,
	PF_Event_DEACTIVATE,
	PF_Event_CLOSE_CONTEXT,
	PF_Event_IDLE,
	PF_Event_KEYDOWN_OBSOLETE,	// As of AE 7, this is no longer used.
	PF_Event_ADJUST_CURSOR,		// new for AE 4.0, sent when mouse moves over custom UI
	PF_Event_KEYDOWN,			// new for AE 7.0, replaces previous keydown event with cross platform codes and unicode characters.
	PF_Event_MOUSE_EXITED,		// new for AE 11.0, notification that the mouse is no longer over a specific view (layer or comp only).
	
	PF_Event_NUM_EVENTS
};
typedef A_long PF_EventType;


enum {
	PF_Cursor_NONE = 0,			// see comment in PF_AdjustCursorEventInfo
	PF_Cursor_CUSTOM,			// means effect set cursor itself with platform-specific calls
	PF_Cursor_ARROW,
	PF_Cursor_HOLLOW_ARROW,
	PF_Cursor_WATCH_N_WAIT,		// watch on the Mac, wait (hourglass) on Windows
	PF_Cursor_MAGNIFY,
	PF_Cursor_MAGNIFY_PLUS,
	PF_Cursor_MAGNIFY_MINUS,
	PF_Cursor_CROSSHAIRS,
	PF_Cursor_CROSS_RECT,
	PF_Cursor_CROSS_OVAL,
	PF_Cursor_CROSS_ROTATE,
	PF_Cursor_PAN,
	PF_Cursor_EYEDROPPER,
	PF_Cursor_HAND,
	PF_Cursor_PEN,
	PF_Cursor_PEN_ADD,
	PF_Cursor_PEN_DELETE,
	PF_Cursor_PEN_CLOSE,
	PF_Cursor_PEN_DRAG,
	PF_Cursor_PEN_CORNER,
	PF_Cursor_RESIZE_VERTICAL,
	PF_Cursor_RESIZE_HORIZONTAL,
	PF_Cursor_FINGER_POINTER,
	PF_Cursor_SCALE_HORIZ, 
	PF_Cursor_SCALE_DIAG_LR, 
	PF_Cursor_SCALE_VERT, 
	PF_Cursor_SCALE_DIAG_UR, 
	PF_Cursor_ROT_TOP, 
	PF_Cursor_ROT_TOP_RIGHT, 
	PF_Cursor_ROT_RIGHT, 
	PF_Cursor_ROT_BOT_RIGHT, 
	PF_Cursor_ROT_BOTTOM, 
	PF_Cursor_ROT_BOT_LEFT, 
	PF_Cursor_ROT_LEFT, 
	PF_Cursor_ROT_TOP_LEFT, 
	PF_Cursor_DRAG_CENTER,
	PF_Cursor_COPY, 
	PF_Cursor_ALIAS, 
	PF_Cursor_CONTEXT, 
	PF_Cursor_SLIP_EDIT, 
	PF_Cursor_CAMERA_ORBIT_CAMERA, 		//changed from PF_Cursor_ORBIT
	PF_Cursor_CAMERA_PAN_CAMERA,		//changed from PF_Cursor_TRACK_XY
	PF_Cursor_CAMERA_DOLLY_CAMERA, 		//changed from PF_Cursor_TRACK_Z
	PF_Cursor_ROTATE_X, 
	PF_Cursor_ROTATE_Y, 
	PF_Cursor_ROTATE_Z, 
	PF_Cursor_ARROW_X, 
	PF_Cursor_ARROW_Y, 
	PF_Cursor_ARROW_Z, 
	PF_Cursor_SCISSORS, 
	PF_Cursor_FAT_EYEDROPPER, 
	PF_Cursor_FINGER_POINTER_SCRUB, 
	PF_Cursor_HORZ_I_BEAM, 
	PF_Cursor_VERT_I_BEAM, 
	PF_Cursor_HORZ_BOX_I_BEAM, 
	PF_Cursor_VERT_BOX_I_BEAM, 
	PF_Cursor_I_BEAM_0,
	PF_Cursor_I_BEAM_11_25,
	PF_Cursor_I_BEAM_22_5,
	PF_Cursor_I_BEAM_33_75,
	PF_Cursor_I_BEAM_45,
	PF_Cursor_I_BEAM_56_25,
	PF_Cursor_I_BEAM_67_5,
	PF_Cursor_I_BEAM_78_75,
	PF_Cursor_I_BEAM_90,
	PF_Cursor_I_BEAM_101_25,
	PF_Cursor_I_BEAM_112_5,
	PF_Cursor_I_BEAM_123_75,
	PF_Cursor_I_BEAM_135,
	PF_Cursor_I_BEAM_146_25,
	PF_Cursor_I_BEAM_157_5,
	PF_Cursor_I_BEAM_168_75,
	PF_Cursor_CROSSHAIRS_PICKUP,
	PF_Cursor_ARROW_SELECTOR,
	PF_Cursor_LAYER_MOVE,
	PF_Cursor_MOVE_START_MARGIN,
	PF_Cursor_MOVE_END_MARGIN,
	PF_Cursor_SOLID_ARROW,
	PF_Cursor_HOLLOW_ARROW_PLUS,
	PF_Cursor_BRUSH_CENTER,
	PF_Cursor_CLONE_SOURCE,
	PF_Cursor_CLONE_SOURCE_OFFSET,
	PF_Cursor_HOLLOW_LAYER_MOVE,
	PF_Cursor_MOVE_TRACK_SEARCH_REGION,
	PF_Cursor_MOVE_TRACK_ATTACH_POINT,
	PF_Cursor_COLOR_CUBE_CROSS_SECTION,
	PF_Cursor_PEN_CORNER_ROTOBEZ_TENSION, 
	PF_Cursor_PIN, 
	PF_Cursor_PIN_ADD, 
	PF_Cursor_MESH_ADD, 
	PF_Cursor_MARQUEE, 
	PF_Cursor_CROSS_ROUNDED_RECT, 
	PF_Cursor_CROSS_POLYGON, 
	PF_Cursor_CROSS_STAR, 
	PF_Cursor_PIN_STARCH, 
	PF_Cursor_PIN_OVERLAP, 
	PF_Cursor_STOPWATCH,
	PF_Cursor_DRAG_DOT,
	PF_Cursor_DRAG_CIRCLE,
	PF_Cursor_DIRECT_SELECT,
	PF_Cursor_DRAG_COPY_MOVE,
	PF_Cursor_DRAG_COPY_ROTATE,
	PF_Cursor_CAMERA_MAYA,			//Changed from PF_Cursor_MAYA
	PF_Cursor_RESIZE_HORIZONTAL_LEFT,
	PF_Cursor_RESIZE_HORIZONTAL_RIGHT,
	PF_Cursor_FEATHER,
	PF_Cursor_FEATHER_ADD,
	PF_Cursor_FEATHER_DELETE,
	PF_Cursor_FEATHER_MOVE,
	PF_Cursor_FEATHER_TENSION, 
	PF_Cursor_FEATHER_MARQUEE,
	PF_Cursor_LASSO_ARROW, 
	PF_Cursor_DRAG_NO_DROP,
	PF_Cursor_DRAG_COPY,
	PF_Cursor_DRAG_LINK,
	PF_Cursor_PIN_BEND,
	PF_Cursor_PIN_ADVANCED,
	PF_Cursor_CAMERA_ORBIT_CURSOR,
	PF_Cursor_CAMERA_ORBIT_SCENE,
	PF_Cursor_CAMERA_PAN_CURSOR,
	PF_Cursor_CAMERA_DOLLY_TOWARDS_CURSOR,
	PF_Cursor_CAMERA_DOLLY_TO_CURSOR,

	PF_MAX_CURSOR_PLUS_ONE
};
typedef A_long PF_CursorType;


enum {
	PF_Mod_NONE					= 0x0000,
	PF_Mod_CMD_CTRL_KEY			= 0x0100,		// cmd on Mac, ctrl on Windows
	PF_Mod_SHIFT_KEY			= 0x0200,
	PF_Mod_CAPS_LOCK_KEY		= 0x0400,
	PF_Mod_OPT_ALT_KEY			= 0x0800,		// option on Mac, alt on Windows
	PF_Mod_MAC_CONTROL_KEY		= 0x1000		// Mac control key only
};
typedef A_long PF_Modifiers;


typedef struct {
	PF_Point			screen_point;		/* >> where the mouse is right now */
	PF_Modifiers		modifiers;			/* >> modifiers right now */
	PF_CursorType		set_cursor;			/* << set this to your desired cursor, or PF_Cursor_CUSTOM if you
													set the cursor yourself, or PF_Cursor_NONE if you don't
													want to override the cursor (i.e. the app will set the
													cursor however it wants) */
} PF_AdjustCursorEventInfo;

typedef struct {
	A_u_long			when;
	PF_Point			screen_point;
	A_long				num_clicks;
	PF_Modifiers		modifiers;
	A_intptr_t			continue_refcon[4];	/* <> if send_drag is TRUE, set this */
	PF_Boolean			send_drag;			/* << set this from a do_click to get a drag */
	PF_Boolean			last_time;			/* >> set the last time you get a drag */
} PF_DoClickEventInfo;


typedef struct {
	PF_UnionableRect	update_rect;		// in window's coordinate system
	A_long				depth;
} PF_DrawEventInfo;


typedef struct {				
	A_u_long			when;
	PF_Point			screen_point;
	A_long				char_code;
	A_long				key_code;
	PF_Modifiers		modifiers;
} PF_KeyDownEventObsolete;	// As of AE 7, this is no longer used.


typedef A_u_long	PF_KeyCode;				// For printable characters, we use the unshifted upper case version (A not a, 7 not &).
typedef A_u_short	PF_ControlCode;

typedef struct {					
	A_u_long			when;
	PF_Point			screen_point;
	PF_KeyCode			keycode;		// as of AE 7, this is either a control code, or character code
	PF_Modifiers		modifiers;

} PF_KeyDownEvent;


enum {
	PF_KEYCODE_FLAG_Printable			= 1 << 31,		// a printable key, otherwise a control code like page up
	PF_KEYCODE_FLAG_Extended			= 1 << 30		// an alternate key, typically on the numeric keypad.
};

#define PF_GENERATE_KEYBOARD_CODE_VALUE(CODE_VALUE, FLAGS)					(((CODE_VALUE) & 0xFFFF) | (FLAGS))
#define PF_GENERATE_KEYBOARD_CODE_VALUE_FROM_CHARACTER(CHAR)				PF_GENERATE_KEYBOARD_CODE_VALUE(CHAR, PF_KEYCODE_FLAG_Printable)
#define PF_GENERATE_KEYBOARD_CODE_VALUE_FROM_CHARACTER_EXTENDED(CHAR)		PF_GENERATE_KEYBOARD_CODE_VALUE(CHAR, PF_KEYCODE_FLAG_Printable | PF_KEYCODE_FLAG_Extended)
#define PF_GENERATE_KEYBOARD_CODE_VALUE_FROM_CONTROL_CODE(CODE)				PF_GENERATE_KEYBOARD_CODE_VALUE(CODE, 0)
#define PF_GENERATE_KEYBOARD_CODE_VALUE_FROM_CONTROL_CODE_EXTENDED(CODE)	PF_GENERATE_KEYBOARD_CODE_VALUE(CODE, PF_KEYCODE_FLAG_Extended)

#define PF_KEYCODE_IS_PRINTABLE(_KEY_CODE)				(((_KEY_CODE) & PF_KEYCODE_FLAG_Printable ) != 0) 
#define PF_KEYCODE_IS_EXTENDED(_KEY_CODE)				(((_KEY_CODE) & PF_KEYCODE_FLAG_Extended ) != 0) 

#define PF_KEYCODE_GET_SHORTCUT_CHARACTER(_KEY_CODE)	(PF_KEYCODE_IS_PRINTABLE(_KEY_CODE)) ? ((_KEY_CODE) & 0xFFFF) : 0
#define PF_KEYCODE_GET_CONTROL_CODE(_KEY_CODE)			(!PF_KEYCODE_IS_PRINTABLE(_KEY_CODE)) ? ((_KEY_CODE) & 0xFFFF) : 0

enum {
	PF_ControlCode_Unknown		= (PF_ControlCode)0xFFFF,

	PF_ControlCode_Space		= 1,
	PF_ControlCode_Backspace,
	PF_ControlCode_Tab,
	PF_ControlCode_Return,
	PF_ControlCode_Enter,

	PF_ControlCode_Escape,

	PF_ControlCode_F1,
	PF_ControlCode_F2,
	PF_ControlCode_F3,
	PF_ControlCode_F4,
	PF_ControlCode_F5,
	PF_ControlCode_F6,
	PF_ControlCode_F7,
	PF_ControlCode_F8,
	PF_ControlCode_F9,
	PF_ControlCode_F10,
	PF_ControlCode_F11,
	PF_ControlCode_F12,
	PF_ControlCode_F13,
	PF_ControlCode_F14,
	PF_ControlCode_F15,
	PF_ControlCode_F16,
	PF_ControlCode_F17,
	PF_ControlCode_F18,
	PF_ControlCode_F19,
	PF_ControlCode_F20,
	PF_ControlCode_F21,
	PF_ControlCode_F22,
	PF_ControlCode_F23,
	PF_ControlCode_F24,

	PF_ControlCode_PrintScreen,
	PF_ControlCode_ScrollLock,
	PF_ControlCode_Pause,

	PF_ControlCode_Insert,
	PF_ControlCode_Delete,
	PF_ControlCode_Home,
	PF_ControlCode_End,
	PF_ControlCode_PageUp,
	PF_ControlCode_PageDown,
	PF_ControlCode_Help,
	PF_ControlCode_Clear,

	PF_ControlCode_Left,
	PF_ControlCode_Right,
	PF_ControlCode_Up,
	PF_ControlCode_Down,

	PF_ControlCode_NumLock,

	PF_ControlCode_Command,
	PF_ControlCode_Option,
	PF_ControlCode_Alt = PF_ControlCode_Option,
	PF_ControlCode_Control,
	PF_ControlCode_Shift,
	PF_ControlCode_CapsLock,

	PF_ControlCode_ContextMenu
};


typedef union {
	PF_DoClickEventInfo			do_click;		// also drag
	PF_DrawEventInfo			draw;
	PF_KeyDownEvent				key_down;
	PF_AdjustCursorEventInfo	adjust_cursor;

	// add other event types here
		
} PF_EventUnion;

#if	defined(A_INTERNAL) && defined (__cplusplus)
	class FLT_FCSeqSpec;
	typedef	FLT_FCSeqSpec				*PF_ContextRefcon;
#else
	typedef	struct _PF_ContextRefcon	*PF_ContextRefcon;
#endif

#define			PF_CONTEXT_MAGIC		0x05ea771e
typedef struct {
	A_u_long			magic;
	PF_WindowType		w_type;
	PF_ContextRefcon	reserved_flt;
	A_intptr_t			plugin_state[4];	// plug-in specific data
	DRAWBOT_DrawRef		reserved_drawref;
	void				*reserved_paneP;
  	void                            *reserved_job_manageP;  // used for managing async job requests for UI in the context of Effect pane custom UI
} PF_Context, *PF_ContextPtr, **PF_ContextH;

typedef enum {
	PenTip,
	PenEraser
} PF_StylusTool;

typedef struct {
	// only applies when stylus_dataB == true;
	A_FpShort			stylus_tiltxF;		// 	-1.0 to 1.0	0.0 == vertical
	A_FpShort			stylus_tiltyF;		// 	-1.0 to 1.0	0.0 == vertical
	A_FpShort			stylus_pressureF;	//  0.0 to 1.0	1.0 == full pressure
	A_FpShort			stylus_wheelF;		// 	-1.0 to 1.0	0.0 == none
} PF_StylusEventInfo;

typedef struct PF_PointerEventInfo {
	A_FpLong			when_secondsF;
	PF_Point			screen_point;
	A_short				num_clicksS;
	A_long				mod_keysL;

	PF_StylusTool		stylus_tool;		// set to PenTip when using mouse
	PF_Boolean			stylus_extra_dataB;
	PF_StylusEventInfo	stylus_extra_data;  // only valid when stylus_extra_dataB == true
} PF_PointerEventInfo;

typedef struct {
	void		*refcon;			// front-end's refcon
	
	PF_Err		(*layer_to_comp)(void *refcon, 			/* >> */
								PF_ContextH context,	/* >> */
								A_long curr_time,			/* >> */
								A_long time_scale,		/* >> */
								PF_FixedPoint *pt);		/* << */
								
	PF_Err		(*comp_to_layer)(void *refcon, 			/* >> */
								PF_ContextH context, 	/* >> */
								A_long curr_time, 		/* >> */
								A_long time_scale,	 	/* >> */
								PF_FixedPoint *pt);		/* << */

	PF_Err		(*get_comp2layer_xform)(void *refcon, 	/* >> */
								PF_ContextH context,	/* >> */
								A_long curr_time, 		/* >> */
								A_long time_scale, 		/* >> */
								A_long *exists,			/* << non-zero if exists */
								PF_FloatMatrix *c2l);	/* << */
								
	PF_Err		(*get_layer2comp_xform)(void *refcon,	/* >> */
								PF_ContextH context, 	/* >> */
								A_long curr_time, 		/* >> */
								A_long time_scale, 		/* >> */
								PF_FloatMatrix *l2c);	/* << */
	
	PF_Err		(*source_to_frame)(void *refcon, PF_ContextH context, PF_FixedPoint *pt);
	PF_Err		(*frame_to_source)(void *refcon, PF_ContextH context, PF_FixedPoint *pt);

	PF_Err		(*info_draw_color)(void *refcon, PF_Pixel color);

	// 2 lines of text, same as calling PF_InfoDrawText3( line1Z0, line2Z0, NULL)
	PF_Err		(*info_draw_text)(void *refcon, const A_char *text1Z0, const A_char *text2Z0);	// Cstrings

	// Due to this structure's containment within PF_EventExtra,
	// we are unable to add new functions to this structure in order
	// to remain backwards compatible.  In other words, do not add any
	// new functions here, add them to the PF_AdvAppSuite1 suite
	// within AE_AdvEffectSuites.h. -jja 10-24-2000
	
} PF_EventCallbacks, *PF_EventCallbacksPtr;

enum {
	PF_EA_NONE = 0,
	PF_EA_PARAM_TITLE,
	PF_EA_CONTROL
};
typedef	A_long	PF_EffectArea;

typedef struct {
	PF_ParamIndex			index;
	PF_EffectArea			area;
	PF_UnionableRect		current_frame;		// full frame of the current area
	PF_UnionableRect		param_title_frame;	// full frame of the param title area
 	A_long					horiz_offset;		// h offset to draw into title
} PF_EffectWindowInfo;

enum {
	PF_EO_NONE				= 0,
	PF_EO_HANDLED_EVENT		= (1L << 0),
	PF_EO_ALWAYS_UPDATE		= (1L << 1),	//rerender the comp
	PF_EO_NEVER_UPDATE		= (1L << 2),	//do not rerender the comp
	PF_EO_UPDATE_NOW		= (1L << 3)		//update the view immediately after the event returns when using PF_InvalidateRect
};
typedef	A_long	PF_EventOutFlags;

enum {
	PF_EI_NONE = 0,
	PF_EI_DONT_DRAW			= 1L << 0		// don't draw controls in comp or layer window
};
typedef	A_long	PF_EventInFlags;

//	this info is passed for ALL event types, except those in the Effect Control Window
//	in ECW, you get the PF_EffectWindowInfo, during all events
typedef struct {
	PF_UnionableRect		port_rect;
} PF_ItemWindowInfo;

/*
	to know what union to take, look in 
	(**contextH).w_type.  if == effect win, you get effect win, else you get item win
*/
typedef union {
	PF_EffectWindowInfo		effect_win;			/* >> only for Effect window do_click, draw, and adjust-cursor */
	PF_ItemWindowInfo		item_win;			/* >> comp or layer		*/
} PF_WindowUnion;

//	uncomment if you want to get the port rect during most events
//	#define	PF_USE_NEW_WINDOW_UNION
typedef struct {
	PF_ContextH				contextH;			/* >> */
	PF_EventType			e_type;				/* >> */
	PF_EventUnion			u;					/* >> based on e_type */
	
	#ifdef PF_USE_NEW_WINDOW_UNION
		PF_WindowUnion			window_union;		/* >> union of window-specific handy information	*/
	#else
		PF_EffectWindowInfo		effect_win;			/* >> only for Effect window do_click, draw, and adjust-cursor */
	#endif

	PF_EventCallbacks		cbs;				/* >> not for new_context or close_context */
	PF_EventInFlags			evt_in_flags;		/* >> */
	PF_EventOutFlags		evt_out_flags;		/* << */
} PF_EventExtra;

enum {
	PF_UIAlignment_NONE = 0,					// No values other than PF_UIAlignment_NONE are honored, in AE or PPro.
	PF_UIAlignment_TOP = 1L << 0,
	PF_UIAlignment_LEFT = 1L << 1,
	PF_UIAlignment_BOTTOM = 1L << 2,
	PF_UIAlignment_RIGHT = 1L << 3
};

typedef A_long PF_UIAlignment;

struct _PF_CustomUIInfo {
	
	A_long					reserved;
	PF_CustomEventFlags		events;

	A_long				comp_ui_width;
	A_long				comp_ui_height;
	PF_UIAlignment		comp_ui_alignment;			// unused
	
	A_long				layer_ui_width;
	A_long				layer_ui_height;
	PF_UIAlignment		layer_ui_alignment;			// unused

	A_long				preview_ui_width;			// unused
	A_long				preview_ui_height;			// unused
	PF_UIAlignment		preview_ui_alignment;		// unused

};


#ifdef __cplusplus
}
#endif


#include <adobesdk/config/PostConfig.h>



#endif /* _H_AE_EffectUI */
