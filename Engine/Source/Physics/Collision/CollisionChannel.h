#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnum.h"
#include "Core/String/OpaaxStringID.hpp"

namespace Opaax
{
    // =============================================================================
    // ECollisionChannel — generated from Physics/Collision/CollisionChannelList.h
    // =============================================================================
    /**
     * What kind of object a collider is (used for collision filtering). Add a channel in
     * CollisionChannelList.h. The value is also the filter bit index (at most 64; append only).
     */
    enum class ECollisionChannel : Uint8
    {
        #define OPAAX_COLLISION_CHANNEL(Name) Name,
        #include "Physics/Collision/CollisionChannelList.h"
        #undef OPAAX_COLLISION_CHANNEL
        Count
    };

    static_assert(static_cast<Uint8>(ECollisionChannel::Count) <= 64,
                  "ECollisionChannel exceeds the 64-bit collision filter category limit.");

    // =============================================================================
    // Labels + values
    // =============================================================================
    /** The channel's label, for logs and dropdowns. */
    inline const char* ToString(ECollisionChannel InChannel) noexcept
    {
        switch (InChannel)
        {
            #define OPAAX_COLLISION_CHANNEL(Name) case ECollisionChannel::Name: return #Name;
            #include "Physics/Collision/CollisionChannelList.h"
            #undef OPAAX_COLLISION_CHANNEL
            default: return "Unknown";
        }
    }

    /**
     * The channels as data, for editor dropdowns and saving by name. Written out by hand
     * (a #include cannot go inside OPAAX_ENUM_VALUES). Count is not a channel.
     */
    template<>
    struct TEnumValues<ECollisionChannel>
    {
        static constexpr ECollisionChannel Values[] =
        {
            #define OPAAX_COLLISION_CHANNEL(Name) ECollisionChannel::Name,
            #include "Physics/Collision/CollisionChannelList.h"
            #undef OPAAX_COLLISION_CHANNEL
        };
    };

    /** Name of each channel. Index with static_cast<Uint8>(ECollisionChannel). */
    inline const OpaaxStringID g_CollisionChannelIDs[] =
    {
        #define OPAAX_COLLISION_CHANNEL(Name) OPAAX_ID(#Name),
        #include "Physics/Collision/CollisionChannelList.h"
        #undef OPAAX_COLLISION_CHANNEL
    };

    /** Channel -> name id. */
    inline const OpaaxStringID& ToStringID(ECollisionChannel InChannel) noexcept
    {
        const Uint8 lIdx = static_cast<Uint8>(InChannel);
        return lIdx < static_cast<Uint8>(ECollisionChannel::Count)
                   ? g_CollisionChannelIDs[lIdx]
                   : g_CollisionChannelIDs[0];
    }

    /** Name id -> channel (linear search). */
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
    /** The category bit of a collider on this channel. */
    inline Uint64 CategoryBit(ECollisionChannel InChannel) noexcept
    {
        return Uint64(1) << static_cast<Uint8>(InChannel);
    }

    /** Every channel bit (collides with everything). */
    inline constexpr Uint64 AllChannelsMask() noexcept { return ~0ull; }
}
