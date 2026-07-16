#pragma once

#include <type_traits>

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Events/DelegateHandle.h"

namespace Opaax
{
    // =============================================================================
    // Type key — compile-time, module-stable
    // =============================================================================

    namespace Detail
    {
        /** Compile-time FNV-1a over a null-terminated string. */
        constexpr Uint64 Fnv1aHash(const char* InStr) noexcept
        {
            Uint64 lHash = 14695981039346656037ull;
            while (*InStr != '\0')
            {
                lHash = (lHash ^ static_cast<Uint64>(*InStr)) * 1099511628211ull;
                ++InStr;
            }
            return lHash;
        }

        /**
         * Stable per-type id: hash of the compiler's decorated signature for this
         * instantiation (contains T). Identical in every module under one compiler —
         * the DLL-safe alternative to a monotonic family-id counter (which, as a
         * function-local template static, would differ per DLL/exe).
         */
        template<typename T>
        constexpr Uint64 EventTypeKey() noexcept
        {
            return Fnv1aHash(__FUNCSIG__);
        }
    }

    // =============================================================================
    // EventBus — Tier-3 (decoupled pub/sub)
    // =============================================================================

    /**
     * @class EventBus
     * Reusable pub/sub bus. One instance is owned per tier (Engine owns one; the
     * editor will own its own). Payloads are trivially-copyable POD structs keyed by
     * a compile-time hashed type id — sender and receiver never reference each other.
     *
     * Dispatch is immediate (Publish) or queued (Enqueue, delivered at Flush — the
     * default choice). Dispatch snapshots the bucket, so a handler may (un)subscribe
     * or publish during delivery. Not thread-safe in v1.
     *
     * Every Subscribe returns a DelegateHandle; the subscriber MUST Unsubscribe (or
     * UnsubscribeAll(this)) before it dies — a live handler capturing a dangling
     * object crashes the next Publish.
     */
    class OPAAX_API EventBus
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
        /** 
         * Register a free function / lambda for TEvent. @return handle for Unsubscribe. 
         */
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

        /** 
         * Register a member function on InObj for TEvent. 
         * @return handle for Unsubscribe. 
         */
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

        /** Remove every subscription owned by InObj (call from the owner's teardown). */
        void UnsubscribeAll(void* InOwner);

        // =============================================================================
        // Publishing
        // =============================================================================
    public:
        /** Immediate dispatch — invokes all TEvent subscribers now, on this thread. */
        template<typename TEvent>
        void Publish(const TEvent& InEvent)
        {
            static_assert(std::is_trivially_copyable_v<TEvent>,
                "EventBus payloads must be trivially-copyable POD structs");

            PublishImpl(Detail::EventTypeKey<TEvent>(), &InEvent);
        }

        /**
         * Queued dispatch (the default choice). Copies the payload into the frame queue;
         * delivered at the next Flush. Events enqueued DURING Flush land in the next
         * frame's queue (double-buffered).
         */
        template<typename TEvent>
        void Enqueue(const TEvent& InEvent)
        {
            static_assert(std::is_trivially_copyable_v<TEvent>,
                "EventBus payloads must be trivially-copyable POD structs");

            EnqueueImpl([this, lPayload = InEvent]() { PublishImpl(Detail::EventTypeKey<TEvent>(), &lPayload); });
        }

        /** Drain the queued events in FIFO order. Called once per frame by the owner. */
        void Flush();

        // =============================================================================
        // Members
        // =============================================================================
    private:
        using ErasedHandler = TFunction<void(const void*)>;

        struct HandlerEntry
        {
            DelegateHandle Handle;
            void*          Owner = nullptr;   // non-null for member subs; keyed by UnsubscribeAll
            ErasedHandler  Handler;
        };

        DelegateHandle SubscribeImpl(Uint64 InTypeKey, void* InOwner, ErasedHandler InHandler);
        void           PublishImpl(Uint64 InTypeKey, const void* InPayload);
        void           EnqueueImpl(TFunction<void()> InDispatch);

        UnorderedMap<Uint64, TDynArray<HandlerEntry>> m_Handlers;
        TDynArray<TFunction<void()>>                  m_Queue;
    };
}
