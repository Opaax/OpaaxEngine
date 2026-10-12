#pragma once

#include <new>

#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // TSpscQueue — a fixed-size, lock-free queue between one producer thread and one consumer
    //   thread (the game thread and the audio thread). Never allocates after construction: a push
    //   on a full queue fails instead.
    // =============================================================================
    template<typename T>
    class TSpscQueue
    {
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        /** Holds at least InCapacity items (rounded up to a power of two). */
        explicit TSpscQueue(const Uint32 InCapacity)
        {
            Uint32 lSize = 2;
            while (lSize < InCapacity + 1)
            {
                lSize <<= 1;
            }

            m_Slots.resize(lSize);
            m_Mask = lSize - 1;
        }

        TSpscQueue(const TSpscQueue&)            = delete;
        TSpscQueue& operator=(const TSpscQueue&) = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * Producer thread only.
         * @return False when the queue is full (InItem is not queued)
         */
        bool Push(const T& InItem)
        {
            const Uint32 lTail = m_Tail.load(EMemoryOrder::Relaxed);
            const Uint32 lNext = (lTail + 1) & m_Mask;
            if (lNext == m_Head.load(EMemoryOrder::Acquire))
            {
                return false;
            }

            m_Slots[lTail] = InItem;
            m_Tail.store(lNext, EMemoryOrder::Release);
            return true;
        }

        /**
         * Consumer thread only.
         * @return False when the queue is empty
         */
        bool Pop(T& OutItem)
        {
            const Uint32 lHead = m_Head.load(EMemoryOrder::Relaxed);
            if (lHead == m_Tail.load(EMemoryOrder::Acquire))
            {
                return false;
            }

            OutItem = m_Slots[lHead];
            m_Head.store((lHead + 1) & m_Mask, EMemoryOrder::Release);
            return true;
        }

        /** Items that fit at once. */
        Uint32 Capacity() const noexcept { return m_Mask; }

        /** Approximate when the other thread is working on the queue. */
        bool IsEmpty() const noexcept
        {
            return m_Head.load(EMemoryOrder::Acquire) == m_Tail.load(EMemoryOrder::Acquire);
        }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TDynArray<T> m_Slots;
        Uint32       m_Mask = 0;

        // Each index on its own cache line: the two threads never write the same line.
        alignas(64) TAtomic<Uint32> m_Head{ 0 };   // next item to pop, written by the consumer
        alignas(64) TAtomic<Uint32> m_Tail{ 0 };   // next free slot, written by the producer
    };
}
