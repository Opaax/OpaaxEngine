#include "Editor/EditorEventBus.h"

#if OPAAX_WITH_EDITOR

#include <algorithm>

#include "Core/Log/OpaaxLog.h"

namespace Opaax::Editor
{
    // =============================================================================
    // SubscriptionToken
    // =============================================================================

    SubscriptionToken::~SubscriptionToken()
    {
        Reset();
    }

    SubscriptionToken::SubscriptionToken(SubscriptionToken&& Other) noexcept
        : m_Bus(Other.m_Bus), m_Type(Other.m_Type), m_ID(Other.m_ID)
    {
        Other.m_Bus  = nullptr;
        Other.m_Type = EEventType::None;
        Other.m_ID   = 0;
    }

    SubscriptionToken& SubscriptionToken::operator=(SubscriptionToken&& Other) noexcept
    {
        if (this != &Other)
        {
            Reset();
            m_Bus        = Other.m_Bus;
            m_Type       = Other.m_Type;
            m_ID         = Other.m_ID;
            Other.m_Bus  = nullptr;
            Other.m_Type = EEventType::None;
            Other.m_ID   = 0;
        }
        return *this;
    }

    void SubscriptionToken::Reset() noexcept
    {
        if (m_Bus != nullptr)
        {
            m_Bus->Unsubscribe(m_Type, m_ID);
            m_Bus  = nullptr;
            m_Type = EEventType::None;
            m_ID   = 0;
        }
    }

    // =============================================================================
    // EditorEventBus
    // =============================================================================

    EditorEventBus::~EditorEventBus()
    {
        for (const auto& [lType, lBucket] : m_Handlers)
        {
            if (!lBucket.empty())
            {
                OPAAX_CORE_ERROR(
                    "EditorEventBus dtor: subscribers still registered for EEventType={} (count={}). "
                    "Token leak or lifetime inversion (token outlived the bus).",
                    static_cast<Uint32>(lType), lBucket.size());
                OPAAX_ASSERT(lBucket.empty());
            }
        }
    }

    void EditorEventBus::Publish(const OpaaxEvent& InEvent)
    {
        const auto lIt = m_Handlers.find(InEvent.GetEventType());
        if (lIt == m_Handlers.end()) { return; }

        // Snapshot the bucket — a handler may publish or (un)subscribe during dispatch,
        // mutating m_Handlers under our feet. Iterating a copy keeps delivery stable.
        const auto lSnapshot = lIt->second;
        for (const auto& lEntry : lSnapshot)
        {
            lEntry.Handler(InEvent);
        }
    }

    void EditorEventBus::Unsubscribe(EEventType InType, Uint64 InID)
    {
        const auto lIt = m_Handlers.find(InType);
        if (lIt == m_Handlers.end()) { return; }

        auto& lBucket = lIt->second;
        lBucket.erase(
            std::remove_if(lBucket.begin(), lBucket.end(),
                [InID](const HandlerEntry& InEntry) { return InEntry.ID == InID; }),
            lBucket.end());

        if (lBucket.empty())
        {
            m_Handlers.erase(lIt);
        }
    }
}

#endif // OPAAX_WITH_EDITOR
