#pragma once

#include <type_traits>

namespace glbinding 
{

template <typename... Types>
class SharedBitfield;

// intersection

template<typename T, typename... Types>
struct is_member_of_SharedBitfield
{
    static const bool value = false;
};

template<typename T, typename U, typename... Types>
struct is_member_of_SharedBitfield<T, U, Types...>
{
    static const bool value = std::conditional<std::is_same<T, U>::value, std::true_type, is_member_of_SharedBitfield<T, Types...>>::type::value;
};

template<typename, typename>
struct prepend_to_SharedBitfield
{};

template<typename T, typename... Types>
struct prepend_to_SharedBitfield<T, SharedBitfield<Types...>>
{
    using type = SharedBitfield<T, Types...>;
};

template<typename, typename>
struct intersect_SharedBitfield
{
    using type = SharedBitfield<>;
};

template<typename T, typename... Types, typename... OtherTypes>
struct intersect_SharedBitfield<SharedBitfield<T, Types...>, SharedBitfield<OtherTypes...>>
{
    using type = typename std::conditional<!is_member_of_SharedBitfield<T, OtherTypes...>::value, typename intersect_SharedBitfield<SharedBitfield<Types...>, SharedBitfield<OtherTypes...>>::type, typename prepend_to_SharedBitfield<T, typename intersect_SharedBitfield<SharedBitfield<Types...>, SharedBitfield<OtherTypes...>>::type>::type>::type;
};

// implementation

template <typename T>
class SharedBitfieldBase
{
public:
    using UnderlyingType = T;

    SharedBitfieldBase(T value);

    explicit operator T() const;
protected:
    T m_value;
};

template <>
class SharedBitfield<>
{};

template <typename Type>
class SharedBitfield<Type> : public SharedBitfieldBase<typename std::underlying_type<Type>::type>
{
public:
    using UnderlyingType = typename SharedBitfieldBase<typename std::underlying_type<Type>::type>::UnderlyingType;

    template <typename ConstructionType>
    SharedBitfield(ConstructionType value);
    SharedBitfield(typename std::underlying_type<Type>::type value);

    operator Type() const;

    template <typename... T>
    auto operator|(SharedBitfield<T...> other) const -> typename std::enable_if<!std::is_same<typename intersect_SharedBitfield<SharedBitfield, SharedBitfield<T...>>::type, SharedBitfield<>>::value, typename intersect_SharedBitfield<SharedBitfield, SharedBitfield<T...>>::type>::type;

    template <typename... T>
    auto operator|=(SharedBitfield<T...> other) -> typename std::enable_if<!std::is_same<typename intersect_SharedBitfield<SharedBitfield, SharedBitfield<T...>>::type, SharedBitfield<>>::value, SharedBitfield &>::type;

    template <typename... T>
    auto operator&(SharedBitfield<T...> other) const -> typename std::enable_if<!std::is_same<typename intersect_SharedBitfield<SharedBitfield, SharedBitfield<T...>>::type, SharedBitfield<>>::value, typename intersect_SharedBitfield<SharedBitfield, SharedBitfield<T...>>::type>::type;

    template <typename... T>
    auto operator&=(SharedBitfield<T...> other) -> typename std::enable_if<!std::is_same<typename intersect_SharedBitfield<SharedBitfield, SharedBitfield<T...>>::type, SharedBitfield<>>::value, SharedBitfield &>::type;

    template <typename... T>
    auto operator^(SharedBitfield<T...> other) const -> typename std::enable_if<!std::is_same<typename intersect_SharedBitfield<SharedBitfield, SharedBitfield<T...>>::type, SharedBitfield<>>::value, typename intersect_SharedBitfield<SharedBitfield, SharedBitfield<T...>>::type>::type;

    template <typename... T>
    auto operator^=(SharedBitfield<T...> other) -> typename std::enable_if<!std::is_same<typename intersect_SharedBitfield<SharedBitfield, SharedBitfield<T...>>::type, SharedBitfield<>>::value, SharedBitfield &>::type;

    template <typename... T>
    auto operator==(SharedBitfield<T...> other) const -> typename std::enable_if<!std::is_same<typename intersect_SharedBitfield<SharedBitfield, SharedBitfield<T...>>::type, SharedBitfield<>>::value, bool>::type;

    template <typename T>
    auto operator==(T other) const -> typename std::enable_if<is_member_of_SharedBitfield<T, Type>::value, bool>::type;
};

template <typename Type, typename... Types>
class SharedBitfield<Type, Types...> : public SharedBitfield<Types...>
{
public:
    using UnderlyingType = typename SharedBitfield<Types...>::UnderlyingType;

    template <typename ConstructionType>
    SharedBitfield(ConstructionType value);
    SharedBitfield(typename std::underlying_type<Type>::type value);

    operator Type() const;

    template <typename... T>
    auto operator|(SharedBitfield<T...> other) const -> typename std::enable_if<!std::is_same<typename intersect_SharedBitfield<SharedBitfield, SharedBitfield<T...>>::type, SharedBitfield<>>::value, typename intersect_SharedBitfield<SharedBitfield, SharedBitfield<T...>>::type>::type;

