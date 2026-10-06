#pragma once

#include <type_traits>

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Events/DelegateHandle.h"
#include "Core/Reflection/TypeInfo.h"

namespace Opaax
{
    // =============================================================================
    // Type key — compile-time, the same in every translation unit and every run
    // =============================================================================

    namespace Detail
    {
        template<typename T>
        constexpr Uint64 EventTypeKey() noexcept
        {
            return TypeIdOf<T>();
        }
    }

    // =============================================================================
    // EventBus — decoupled publish/subscribe
    // =============================================================================

    /**
     * Publish/subscribe bus. Events are plain structs keyed by type.
     * Publish dispatches now; Enqueue dispatches at the next Flush (preferred).
     * Unsubscribe before the subscriber is destroyed. Not thread-safe.
     */
    class EventBus
    {
        // =============================================================================
        // CTORs - DTOR
        // =============================================================================
    public:
        EventBus() = default;
        ~EventBus();

        // =============================================================================
        // Copy - Move - Delete
        // =============================================================================
        EventBus(const EventBus&)            = delete;
        EventBus& operator=(const EventBus&) = delete;
        EventBus(EventBus&&)                 = delete;
        EventBus& operator=(EventBus&&)      = delete;

        // =============================================================================
        // Subscription
        // =============================================================================
    public:
        /** Subscribes a function or lambda. @return Handle for Unsubscribe. */
        template<typename TEvent>
        DelegateHandle Subscribe(TFunction<void(const TEvent&)> InHandler)
        {
            static_assert(std::is_trivially_copyable_v<TEvent>,
                "EventBus payloads must be trivially-copyable POD structs");

            return SubscribeImpl(Detail::EventTypeKey<TEvent>(), nullptr,
                [Callback = Move(InHandler)](const void* InPayload)
                {
                    Callback(*static_cast<const TEvent*>(InPayload));
                });
        }

        /** Subscribes a member function on InObj. @return Handle for Unsubscribe. */
        template<typename TEvent, typename T>
        DelegateHandle Subscribe(T* InObj, void (T::*InMember)(const TEvent&))
        {
            static_assert(std::is_trivially_copyable_v<TEvent>,
                "EventBus payloads must be trivially-copyable POD structs");

            return SubscribeImpl(Detail::EventTypeKey<TEvent>(), InObj,
                [InObj, InMember](const void* InPayload)
                {
                    (InObj->*InMember)(*static_cast<const TEvent*>(InPayload));
                });
        }

        /** Remove one subscription by handle. @return true if removed. */
        bool Unsubscribe(DelegateHandle InHandle);

        /** Removes every subscription owned by InObj. */
        void UnsubscribeAll(void* InOwner);

        // =============================================================================
        // Publishing
        // =============================================================================
    public:
        /** Dispatches now, on this thread. */
        template<typename TEvent>
        void Publish(const TEvent& InEvent)
        {
            static_assert(std::is_trivially_copyable_v<TEvent>,
                "EventBus payloads must be trivially-copyable POD structs");

            PublishImpl(Detail::EventTypeKey<TEvent>(), &InEvent);
        }

        /**
         * Queues the event for the next Flush. Events queued during Flush go to the next frame.
         */
        template<typename TEvent>
        void Enqueue(const TEvent& InEvent)
        {
            static_assert(std::is_trivially_copyable_v<TEvent>,
                "EventBus payloads must be trivially-copyable POD structs");

            EnqueueImpl([this, lPayload = InEvent]() { PublishImpl(Detail::EventTypeKey<TEvent>(), &lPayload); });
        }

        /** Dispatches the queued events in order. Called once per frame. */
        void Flush();

        // =============================================================================
        // Members
        // =============================================================================
    private:
        using ErasedHandler = TFunction<void(const void*)>;

        struct HandlerEntry
        {
            DelegateHandle Handle;
            void*          Owner = nullptr;   // set for member subscriptions (used by UnsubscribeAll)
            ErasedHandler  Handler;
        };

        DelegateHandle SubscribeImpl(Uint64 InTypeKey, void* InOwner, ErasedHandler InHandler);
        void           PublishImpl(Uint64 InTypeKey, const void* InPayload);
        void           EnqueueImpl(TFunction<void()> InDispatch);

        TUnorderedMap<Uint64, TDynArray<HandlerEntry>> m_Handlers;
        TDynArray<TFunction<void()>>                  m_Queue;
    };
}
