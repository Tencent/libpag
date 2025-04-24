
#include "AE_Effect.h"
#include "AEConfig.h"

//OS specific includes
#ifdef AE_OS_WIN
	// mambo jambo with stdint
	#define _STDINT
	#define WIN32_LEAN_AND_MEAN 
	#define NOMINMAX
	#define DVACORE_MATCH_MAC_STDINT_ON_WINDOWS
	#include <stdint.h>
	#ifndef INT64_MAX
		#define INT64_MAX       LLONG_MAX
	#endif
	#ifndef INTMAX_MAX
		#define INTMAX_MAX       INT64_MAX
	#endif

	#include <windows.h>
	#include <stdlib.h>

	#if (_MSC_VER >= 1400)
		#define THREAD_LOCAL __declspec(thread)
	#endif
#else
	#define THREAD_LOCAL __thread
#endif
