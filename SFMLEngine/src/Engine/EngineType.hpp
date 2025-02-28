#pragma once
#include <type_traits>
#include "Math/Vector2D.hpp"

template <typename Base, typename Derived>
using TDerive = std::enable_if_t<std::is_base_of_v<Base, Derived>>;

template <typename T>
using TSharedPtr = std::shared_ptr<T>;

template <typename T>
using TOptional = std::optional<T>;

template <typename T>
using TRefWrapper = std::reference_wrapper<T>;

template <typename T>
using TUniquePtr = std::unique_ptr<T>;

using STDString = std::string;

using FVector2D = Vector2D<float>;
using IVector2D = Vector2D<int>;
using DVector2D = Vector2D<double>;

/**
 * Allocates a new object of type T with the given arguments and returns it as a TUniquePtr.  Disabled for array-type TUniquePtrs.
 * The object is value-initialized, which will call a user-defined default constructor if it exists, but a trivial type will be zeroed.
 *
 * @param Args The arguments to pass to the constructor of T.
 *
 * @return A TUniquePtr which points to a newly-constructed T with the specified Args.
 */
template < typename T, typename... TArgs, std::enable_if_t<(!std::is_array_v<T>), int> = 0>
[[nodiscard]] TUniquePtr<T> MakeUnique(TArgs&&... Args)
{
    return TUniquePtr<T>(std::make_unique<T>(std::forward<TArgs>(Args)...));
}

template <typename T>
[[nodiscard]] TRefWrapper<T> Make_RefWrapper(T& Ref) noexcept
{
    return std::ref(Ref);
}