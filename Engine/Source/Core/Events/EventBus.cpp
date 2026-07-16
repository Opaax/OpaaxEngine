#include "EventBus.h"

namespace Opaax
{
    EventBus::~EventBus() = default;

    // =============================================================================
    // Subscription
    // =============================================================================

    DelegateHandle EventBus::SubscribeImpl(Uint64 InTypeKey, void* InOwner, ErasedHandler InHandler)
    {
        const DelegateHandle lHandle = DelegateHandle::Generate();
        m_Handlers[InTypeKey].push_back({lHandle, InOwner, Move(InHandler)});
        return lHandle;
    }

    bool EventBus::Unsubscribe(DelegateHandle InHandle)
    {
        for (auto& [lKey, lBucket] : m_Handlers)
        {
            for (auto lIt = lBucket.begin(); lIt != lBucket.end(); ++lIt)
            {
                if (lIt->Handle == InHandle)
                {
                    lBucket.erase(lIt);
                    return true;
                }
            }
        }
        return false;
    }

    void EventBus::UnsubscribeAll(void* InOwner)
    {
        for (auto& [lKey, lBucket] : m_Handlers)
        {
            for (auto lIt = lBucket.begin(); lIt != lBucket.end(); )
            {
                if (lIt->Owner == InOwner) { lIt = lBucket.erase(lIt); }
                else                       { ++lIt; }
            }
        }
    }

    // =============================================================================
    // Publishing
    // =============================================================================

    void EventBus::PublishImpl(Uint64 InTypeKey, const void* InPayload)
    {
        const auto lIt = m_Handlers.find(InTypeKey);
        if (lIt == m_Handlers.end()) { return; }

        // Snapshot the bucket — a handler may (un)subscribe or publish during dispatch,
        // mutating m_Handlers under our feet. Iterating a copy keeps delivery stable.
        const TDynArray<HandlerEntry> lSnapshot = lIt->second;
        for (const HandlerEntry& lEntry : lSnapshot)
        {
            lEntry.Handler(InPayload);
        }
    }

    void EventBus::EnqueueImpl(TFunction<void()> InDispatch)
    {
        m_Queue.push_back(Move(InDispatch));
    }

    void EventBus::Flush()
    {
        // Swap first: events enqueued DURING dispatch go into the now-empty m_Queue and
        // are delivered next frame — makes an infinite publish loop impossible.
        TDynArray<TFunction<void()>> lDraining;
        lDraining.swap(m_Queue);

        for (TFunction<void()>& lDispatch : lDraining)
        {
            lDispatch();
        }
    }
}
