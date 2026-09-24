#pragma once

#include "Core/Events/Delegate.h"

namespace Opaax
{
    class World;

    // =============================================================================
    // WorldEvents.h — world lifetime events, in both tiers that carry them.
    //
    // NOTE: none of these get an EEventType entry, and none ever should. That enum is
    // Tier-1 only (OS/window/input), closed on purpose so EventDispatcher can match on
    // an integer compare instead of RTTI on the per-keystroke path — see the comment on
    // EEventType itself. Domain events live in the OPEN tiers:
    //
    //   Tier-2 (TMulticastDelegate) — WorldManager owns the delegates below and
    //     Broadcasts them. It depends on nothing; listeners come to it.
    //   Tier-3 (EventBus)           — Engine binds those delegates and re-publishes the
    //     POD payloads below, keyed by a compile-time hash of the type name. Adding a
    //     world event = adding a struct here. No central registration, ever.
    //
    // Engine is the only bridge between the two (same shape as OpaaxApplication turning a
    // Tier-1 WindowResize into a bus payload — the host bridges, never the producer).
    //
    // NOTE: the startup world is created in FinishStartup, AFTER Engine::Startup has bound
    // these (BO4), so it does reach subscribers. A listener binding later still calls
    // WorldManager::GetActiveWorld() rather than relying on having witnessed the creation.
    // =============================================================================

    // =============================================================================
    // Tier-3 bus payloads — POD, trivially copyable (EventBus asserts this).
    //
    // These carry a raw World*, which is only valid for the duration of the dispatch,
    // so Engine bridges them with Publish (immediate) and never Enqueue. See the bridge
    // in Engine::Startup for the full reasoning.
    // =============================================================================

    /** A world was just created and is owned by WorldManager. Never null. */
    struct WorldCreated
    {
        World* NewWorld = nullptr;
    };

    /**
     * A world is about to be removed from WorldManager. Published BEFORE the erase, so
     * DyingWorld is still safe to dereference — that is the window in which a listener
     * drops any per-world state it cached.
     */
    struct WorldDestroyed
    {
        World* DyingWorld = nullptr;
    };

    /**
     * The active (rendered) world changed.
     * OldWorld is null on the first activation; NewWorld is null when the active world
     * is being destroyed. Both are never null at once.
     */
    struct ActiveWorldChanged
    {
        World* OldWorld = nullptr;
        World* NewWorld = nullptr;
    };

    // =============================================================================
    // Tier-2 delegate surface — the members WorldManager broadcasts.
    //
    // Broadcast is synchronous and iterates a snapshot, so a listener may bind/unbind
    // during dispatch and can never be handed a freed World*.
    //
    // F-prefixed so the alias never collides with the member it types
    // (FOnActiveWorldChanged OnActiveWorldChanged;).
    // =============================================================================
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnWorldCreated, World* /*NewWorld*/)
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnWorldDestroyed, World* /*DyingWorld*/)
    DECLARE_MULTICAST_DELEGATE_TwoParams(FOnActiveWorldChanged, World* /*Old*/, World* /*New*/)
}
