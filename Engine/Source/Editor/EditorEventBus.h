#pragma once

#if OPAAX_WITH_EDITOR

#include <functional>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Core/EngineAPI.h"
#include "Core/Event/OpaaxEvent.hpp"
#include "Core/Event/OpaaxEventTypes.hpp"
#include "Core/OpaaxTypes.h"

namespace Opaax::Editor
{
    class EditorEventBus;

    // =============================================================================
    // SubscriptionToken
    // =============================================================================

    /**
     * @class SubscriptionToken
     * RAII unsubscribe handle returned by EditorEventBus::Subscribe.
     *
     * Move-only. Dtor calls back into the bus and removes the registration.
     *
     * Lifetime invariant: a SubscriptionToken MUST NOT outlive its owning
     * EditorEventBus. Under the editor's ownership graph this is guaranteed —
     * panels are owned by EditorSubsystem, the bus is owned by EditorSubsystem,
     * and panel destruction runs before bus destruction. The bus dtor asserts
     * the registry is empty as a tripwire for that invariant.
     */
    class OPAAX_API SubscriptionToken final
    {
        // =============================================================================
        // CTORs - DTOR
        // =============================================================================
    public:
        SubscriptionToken() noexcept = default;
        SubscriptionToken(EditorEventBus* InBus, EEventType InType, Uint64 InID) noexcept
            : m_Bus(InBus), m_Type(InType), m_ID(InID)
        {}
        ~SubscriptionToken();

        // =============================================================================
        // Copy - Delete
        // =============================================================================
        SubscriptionToken(const SubscriptionToken&)            = delete;
        SubscriptionToken& operator=(const SubscriptionToken&) = delete;

        // =============================================================================
        // Move
        // =============================================================================
        SubscriptionToken(SubscriptionToken&& Other) noexcept;
        SubscriptionToken& operator=(SubscriptionToken&& Other) noexcept;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /** Cancels the subscription early (idempotent). */
        void Reset() noexcept;

        FORCEINLINE bool IsValid() const noexcept { return m_Bus != nullptr; }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        EditorEventBus* m_Bus  = nullptr;
        EEventType      m_Type = EEventType::None;
        Uint64          m_ID   = 0;
    };

    // =============================================================================
    // EditorEventBus
    // =============================================================================

    /**
     * @class EditorEventBus
     * Editor-only pub/sub bus. Type-keyed — subscribers register against a single
     * concrete event type and only receive events of that type.
     *
     * Dispatch is immediate-synchronous: Publish walks the bucket for
     * event.GetEventType() and calls each handler in registration order.
     * The bucket is snapshotted before iteration so subscribers may publish or
     * (un)subscribe during dispatch without invalidating iteration.
     *
     * Owned by EditorSubsystem. Editor-only — gated by OPAAX_WITH_EDITOR.
     * Non-movable: SubscriptionTokens hold a raw EditorEventBus* and a move
     * would silently dangle them.
     */
    class OPAAX_API EditorEventBus final
    {
        // =============================================================================
        // CTORs - DTOR
        // =============================================================================
    public:
        EditorEventBus() = default;
        ~EditorEventBus();

        // =============================================================================
        // Copy - Move - Delete
        // =============================================================================
        EditorEventBus(const EditorEventBus&)            = delete;
        EditorEventBus& operator=(const EditorEventBus&) = delete;
        EditorEventBus(EditorEventBus&&)                 = delete;
        EditorEventBus& operator=(EditorEventBus&&)      = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * Register a handler for a concrete event type.
         * @param Callback void(const TEvent&) — receives the down-cast event.
         * @return SubscriptionToken — keep it alive for as long as the subscription should persist.
         */
        template<typename TEvent>
        SubscriptionToken Subscribe(std::function<void(const TEvent&)> Callback)
        {
            static_assert(std::is_base_of_v<OpaaxEvent, TEvent>,
                "EditorEventBus::Subscribe: TEvent must derive from OpaaxEvent");

            const EEventType lType = TEvent::GetStaticType();
            const Uint64     lID   = ++m_NextID;

            auto lShim = [Cb = std::move(Callback)](const OpaaxEvent& InEvent)
            {
                Cb(static_cast<const TEvent&>(InEvent));
            };

            m_Handlers[lType].push_back({lID, std::move(lShim)});
            return SubscriptionToken{this, lType, lID};
        }

        /** Synchronous dispatch. Safe under reentrant publish / (un)subscribe. */
        void Publish(const OpaaxEvent& InEvent);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        friend class SubscriptionToken;
        void Unsubscribe(EEventType InType, Uint64 InID);

        struct HandlerEntry
        {
            Uint64                                 ID;
            std::function<void(const OpaaxEvent&)> Handler;
        };

        std::unordered_map<EEventType, std::vector<HandlerEntry>> m_Handlers;
        Uint64                                                    m_NextID = 0;
    };

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
