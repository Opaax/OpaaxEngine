#pragma once
#include <array>
#include <atomic>
#include <chrono>
#include <compare>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <shared_mutex>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Core/Templates/TypeTraits.h"

// =============================================================================
// Type aliases — use these in engine code instead of bare std types.
//
//   Integers:   Uint8/16/32/64, Int8/16/32/64     (cstdint fixed-width)
//               PtrDiff, UintPtr                  (std::ptrdiff_t, std::uintptr_t)
//   Container:  TDynArray<T>                      (std::vector<T>)
//               TFixedArray<T, N>                 (std::array<T, N>)
//               TInitArray<T>                     (std::initializer_list<T>)
//               TDeque<T>                         (std::deque<T>)
//               TUnorderedMap<K, V[, Hash...]>    (std::unordered_map)
//               TUnorderedSet<K[, Hash...]>       (std::unordered_set)
//               TQueue<T>                         (std::queue<T>)
//   Hash:       THash<T>                          (std::hash<T>; specialize it for a key type)
//   Values:     TOptional<T>, NullOpt             (std::optional<T>, std::nullopt)
//               TPair<A, B>                       (std::pair<A, B>)
//               TTuple<T...>, MakeTuple(args...)  (std::tuple, std::make_tuple)
//               TupleGet<N>(t), TupleApply(f, t)  (std::get, std::apply)
//               TTupleSize<T>                     (std::tuple_size_v)
//   Function:   TFunction<Sig>                    (std::function<Sig>)
//   Pointers:   TUniquePtr<T>, MakeUnique<T>(args...)
//               TSharedPtr<T>, MakeShared<T>(args...)
//   Atomics:    TAtomic<T>, AtomicFlag            (std::atomic<T>, std::atomic_flag)
//               EMemoryOrder::Acquire...          (std::memory_order_acquire...)
//   Threading:  Thread, ThreadId                  (std::thread, std::thread::id)
//               Mutex, RecursiveMutex             (std::mutex, std::recursive_mutex)
//               SharedMutex                       (std::shared_mutex)
//               ConditionVariable                 (std::condition_variable)
//               TLockGuard<T>, TUniqueLock<T>     (std::lock_guard, std::unique_lock)
//               TSharedLock<T>                    (std::shared_lock)
//               Milliseconds                      (std::chrono::milliseconds, for waits)
//               ThisThread::GetId/YieldNow/SleepFor (std::this_thread)
//   Misc:       Move(arg), Forward<T>(arg)        (std::move, std::forward)
//               Swap(a, b)                        (std::swap)
//               TRef<T>, Ref(arg)                 (std::reference_wrapper<T>, std::ref)
//               ArraySize(array)                  (std::size of a C array)
//
// The T prefix marks a template alias. A lock names its mutex type: TLockGuard<Mutex> lLock(m_Mutex).
// Strings: see Core/String/OpaaxString.hpp and Core/String/OpaaxStringID.hpp.
// Type traits and concepts: see Core/Templates/TypeTraits.h.
// Bare types (int, size_t, ...) are fine at third-party API boundaries.
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

    /** The difference of two pointers or iterators: Array.begin() + static_cast<PtrDiff>(Index). */
    using PtrDiff = std::ptrdiff_t;

    /** An unsigned integer that can hold a pointer. */
    using UintPtr = std::uintptr_t;

    // =============================================================================
    // Container Aliases
    // =============================================================================
    template<typename T>
    using TDynArray = std::vector<T>;

    template<typename T, size_t Size>
    using TFixedArray = std::array<T, Size>;

    template<typename FType>
    using TInitArray = std::initializer_list<FType>;

    /** Fast insertion and removal at both ends; elements do not move when it grows. */
    template<typename T>
    using TDeque = std::deque<T>;

    /**
     * The hash TUnorderedMap and TUnorderedSet use by default. A type becomes a key by
     * specializing it next to its definition: template<> struct THash<Guid> { size_t operator()... }.
     */
    template<typename T>
    struct THash : std::hash<T>
    {
    };

    template <
        typename TKey,
        typename TValue,
        typename Hash = THash<TKey>,
        typename KeyEqual = std::equal_to<TKey>,
        typename Allocator = std::allocator<std::pair<const TKey, TValue>>
    >
    using TUnorderedMap = std::unordered_map<TKey, TValue, Hash, KeyEqual, Allocator>;

    template <
        typename TKey,
        typename Hash = THash<TKey>,
        typename KeyEqual = std::equal_to<TKey>,
        typename Allocator = std::allocator<TKey>
    >
    using TUnorderedSet = std::unordered_set<TKey, Hash, KeyEqual, Allocator>;

    template<typename T>
    using TQueue = std::queue<T>;

    // =============================================================================
    // Value Aliases
    // =============================================================================
    /** A value or nothing. Empty is NullOpt. */
    template<typename T>
    using TOptional = std::optional<T>;

    /** The empty TOptional: return NullOpt; */
    inline constexpr std::nullopt_t NullOpt = std::nullopt;

    template<typename TFirst, typename TSecond>
    using TPair = std::pair<TFirst, TSecond>;

    template<typename... TElements>
    using TTuple = std::tuple<TElements...>;

    /** A TTuple of copies of the arguments (a TRef stays a reference). */
    template<typename... TArgs>
    constexpr auto MakeTuple(TArgs&&... InArgs)
    {
        return std::make_tuple(std::forward<TArgs>(InArgs)...);
    }

    /** Element N of a tuple, as a reference of the tuple's kind. */
    template<size_t N, typename TTupleValue>
    constexpr decltype(auto) TupleGet(TTupleValue&& InTuple) noexcept
    {
        return std::get<N>(std::forward<TTupleValue>(InTuple));
    }

    /** Calls InFunction with the elements of InTuple as its arguments. */
    template<typename TFunc, typename TTupleValue>
    constexpr decltype(auto) TupleApply(TFunc&& InFunction, TTupleValue&& InTuple)
    {
        return std::apply(std::forward<TFunc>(InFunction), std::forward<TTupleValue>(InTuple));
    }

    /** The number of elements of a tuple type (references and const are ignored). */
    template<typename TTupleValue>
    inline constexpr size_t TTupleSize = std::tuple_size_v<TRemoveCVRef<TTupleValue>>;

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

    /** A lock-free boolean flag: test_and_set / clear. */
    using AtomicFlag = std::atomic_flag;

    /** Memory orders for TAtomic operations: m_Head.load(EMemoryOrder::Acquire). */
    namespace EMemoryOrder
    {
        inline constexpr std::memory_order Relaxed                = std::memory_order_relaxed;
        inline constexpr std::memory_order Acquire                = std::memory_order_acquire;
        inline constexpr std::memory_order Release                = std::memory_order_release;
        inline constexpr std::memory_order AcquireRelease         = std::memory_order_acq_rel;
        inline constexpr std::memory_order SequentiallyConsistent = std::memory_order_seq_cst;
    }

    // =============================================================================
    // Threading Aliases
    // =============================================================================
    using Thread            = std::thread;
    using ThreadId          = std::thread::id;
    using Mutex             = std::mutex;
    using RecursiveMutex    = std::recursive_mutex;
    using SharedMutex       = std::shared_mutex;
    using ConditionVariable = std::condition_variable;

    template<typename T>
    using TLockGuard = std::lock_guard<T>;

    template<typename T>
    using TUniqueLock = std::unique_lock<T>;

    /** A reader's lock on a SharedMutex: many readers at once, never with a writer (TUniqueLock). */
    template<typename T>
    using TSharedLock = std::shared_lock<T>;

    /** A duration for waits: ConditionVariable::wait_for, ThisThread::SleepFor. */
    using Milliseconds = std::chrono::milliseconds;

    /** The calling thread. */
    namespace ThisThread
    {
        inline ThreadId GetId() noexcept { return std::this_thread::get_id(); }

        /** Lets another thread run: the body of a spin wait. */
        inline void YieldNow() noexcept { std::this_thread::yield(); }

        /** Blocks the calling thread for at least InDuration. */
        inline void SleepFor(const Milliseconds InDuration) { std::this_thread::sleep_for(InDuration); }
    }

    // =============================================================================
    // Misc Aliases
    // =============================================================================
    template <typename T>
    constexpr TRemoveReference<T>&& Move(T&& Arg) noexcept
    {
        return std::move(Arg);
    }

    template <typename T>
    constexpr T&& Forward(TRemoveReference<T>& Arg) noexcept
    {
        return static_cast<T&&>(Arg);
    }

    template <typename T>
    constexpr T&& Forward(TRemoveReference<T>&& Arg) noexcept
    {
        return static_cast<T&&>(Arg);
    }

    /** Exchanges the values of InA and InB. */
    template <typename T>
    constexpr void Swap(T& InA, T& InB) noexcept(noexcept(std::swap(InA, InB)))
    {
        std::swap(InA, InB);
    }

    /** The element count of a C array, at compile time: ArraySize(TEnumValues<E>::Values). */
    template <typename T, size_t N>
    constexpr size_t ArraySize(const T (&)[N]) noexcept
    {
        return N;
    }

    /**
     * A copyable reference. Passes a reference where a copy is taken: a subsystem factory captures
     * its arguments by value, so RegisterSubsystem<T>(Ref(InContext)) keeps the context itself.
     */
    template <typename T>
    using TRef = std::reference_wrapper<T>;

    template <typename T>
    constexpr TRef<T> Ref(T& InValue) noexcept
    {
        return TRef<T>(InValue);
    }

    /** No reference to a temporary. */
    template <typename T>
    void Ref(const T&&) = delete;
}
