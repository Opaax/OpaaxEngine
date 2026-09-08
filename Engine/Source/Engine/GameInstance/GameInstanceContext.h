#pragma once

namespace Opaax
{
    class WorldManager;
    class ResourceManager;
    class EngineEventBus;
    class InputManager;
    class IPaths;
    struct EngineConfigData;

    // =============================================================================
    // GameInstanceContext — the flat struct of references a game-instance subsystem is
    //   constructed with. WorldContext (World/Systems/WorldContext.h) one tier up, and for
    //   the same reason: the registration site is GameInstanceSubsystems().Register<T>(),
    //   which takes no arguments, so a subsystem needing an engine sibling has nowhere else
    //   to be handed one — and reaching the AppServiceLocator is what D3 forbids.
    //
    // ONE PER GAME, owned by the GameInstance, so a subsystem may store
    //   GameInstanceContext& and it stays valid exactly as long as the game does.
    //
    // WHY THESE SIX. A game session outlives any single world, so it holds WorldManager
    //   rather than a World: "which world is current" is a question it asks, never a
    //   reference it caches. Everything else is what a session-scoped tenant needs to load
    //   its own assets, react to engine events and read raw input.
    //
    // NOT INCLUDED: EngineRegistries (type metadata, MR0 — not a running subsystem's
    //   business), and DebugDraw/FrameProfiler, which are per-frame drawing and per-frame
    //   measurement. Add a member when something has a caller for it, exactly as
    //   WorldContext grew Paths, Config and Input one at a time.
    // =============================================================================
    struct GameInstanceContext
    {
        /**
         * Every world, and which one is active. NOT a World& — a game outlives its worlds
         * (level travel destroys one and creates another), so caching one would dangle.
         */
        WorldManager& Worlds;

        /** Load-by-path resources. Engine-owned, shared by every world and every game. */
        ResourceManager& Resources;

        /**
         * Asset-relative -> absolute, which is what ResourceManager::Load needs and what a
         * TResourcePath deliberately is not (MP8). Const: a session asks where things are.
         */
        const IPaths& Paths;

        /** The engine bus. A session reacts to engine/world events without touching the locator. */
        EngineEventBus& Events;

        /**
         * This frame's raw keys and mouse. The layer that turns them into MEANING is a tenant
         * of this context (InputMappingSubsystem), not something the context does itself.
         *
         * Nothing here clears it — the frame boundary is the host loop's (IN2) and stays there.
         */
        const InputManager& Input;

        /**
         * The engine's boot configuration, read-only — a session is configured BY it, it never
         * reconfigures the project.
         */
        const EngineConfigData& Config;
    };
}
