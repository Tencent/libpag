
//Must be balanced with PostConfig.h!

#ifdef _WINDOWS
	//disable warning in VS2008 about unbalanced struct alignment changes
	#pragma warning( disable : 4103 )

	// This is taken from PreConfig_Win.h in dvacore, there is a bug in the VS2010 xlocnum header in particular where it is
	// incorrectly putting a declspec on a static member of a template class 'numpunct'. 
	#if  defined(_MSC_VER) && (_MSC_VER >= 1600) && defined(PREMIERE_INTERNAL)
		// Compilation problems due to locale id being marked as dllimport. -jacquave 2/18/2010
		// Also, opened a Technical Support Request with Microsoft, # 111021865918375.

		// http://connect.microsoft.com/VisualStudio/feedback/details/572376/msvc10-c-std-numpunct-has-a-hardcoded-dllimport-in-definition
		// http://dotnetforum.net/topic/7346-vs2010-error-c2491-stdnumpunctid-while-using-stdbasic-fstream-in-ccli/
		#ifdef __cplusplus
			#include "dvacore/config/win/xlocnum_hack.h"
		#endif
	#endif
#endif

//8 byte alignment for adobesdk public files.
#pragma pack( push, AdobeSDKExternalAlign, 8 )
