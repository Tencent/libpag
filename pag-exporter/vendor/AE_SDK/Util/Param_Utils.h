#ifndef H_PARAM_UTILS
#define H_PARAM_UTILS

//	do not include DVA headers here
#include <AE_Effect.h>
#include <AE_EffectCB.h>
#include <string.h>

// requires the explicit use of 'def' for the struct name

/* not quite strncpy because this always null terminates - unless SZ <= 0 */
#define PF_STRNNCPY(DST, SRC, SZ) \
	::strncpy((DST), (SRC), (SZ)); \
	if ((SZ) > 0U) (DST)[(SZ)-1] = 0

#define        PF_ParamDef_IS_PUI_FLAG_SET(_defP, _puiFlag)        \
  (((_defP)->ui_flags & _puiFlag) != 0)
   
#define        PF_ParamDef_IS_PARAM_FLAG_SET(_defP, _paramFlag)    \
   (((_defP)->flags & _paramFlag) != 0)


#define PF_ADD_COLOR(NAME, RED, GREEN, BLUE, ID)\
	do {\
		PF_Err	priv_err = PF_Err_NONE; \
		def.param_type = PF_Param_COLOR; \
		PF_STRNNCPY(def.name, (NAME), sizeof(def.name) ); \
		def.u.cd.value.red = (RED); \
		def.u.cd.value.green = (GREEN); \
		def.u.cd.value.blue = (BLUE); \
		def.u.cd.value.alpha = 255; \
		def.u.cd.dephault = def.u.cd.value; \
		def.uu.id = (ID); \
		if ((priv_err = PF_ADD_PARAM(in_data, -1, &def)) != PF_Err_NONE) return priv_err; \
	} while (0)

#define PF_ADD_ARBITRARY2(NAME, WIDTH, HEIGHT, PARAM_FLAGS, PUI_FLAGS, DFLT, ID, REFCON)\
	do {\
		PF_Err	priv_err = PF_Err_NONE; \
		def.param_type = PF_Param_ARBITRARY_DATA; \
		def.flags = (PARAM_FLAGS); \
		PF_STRNNCPY(def.name, (NAME), sizeof(def.name) ); \
		def.ui_width = (WIDTH);\
	    def.ui_height = (HEIGHT);\
		def.ui_flags = (PUI_FLAGS); \
		def.u.arb_d.value = NULL;\
		def.u.arb_d.pad = 0;\
		def.u.arb_d.dephault = (DFLT); \
		def.uu.id = def.u.arb_d.id = (ID); \
		def.u.arb_d.refconPV = REFCON; \
		if ((priv_err = PF_ADD_PARAM(in_data, -1, &def)) != PF_Err_NONE) return priv_err; \
	} while (0)

#define PF_ADD_ARBITRARY(NAME, WIDTH, HEIGHT, PUI_FLAGS, DFLT, ID, REFCON)\
	PF_ADD_ARBITRARY2(NAME, WIDTH, HEIGHT, PF_ParamFlag_NONE, PUI_FLAGS, DFLT, ID, REFCON)

#define PF_ADD_SLIDER(NAME, VALID_MIN, VALID_MAX, SLIDER_MIN, SLIDER_MAX, DFLT, ID) \
	do {\
		PF_Err	priv_err = PF_Err_NONE; \
		def.param_type = PF_Param_SLIDER; \
		PF_STRNNCPY(def.name, (NAME), sizeof(def.name) ); \
		def.u.sd.value_str[0] = '\0'; \
		def.u.sd.value_desc[0] = '\0'; \
		def.u.sd.valid_min = (VALID_MIN); \
		def.u.sd.slider_min = (SLIDER_MIN); \
		def.u.sd.valid_max = (VALID_MAX); \
		def.u.sd.slider_max = (SLIDER_MAX); \
		def.u.sd.value = def.u.sd.dephault = (DFLT); \
		def.uu.id = (ID); \
		if ((priv_err = PF_ADD_PARAM(in_data, -1, &def)) != PF_Err_NONE) return priv_err; \
	} while (0)

