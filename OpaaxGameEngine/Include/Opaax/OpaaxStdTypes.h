#pragma once
#include <string>
#include <cstdint>
#include <memory>
#include <functional>
#include <typeindex>

#include "OpaaxCoreMacros.h"

using OSTDString = std::string;

using UInt8  = std::uint8_t;
using UInt16 = std::uint16_t;
using UInt32 = std::uint32_t;
using UInt64 = std::uint64_t;

using Int8  = std::int8_t;
using Int16 = std::int16_t;
using Int32 = std::int32_t;
using Int64 = std::int64_t;

template<typename FuncArg>
using OPSTDFunc = std::function<FuncArg>;

using OPSTDTypeID = std::type_index;

template <typename T>
using TSharedPtr = std::shared_ptr<T>;

template <typename T, typename... TArgs>
[[nodiscard]] FORCEINLINE TSharedPtr<T> MakeShared(TArgs&&... Args)
{
    return std::make_shared<T>(std::forward<TArgs>(Args)...);
}

template <typename T>
using TUniquePtr = std::unique_ptr<T>;

template <typename T, typename... TArgs>
[[nodiscard]] FORCEINLINE TUniquePtr<T> MakeUnique(TArgs&&... Args)
{
    return std::make_unique<T>(std::forward<TArgs>(Args)...);
}
