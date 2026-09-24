#pragma once

namespace Opaax
{
    // =============================================================================
    // EngineEvents.h — Tier-3 bus payloads for the ENGINE's own lifetime, the shape
    // WorldEvents.h uses for worlds. POD, trivially copyable (EventBus asserts).
    //
    // Both are published with Publish (immediate), never Enqueue: the queue is drained
    // inside Engine::Loop, which has already stopped by TearDown, so an enqueued
    // teardown event would never be delivered (LC2). Nothing to flush, either.
    // =============================================================================

    /**
     * Every engine subsystem is constructed and started.
     * NO WORLD EXISTS YET (BO4) — a listener that needs one takes WorldCreated instead.
     */
    struct EngineStarted {};

    /**
     * The frame loop has stopped and the engine is about to tear its subsystems down.
     * Everything is still alive here — subsystems, app services, window, GPU context —
     * and that is precisely why this fires in TearDown and not in Shutdown, where a
     * sibling may already be gone (LC1).
     */
    struct EngineTearingDown {};

    /**
     * A level swap has been ASKED FOR (IEngine::RequestOpenLevel) and will happen at the start of
     * the next frame. Published at once, mid-frame, so a cover can be drawn over THIS frame
     * (**UI21**). The active world is still the old one.
     */
    struct LevelLoadRequested {};

    /** The swap is done: the new world is active and the old one is gone. The cover comes down. */
    struct LevelLoadFinished {};
}
