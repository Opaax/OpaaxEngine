#pragma once
#include <type_traits>
#include "Math/Vector2D.hpp"

template <typename Base, typename Derived>
using TDerive = std::enable_if_t<std::is_base_of_v<Base, Derived>>;

template <typename T>
using TSharedPtr = std::shared_ptr<T>;

template <typename T>
using TUniquePtr = std::unique_ptr<T>;

using STDString = std::string;

using FVector2D = Vector2D<float>;
using IVector2D = Vector2D<int>;
using DVector2D = Vector2D<double>;
