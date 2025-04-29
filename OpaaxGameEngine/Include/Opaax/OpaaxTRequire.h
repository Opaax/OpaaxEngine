#pragma once
#include <type_traits>

#define TEnebleIf(...)\
std::enable_if_t<__VA_ARGS__ , int> = 0

#define TIsFloat(Type)\
std::is_floating_point_v<Type>

#define TIsSameAs(Type, Type2)\
std::is_same_v<Type, Type2>

#define TIsBaseOf(Type, Base) \
std::is_base_of_v<Base, Type>

/**
 * Took advantage of C++ 20 concept.
 * how to use it:
 * template<typename T>
 * requires CONCEPT_TIsBaseOf<T, int>
 * class MyClass
 * { };
 *
 * MyClass<int> myIntClass; //OK
 * MyClass<float> myIntClass; //error, powerful, the curly red error should pop even before compile.
 */
template<typename T, typename Base>
concept CONCEPT_TIsBaseOf = std::is_same_v<T, Base>;

// Concept to ensure T is a floating-point type
template <typename T>
concept CONCEPT_TIsFloat = std::is_floating_point_v<T>;