    template <typename... T>
    auto operator|=(SharedBitfield<T...> other) -> typename std::enable_if<!std::is_same<typename intersect_SharedBitfield<SharedBitfield, SharedBitfield<T...>>::type, SharedBitfield<>>::value, SharedBitfield &>::type;

    template <typename... T>
    auto operator&(SharedBitfield<T...> other) const -> typename std::enable_if<!std::is_same<typename intersect_SharedBitfield<SharedBitfield, SharedBitfield<T...>>::type, SharedBitfield<>>::value, typename intersect_SharedBitfield<SharedBitfield, SharedBitfield<T...>>::type>::type;

    template <typename... T>
    auto operator&=(SharedBitfield<T...> other) -> typename std::enable_if<!std::is_same<typename intersect_SharedBitfield<SharedBitfield, SharedBitfield<T...>>::type, SharedBitfield<>>::value, SharedBitfield &>::type;

    template <typename... T>
    auto operator^(SharedBitfield<T...> other) const -> typename std::enable_if<!std::is_same<typename intersect_SharedBitfield<SharedBitfield, SharedBitfield<T...>>::type, SharedBitfield<>>::value, typename intersect_SharedBitfield<SharedBitfield, SharedBitfield<T...>>::type>::type;

    template <typename... T>
    auto operator^=(SharedBitfield<T...> other) -> typename std::enable_if<!std::is_same<typename intersect_SharedBitfield<SharedBitfield, SharedBitfield<T...>>::type, SharedBitfield<>>::value, SharedBitfield &>::type;

    template <typename... T>
    auto operator==(SharedBitfield<T...> other) const -> typename std::enable_if<!std::is_same<typename intersect_SharedBitfield<SharedBitfield, SharedBitfield<T...>>::type, SharedBitfield<>>::value, bool>::type;

    template <typename T>
    auto operator==(T other) const -> typename std::enable_if<is_member_of_SharedBitfield<T, Type, Types...>::value, bool>::type;
};

// operators

template <typename Enum, typename ConvertibleEnum>
typename std::enable_if<std::is_base_of<SharedBitfieldBase<typename std::underlying_type<typename std::enable_if<std::is_enum<Enum>::value, Enum>::type>::type>, ConvertibleEnum>::value, Enum>::type
operator|(Enum a, ConvertibleEnum b);

template <typename ConvertibleEnum, typename Enum>
typename std::enable_if<std::is_base_of<SharedBitfieldBase<typename std::underlying_type<typename std::enable_if<std::is_enum<Enum>::value, Enum>::type>::type>, ConvertibleEnum>::value, Enum>::type
operator|(ConvertibleEnum a, Enum b);

template <typename Enum, typename ConvertibleEnum>
typename std::enable_if<std::is_base_of<SharedBitfieldBase<typename std::underlying_type<typename std::enable_if<std::is_enum<Enum>::value, Enum>::type>::type>, ConvertibleEnum>::value, Enum>::type
operator|=(Enum & a, ConvertibleEnum b);

template <typename Enum, typename ConvertibleEnum>
typename std::enable_if<std::is_base_of<SharedBitfieldBase<typename std::underlying_type<typename std::enable_if<std::is_enum<Enum>::value, Enum>::type>::type>, ConvertibleEnum>::value, Enum>::type
operator&(Enum a, ConvertibleEnum b);

template <typename ConvertibleEnum, typename Enum>
typename std::enable_if<std::is_base_of<SharedBitfieldBase<typename std::underlying_type<typename std::enable_if<std::is_enum<Enum>::value, Enum>::type>::type>, ConvertibleEnum>::value, Enum>::type
operator&(ConvertibleEnum a, Enum b);

template <typename Enum, typename ConvertibleEnum>
typename std::enable_if<std::is_base_of<SharedBitfieldBase<typename std::underlying_type<typename std::enable_if<std::is_enum<Enum>::value, Enum>::type>::type>, ConvertibleEnum>::value, Enum>::type
operator&=(Enum & a, ConvertibleEnum b);

template <typename Enum, typename ConvertibleEnum>
typename std::enable_if<std::is_base_of<SharedBitfieldBase<typename std::underlying_type<typename std::enable_if<std::is_enum<Enum>::value, Enum>::type>::type>, ConvertibleEnum>::value, Enum>::type
operator^(Enum a, ConvertibleEnum b);

template <typename ConvertibleEnum, typename Enum>
typename std::enable_if<std::is_base_of<SharedBitfieldBase<typename std::underlying_type<typename std::enable_if<std::is_enum<Enum>::value, Enum>::type>::type>, ConvertibleEnum>::value, Enum>::type
operator^(ConvertibleEnum a, Enum b);

template <typename Enum, typename ConvertibleEnum>
typename std::enable_if<std::is_base_of<SharedBitfieldBase<typename std::underlying_type<typename std::enable_if<std::is_enum<Enum>::value, Enum>::type>::type>, ConvertibleEnum>::value, Enum>::type
operator^=(Enum & a, ConvertibleEnum b);

} // namespace glbinding

#include <glbinding/SharedBitfield.hpp>