#define PF_ADD_FIXED(NAME, VALID_MIN, VALID_MAX, SLIDER_MIN, SLIDER_MAX, DFLT, PREC, DISP, FLAGS, ID) \
	do {\
		PF_Err	priv_err = PF_Err_NONE; \
		def.param_type = PF_Param_FIX_SLIDER; \
		PF_STRNNCPY(def.name, (NAME), sizeof(def.name) ); \
		def.u.fd.value_str[0]	= '\0'; \
		def.u.fd.value_desc[0]	= '\0'; \
		def.u.fd.valid_min = (PF_Fixed)((VALID_MIN) * 65536.0); \
		def.u.fd.slider_min = (PF_Fixed)((SLIDER_MIN) * 65536.0); \
		def.u.fd.valid_max = (PF_Fixed)((VALID_MAX) * 65536.0); \
		def.u.fd.slider_max = (PF_Fixed)((SLIDER_MAX) * 65536.0); \
		def.u.fd.value = def.u.fd.dephault = (PF_Fixed)((DFLT) * 65536.0); \
		def.u.fd.precision		= (A_short)(PREC); \
		def.u.fd.display_flags  |= (A_short)(DISP); \
		def.flags				|= (FLAGS); \
		def.uu.id = (ID); \
		if ((priv_err = PF_ADD_PARAM(in_data, -1, &def)) != PF_Err_NONE) return priv_err; \
	} while (0)

// why does fs_flags get or-ed in? and why is CURVE_TOLERANCE param ignored? and .flags is never set. oy.
#define PF_ADD_FLOAT_SLIDER(NAME, VALID_MIN, VALID_MAX, SLIDER_MIN, SLIDER_MAX, CURVE_TOLERANCE, DFLT, PREC, DISP, WANT_PHASE, ID) \
	do {\
		PF_Err	priv_err = PF_Err_NONE; \
		def.param_type = PF_Param_FLOAT_SLIDER; \
		PF_STRNNCPY(def.name, (NAME), sizeof(def.name) ); \
		def.u.fs_d.valid_min		= (VALID_MIN); \
		def.u.fs_d.slider_min		= (SLIDER_MIN); \
		def.u.fs_d.valid_max		= (VALID_MAX); \
		def.u.fs_d.slider_max		= (SLIDER_MAX); \
		def.u.fs_d.value			= (DFLT); \
		def.u.fs_d.dephault			= (PF_FpShort)(def.u.fs_d.value); \
		def.u.fs_d.precision		= (PREC); \
		def.u.fs_d.display_flags	= (DISP); \
		def.u.fs_d.fs_flags			|= (WANT_PHASE) ? PF_FSliderFlag_WANT_PHASE : 0; \
		def.u.fs_d.curve_tolerance	= AEFX_AUDIO_DEFAULT_CURVE_TOLERANCE;\
		def.uu.id = (ID); \
		if ((priv_err = PF_ADD_PARAM(in_data, -1, &def)) != PF_Err_NONE) return priv_err; \
	} while (0)

// safer newer version
#define PF_ADD_FLOAT_SLIDERX(NAME, VALID_MIN, VALID_MAX, SLIDER_MIN, SLIDER_MAX, DFLT, PREC, DISP, FLAGS, ID)	\
	do {																										\
		AEFX_CLR_STRUCT(def);																					\
		def.flags = (FLAGS);																					\
		PF_ADD_FLOAT_SLIDER(NAME, VALID_MIN, VALID_MAX, SLIDER_MIN, SLIDER_MAX, 0, DFLT, PREC, DISP, 0, ID);	\
	} while (0)

