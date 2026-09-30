#pragma once

namespace Opaax
{
    // =============================================================================
    // Engine lifetime events (EventBus). Published immediately, never enqueued:
    // the queue is no longer flushed during teardown.
    // =============================================================================

    /**
     * Every engine subsystem has started. No world exists yet (use WorldCreated for that).
     */
    struct EngineStarted {};

    /**
     * The frame loop stopped and the engine is about to shut down. Everything is still alive.
     */
    struct EngineTearingDown {};

    /**
     * A level change was requested (IEngine::RequestOpenLevel); it happens at the start of the
     * next frame. The old world is still active. Use it to show a loading screen.
     */
    struct LevelLoadRequested {};

    /** The level change is done: the new world is active and the old one is gone. */
    struct LevelLoadFinished {};
}
