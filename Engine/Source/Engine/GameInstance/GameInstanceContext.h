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
    // GameInstanceContext — what a game gives each of its subsystems (GameInstanceSubsystem::GetContext).
    //   One per game, valid for the whole game.
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