// copied from Pr version of Param_Utils.h. It is used in some of Pr versions of AE effects
#define PF_ADD_FLOAT_EXPONENTIAL_SLIDER(NAME, VALID_MIN, VALID_MAX, SLIDER_MIN, SLIDER_MAX, CURVE_TOLERANCE, DFLT, PREC, DISP, WANT_PHASE, EXPONENT, ID) \
do {\
PF_Err	priv_err = PF_Err_NONE; \
def.param_type = PF_Param_FLOAT_SLIDER; \
PF_STRNNCPY(def.name, (NAME), sizeof(def.name) ); \
def.u.fs_d.valid_min		= (VALID_MIN); \
def.u.fs_d.slider_min		= (SLIDER_MIN); \
def.u.fs_d.valid_max		= (VALID_MAX); \
def.u.fs_d.slider_max		= (SLIDER_MAX); \
def.u.fs_d.value			= (DFLT); \
def.u.fs_d.dephault			= (DFLT); \
def.u.fs_d.precision		= (PREC); \
def.u.fs_d.display_flags	= (DISP); \
def.u.fs_d.fs_flags			|= (WANT_PHASE) ? PF_FSliderFlag_WANT_PHASE : 0; \
def.u.fs_d.curve_tolerance	= AEFX_AUDIO_DEFAULT_CURVE_TOLERANCE;\
def.u.fs_d.useExponent		= true;\
def.u.fs_d.exponent			= EXPONENT;\
def.uu.id = (ID); \
if ((priv_err = PF_ADD_PARAM(in_data, -1, &def)) != PF_Err_NONE) return priv_err; \
} while (0)

enum { PF_Precision_INTEGER, PF_Precision_TENTHS, PF_Precision_HUNDREDTHS, PF_Precision_THOUSANDTHS, PF_Precision_TEN_THOUSANDTHS };

#define PF_ADD_CHECKBOX(NAME_A, NAME_B, DFLT, FLAGS, ID)\
	do {\
		PF_Err	priv_err = PF_Err_NONE; \
		def.param_type = PF_Param_CHECKBOX; \
		PF_STRNNCPY(def.name, NAME_A, sizeof(def.name)); \
		def.u.bd.u.nameptr  = (NAME_B); \
		def.u.bd.value		= (DFLT); \
		def.u.bd.dephault	= (PF_Boolean)(def.u.bd.value); \
		def.flags			|= (FLAGS); \
		def.uu.id = (ID); \
		if ((priv_err = PF_ADD_PARAM(in_data, -1, &def)) != PF_Err_NONE) return priv_err; \
	} while (0)

// safer newer version
#define PF_ADD_CHECKBOXX(NAME, DFLT, FLAGS, ID)\
	do {\
		AEFX_CLR_STRUCT(def); \
		PF_ADD_CHECKBOX(NAME, "", DFLT, FLAGS, ID); \
	} while (0)

#define PF_ADD_BUTTON(PARAM_NAME, BUTTON_NAME, PUI_FLAGS, PARAM_FLAGS, ID)\
	do {\
		AEFX_CLR_STRUCT(def); \
		PF_Err	priv_err = PF_Err_NONE; \
		def.param_type		= PF_Param_BUTTON; \
		PF_STRNNCPY(def.name, PARAM_NAME, sizeof(def.name)); \
		def.u.button_d.u.namesptr  = (BUTTON_NAME); \
		def.flags			= (PARAM_FLAGS); \
		def.ui_flags		= (PUI_FLAGS); \
		def.uu.id			= (ID); \
		if ((priv_err = PF_ADD_PARAM(in_data, -1, &def)) != PF_Err_NONE) return priv_err; \
	} while (0)

#define PF_ADD_ANGLE(NAME, DFLT, ID) \
	do {\
		PF_Err	priv_err = PF_Err_NONE; \
		def.param_type = PF_Param_ANGLE; \
		PF_STRNNCPY(def.name, (NAME), sizeof(def.name) ); \
		def.u.ad.value = def.u.ad.dephault = (PF_Fixed)((DFLT) * 65536.0); \
		def.uu.id = (ID); \
		if ((priv_err = PF_ADD_PARAM(in_data, -1, &def)) != PF_Err_NONE) return priv_err; \
	} while (0)


