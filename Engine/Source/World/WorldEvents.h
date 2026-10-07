#pragma once

#include "Core/Events/Delegate.h"

namespace Opaax
{
    class World;

    // =============================================================================
    // WorldEvents.h — world lifetime events.
    //   Delegates (below) are broadcast by WorldManager; Engine re-publishes them on the
    //   EventBus as the structs below. Adding an event = adding a struct.
    //   The startup world is created after Engine binds these, so it is reported too.
    // =============================================================================

    // =============================================================================
    // EventBus payloads. They hold a raw World*, valid only during dispatch, so they are
    // published immediately (never enqueued).
    // =============================================================================

    /** A world was created (owned by WorldManager). Never null. */
    struct WorldCreated
    {
        World* NewWorld = nullptr;
    };

    /**
     * A world is about to be destroyed; still safe to use. Drop any state kept for it.
     */
    struct WorldDestroyed
    {
        World* DyingWorld = nullptr;
    };

    /**
     * The active world changed. OldWorld is null on the first activation; NewWorld is null when
     * the active world is destroyed. Never both null.
     */
    struct ActiveWorldChanged
    {
        World* OldWorld = nullptr;
        World* NewWorld = nullptr;
    };

    /** The active world's tick was paused or resumed (WorldManager::SetPaused, the editor's Pause). */
    struct WorldPauseChanged
    {
        bool bPaused = false;
    };

    // =============================================================================
    // Delegates broadcast by WorldManager (safe to bind/unbind during a broadcast).
    // =============================================================================
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnWorldCreated, World* /*NewWorld*/)
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnWorldDestroyed, World* /*DyingWorld*/)
    DECLARE_MULTICAST_DELEGATE_TwoParams(FOnActiveWorldChanged, World* /*Old*/, World* /*New*/)
}
