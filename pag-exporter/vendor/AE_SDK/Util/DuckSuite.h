#include "A.h"
#include <SPTypes.h>

#ifdef AE_OS_WIN
	#include <windows.h>
#endif

#define kDuckSuite1				"AEGP Duck Suite"
#define kDuckSuiteVersion1		1

typedef struct DuckSuite1 {
	SPAPI A_Err	(*Quack)(A_u_short	timesSu);
} DuckSuite1;