#pragma once

#include <type_traits>
#include <concepts>

#define TEnebleIf(...)\
std::enable_if_t<__VA_ARGS__ , int> = 0

#define TIsFloat(Type)\
std::is_floating_point_v<Type>

#define TIsSameAs(Type, Type2)\
std::is_same_v<Type, Type2>

#define TIsBaseOf(Type, Base) \
std::is_base_of_v<Base, Type>

// Concept to ensure T is a floating-point type
template <typename T>
concept CONCEPT_TIsFloat = std::is_floating_point_v<T>;

// Concept to ensure T is a integral type
template <typename T>
concept CONCEPT_TIsIntegral = std::integral<T>;

// T must be either float OR int
template <typename T>
concept CONCEPT_TIsFloatOrIntegral = CONCEPT_TIsFloat<T> || CONCEPT_TIsIntegral<T>;