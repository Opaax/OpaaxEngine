// Suite: the engine's aliases in Core/OpaaxTypes.h that are more than a name: TOptional, the tuple
// helpers, TRef, Swap, ArraySize, the atomics with EMemoryOrder, the locks and ThisThread.
#include <doctest.h>

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

using namespace Opaax;

namespace
{
    TOptional<Int32> ParsePositive(const Int32 InValue)
    {
        if (InValue <= 0)
        {
            return NullOpt;
        }
        return InValue;
    }

    constexpr Int32 PRIMES[] = { 2, 3, 5, 7, 11 };
}

TEST_CASE("Types: TOptional holds a value or NullOpt")
{
    const TOptional<Int32> lValue = ParsePositive(4);
    const TOptional<Int32> lEmpty = ParsePositive(-1);

    REQUIRE(lValue.has_value());
    CHECK(*lValue == 4);
    CHECK_FALSE(lEmpty.has_value());
    CHECK(lEmpty == NullOpt);
    CHECK(lEmpty.value_or(9) == 9);
}

TEST_CASE("Types: tuple helpers build, read and unpack a tuple")
{
    constexpr auto lTuple = MakeTuple(1, 2.5f, 'x');

    static_assert(TTupleSize<decltype(lTuple)> == 3);
    static_assert(TIsSame<TRemoveCVRef<decltype(TupleGet<1>(lTuple))>, float>);
    static_assert(TupleGet<0>(lTuple) == 1);

    const Int32 lSum = TupleApply([](const Int32 InA, const float InB, const char InC)
    {
        return InA + static_cast<Int32>(InB) + (InC == 'x' ? 100 : 0);
    }, lTuple);
    CHECK(lSum == 103);

    // A mutable tuple hands out references.
    TTuple<Int32, OpaaxString> lMutable{ 1, "one" };
    TupleGet<0>(lMutable) = 2;
    CHECK(TupleGet<0>(lMutable) == 2);
    CHECK(TupleGet<1>(lMutable) == "one");

    const TPair<Int32, OpaaxString> lPair{ 3, "three" };
    CHECK(lPair.first == 3);
    CHECK(lPair.second == "three");
}

TEST_CASE("Types: Ref keeps a reference through a by-value capture")
{
    Int32 lCounter = 0;

    // A factory that captures its arguments by value.
    auto lMakeIncrement = [](auto InArgument)
    {
        return [InArgument]() mutable { Int32& lTarget = InArgument; ++lTarget; };
    };

    auto lByValue = lMakeIncrement(lCounter);
    lByValue();
    CHECK(lCounter == 0);   // the copy moved, not the counter

    auto lByRef = lMakeIncrement(Ref(lCounter));
    lByRef();
    lByRef();
    CHECK(lCounter == 2);

    static_assert(TIsSame<decltype(Ref(lCounter)), TRef<Int32>>);
}

TEST_CASE("Types: Swap, ArraySize, PtrDiff and TDeque")
{
    OpaaxString lA("left");
    OpaaxString lB("a right string long enough for the heap");
    Swap(lA, lB);
    CHECK(lA == "a right string long enough for the heap");
    CHECK(lB == "left");

    static_assert(ArraySize(PRIMES) == 5);

    TDynArray<Int32> lValues{ 10, 20, 30 };
    lValues.erase(lValues.begin() + static_cast<PtrDiff>(1));
    CHECK(lValues == TDynArray<Int32>{ 10, 30 });

    TDeque<Int32> lQueue;
    lQueue.push_back(2);
    lQueue.push_front(1);
    CHECK(lQueue.front() == 1);
    CHECK(lQueue.back() == 2);
}

TEST_CASE("Types: atomics with EMemoryOrder, AtomicFlag")
{
    TAtomic<Uint32> lValue{ 0 };
    lValue.store(5, EMemoryOrder::Release);
    CHECK(lValue.load(EMemoryOrder::Acquire) == 5);
    CHECK(lValue.fetch_add(1, EMemoryOrder::Relaxed) == 5);
    CHECK(lValue.load(EMemoryOrder::SequentiallyConsistent) == 6);

    AtomicFlag lFlag = ATOMIC_FLAG_INIT;
    CHECK_FALSE(lFlag.test_and_set(EMemoryOrder::AcquireRelease));
    CHECK(lFlag.test_and_set());
    lFlag.clear();
    CHECK_FALSE(lFlag.test_and_set());
}

TEST_CASE("Types: locks, threads and ThisThread")
{
    SharedMutex lMutex;
    Int32       lShared = 0;

    {
        TUniqueLock<SharedMutex> lWriter(lMutex);
        lShared = 1;
    }
    {
        // Two readers at once.
        TSharedLock<SharedMutex> lReaderA(lMutex);
        TSharedLock<SharedMutex> lReaderB(lMutex);
        CHECK(lShared == 1);
    }

    const ThreadId lMain = ThisThread::GetId();
    ThreadId       lWorkerId;
    TAtomic<bool>  bStarted{ false };

    Thread lWorker([&]()
    {
        lWorkerId = ThisThread::GetId();
        bStarted.store(true, EMemoryOrder::Release);
        ThisThread::SleepFor(Milliseconds(1));
    });

    while (!bStarted.load(EMemoryOrder::Acquire))
    {
        ThisThread::YieldNow();
    }
    lWorker.join();

    CHECK(ThisThread::GetId() == lMain);
    CHECK(lWorkerId != lMain);
}
