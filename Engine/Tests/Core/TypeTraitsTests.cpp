// Suite: the engine's names for type traits and concepts (Core/Templates/TypeTraits.h) and THash.
// Each name must mean what the std one it names means, argument order included.
#include <doctest.h>

#include "Core/GUID/Guid.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/Templates/TypeTraits.h"

using namespace Opaax;

namespace
{
    struct Base {};
    struct Derived : Base {};
    struct PrivatelyDerived : private Base {};

    enum class EColour : Uint8 { Red, Green };

    struct NoDefault
    {
        explicit NoDefault(Int32 InValue) : Value(InValue) {}
        Int32 Value;
    };

    struct NotMovable
    {
        NotMovable() = default;
        NotMovable(NotMovable&&) = delete;
    };

    /** A key type with only the engine's hash, no std one. */
    struct GridCell
    {
        Int32 X = 0;
        Int32 Y = 0;

        bool operator==(const GridCell&) const = default;
    };

    template<CArithmetic T>
    constexpr T Twice(T InValue) { return InValue * 2; }

    template<typename T>
    constexpr bool k_CanTwice = requires(T InValue) { Twice(InValue); };
}

namespace Opaax
{
    template<>
    struct THash<GridCell>
    {
        size_t operator()(const GridCell& InCell) const noexcept
        {
            return static_cast<size_t>(InCell.X) * 73856093u ^ static_cast<size_t>(InCell.Y) * 19349663u;
        }
    };
}

TEST_CASE("TypeTraits: values mean what the std ones mean, argument order included")
{
    static_assert(TIsSame<Int32, Int32>);
    static_assert(!TIsSame<Int32, Uint32>);

    // Base first, like std::is_base_of; access does not matter.
    static_assert(TIsBaseOf<Base, Derived>);
    static_assert(!TIsBaseOf<Derived, Base>);
    static_assert(TIsBaseOf<Base, PrivatelyDerived>);

    static_assert(TIsEnum<EColour> && !TIsEnum<Int32>);
    static_assert(TIsIntegral<Uint8> && TIsIntegral<bool> && !TIsIntegral<float>);
    static_assert(TIsFloatingPoint<double> && !TIsFloatingPoint<Int64>);
    static_assert(TIsArithmetic<Int16> && TIsArithmetic<float> && !TIsArithmetic<EColour>);

    static_assert(TIsTriviallyCopyable<GridCell> && !TIsTriviallyCopyable<OpaaxString>);
    static_assert(TIsDefaultConstructible<Base> && !TIsDefaultConstructible<NoDefault>);
    static_assert(TIsMoveConstructible<OpaaxString> && !TIsMoveConstructible<NotMovable>);
    static_assert(TIsLValueReference<Int32&> && !TIsLValueReference<Int32&&> && !TIsLValueReference<Int32>);
}

TEST_CASE("TypeTraits: type transforms and limits")
{
    static_assert(TIsSame<TRemoveReference<Int32&>, Int32>);
    static_assert(TIsSame<TRemoveReference<const Int32&&>, const Int32>);
    static_assert(TIsSame<TRemoveCVRef<const volatile Int32&>, Int32>);
    static_assert(TIsSame<TDecay<const Int32&>, Int32>);
    static_assert(TIsSame<TDecay<Int32[3]>, Int32*>);
    static_assert(TIsSame<TConditional<true, Int8, Int16>, Int8>);
    static_assert(TIsSame<TConditional<false, Int8, Int16>, Int16>);
    static_assert(TIsSame<TEnableIf<true>, void>);
    static_assert(TIsSame<TEnableIf<true, Int32>, Int32>);
    static_assert(TrueType::value && !FalseType::value);

    CHECK(TNumericLimits<Int16>::max() == 32767);
    CHECK(TNumericLimits<Uint8>::max() == 255);
    CHECK(TNumericLimits<Int32>::lowest() == -2147483647 - 1);
}

TEST_CASE("TypeTraits: concepts mean what the std ones mean")
{
    static_assert(CSameAs<Int32, Int32> && !CSameAs<Int32, Int64>);

    // Derived first, like std::derived_from; only a public base counts.
    static_assert(CDerivedFrom<Derived, Base>);
    static_assert(!CDerivedFrom<Base, Derived>);
    static_assert(!CDerivedFrom<PrivatelyDerived, Base>);

    static_assert(CConvertibleTo<Derived*, Base*> && !CConvertibleTo<Base*, Derived*>);
    static_assert(CConstructibleFrom<NoDefault, Int32> && !CConstructibleFrom<NoDefault>);
    static_assert(CDefaultInitializable<GridCell> && !CDefaultInitializable<NoDefault>);
    static_assert(CIntegral<Uint64> && !CIntegral<double>);
    static_assert(CFloatingPoint<float> && !CFloatingPoint<Int32>);
    static_assert(CArithmetic<Int8> && CArithmetic<double> && !CArithmetic<OpaaxString>);

    // A constrained template takes what the concept allows and refuses the rest.
    CHECK(Twice(21) == 42);
    CHECK(Twice(1.5f) == doctest::Approx(3.f));
    static_assert(k_CanTwice<Int32> && !k_CanTwice<OpaaxString>);
}

TEST_CASE("THash: a type becomes a map key through its own specialization")
{
    TUnorderedSet<GridCell> lCells;
    lCells.insert({ 0, 0 });
    lCells.insert({ 1, 0 });
    lCells.insert({ 0, 1 });
    lCells.insert({ 1, 0 });   // already there

    CHECK(lCells.size() == 3u);
    CHECK(lCells.count({ 0, 1 }) == 1u);
    CHECK(lCells.count({ 2, 2 }) == 0u);

    // The specialization is the one the container calls.
    CHECK(THash<GridCell>{}({ 1, 0 }) == 73856093u);
}

TEST_CASE("THash: engine types and built-in types are keys with the default hasher")
{
    TUnorderedMap<Guid, Int32> lByGuid;
    const Guid lFirst  = Guid::New();
    const Guid lSecond = Guid::New();
    lByGuid[lFirst]  = 1;
    lByGuid[lSecond] = 2;

    CHECK(lByGuid.size() == 2u);
    CHECK(lByGuid.at(lFirst) == 1);
    CHECK(THash<Guid>{}(lFirst) == THash<Guid>{}(lFirst));

    TUnorderedMap<Uint64, Int32> lByNumber;
    lByNumber[7u] = 70;
    CHECK(lByNumber.at(7u) == 70);
    CHECK(THash<Uint64>{}(7u) == THash<Uint64>{}(7u));
}
