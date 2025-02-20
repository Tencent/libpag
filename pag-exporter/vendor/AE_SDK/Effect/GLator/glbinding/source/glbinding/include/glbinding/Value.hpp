#pragma once

#include <glbinding/Value.h>

namespace glbinding 
{

template <typename... Arguments>
struct ValueAdder;

template <>
struct ValueAdder<>
{
    static void add(std::vector<glbinding::AbstractValue*> &)
    {
    }
};

template <typename Argument, typename... Arguments>
struct ValueAdder<Argument, Arguments...>
{
    static void add(std::vector<glbinding::AbstractValue*> & values, Argument value, Arguments&&... rest)
    {
        values.push_back(glbinding::createValue<Argument>(value));
        ValueAdder<Arguments...>::add(values, std::forward<Arguments>(rest)...);
    }
};

template <typename... Arguments>
void addValuesTo(std::vector<glbinding::AbstractValue*> & values, Arguments&&... arguments)
{
    ValueAdder<Arguments...>::add(values, std::forward<Arguments>(arguments)...);
}


template <typename T>
Value<T>::Value(const T & _value)
: value(_value)
{
}

template <typename T>
void Value<T>::printOn(std::ostream & stream) const
{
    stream << value;
}

template <typename Argument>
AbstractValue * createValue(Argument argument)
{
    return new Value<Argument>(argument);
}

template <typename... Arguments>
std::vector<AbstractValue*> createValues(Arguments&&... arguments)
{
    auto values = std::vector<AbstractValue*>{};
    addValuesTo(values, std::forward<Arguments>(arguments)...);
    return values;
}

} // namespace glbinding
