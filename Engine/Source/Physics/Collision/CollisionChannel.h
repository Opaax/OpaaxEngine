#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnum.h"
#include "Core/String/OpaaxStringID.hpp"

namespace Opaax
{
    // =============================================================================
    // ECollisionChannel — X-macro driven (Physics/Collision/CollisionChannelList.h)
    // =============================================================================
    /**
     * @enum ECollisionChannel
     * The object-type category of a collider — the unit collision filtering works in. The enum
     * body and the matching name table are both generated from CollisionChannelList.h, so
     * adding a channel is one new line there.
     *
     * The ordinal is also the filter category-bit index (see CategoryBit), which caps the set
     * at 64 and means the list may only ever be appended to.
     */
    enum class ECollisionChannel : Uint8
    {
        #define OPAAX_COLLISION_CHANNEL(Name) Name,
        #include "CollisionChannelList.h"
        #undef OPAAX_COLLISION_CHANNEL
        Count
    };

    static_assert(static_cast<Uint8>(ECollisionChannel::Count) <= 64,
                  "ECollisionChannel exceeds the 64-bit collision filter category limit.");

    // =============================================================================
    // Labels + values
    // =============================================================================
    /** The channel's label (I11 — a free ToString found by ADL), for logs and the dropdown. */
    inline const char* ToString(ECollisionChannel InChannel) noexcept
    {
        switch (InChannel)
        {
            #define OPAAX_COLLISION_CHANNEL(Name) case ECollisionChannel::Name: return #Name;
            #include "CollisionChannelList.h"
            #undef OPAAX_COLLISION_CHANNEL
            default: return "Unknown";
        }
    }

    /**
     * The channels as DATA, so any ECollisionChannel field draws as a dropdown and serializes by
     * label with no per-type editor code. Written out rather than stamped with OPAAX_ENUM_VALUES
     * for RenderLayer.h's reason: the list lives in an #include, and a preprocessor directive
     * cannot appear inside a macro argument. `Count` is absent on purpose — it is a bound, not a
     * channel.
     */
    template<>
    struct TEnumValues<ECollisionChannel>
    {
        static constexpr ECollisionChannel Values[] =
        {
            #define OPAAX_COLLISION_CHANNEL(Name) ECollisionChannel::Name,
            #include "CollisionChannelList.h"
            #undef OPAAX_COLLISION_CHANNEL
        };
    };

    /** Parallel canonical-name array. Index by static_cast<Uint8>(ECollisionChannel). */
    inline const OpaaxStringID g_CollisionChannelIDs[] =
    {
        #define OPAAX_COLLISION_CHANNEL(Name) OPAAX_ID(#Name),
        #include "CollisionChannelList.h"
        #undef OPAAX_COLLISION_CHANNEL
    };

    /** Channel -> canonical id. O(1). */
    inline const OpaaxStringID& ToStringID(ECollisionChannel InChannel) noexcept
    {
        const Uint8 lIdx = static_cast<Uint8>(InChannel);
        return lIdx < static_cast<Uint8>(ECollisionChannel::Count)
                   ? g_CollisionChannelIDs[lIdx]
                   : g_CollisionChannelIDs[0];
    }

    /** Id -> channel. Linear scan of a handful of integer compares. */
    inline ECollisionChannel CollisionChannelFromStringID(const OpaaxStringID& InID) noexcept
    {
        for (Uint8 i = 0; i < static_cast<Uint8>(ECollisionChannel::Count); ++i)
        {
            if (g_CollisionChannelIDs[i] == InID)
            {
                return static_cast<ECollisionChannel>(i);
            }
        }
        return ECollisionChannel::WorldStatic;
    }

    // =============================================================================
    // Filter bits
    // =============================================================================
    /** The single category bit a collider on this channel belongs to. */
    inline Uint64 CategoryBit(ECollisionChannel InChannel) noexcept
    {
        return Uint64(1) << static_cast<Uint8>(InChannel);
    }

    /** Every channel bit — the default "interacts with everything" mask. */
    inline constexpr Uint64 AllChannelsMask() noexcept { return ~0ull; }
}