#define PF_ADD_NULL(NAME, ID) \
	do {\
		PF_Err	priv_err = PF_Err_NONE; \
		def.param_type = PF_Param_NO_DATA; \
		PF_STRNNCPY(def.name, (NAME), sizeof(def.name) ); \
		def.uu.id = (ID); \
		if ((priv_err = PF_ADD_PARAM(in_data, -1, &def)) != PF_Err_NONE) return priv_err; \
	} while (0)


#define PF_ADD_POPUP(NAME, CHOICES, DFLT, STRING, ID) \
	do {\
		PF_Err	priv_err = PF_Err_NONE; \
		def.param_type = PF_Param_POPUP; \
		PF_STRNNCPY(def.name, (NAME), sizeof(def.name) ); \
		def.u.pd.num_choices = (CHOICES); \
		def.u.pd.dephault = (DFLT); \
		def.u.pd.value = def.u.pd.dephault; \
		def.u.pd.u.namesptr = (STRING); \
		def.uu.id = (ID); \
		if ((priv_err = PF_ADD_PARAM(in_data, -1, &def)) != PF_Err_NONE) return priv_err; \
	} while (0)
	
#define PF_ADD_LAYER(NAME, DFLT, ID) \
	do	{\
		PF_Err	priv_err = PF_Err_NONE; \
		def.param_type = PF_Param_LAYER; \
		PF_STRNNCPY(def.name, (NAME), sizeof(def.name) ); \
		def.u.ld.dephault = (DFLT); \
		def.uu.id = ID;\
		if ((priv_err = PF_ADD_PARAM(in_data, -1, &def)) != PF_Err_NONE) return priv_err; \
	} while (0)			

#define PF_ADD_255_SLIDER(NAME, DFLT, ID)\
	PF_ADD_SLIDER( (NAME), 0, 255, 0, 255, (DFLT), (ID))

#define PF_ADD_PERCENT(NAME, DFLT, ID)\
	PF_ADD_FIXED( (NAME), 0, 100, 0, 100, (DFLT), 1, 1, 0, (ID))

#define PF_ADD_POINT(NAME, X_DFLT, Y_DFLT, RESTRICT_BOUNDS, ID) \
	do	{\
		PF_Err	priv_err = PF_Err_NONE; \
		def.param_type = PF_Param_POINT; \
		PF_STRNNCPY(def.name, (NAME), sizeof(def.name) ); \
		def.u.td.restrict_bounds = RESTRICT_BOUNDS;\
		def.u.td.x_value = def.u.td.x_dephault = (X_DFLT << 16); \
		def.u.td.y_value = def.u.td.y_dephault = (Y_DFLT << 16); \
		def.uu.id = (ID); \
		if ((priv_err = PF_ADD_PARAM(in_data, -1, &def)) != PF_Err_NONE) return priv_err; \
	} while (0)

#define PF_ADD_POINT_3D(NAME, X_DFLT, Y_DFLT, Z_DFLT, ID) \
	do	{\
		AEFX_CLR_STRUCT(def); \
		PF_Err	priv_err = PF_Err_NONE; \
		def.param_type = PF_Param_POINT_3D; \
		PF_STRNNCPY(def.name, (NAME), sizeof(def.name) ); \
		def.u.point3d_d.x_value = def.u.point3d_d.x_dephault = X_DFLT; \
		def.u.point3d_d.y_value = def.u.point3d_d.y_dephault = Y_DFLT; \
		def.u.point3d_d.z_value = def.u.point3d_d.z_dephault = Y_DFLT; \
		def.uu.id = (ID); \
		if ((priv_err = PF_ADD_PARAM(in_data, -1, &def)) != PF_Err_NONE) return priv_err; \
	} while (0)

