#pragma once

#include <concepts>
#include <limits>
#include <type_traits>

// =============================================================================
// Type traits and concepts — the engine's names for <type_traits>, <concepts> and <limits>.
//   Engine code uses these instead of std. Each keeps the meaning and the argument order of the
//   std one it names.
//
//   Values:     TIsSame<A, B>, TIsBaseOf<Base, Derived>, TIsEnum<T>, TIsIntegral<T>,
//               TIsFloatingPoint<T>, TIsArithmetic<T>, TIsTriviallyCopyable<T>,
//               TIsDefaultConstructible<T>, TIsMoveConstructible<T>, TIsLValueReference<T>
//   Types:      TRemoveReference<T>, TRemoveCVRef<T>, TDecay<T>, TConditional<B, TTrue, TFalse>,
//               TEnableIf<B[, T]>, TrueType, FalseType, TNumericLimits<T>
//   Concepts:   CSameAs<A, B>, CDerivedFrom<Derived, Base>, CConvertibleTo<From, To>,
//               CConstructibleFrom<T, TArgs...>, CDefaultInitializable<T>, CIntegral<T>,
//               CFloatingPoint<T>, CArithmetic<T>
// =============================================================================
namespace Opaax
{
    // =============================================================================
    // Values
    // =============================================================================
    template<typename TA, typename TB>
    inline constexpr bool TIsSame = std::is_same_v<TA, TB>;

    /** TDerived is TBase or derives from it (privately too). */
    template<typename TBase, typename TDerived>
    inline constexpr bool TIsBaseOf = std::is_base_of_v<TBase, TDerived>;

    template<typename T>
    inline constexpr bool TIsEnum = std::is_enum_v<T>;

    template<typename T>
    inline constexpr bool TIsIntegral = std::is_integral_v<T>;

    template<typename T>
    inline constexpr bool TIsFloatingPoint = std::is_floating_point_v<T>;

    template<typename T>
    inline constexpr bool TIsArithmetic = std::is_arithmetic_v<T>;

    template<typename T>
    inline constexpr bool TIsTriviallyCopyable = std::is_trivially_copyable_v<T>;

    template<typename T>
    inline constexpr bool TIsDefaultConstructible = std::is_default_constructible_v<T>;

    template<typename T>
    inline constexpr bool TIsMoveConstructible = std::is_move_constructible_v<T>;

    template<typename T>
    inline constexpr bool TIsLValueReference = std::is_lvalue_reference_v<T>;

    // =============================================================================
    // Types
    // =============================================================================
    template<typename T>
    using TRemoveReference = std::remove_reference_t<T>;

    /** T without reference, const and volatile. */
    template<typename T>
    using TRemoveCVRef = std::remove_cvref_t<T>;

    /** T as a by-value parameter would hold it (no reference, no cv, arrays and functions as pointers). */
    template<typename T>
    using TDecay = std::decay_t<T>;

    template<bool bCondition, typename TTrue, typename TFalse>
    using TConditional = std::conditional_t<bCondition, TTrue, TFalse>;

    /** T when bCondition holds; otherwise the template using it drops out of overload resolution. */
    template<bool bCondition, typename T = void>
    using TEnableIf = std::enable_if_t<bCondition, T>;

    using TrueType  = std::true_type;
    using FalseType = std::false_type;

    /** Limits of an arithmetic type: TNumericLimits<Int16>::max(). */
    template<typename T>
    using TNumericLimits = std::numeric_limits<T>;

    // =============================================================================
    // Concepts
    // =============================================================================
    template<typename TA, typename TB>
    concept CSameAs = std::same_as<TA, TB>;

    /** TDerived is TBase or derives from it publicly and unambiguously. */
    template<typename TDerived, typename TBase>
    concept CDerivedFrom = std::derived_from<TDerived, TBase>;

    template<typename TFrom, typename TTo>
    concept CConvertibleTo = std::convertible_to<TFrom, TTo>;

    template<typename T, typename... TArgs>
    concept CConstructibleFrom = std::constructible_from<T, TArgs...>;

    template<typename T>
    concept CDefaultInitializable = std::default_initializable<T>;

    template<typename T>
    concept CIntegral = std::integral<T>;

    template<typename T>
    concept CFloatingPoint = std::floating_point<T>;

    /** An integral or a floating-point type. */
    template<typename T>
    concept CArithmetic = CIntegral<T> || CFloatingPoint<T>;
}
