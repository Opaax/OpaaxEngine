#pragma once

#include <algorithm>
#include <iterator>

#include "Core/OpaaxTypes.h"

// =============================================================================
// Algo — algorithms over a whole range (a TDynArray, a TFixedArray, a C array, a json array...):
//   the engine's names for <algorithm>. Each does what the std algorithm does on [begin, end) and
//   returns an iterator where std returns one.
//
//   Order:      Sort(Range[, Less]), StableSort(Range[, Less]), Reverse(Range)
//   Search:     Find(Range, Value), FindIf(Range, Pred), Contains(Range, Value),
//               LowerBound(Range, Value[, Less]), MaxElement(Range), MinElement(Range)
//   Count:      Count(Range, Value), CountIf(Range, Pred), AnyOf / AllOf / NoneOf(Range, Pred)
//   Remove:     RemoveIf(Container, Pred), Unique(Container[, Equal]): remove and shrink
// =============================================================================
namespace Opaax::Algo
{
    // =============================================================================
    // Order
    // =============================================================================
    /** Sorts with operator<. Equal elements end in any order. */
    template<typename TRange>
    void Sort(TRange&& InRange)
    {
        std::sort(std::begin(InRange), std::end(InRange));
    }

    template<typename TRange, typename TLess>
    void Sort(TRange&& InRange, TLess InLess)
    {
        std::sort(std::begin(InRange), std::end(InRange), InLess);
    }

    /** Sorts and keeps the order of equal elements. */
    template<typename TRange>
    void StableSort(TRange&& InRange)
    {
        std::stable_sort(std::begin(InRange), std::end(InRange));
    }

    template<typename TRange, typename TLess>
    void StableSort(TRange&& InRange, TLess InLess)
    {
        std::stable_sort(std::begin(InRange), std::end(InRange), InLess);
    }

    template<typename TRange>
    void Reverse(TRange&& InRange)
    {
        std::reverse(std::begin(InRange), std::end(InRange));
    }

    // =============================================================================
    // Search
    // =============================================================================
    /** @return The first element equal to InValue, or the range's end */
    template<typename TRange, typename TValue>
    auto Find(TRange&& InRange, const TValue& InValue)
    {
        return std::find(std::begin(InRange), std::end(InRange), InValue);
    }

    /** @return The first element InPredicate accepts, or the range's end */
    template<typename TRange, typename TPredicate>
    auto FindIf(TRange&& InRange, TPredicate InPredicate)
    {
        return std::find_if(std::begin(InRange), std::end(InRange), InPredicate);
    }

    template<typename TRange, typename TValue>
    bool Contains(const TRange& InRange, const TValue& InValue)
    {
        return std::find(std::begin(InRange), std::end(InRange), InValue) != std::end(InRange);
    }

    /** @return In a range sorted with operator<, the first element not less than InValue */
    template<typename TRange, typename TValue>
    auto LowerBound(TRange&& InRange, const TValue& InValue)
    {
        return std::lower_bound(std::begin(InRange), std::end(InRange), InValue);
    }

    /** @return In a range sorted with InLess, the first element not less than InValue */
    template<typename TRange, typename TValue, typename TLess>
    auto LowerBound(TRange&& InRange, const TValue& InValue, TLess InLess)
    {
        return std::lower_bound(std::begin(InRange), std::end(InRange), InValue, InLess);
    }

    /** @return The first largest element, or the end of an empty range */
    template<typename TRange>
    auto MaxElement(TRange&& InRange)
    {
        return std::max_element(std::begin(InRange), std::end(InRange));
    }

    /** @return The first smallest element, or the end of an empty range */
    template<typename TRange>
    auto MinElement(TRange&& InRange)
    {
        return std::min_element(std::begin(InRange), std::end(InRange));
    }

    // =============================================================================
    // Count
    // =============================================================================
    template<typename TRange, typename TValue>
    Uint64 Count(const TRange& InRange, const TValue& InValue)
    {
        return static_cast<Uint64>(std::count(std::begin(InRange), std::end(InRange), InValue));
    }

    template<typename TRange, typename TPredicate>
    Uint64 CountIf(const TRange& InRange, TPredicate InPredicate)
    {
        return static_cast<Uint64>(std::count_if(std::begin(InRange), std::end(InRange), InPredicate));
    }

    /** False for an empty range. */
    template<typename TRange, typename TPredicate>
    bool AnyOf(const TRange& InRange, TPredicate InPredicate)
    {
        return std::any_of(std::begin(InRange), std::end(InRange), InPredicate);
    }

    /** True for an empty range. */
    template<typename TRange, typename TPredicate>
    bool AllOf(const TRange& InRange, TPredicate InPredicate)
    {
        return std::all_of(std::begin(InRange), std::end(InRange), InPredicate);
    }

    /** True for an empty range. */
    template<typename TRange, typename TPredicate>
    bool NoneOf(const TRange& InRange, TPredicate InPredicate)
    {
        return std::none_of(std::begin(InRange), std::end(InRange), InPredicate);
    }

    // =============================================================================
    // Remove
    // =============================================================================
    /**
     * Removes the elements InPredicate accepts; the others keep their order.
     * @return How many were removed
     */
    template<typename TContainer, typename TPredicate>
    Uint64 RemoveIf(TContainer& InOutContainer, TPredicate InPredicate)
    {
        return static_cast<Uint64>(std::erase_if(InOutContainer, InPredicate));
    }

    /**
     * Removes each element equal (InEqual) to the one kept before it: sort first to drop every
     * duplicate. The first of each run stays.
     * @return How many were removed
     */
    template<typename TContainer, typename TEqual>
    Uint64 Unique(TContainer& InOutContainer, TEqual InEqual)
    {
        const auto   lNewEnd  = std::unique(std::begin(InOutContainer), std::end(InOutContainer), InEqual);
        const Uint64 lRemoved = static_cast<Uint64>(std::distance(lNewEnd, std::end(InOutContainer)));
        InOutContainer.erase(lNewEnd, std::end(InOutContainer));
        return lRemoved;
    }

    /** Unique with operator==. */
    template<typename TContainer>
    Uint64 Unique(TContainer& InOutContainer)
    {
        const auto   lNewEnd  = std::unique(std::begin(InOutContainer), std::end(InOutContainer));
        const Uint64 lRemoved = static_cast<Uint64>(std::distance(lNewEnd, std::end(InOutContainer)));
        InOutContainer.erase(lNewEnd, std::end(InOutContainer));
        return lRemoved;
    }
}
