#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

    // =============================================================================
    // std type use alises
    // =============================================================================
namespace Opaax
{
    // =============================================================================
    // Types Aliases
    // =============================================================================
    using Uint8 = uint8_t;
    using Uint16 = uint16_t;
    using Uint32 = uint32_t;
    using Uint64 = uint64_t;

    using Int8 = int8_t;
    using Int16 = int16_t;
    using Int32 = int32_t;
    using Int64 = int64_t;

    // =============================================================================
    // Container Aliases
    // =============================================================================
    template<typename T>
    using TDynArray = std::vector<T>;

    template<typename T, size_t Size>
    using TFixedArray = std::array<T, Size>;

    template<typename FType>
    using TInitArray = std::initializer_list<FType>;
    
    template <
        typename TKey,
        typename TValue,
        typename Hash = std::hash<TKey>,
        typename KeyEqual = std::equal_to<TKey>,
        typename Allocator = std::allocator<std::pair<const TKey, TValue>>
    >
    using UnorderedMap = std::unordered_map<TKey, TValue, Hash, KeyEqual, Allocator>;

    // =============================================================================
    // Function Aliases
    // =============================================================================
    template<typename FType>
    using TFunction = std::function<FType>;

    // =============================================================================
    // Smart Pointers Aliases
    // =============================================================================
    template<typename T>
    using UniquePtr = std::unique_ptr<T>;

    template<typename T, typename ... Args>
    constexpr UniquePtr<T> MakeUnique(Args&& ... InArgs)
    {
        return std::make_unique<T>(std::forward<Args>(InArgs)...);
    }

    template<typename T>
    using SharedPtr = std::shared_ptr<T>;

    template<typename T, typename ... Args>
    constexpr SharedPtr<T> MakeShared(Args&& ... InArgs)
    {
        return std::make_shared<T>(std::forward<Args>(InArgs)...);
    }

    // =============================================================================
    // Misc Aliases
    // =============================================================================
    template <typename T>
    constexpr std::remove_reference_t<T>&& Move(T&& Arg) noexcept
    {
        return std::move(Arg);
    }
}
