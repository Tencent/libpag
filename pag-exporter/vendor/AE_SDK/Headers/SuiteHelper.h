#ifndef _H_SUITEHELPER
#define _H_SUITEHELPER

#include "A.h"
#include "SPBasic.h"

#include <cassert>

template <typename SuiteType>
struct SuiteTraits
{
	static const A_char*	i_name;
	static const int32_t	i_version;
};

template <typename SuiteType>
class AssertAndThrowOnMissingSuite
{
public:
	void operator()()
	{
		assert( false );
		A_THROW(A_Err_MISSING_SUITE);
	}
};


class MissingSuiteErrFunc_NoOp
{
public:
	void operator()()
	{
		//swallow the error, not much we want to do or can do
	}
};


template<typename SuiteType, class MissingSuiteErrFunc = AssertAndThrowOnMissingSuite<SuiteType> >
class SuiteHelper
{
public:
	SuiteHelper(const SPBasicSuite* const basic_suiteP);
	~SuiteHelper();

	const SuiteType* operator->() const;
	SuiteType* get() const;

private:
	mutable SuiteType*	i_SuiteP;
	const SPBasicSuite* const i_basic_suiteP;
};


template<typename SuiteType, class MissingSuiteErrFunc >
SuiteHelper<SuiteType, MissingSuiteErrFunc>::SuiteHelper(const SPBasicSuite* const basic_suiteP) : i_basic_suiteP(basic_suiteP), i_SuiteP( NULL )
{
	assert(basic_suiteP);
	const void * acquired_suite = NULL;

	A_Err err = i_basic_suiteP->AcquireSuite(SuiteTraits<SuiteType>::i_name, SuiteTraits<SuiteType>::i_version, &acquired_suite);
	if (err || !acquired_suite) {
		MissingSuiteErrFunc error_func;
		error_func();
	} else {
		i_SuiteP = reinterpret_cast<SuiteType*>(const_cast<void*>(acquired_suite));
	}
}

template<typename SuiteType, class MissingSuiteErrFunc>
SuiteHelper<SuiteType, MissingSuiteErrFunc>::~SuiteHelper()
{
	if (i_SuiteP) {
		
		#ifdef DEBUG
			A_Err err = 
		#endif
				
		i_basic_suiteP->ReleaseSuite(SuiteTraits<SuiteType>::i_name, SuiteTraits<SuiteType>::i_version);
		
		#ifdef DEBUG
			assert( !err );
		#endif		
	}
}

template<typename SuiteType, class MissingSuiteErrFunc>
const SuiteType* SuiteHelper<SuiteType, MissingSuiteErrFunc>::operator->() const
{
	return i_SuiteP;
}	

template<typename SuiteType, class MissingSuiteErrFunc>
SuiteType* SuiteHelper<SuiteType, MissingSuiteErrFunc>::get() const
{
	return i_SuiteP;
}
#endif
