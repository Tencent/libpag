#pragma once

#include <glbinding/Function.h>
#include <glbinding/logging.h>
#include <glbinding/Value.h>

#include <utility>
#include <functional>
#include <memory>


namespace glbinding 
{

template <typename ReturnType, typename... Arguments>
struct FunctionHelper
{
    ReturnType call(const glbinding::Function<ReturnType, Arguments...> * function, Arguments&&... arguments) const
    {
        std::unique_ptr<glbinding::FunctionCall> functionCall{new glbinding::FunctionCall(function)};

        if (function->isAnyEnabled(glbinding::CallbackMask::Parameters | glbinding::CallbackMask::Logging))
        {
            functionCall->parameters = glbinding::createValues(std::forward<Arguments>(arguments)...);
        }

        if (function->isEnabled(glbinding::CallbackMask::Before))
        {
            function->before(*functionCall);
        }

        if (function->m_beforeCallback)
        {
            function->m_beforeCallback(std::forward<Arguments>(arguments)...);
        }

        auto value = basicCall(function, std::forward<Arguments>(arguments)...);

        if (function->m_afterCallback)
        {
            function->m_afterCallback(value, std::forward<Arguments>(arguments)...);
        }

        if (function->isAnyEnabled(glbinding::CallbackMask::ReturnValue | glbinding::CallbackMask::Logging))
        {
            functionCall->returnValue = glbinding::createValue(value);
        }

        if (function->isEnabled(glbinding::CallbackMask::After))
        {
            function->after(*functionCall);
        }

        if(function->isEnabled(glbinding::CallbackMask::Logging))
        {
            glbinding::logging::log(functionCall.release());
        }

        return value;
    }

    ReturnType basicCall(const glbinding::Function<ReturnType, Arguments...> * function, Arguments&&... arguments) const
    {
        return reinterpret_cast<typename glbinding::Function<ReturnType, Arguments...>::Signature>(function->address())(std::forward<Arguments>(arguments)...);
    }
};

template <typename... Arguments>
struct FunctionHelper<void, Arguments...>
{
    void call(const glbinding::Function<void, Arguments...> * function, Arguments&&... arguments) const
    {
        std::unique_ptr<glbinding::FunctionCall> functionCall(new glbinding::FunctionCall(function));

        if (function->isAnyEnabled(glbinding::CallbackMask::Parameters | glbinding::CallbackMask::Logging))
        {
            functionCall->parameters = glbinding::createValues(std::forward<Arguments>(arguments)...);
        }

        if (function->isEnabled(glbinding::CallbackMask::Before))
        {
            function->before(*functionCall);
        }

        if (function->m_beforeCallback)
        {
            function->m_beforeCallback(std::forward<Arguments>(arguments)...);
        }

        basicCall(function, std::forward<Arguments>(arguments)...);

        if (function->m_afterCallback)
        {
            function->m_afterCallback(std::forward<Arguments>(arguments)...);
        }

        if (function->isEnabled(glbinding::CallbackMask::After))
        {
            function->after(*functionCall);
        }

        if(function->isEnabled(glbinding::CallbackMask::Logging))
        {
            glbinding::logging::log(functionCall.release());
        }
    }

    void basicCall(const glbinding::Function<void, Arguments...> * function, Arguments&&... arguments) const
    {
        reinterpret_cast<typename glbinding::Function<void, Arguments...>::Signature>(function->address())(std::forward<Arguments>(arguments)...);
    }
};


template <typename ReturnType, typename... Arguments>
Function<ReturnType, Arguments...>::Function(const char * _name)
    : AbstractFunction{_name}
, m_beforeCallback{nullptr}
, m_afterCallback{nullptr}
{
}

template <typename ReturnType, typename... Arguments>
ReturnType Function<ReturnType, Arguments...>::operator()(Arguments&... arguments) const
{
    auto myAddress = address();

    if (myAddress != nullptr)
    {
        if (isAnyEnabled(CallbackMask::Before | CallbackMask::After | CallbackMask::Logging))
        {
            return FunctionHelper<ReturnType, Arguments...>().call(this, std::forward<Arguments>(arguments)...);
        }
        else
        {
            return FunctionHelper<ReturnType, Arguments...>().basicCall(this, std::forward<Arguments>(arguments)...);
        }
    }
    else
    {
         if (isEnabled(CallbackMask::Unresolved))
         {
            unresolved();
         }

         return ReturnType();
    }
}

template <typename ReturnType, typename... Arguments>
ReturnType Function<ReturnType, Arguments...>::directCall(Arguments... arguments) const
{
    return FunctionHelper<ReturnType, Arguments...>().basicCall(this, std::forward<Arguments>(arguments)...);
}

template <typename ReturnType, typename... Arguments>
void Function<ReturnType, Arguments...>::setBeforeCallback(BeforeCallback callback)
{
    m_beforeCallback = std::move(callback);
}

template <typename ReturnType, typename... Arguments>
void Function<ReturnType, Arguments...>::clearBeforeCallback()
{
    m_beforeCallback = nullptr;
}

template <typename ReturnType, typename... Arguments>
void Function<ReturnType, Arguments...>::setAfterCallback(AfterCallback callback)
{
    m_afterCallback = std::move(callback);
}

template <typename ReturnType, typename... Arguments>
void Function<ReturnType, Arguments...>::clearAfterCallback()
{
    m_afterCallback = nullptr;
}

} // namespace glbinding