#define PF_ADD_TOPIC(NAME, ID) \
	do	{\
		PF_Err	priv_err = PF_Err_NONE; \
		def.param_type = PF_Param_GROUP_START; \
		PF_STRNNCPY(def.name, (NAME), sizeof(def.name) ); \
		def.uu.id = (ID); \
		if ((priv_err = PF_ADD_PARAM(in_data, -1, &def)) != PF_Err_NONE) return priv_err; \
	} while (0)

#define PF_END_TOPIC(ID) \
	do	{\
		PF_Err	priv_err = PF_Err_NONE; \
		def.param_type = PF_Param_GROUP_END; \
		def.uu.id = (ID); \
		if ((priv_err = PF_ADD_PARAM(in_data, -1, &def)) != PF_Err_NONE) return priv_err; \
	} while (0)	

#define PF_ADD_VERSIONED_FLAG(NAME) \
	do {\
		PF_Err	priv_err = PF_Err_NONE; \
		def.param_type = PF_Param_CHECKBOX; \
		def.name[0] = 0; \
		def.u.bd.u.nameptr  = (NAME); \
		def.u.bd.value = true; \
		def.u.bd.dephault = false; \
		def.flags = PF_ParamFlag_USE_VALUE_FOR_OLD_PROJECTS; \
		def.ui_flags = PF_PUI_INVISIBLE; \
		if ((priv_err = PF_ADD_PARAM(in_data, -1, &def)) != PF_Err_NONE) return priv_err; \
	} while (0)

// newer safer version
#define PF_ADD_TOPICX(NAME, FLAGS, ID) \
	do {\
		AEFX_CLR_STRUCT(def); \
		def.flags = (FLAGS); \
		PF_ADD_TOPIC(NAME, ID); \
	} while (0)

#define PF_ADD_POPUPX(NAME, NUM_CHOICES, DFLT, ITEMS_STRING, FLAGS, ID) \
	do {															\
		AEFX_CLR_STRUCT(def);										\
		def.flags = (FLAGS);										\
		PF_ADD_POPUP(NAME, NUM_CHOICES, DFLT, ITEMS_STRING, ID);	\
	} while (0)

enum { PF_ParamFlag_NONE=0 };		// SBI:AE_Effect.h

#define PF_ADD_FLOAT_SLIDERX_DISABLED(NAME, VALID_MIN, VALID_MAX, SLIDER_MIN, SLIDER_MAX, DFLT, PREC, DISP, FLAGS, ID)	\
	do {																										\
	AEFX_CLR_STRUCT(def);																					\
	def.flags = (FLAGS);\
	def.ui_flags = PF_PUI_DISABLED;\
	PF_ADD_FLOAT_SLIDER(NAME, VALID_MIN, VALID_MAX, SLIDER_MIN, SLIDER_MAX, 0, DFLT, PREC, DISP, 0, ID);	\
	} while (0)

namespace fxparam_utility {

	template <typename T>
	inline int RoundF(T	x)
	{
		int	ret;

		if (x > 0) {
			ret = (int)(x + (T)0.5);
		} else {
			if ((int)(x + (T)0.5) == (x + (T)0.5)) {
				ret = (int)x;
			} else {
				ret = (int)(x - (T)0.5);
			}
		}

		return ret;
	}
};

inline PF_Err PF_AddPointControl(PF_InData *in_data,
								 const char *nameZ,
								 float x_defaultF,			// 0-1
								 float y_defaultF,		// 0-1
								 bool restrict_boundsB,
								 PF_ParamFlags param_flags,
								 PF_ParamUIFlags pui_flags,
								 A_long param_id)
{
    PF_ParamDef		def = {{0}};
	namespace du = fxparam_utility;

	def.flags = param_flags;
	def.ui_flags = pui_flags;

	PF_ADD_POINT(nameZ, du::RoundF(x_defaultF*100), du::RoundF(y_defaultF*100), restrict_boundsB, param_id);	// has error return in macro

	return PF_Err_NONE;
}


#endif // H_PARAM_UTILS
