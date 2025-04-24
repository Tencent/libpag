#ifndef _H_AE_EffectVers
#define _H_AE_EffectVers


#define AEFX_API_VERSION			8
#define AEFX_API_SUBVERS			' '
#define	AEFXp_CODE_VERSION			0		// no longer user, we have p4 now

// these are here as copies -> when these change in AE_Effect.h, AEFX won't compile
// when they change, decide how to update above versions
// NOTE: this file is included by effect\shared\AEFX_PiPL.r and these versions are
//			used when building PiPLs
#define PF_PLUG_IN_VERSION			13	// auto-set by prep_codeline_for_release.py, adjust comment if manually edit is okay
#define PF_PLUG_IN_SUBVERS			28	// manually set for new 'Support URL' field in PiPL and new entry point


#endif
