#pragma once

#include <functional>

#include <glbinding/AbstractFunction.h>

#ifndef WINAPI
#ifdef WIN32
#define WINAPI __stdcall
#else
#define WINAPI
#endif
#endif

namespace glbinding 
{

template <typename ReturnType, typename... Arguments> struct FunctionHelper;

template <typename ReturnType, typename... Arguments>
struct CallbackType
{
    using type = std::function<void(ReturnType, Arguments...)>;
};

template <typename... Arguments>
struct CallbackType<void, Arguments...>
{
    using type = std::function<void(Arguments...)>;
};


template <typename ReturnType, typename... Arguments>
class Function : public AbstractFunction
{
    friend struct FunctionHelper<ReturnType, Arguments...>;

    using Signature = ReturnType(WINAPI *) (Arguments...);

    using BeforeCallback = typename CallbackType<void, Arguments...>::type;
    using AfterCallback = typename CallbackType<ReturnType, Arguments...>::type;

public:
    Function(const char * name);

    ReturnType operator()(Arguments&... arguments) const;
    ReturnType directCall(Arguments... arguments) const;

    void setBeforeCallback(BeforeCallback callback);
    void clearBeforeCallback();
    void setAfterCallback(AfterCallback callback);
    void clearAfterCallback();

protected:
    BeforeCallback m_beforeCallback;
    AfterCallback m_afterCallback;
};

} // namespace glbinding

#include <glbinding/Function.hpp>
