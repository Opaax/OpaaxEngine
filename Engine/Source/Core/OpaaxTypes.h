#pragma once
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <filesystem>

// =============================================================================
// Canonical type aliases — use these in engine code instead of bare std types.
//
//   Integers:   Uint8/16/32/64, Int8/16/32/64        (cstdint fixed-width)
//   Container:  TDynArray<T>                         (std::vector<T>)
//               TFixedArray<T, N>                    (std::array<T, N>)
//               TInitArray<T>                        (std::initializer_list<T>)
//               TUnorderedMap<K, V[, Hash, Eq, Alloc]> (std::unordered_map)
//               TUnorderedSet<K[, Hash, Eq, Alloc]>  (std::unordered_set)
//   Function:   TFunction<Sig>                       (std::function<Sig>)
//   Pointers:   TUniquePtr<T>, MakeUnique<T>(args...)
//               TSharedPtr<T>, MakeShared<T>(args...)
//               TAtomic<T>                           (std::atomic<T>)
//   Threading:  Thread                               (std::thread)
//               Mutex                                (std::mutex)
//               ConditionVariable                    (std::condition_variable)
//               TLockGuard<T>                        (std::lock_guard<T>)
//               TUniqueLock<T>                       (std::unique_lock<T>)
//               TQueue<T>                            (std::queue<T>)
//
// The T prefix marks a TEMPLATE alias. Non-template aliases (Thread, Mutex,
// RecursiveMutex, ConditionVariable) stay bare, as do the MakeUnique/MakeShared
// helpers — they are functions, not types.
//   Misc:       Move(arg)                            (std::move)
//
// Strings: see Core/OpaaxString.hpp (OpaaxString) and Core/OpaaxStringID.hpp.
//
// Bare primitives (`int`, `unsigned`, `size_t`) are tolerated only at third-party
// API boundaries (GLFW callbacks, stb_image, std::filesystem, nlohmann::json,
// spdlog, GL types). New engine code should prefer the aliases above.
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
    using TUnorderedMap = std::unordered_map<TKey, TValue, Hash, KeyEqual, Allocator>;

    template <
        typename TKey,
        typename Hash = std::hash<TKey>,
        typename KeyEqual = std::equal_to<TKey>,
        typename Allocator = std::allocator<TKey>
    >
    using TUnorderedSet = std::unordered_set<TKey, Hash, KeyEqual, Allocator>;

    // =============================================================================
    // Function Aliases
    // =============================================================================
    template<typename FType>
    using TFunction = std::function<FType>;

    // =============================================================================
    // Smart Pointers Aliases
    // =============================================================================
    template<typename T>
    using TUniquePtr = std::unique_ptr<T>;

    template<typename T, typename ... Args>
    constexpr TUniquePtr<T> MakeUnique(Args&& ... InArgs)
    {
        return std::make_unique<T>(std::forward<Args>(InArgs)...);
    }

    template<typename T>
    using TSharedPtr = std::shared_ptr<T>;

    template<typename T, typename ... Args>
    constexpr TSharedPtr<T> MakeShared(Args&& ... InArgs)
    {
        return std::make_shared<T>(std::forward<Args>(InArgs)...);
    }

    template<typename T>
    using TAtomic = std::atomic<T>;

    // =============================================================================
    // Threading Aliases
    // =============================================================================
    using Thread            = std::thread;
    using Mutex             = std::mutex;
    using RecursiveMutex    = std::recursive_mutex;
    using ConditionVariable = std::condition_variable;

    template<typename T>
    using TLockGuard = std::lock_guard<T>;

    template<typename T>
    using TUniqueLock = std::unique_lock<T>;

    template<typename T>
    using TQueue = std::queue<T>;

    // =============================================================================
    // Misc Aliases
    // =============================================================================
    template <typename T>
    constexpr std::remove_reference_t<T>&& Move(T&& Arg) noexcept
    {
        return std::move(Arg);
    }
}
