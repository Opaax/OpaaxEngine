#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxStringID.hpp"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // ECollisionChannel — X-Macro driven (Physics/Collision/CollisionChannelList.h)
    // =============================================================================
    /**
     * @enum ECollisionChannel
     * Object-type category of a collider, the unit a CollisionProfile filters against.
     * The enum body and the matching g_CollisionChannelIDs[] are both generated from
     * CollisionChannelList.h — adding a channel means a single new line in that list.
     *
     * The ordinal is also the Box2D category-bit index (see CategoryBit), so the set is
     * capped at 64 and must only ever be appended to — reordering renumbers saved scenes.
     */
    enum class ECollisionChannel : Uint8
    {
        #define OPAAX_COLLISION_CHANNEL(Name) Name,
        #include "CollisionChannelList.h"
        #undef OPAAX_COLLISION_CHANNEL
        Count
    };

    static_assert(static_cast<Uint8>(ECollisionChannel::Count) <= 64,
                  "ECollisionChannel exceeds the 64-bit Box2D filter category limit.");

    // =============================================================================
    // Channel name LUT
    // =============================================================================
    /*** Parallel canonical-name array. Index by static_cast<Uint8>(ECollisionChannel). */
    inline const OpaaxStringID g_CollisionChannelIDs[] =
    {
        #define OPAAX_COLLISION_CHANNEL(Name) OPAAX_ID(#Name),
        #include "CollisionChannelList.h"
        #undef OPAAX_COLLISION_CHANNEL
    };

    /*** ECollisionChannel -> canonical OpaaxStringID. O(1) LUT. */
    inline const OpaaxStringID& ToStringID(ECollisionChannel InChannel) noexcept
    {
        const Uint8 lIdx = static_cast<Uint8>(InChannel);
        return (lIdx < static_cast<Uint8>(ECollisionChannel::Count)) ? g_CollisionChannelIDs[lIdx] : g_CollisionChannelIDs[0];
    }

    /*** OpaaxStringID -> ECollisionChannel. Linear scan; pure integer compare per slot. */
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
    /*** The single category bit a collider on this channel belongs to (1 << ordinal). */
    inline Uint64 CategoryBit(ECollisionChannel InChannel) noexcept
    {
        return Uint64(1) << static_cast<Uint8>(InChannel);
    }
}
