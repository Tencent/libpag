#pragma once

#include <glbinding/SharedBitfield.h>

#include <glbinding/gl/types.h>

namespace glbinding 
{

template <typename T>
SharedBitfieldBase<T>::SharedBitfieldBase(T value)
: m_value{value}
{
}

template <typename T>
SharedBitfieldBase<T>::operator T() const
{
    return m_value;
}


template <typename Type>
template <typename ConstructionType>
SharedBitfield<Type>::SharedBitfield(ConstructionType value)
: SharedBitfieldBase<typename std::underlying_type<Type>::type>(static_cast<typename std::underlying_type<Type>::type>(value))
{
    static_assert(is_member_of_SharedBitfield<ConstructionType, Type>::value, "Not a member of SharedBitfield");
}

template <typename Type>
SharedBitfield<Type>::SharedBitfield(typename std::underlying_type<Type>::type value)
: SharedBitfieldBase<typename std::underlying_type<Type>::type>(value)
{
}

template <typename Type>
SharedBitfield<Type>::operator Type() const
{
    return static_cast<Type>(this->m_value);
}

template <typename Type>
template <typename... T>
auto SharedBitfield<Type>::operator|(SharedBitfield<T...> other) const -> typename std::enable_if<!std::is_same<typename intersect_SharedBitfield<SharedBitfield, SharedBitfield<T...>>::type, SharedBitfield<>>::value, typename intersect_SharedBitfield<SharedBitfield, SharedBitfield<T...>>::type>::type
{
    return typename intersect_SharedBitfield<SharedBitfield, SharedBitfield<T...>>::type(this->m_value | static_cast<decltype(this->m_value)>(other));
}

template <typename Type>
template <typename... T>
auto SharedBitfield<Type>::operator|=(SharedBitfield<T...> other) -> typename std::enable_if<!std::is_same<typename intersect_SharedBitfield<SharedBitfield, SharedBitfield<T...>>::type, SharedBitfield<>>::value, SharedBitfield &>::type
{
    this->m_value |= static_cast<decltype(this->m_value)>(other);

    return *this;
}

template <typename Type>
template <typename... T>
auto SharedBitfield<Type>::operator&(SharedBitfield<T...> other) const -> typename std::enable_if<!std::is_same<typename intersect_SharedBitfield<SharedBitfield, SharedBitfield<T...>>::type, SharedBitfield<>>::value, typename intersect_SharedBitfield<SharedBitfield, SharedBitfield<T...>>::type>::type
{
    return typename intersect_SharedBitfield<SharedBitfield, SharedBitfield<T...>>::type(this->m_value & static_cast<decltype(this->m_value)>(other));
}

template <typename Type>
template <typename... T>
auto SharedBitfield<Type>::operator&=(SharedBitfield<T...> other) -> typename std::enable_if<!std::is_same<typename intersect_SharedBitfield<SharedBitfield, SharedBitfield<T...>>::type, SharedBitfield<>>::value, SharedBitfield &>::type
{
    this->m_value &= static_cast<decltype(this->m_value)>(other);

    return *this;
}

template <typename Type>
template <typename... T>
auto SharedBitfield<Type>::operator^(SharedBitfield<T...> other) const -> typename std::enable_if<!std::is_same<typename intersect_SharedBitfield<SharedBitfield, SharedBitfield<T...>>::type, SharedBitfield<>>::value, typename intersect_SharedBitfield<SharedBitfield, SharedBitfield<T...>>::type>::type
{
    return typename intersect_SharedBitfield<SharedBitfield, SharedBitfield<T...>>::type(this->m_value ^static_cast<decltype(this->m_value)>(other));
}

template <typename Type>
template <typename... T>
auto SharedBitfield<Type>::operator^=(SharedBitfield<T...> other) -> typename std::enable_if<!std::is_same<typename intersect_SharedBitfield<SharedBitfield, SharedBitfield<T...>>::type, SharedBitfield<>>::value, SharedBitfield &>::type
{
    this->m_value ^= static_cast<decltype(this->m_value)>(other);

    return *this;
}

template <typename Type>
template <typename... T>
auto SharedBitfield<Type>::operator==(SharedBitfield<T...> other) const -> typename std::enable_if<!std::is_same<typename intersect_SharedBitfield<SharedBitfield, SharedBitfield<T...>>::type, SharedBitfield<>>::value, bool>::type
{
    return static_cast<UnderlyingType>(*this) == static_cast<UnderlyingType>(other);
}

template <typename Type>
template <typename T>
auto SharedBitfield<Type>::operator==(T other) const -> typename std::enable_if<is_member_of_SharedBitfield<T, Type>::value, bool>::type
{
    return static_cast<UnderlyingType>(*this) == static_cast<UnderlyingType>(other);
}


template <typename Type, typename... Types>
template <typename ConstructionType>
SharedBitfield<Type, Types...>::SharedBitfield(ConstructionType value)
: SharedBitfield<Types...>(static_cast<typename std::underlying_type<Type>::type>(value))
{
    static_assert(is_member_of_SharedBitfield<ConstructionType, Type, Types...>::value, "Not a member of SharedBitfield");
}

template <typename Type, typename... Types>
SharedBitfield<Type, Types...>::SharedBitfield(typename std::underlying_type<Type>::type value)
: SharedBitfield<Types...>(value)
{
}

template <typename Type, typename... Types>
SharedBitfield<Type, Types...>::operator Type() const
{
    return static_cast<Type>(this->m_value);
}

template <typename Type, typename... Types>
template <typename... T>
auto SharedBitfield<Type, Types...>::operator|(SharedBitfield<T...> other) const -> typename std::enable_if<!std::is_same<typename intersect_SharedBitfield<SharedBitfield, SharedBitfield<T...>>::type, SharedBitfield<>>::value, typename intersect_SharedBitfield<SharedBitfield, SharedBitfield<T...>>::type>::type
{
    return typename intersect_SharedBitfield<SharedBitfield, SharedBitfield<T...>>::type(this->m_value | static_cast<decltype(this->m_value)>(other));
}

template <typename Type, typename... Types>
template <typename... T>
auto SharedBitfield<Type, Types...>::operator|=(SharedBitfield<T...> other) -> typename std::enable_if<!std::is_same<typename intersect_SharedBitfield<SharedBitfield, SharedBitfield<T...>>::type, SharedBitfield<>>::value, SharedBitfield &>::type
{
    this->m_value |= static_cast<decltype(this->m_value)>(other);

    return *this;
}

template <typename Type, typename... Types>
template <typename... T>
auto SharedBitfield<Type, Types...>::operator&(SharedBitfield<T...> other) const -> typename std::enable_if<!std::is_same<typename intersect_SharedBitfield<SharedBitfield, SharedBitfield<T...>>::type, SharedBitfield<>>::value, typename intersect_SharedBitfield<SharedBitfield, SharedBitfield<T...>>::type>::type
{
    return typename intersect_SharedBitfield<SharedBitfield, SharedBitfield<T...>>::type(this->m_value & static_cast<decltype(this->m_value)>(other));
}

template <typename Type, typename... Types>
template <typename... T>
auto SharedBitfield<Type, Types...>::operator&=(SharedBitfield<T...> other) -> typename std::enable_if<!std::is_same<typename intersect_SharedBitfield<SharedBitfield, SharedBitfield<T...>>::type, SharedBitfield<>>::value, SharedBitfield &>::type
{
    this->m_value &= static_cast<decltype(this->m_value)>(other);

    return *this;
}

template <typename Type, typename... Types>
template <typename... T>
auto SharedBitfield<Type, Types...>::operator^(SharedBitfield<T...> other) const -> typename std::enable_if<!std::is_same<typename intersect_SharedBitfield<SharedBitfield, SharedBitfield<T...>>::type, SharedBitfield<>>::value, typename intersect_SharedBitfield<SharedBitfield, SharedBitfield<T...>>::type>::type
{
    return typename intersect_SharedBitfield<SharedBitfield, SharedBitfield<T...>>::type(this->m_value ^static_cast<decltype(this->m_value)>(other));
}

template <typename Type, typename... Types>
template <typename... T>
auto SharedBitfield<Type, Types...>::operator^=(SharedBitfield<T...> other) -> typename std::enable_if<!std::is_same<typename intersect_SharedBitfield<SharedBitfield, SharedBitfield<T...>>::type, SharedBitfield<>>::value, SharedBitfield &>::type
{
    this->m_value ^= static_cast<decltype(this->m_value)>(other);

    return *this;
}

template <typename Type, typename... Types>
template <typename... T>
auto SharedBitfield<Type, Types...>::operator==(SharedBitfield<T...> other) const -> typename std::enable_if<!std::is_same<typename intersect_SharedBitfield<SharedBitfield, SharedBitfield<T...>>::type, SharedBitfield<>>::value, bool>::type
{
    return static_cast<UnderlyingType>(*this) == static_cast<UnderlyingType>(other);
}

template <typename Type, typename... Types>
template <typename T>
auto SharedBitfield<Type, Types...>::operator==(T other) const -> typename std::enable_if<is_member_of_SharedBitfield<T, Type, Types...>::value, bool>::type
{
    return static_cast<UnderlyingType>(*this) == static_cast<UnderlyingType>(other);
}


template <typename Enum, typename ConvertibleEnum>
typename std::enable_if<std::is_base_of<SharedBitfieldBase<typename std::underlying_type<typename std::enable_if<std::is_enum<Enum>::value, Enum>::type>::type>, ConvertibleEnum>::value, Enum>::type
operator|(Enum a, ConvertibleEnum b)
{
    return a | static_cast<Enum>(b);
}

template <typename ConvertibleEnum, typename Enum>
typename std::enable_if<std::is_base_of<SharedBitfieldBase<typename std::underlying_type<typename std::enable_if<std::is_enum<Enum>::value, Enum>::type>::type>, ConvertibleEnum>::value, Enum>::type
operator|(ConvertibleEnum a, Enum b)
{
    return static_cast<Enum>(a) | b;
}

template <typename Enum, typename ConvertibleEnum>
typename std::enable_if<std::is_base_of<SharedBitfieldBase<typename std::underlying_type<typename std::enable_if<std::is_enum<Enum>::value, Enum>::type>::type>, ConvertibleEnum>::value, Enum>::type
operator|=(Enum & a, ConvertibleEnum b)
{
    return a |= static_cast<Enum>(b);
}

template <typename Enum, typename ConvertibleEnum>
typename std::enable_if<std::is_base_of<SharedBitfieldBase<typename std::underlying_type<typename std::enable_if<std::is_enum<Enum>::value, Enum>::type>::type>, ConvertibleEnum>::value, Enum>::type
operator&(Enum a, ConvertibleEnum b)
{
    return a & static_cast<Enum>(b);
}

template <typename ConvertibleEnum, typename Enum>
typename std::enable_if<std::is_base_of<SharedBitfieldBase<typename std::underlying_type<typename std::enable_if<std::is_enum<Enum>::value, Enum>::type>::type>, ConvertibleEnum>::value, Enum>::type
operator&(ConvertibleEnum a, Enum b)
{
    return static_cast<Enum>(a) & b;
}

template <typename Enum, typename ConvertibleEnum>
typename std::enable_if<std::is_base_of<SharedBitfieldBase<typename std::underlying_type<typename std::enable_if<std::is_enum<Enum>::value, Enum>::type>::type>, ConvertibleEnum>::value, Enum>::type
operator&=(Enum & a, ConvertibleEnum b)
{
    return a &= static_cast<Enum>(b);
}

template <typename Enum, typename ConvertibleEnum>
typename std::enable_if<std::is_base_of<SharedBitfieldBase<typename std::underlying_type<typename std::enable_if<std::is_enum<Enum>::value, Enum>::type>::type>, ConvertibleEnum>::value, Enum>::type
operator^(Enum a, ConvertibleEnum b)
{
    return a ^ static_cast<Enum>(b);
}

template <typename ConvertibleEnum, typename Enum>
typename std::enable_if<std::is_base_of<SharedBitfieldBase<typename std::underlying_type<typename std::enable_if<std::is_enum<Enum>::value, Enum>::type>::type>, ConvertibleEnum>::value, Enum>::type
operator^(ConvertibleEnum a, Enum b)
{
    return static_cast<Enum>(a) ^ b;
}

template <typename Enum, typename ConvertibleEnum>
typename std::enable_if<std::is_base_of<SharedBitfieldBase<typename std::underlying_type<typename std::enable_if<std::is_enum<Enum>::value, Enum>::type>::type>, ConvertibleEnum>::value, Enum>::type
operator^=(Enum & a, ConvertibleEnum b)
{
    return a ^= static_cast<Enum>(b);
}

} // namespace glbinding
