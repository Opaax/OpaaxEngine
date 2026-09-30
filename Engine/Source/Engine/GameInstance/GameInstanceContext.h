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
    // GameInstanceContext — references given to each game-instance subsystem at construction.
    //   One per game; a subsystem may keep a reference to it for the whole game.
    // =============================================================================
    struct GameInstanceContext
    {
        /**
         * Every world, and the active one. A game outlives its worlds, so never cache a World.
         */
        WorldManager& Worlds;

        /** Resources, shared by every world and game. */
        ResourceManager& Resources;

        /**
         * Asset-relative -> absolute paths (needed by ResourceManager::Load).
         */
        const IPaths& Paths;

        /** The engine event bus. */
        EngineEventBus& Events;

        /**
         * This frame's raw keyboard and mouse state.
         */
        const InputManager& Input;

        /**
         * The engine config (read-only).
         */
        const EngineConfigData& Config;
    };
}
